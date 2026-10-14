#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
#include <SFML/Audio.hpp>
#include <list>
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
    float startVolume = 100.0f;
    float targetVolume = 100.0f;
    float fadeDuration = 0.0f;
    float fadeTimer = 0.0f;
    bool isFading = false;
    bool isLooping = false;
};

struct AudioMixer::Impl
{
    ResourceManager& resourceManager;
    std::list<SoundInstance> sfxPool;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastTriggerTime;

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
        return rawVolume * (musicVolume / 100.0f) * (masterVolume / 100.0f);
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

    // 1. Active track fade update
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->activeTrack.isFading && mp_impl->activeTrack.fadeDuration > 0.0f)
        {
            mp_impl->activeTrack.fadeTimer += dt;
            float t = std::clamp(mp_impl->activeTrack.fadeTimer / mp_impl->activeTrack.fadeDuration, 0.0f, 1.0f);
            mp_impl->activeTrack.currentVolume = mp_impl->activeTrack.startVolume + (mp_impl->activeTrack.targetVolume - mp_impl->activeTrack.startVolume) * t;
            if (t >= 1.0f)
            {
                mp_impl->activeTrack.isFading = false;
                mp_impl->activeTrack.currentVolume = mp_impl->activeTrack.targetVolume;
            }
        }
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->activeTrack.currentVolume));
    }
    else if (mp_impl->activeTrack.music &&
             mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Stopped)
    {
        mp_impl->activeTrack.music.reset();
        mp_impl->activeTrack = MusicTrack{};
    }

    // 2. Fading-out track update
    if (mp_impl->fadingTrack.music &&
        mp_impl->fadingTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->fadingTrack.isFading && mp_impl->fadingTrack.fadeDuration > 0.0f)
        {
            mp_impl->fadingTrack.fadeTimer += dt;
            float t = std::clamp(mp_impl->fadingTrack.fadeTimer / mp_impl->fadingTrack.fadeDuration, 0.0f, 1.0f);
            mp_impl->fadingTrack.currentVolume = mp_impl->fadingTrack.startVolume + (mp_impl->fadingTrack.targetVolume - mp_impl->fadingTrack.startVolume) * t;
            if (t >= 1.0f)
            {
                mp_impl->fadingTrack.isFading = false;
                mp_impl->fadingTrack.music->stop();
                mp_impl->fadingTrack.music.reset();
                mp_impl->fadingTrack = MusicTrack{};
            }
            else
            {
                mp_impl->fadingTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(mp_impl->fadingTrack.currentVolume));
            }
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
        mp_impl->activeTrack.targetVolume = volume;
        mp_impl->activeTrack.currentVolume = volume;
        mp_impl->activeTrack.isFading = false;
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(volume));
        return;
    }

    // Stop any existing tracks
    if (mp_impl->fadingTrack.music)
    {
        mp_impl->fadingTrack.music->stop();
        mp_impl->fadingTrack.music.reset();
        mp_impl->fadingTrack = MusicTrack{};
    }
    if (mp_impl->activeTrack.music)
    {
        mp_impl->activeTrack.music->stop();
        mp_impl->activeTrack.music.reset();
        mp_impl->activeTrack = MusicTrack{};
    }

    auto newMusic = std::make_unique<sf::Music>();
    if (newMusic->openFromFile(filepath))
    {
        mp_impl->activeTrack.music = std::move(newMusic);
        mp_impl->activeTrack.path = filepath;
        mp_impl->activeTrack.baseVolume = volume;
        mp_impl->activeTrack.currentVolume = volume;
        mp_impl->activeTrack.startVolume = volume;
        mp_impl->activeTrack.targetVolume = volume;
        mp_impl->activeTrack.fadeDuration = 0.0f;
        mp_impl->activeTrack.fadeTimer = 0.0f;
        mp_impl->activeTrack.isFading = false;
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
    if (!mp_impl) return;

    // If the exact same music is ALREADY playing, don't restart or cut
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.path == filepath &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        mp_impl->activeTrack.baseVolume = volume;
        mp_impl->activeTrack.targetVolume = volume;
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        if (mp_impl->activeTrack.isFading && mp_impl->activeTrack.targetVolume < mp_impl->activeTrack.startVolume)
        {
            mp_impl->activeTrack.isFading = false;
            mp_impl->activeTrack.currentVolume = volume;
            mp_impl->activeTrack.music->setVolume(mp_impl->GetEffectiveMusicVolume(volume));
        }
        return;
    }

    if (fadeDuration <= 0.05f)
    {
        PlayMusic(filepath, volume, loop);
        return;
    }

    // Move currently active music to fadingTrack to crossfade it out
    if (mp_impl->activeTrack.music &&
        mp_impl->activeTrack.music->getStatus() == sf::SoundSource::Status::Playing)
    {
        if (mp_impl->fadingTrack.music)
        {
            mp_impl->fadingTrack.music->stop();
            mp_impl->fadingTrack.music.reset();
        }
        mp_impl->fadingTrack = std::move(mp_impl->activeTrack);
        mp_impl->fadingTrack.startVolume = mp_impl->fadingTrack.currentVolume;
        mp_impl->fadingTrack.targetVolume = 0.0f;
        mp_impl->fadingTrack.fadeDuration = fadeDuration;
        mp_impl->fadingTrack.fadeTimer = 0.0f;
        mp_impl->fadingTrack.isFading = true;
    }
    else
    {
        mp_impl->activeTrack = MusicTrack{};
    }

    auto newMusic = std::make_unique<sf::Music>();
    if (newMusic->openFromFile(filepath))
    {
        mp_impl->activeTrack.music = std::move(newMusic);
        mp_impl->activeTrack.path = filepath;
        mp_impl->activeTrack.baseVolume = volume;
        mp_impl->activeTrack.currentVolume = 0.0f;
        mp_impl->activeTrack.startVolume = 0.0f;
        mp_impl->activeTrack.targetVolume = volume;
        mp_impl->activeTrack.fadeDuration = fadeDuration;
        mp_impl->activeTrack.fadeTimer = 0.0f;
        mp_impl->activeTrack.isFading = true;
        mp_impl->activeTrack.isLooping = loop;
        mp_impl->activeTrack.music->setLooping(loop);
        mp_impl->activeTrack.music->setVolume(0.0f);
        mp_impl->activeTrack.music->play();
    }
    else
    {
        std::cerr << "[AudioMixer] Warning: Failed to open music file \"" << filepath << "\".\n";
    }
}

void AudioMixer::FadeOutMusic(float fadeDuration) const
{
    if (!mp_impl) return;

    if (!mp_impl->activeTrack.music ||
        mp_impl->activeTrack.music->getStatus() != sf::SoundSource::Status::Playing)
    {
        return;
    }

    if (fadeDuration <= 0.05f)
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
    mp_impl->fadingTrack.startVolume = mp_impl->fadingTrack.currentVolume;
    mp_impl->fadingTrack.targetVolume = 0.0f;
    mp_impl->fadingTrack.fadeDuration = fadeDuration;
    mp_impl->fadingTrack.fadeTimer = 0.0f;
    mp_impl->fadingTrack.isFading = true;

    mp_impl->activeTrack = MusicTrack{};
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