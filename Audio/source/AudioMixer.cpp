#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
#include <SFML/Audio.hpp>
#include <list>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <memory>

struct SoundInstance
{
    uint32_t soundId = 0;
    sf::Sound sound;
};

struct MusicTrack
{
    std::unique_ptr<sf::Music> music;
    std::string path;
    float baseVolume = 100.0f;
    float currentVolume = 100.0f;
    std::unique_ptr<Tween<float>> volumeTween;
    bool isLooping = false;
};

struct AudioMixer::Impl
{
    ResourceManager& resourceManager;
    std::list<SoundInstance> sfxPool;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastTriggerTime;

    // Ducking state: lower music by 30-50% during heavy streaks, restoring over 0.5s
    bool duckingEnabled = true;
    float duckingMultiplier = 1.0f;      // 1.0 = normal, e.g. 0.6 = lowered by 40%
    float duckingHoldTimer = 0.0f;       // hold duration during active streak
    float duckingRestoreDuration = 0.5f; // restore smoothly over 0.5s

    // Streak detection
    static constexpr std::chrono::milliseconds kStreakWindow{350};
    std::deque<std::chrono::steady_clock::time_point> recentSfxTriggers;

    MusicTrack activeTrack;
    MusicTrack fadingTrack;

    float masterVolume = 100.0f;
    float sfxVolume = 100.0f;
    float musicVolume = 100.0f;

    static constexpr size_t kMaxConcurrentVoicesPerSound = 3;
    static constexpr std::chrono::milliseconds kMinTriggerCooldown{30};

    explicit Impl(ResourceManager& rm) : resourceManager(rm) {}

    void CleanUpSfx()
    {
        sfxPool.remove_if([](const SoundInstance& item) {
            return item.sound.getStatus() == sf::SoundSource::Status::Stopped;
        });
    }

    [[nodiscard]] float GetEffectiveMusicVolume(float rawVolume) const
    {
        return rawVolume * (musicVolume / 100.0f) * (masterVolume / 100.0f) * duckingMultiplier;
    }
};

AudioMixer::AudioMixer(ResourceManager& resourceManager)
    : mp_impl(std::make_unique<Impl>(resourceManager)) {}

AudioMixer::~AudioMixer()
{
    StopAll();
}

AudioMixer::AudioMixer(AudioMixer&&) noexcept = default;
AudioMixer& AudioMixer::operator=(AudioMixer&&) noexcept = default;

void AudioMixer::Update(float dt) const
{
    if (!mp_impl) return;

    mp_impl->CleanUpSfx();

    // 0. Ducking recovery
    if (mp_impl->duckingHoldTimer > 0.0f)
    {
        mp_impl->duckingHoldTimer -= dt;
    }
    else if (mp_impl->duckingMultiplier < 1.0f)
    {
        float speed = 1.0f / (mp_impl->duckingRestoreDuration > 0.0f ? mp_impl->duckingRestoreDuration : 0.5f);
        mp_impl->duckingMultiplier = std::min(1.0f, mp_impl->duckingMultiplier + speed * dt);
    }

    // 1. Active track tween update
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->activeTrack.volumeTween)
        {
            mp_impl->activeTrack.volumeTween->Update(dt);
            if (mp_impl->activeTrack.volumeTween && mp_impl->activeTrack.volumeTween->IsFinished())
            {
                mp_impl->activeTrack.volumeTween.reset();
            }
        }
        else
        {
            // Continuously apply effective volume (handles ducking and master/music volume changes)
            mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->activeTrack.currentVolume));
        }
    }
    else if (mp_impl->activeTrack.music &&
             mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Stopped)
    {
        mp_impl->activeTrack.music.reset();
        mp_impl->activeTrack = MusicTrack{};
    }

    // 2. Fading-out track tween update
    if (mp_impl->fadingTrack.music &&
        mp_impl->fadingTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->fadingTrack.volumeTween)
        {
            mp_impl->fadingTrack.volumeTween->Update(dt);
            if (mp_impl->fadingTrack.volumeTween && mp_impl->fadingTrack.volumeTween->IsFinished())
            {
                mp_impl->fadingTrack.volumeTween.reset();
                if (mp_impl->fadingTrack.music)
                {
                    mp_impl->fadingTrack.music->stop();
                    mp_impl->fadingTrack.music.reset();
                }
                mp_impl->fadingTrack = MusicTrack{};
            }
        }
        else
        {
            mp_impl->fadingTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->fadingTrack.currentVolume));
        }
    }
    else if (mp_impl->fadingTrack.music &&
             mp_impl->fadingTrack.music->getStatus() == sf::SoundSource::Status::Stopped)
    {
        mp_impl->fadingTrack.music.reset();
        mp_impl->fadingTrack = MusicTrack{};
    }
}

void AudioMixer::PlaySfx(uint32_t soundId, float volume, float pitch, float pan) const
{
    if (!mp_impl) return;

    sf::SoundBuffer* buffer = mp_impl->resourceManager.Get<sf::SoundBuffer>(soundId);
    if (!buffer) return;

    mp_impl->CleanUpSfx();

    const auto now = std::chrono::steady_clock::now();

    // 1. Cooldown check per soundId to prevent acoustic harshness on simultaneous triggers
    if (auto it = mp_impl->lastTriggerTime.find(soundId); it != mp_impl->lastTriggerTime.end())
    {
        if (now - it->second < Impl::kMinTriggerCooldown)
        {
            return;
        }
    }

    // 2. Voice limiter: Count concurrent playing instances of this soundId
    size_t count = 0;
    for (const auto& item : mp_impl->sfxPool)
    {
        if (item.soundId == soundId && item.sound.getStatus() == sf::SoundSource::Status::Playing)
        {
            count++;
        }
    }

    // If max concurrent voices reached, steal the oldest playing voice of this soundId
    if (count >= Impl::kMaxConcurrentVoicesPerSound)
    {
        for (auto it = mp_impl->sfxPool.begin(); it != mp_impl->sfxPool.end(); ++it)
        {
            if (it->soundId == soundId && it->sound.getStatus() == sf::SoundSource::Status::Playing)
            {
                it->sound.stop();
                mp_impl->sfxPool.erase(it);
                break;
            }
        }
    }

    if (mp_impl->sfxPool.size() > 64) return;

    mp_impl->lastTriggerTime[soundId] = now;

    // 3. Audio ducking on explosion / heavy SFX streaks
    if (mp_impl->duckingEnabled)
    {
        while (!mp_impl->recentSfxTriggers.empty() &&
               (now - mp_impl->recentSfxTriggers.front()) > Impl::kStreakWindow)
        {
            mp_impl->recentSfxTriggers.pop_front();
        }
        mp_impl->recentSfxTriggers.push_back(now);

        // Streak trigger: >= 2 triggers in streak window, or loud sound with multiple active voices
        if (mp_impl->recentSfxTriggers.size() >= 2 || (volume >= 85.0f && mp_impl->sfxPool.size() >= 2))
        {
            TriggerDucking(0.4f, 0.5f);
        }
    }

    float finalVolume = volume * (mp_impl->sfxVolume / 100.0f) * (mp_impl->masterVolume / 100.0f);
    float clampedPan = std::clamp(pan, -1.0f, 1.0f);

    mp_impl->sfxPool.emplace_back(SoundInstance{soundId, sf::Sound(*buffer)});

    sf::Sound& newSound = mp_impl->sfxPool.back().sound;
    newSound.setVolume(finalVolume);
    newSound.setPitch(pitch);
    newSound.setPan(clampedPan);
    newSound.play();
}

void AudioMixer::StopSfx() const
{
    if (!mp_impl) return;
    for (auto& item : mp_impl->sfxPool)
    {
        item.sound.stop();
    }
    mp_impl->sfxPool.clear();
    mp_impl->lastTriggerTime.clear();
    mp_impl->recentSfxTriggers.clear();
}

void AudioMixer::PlayMusic(const std::string& filepath, float volume, bool loop) const
{
    if (!mp_impl) return;

    // If the exact same music is ALREADY playing, simply update volume and looping without interrupting
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.path == filepath &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->activeTrack.baseVolume = volume;
        mp_impl->activeTrack.currentVolume = volume;
        mp_impl->activeTrack.volumeTween.reset();
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(volume));
        return;
    }

    StopMusic();

    auto newMusic = std::make_unique<sf::Music>();
    if (newMusic->openFromFile(filepath))
    {
        mp_impl->activeTrack.music = std::move(newMusic);
        mp_impl->activeTrack.path = filepath;
        mp_impl->activeTrack.baseVolume = volume;
        mp_impl->activeTrack.currentVolume = volume;
        mp_impl->activeTrack.volumeTween.reset();
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(volume));
        mp_impl->activeTrack.music->play();
    }
    else
    {
        std::cerr << "[AudioMixer] Warning: Failed to open music file \"" << filepath << "\".\n";
    }
}

void AudioMixer::FadeToMusic(const std::string& filepath, float fadeDuration, float volume, bool loop) const
{
    if (fadeDuration <= 0.05f)
    {
        PlayMusic(filepath, volume, loop);
        return;
    }

    TweenConfig<float> inConfig;
    inConfig.Start = 0.0f;
    inConfig.End = volume;
    inConfig.Duration = fadeDuration;
    inConfig.Ease = EasingFunctions::EasingType::EaseInOutSine;
    FadeToMusic(filepath, inConfig, loop);
}

void AudioMixer::FadeToMusic(const std::string& filepath, const TweenConfig<float>& inConfig, bool loop) const
{
    if (!mp_impl) return;

    // If the exact same music is ALREADY playing, adjust volume smoothly with TweenConfig
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.path == filepath &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->activeTrack.baseVolume = inConfig.End;
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);

        TweenConfig<float> config = inConfig;
        config.Start = mp_impl->activeTrack.currentVolume;
        config.Setter = [this](float vol) {
            if (mp_impl && mp_impl->activeTrack.music)
            {
                mp_impl->activeTrack.currentVolume = vol;
                mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(vol));
            }
        };
        mp_impl->activeTrack.volumeTween = std::make_unique<Tween<float>>(config);
        return;
    }

    // 1. Move currently active music to fadingTrack to crossfade it out using TweenConfig
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->fadingTrack.music)
        {
            mp_impl->fadingTrack.music->stop();
            mp_impl->fadingTrack.music.reset();
        }
        mp_impl->fadingTrack = std::move(mp_impl->activeTrack);
        mp_impl->activeTrack = MusicTrack{};

        TweenConfig<float> outConfig;
        outConfig.Start = mp_impl->fadingTrack.currentVolume;
        outConfig.End = 0.0f;
        outConfig.Duration = inConfig.Duration;
        outConfig.Ease = inConfig.Ease;
        outConfig.Setter = [this](float vol) {
            if (mp_impl && mp_impl->fadingTrack.music)
            {
                mp_impl->fadingTrack.currentVolume = vol;
                mp_impl->fadingTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(vol));
            }
        };
        mp_impl->fadingTrack.volumeTween = std::make_unique<Tween<float>>(outConfig);
    }
    else
    {
        mp_impl->activeTrack = MusicTrack{};
    }

    // 2. Open and fade in new track using TweenConfig
    auto newMusic = std::make_unique<sf::Music>();
    if (newMusic->openFromFile(filepath))
    {
        mp_impl->activeTrack.music = std::move(newMusic);
        mp_impl->activeTrack.path = filepath;
        mp_impl->activeTrack.baseVolume = inConfig.End;
        mp_impl->activeTrack.currentVolume = inConfig.Start;
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(inConfig.Start));
        mp_impl->activeTrack.music->play();

        TweenConfig<float> activeConfig = inConfig;
        activeConfig.Setter = [this](float vol) {
            if (mp_impl && mp_impl->activeTrack.music)
            {
                mp_impl->activeTrack.currentVolume = vol;
                mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(vol));
            }
        };
        mp_impl->activeTrack.volumeTween = std::make_unique<Tween<float>>(activeConfig);
    }
    else
    {
        std::cerr << "[AudioMixer] Warning: Failed to open music file \"" << filepath << "\".\n";
    }
}

void AudioMixer::FadeOutMusic(float fadeDuration) const
{
    if (fadeDuration <= 0.05f)
    {
        StopMusic();
        return;
    }

    TweenConfig<float> config;
    config.Start = mp_impl ? mp_impl->activeTrack.currentVolume : 100.0f;
    config.End = 0.0f;
    config.Duration = fadeDuration;
    config.Ease = EasingFunctions::EasingType::EaseInOutSine;
    FadeOutMusic(config);
}

void AudioMixer::FadeOutMusic(const TweenConfig<float>& config) const
{
    if (!mp_impl) return;

    if (!mp_impl->activeTrack.music ||
        mp_impl->activeTrack.music->getStatus() != sf::SoundSource::Status::Playing)
    {
        return;
    }

    if (config.Duration <= 0.05f)
    {
        StopMusic();
        return;
    }

    if (mp_impl->fadingTrack.music)
    {
        mp_impl->fadingTrack.music->stop();
        mp_impl->fadingTrack.music.reset();
    }

    mp_impl->fadingTrack = std::move(mp_impl->activeTrack);
    mp_impl->activeTrack = MusicTrack{};

    TweenConfig<float> outConfig = config;
    outConfig.Start = mp_impl->fadingTrack.currentVolume;
    outConfig.End = 0.0f;
    outConfig.Setter = [this](float vol) {
        if (mp_impl && mp_impl->fadingTrack.music)
        {
            mp_impl->fadingTrack.currentVolume = vol;
            mp_impl->fadingTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(vol));
        }
    };
    mp_impl->fadingTrack.volumeTween = std::make_unique<Tween<float>>(outConfig);
}

void AudioMixer::FadeMusicVolume(float targetVolume, float duration, EasingFunctions::EasingType ease) const
{
    if (!mp_impl || !mp_impl->activeTrack.music) return;
    if (mp_impl->activeTrack.music->getStatus() != sf::SoundSource::Status::Playing) return;

    TweenConfig<float> config;
    config.Start = mp_impl->activeTrack.currentVolume;
    config.End = targetVolume;
    config.Duration = duration;
    config.Ease = ease;
    config.Setter = [this](float vol) {
        if (mp_impl && mp_impl->activeTrack.music)
        {
            mp_impl->activeTrack.currentVolume = vol;
            mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(vol));
        }
    };
    mp_impl->activeTrack.volumeTween = std::make_unique<Tween<float>>(config);
}

void AudioMixer::StopMusic() const
{
    if (!mp_impl) return;

    if (mp_impl->activeTrack.music)
    {
        mp_impl->activeTrack.music->stop();
        mp_impl->activeTrack.music.reset();
    }
    mp_impl->activeTrack = MusicTrack{};

    if (mp_impl->fadingTrack.music)
    {
        mp_impl->fadingTrack.music->stop();
        mp_impl->fadingTrack.music.reset();
    }
    mp_impl->fadingTrack = MusicTrack{};
}

void AudioMixer::TriggerDucking(float duckAmount, float restoreDuration) const
{
    if (!mp_impl || !mp_impl->duckingEnabled) return;

    // Lower by 30-50% (default 0.40 = 40% reduction -> multiplier 0.60)
    float targetMultiplier = std::clamp(1.0f - duckAmount, 0.2f, 1.0f);
    mp_impl->duckingMultiplier = std::min(mp_impl->duckingMultiplier, targetMultiplier);
    mp_impl->duckingHoldTimer = 0.15f; // hold ducked while explosions continue
    mp_impl->duckingRestoreDuration = (restoreDuration > 0.0f) ? restoreDuration : 0.5f;

    if (mp_impl->activeTrack.music && mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->activeTrack.currentVolume));
    }
}

void AudioMixer::SetDuckingEnabled(bool enabled) const
{
    if (!mp_impl) return;
    mp_impl->duckingEnabled = enabled;
    if (!enabled)
    {
        mp_impl->duckingMultiplier = 1.0f;
        mp_impl->duckingHoldTimer = 0.0f;
    }
}

bool AudioMixer::IsDuckingEnabled() const
{
    return mp_impl && mp_impl->duckingEnabled;
}

float AudioMixer::GetDuckingMultiplier() const
{
    return mp_impl ? mp_impl->duckingMultiplier : 1.0f;
}

void AudioMixer::SetMasterVolume(float volume) const
{
    if (!mp_impl) return;
    mp_impl->masterVolume = volume;
    SetMusicVolume(mp_impl->musicVolume);

    for (auto& item : mp_impl->sfxPool)
    {
        item.sound.setVolume(mp_impl->sfxVolume * (mp_impl->masterVolume / 100.0f));
    }
}

void AudioMixer::SetSfxVolume(float volume) const
{
    if (!mp_impl) return;
    mp_impl->sfxVolume = volume;
}

void AudioMixer::SetMusicVolume(float volume) const
{
    if (!mp_impl) return;
    mp_impl->musicVolume = volume;
    if (mp_impl->activeTrack.music && mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->activeTrack.currentVolume));
    }
    if (mp_impl->fadingTrack.music && mp_impl->fadingTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->fadingTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->fadingTrack.currentVolume));
    }
}

void AudioMixer::StopAll() const
{
    StopSfx();
    StopMusic();
}

bool AudioMixer::IsMusicPlaying() const
{
    if (!mp_impl) return false;
    if (mp_impl->activeTrack.music && mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
        return true;
    if (mp_impl->fadingTrack.music && mp_impl->fadingTrack.music->getStatus() == sf::SoundSource::Status::Playing)
        return true;
    return false;
}

const std::string& AudioMixer::GetCurrentMusicPath() const
{
    static const std::string kEmpty;
    if (!mp_impl) return kEmpty;
    return mp_impl->activeTrack.path;
}

float AudioMixer::GetMusicTimeRemaining() const
{
    if (!mp_impl || !mp_impl->activeTrack.music) return -1.0f;
    if (mp_impl->activeTrack.music->getStatus() != sf::SoundSource::Status::Playing) return -1.0f;
    if (mp_impl->activeTrack.isLooping) return 999999.0f;

    float dur = mp_impl->activeTrack.music->getDuration().asSeconds();
    float offset = mp_impl->activeTrack.music->getPlayingOffset().asSeconds();
    return std::max(0.0f, dur - offset);
}

float AudioMixer::GetSfxVolume() const
{
    if (!mp_impl) return 0.0f;
    return mp_impl->sfxVolume;
}

float AudioMixer::GetMusicVolume() const
{
    if (!mp_impl) return 0.0f;
    return mp_impl->musicVolume;
}

std::vector<std::string> AudioMixer::GetAvailableAudioDevices() const
{
    return sf::PlaybackDevice::getAvailableDevices();
}

std::string AudioMixer::GetCurrentAudioDevice() const
{
    auto dev = sf::PlaybackDevice::getDevice();
    return dev.value_or("Default Device");
}

bool AudioMixer::SetAudioDevice(const std::string& name) const
{
    return sf::PlaybackDevice::setDevice(name);
}