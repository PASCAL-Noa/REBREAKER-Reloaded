#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
#include <SFML/Audio.hpp>
#include <list>

struct AudioMixer::Impl
{
    ResourceManager& resourceManager;
    std::list<sf::Sound> sfxPool;
    sf::Music music;

    float masterVolume = 100.0f;
    float sfxVolume = 100.0f;
    float musicVolume = 100.0f;

    explicit Impl(ResourceManager& rm) : resourceManager(rm) {}

    void CleanUpSfx()
    {
        sfxPool.remove_if([](const sf::Sound& sound) {
            return sound.getStatus() == sf::SoundSource::Status::Stopped;
        });
    }
};

AudioMixer::AudioMixer(ResourceManager& resourceManager)
    : mp_impl(new Impl(resourceManager)) {}

AudioMixer::~AudioMixer()
{
    StopAll();
    delete mp_impl;
}

void AudioMixer::PlaySfx(uint32_t soundId, float volume, float pitch) const
{
    sf::SoundBuffer* buffer = mp_impl->resourceManager.Get<sf::SoundBuffer>(soundId);
    if (!buffer) return;

    mp_impl->CleanUpSfx();

    if (mp_impl->sfxPool.size() > 64) return;

    float finalVolume = volume * (mp_impl->sfxVolume / 100.0f) * (mp_impl->masterVolume / 100.0f);

    mp_impl->sfxPool.emplace_back(*buffer);

    sf::Sound& newSound = mp_impl->sfxPool.back();
    newSound.setVolume(finalVolume);
    newSound.setPitch(pitch);
    newSound.play();
}

void AudioMixer::PlayMusic(const std::string& filepath, float volume, bool loop) const
{
    if (mp_impl->music.openFromFile(filepath))
    {
        float finalVolume = volume * (mp_impl->musicVolume / 100.0f) * (mp_impl->masterVolume / 100.0f);
        mp_impl->music.setVolume(finalVolume);
        mp_impl->music.setLooping(loop);
        mp_impl->music.play();
    }
}

void AudioMixer::StopMusic() const
{
    mp_impl->music.stop();
}

void AudioMixer::SetMasterVolume(float volume) const
{
    mp_impl->masterVolume = volume;
    SetMusicVolume(mp_impl->musicVolume);

    for (sf::Sound& sound : mp_impl->sfxPool)
    {
        sound.setVolume(mp_impl->sfxVolume * (mp_impl->masterVolume / 100.0f));
    }
}

void AudioMixer::SetSfxVolume(float volume) const
{
    mp_impl->sfxVolume = volume;
}

void AudioMixer::SetMusicVolume(float volume) const
{
    mp_impl->musicVolume = volume;
    float finalVolume = mp_impl->musicVolume * (mp_impl->masterVolume / 100.0f);
    mp_impl->music.setVolume(finalVolume);
}

void AudioMixer::StopAll() const
{
    mp_impl->sfxPool.clear();
    mp_impl->music.stop();
}

bool AudioMixer::IsMusicPlaying() const
{
    return mp_impl->music.getStatus() == sf::SoundSource::Status::Playing;
}

float AudioMixer::GetSfxVolume() const
{
    return mp_impl->sfxVolume;
}

float AudioMixer::GetMusicVolume() const
{
    return mp_impl->musicVolume;
}