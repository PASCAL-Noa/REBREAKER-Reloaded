#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
#include <SFML/Audio.hpp>
#include <list>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <iostream>

struct SoundInstance
{
    uint32_t soundId = 0;
    sf::Sound sound;
};

struct AudioMixer::Impl
{
    ResourceManager& resourceManager;
    std::list<SoundInstance> sfxPool;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastTriggerTime;
    sf::Music music;

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
};

AudioMixer::AudioMixer(ResourceManager& resourceManager)
    : mp_impl(std::make_unique<Impl>(resourceManager)) {}

AudioMixer::~AudioMixer()
{
    StopAll();
}

AudioMixer::AudioMixer(AudioMixer&&) noexcept = default;
AudioMixer& AudioMixer::operator=(AudioMixer&&) noexcept = default;

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

void AudioMixer::PlayMusic(const std::string& filepath, float volume, bool loop) const
{
    if (!mp_impl) return;

    if (mp_impl->music.openFromFile(filepath))
    {
        float finalVolume = volume * (mp_impl->musicVolume / 100.0f) * (mp_impl->masterVolume / 100.0f);
        mp_impl->music.setVolume(finalVolume);
        mp_impl->music.setLooping(loop);
        mp_impl->music.play();
    }
    else
    {
        std::cerr << "[AudioMixer] Warning: Failed to open music file \"" << filepath << "\".\n";
    }
}

void AudioMixer::StopMusic() const
{
    if (!mp_impl) return;
    mp_impl->music.stop();
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
    float finalVolume = mp_impl->musicVolume * (mp_impl->masterVolume / 100.0f);
    mp_impl->music.setVolume(finalVolume);
}

void AudioMixer::StopAll() const
{
    if (!mp_impl) return;
    mp_impl->sfxPool.clear();
    mp_impl->lastTriggerTime.clear();
    mp_impl->music.stop();
}

bool AudioMixer::IsMusicPlaying() const
{
    if (!mp_impl) return false;
    return mp_impl->music.getStatus() == sf::SoundSource::Status::Playing;
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