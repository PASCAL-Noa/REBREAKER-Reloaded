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
    : m_impl(new Impl(resourceManager)) {}

AudioMixer::~AudioMixer()
{
    StopAll();
    delete m_impl;
}

void AudioMixer::PlaySfx(uint32_t soundId, float volume, float pitch) const
{
    sf::SoundBuffer* buffer = m_impl->resourceManager.Get<sf::SoundBuffer>(soundId);
    if (!buffer) return;

    m_impl->CleanUpSfx();

    if (m_impl->sfxPool.size() > 64) return;

    float finalVolume = volume * (m_impl->sfxVolume / 100.0f) * (m_impl->masterVolume / 100.0f);

    m_impl->sfxPool.emplace_back(*buffer);

    sf::Sound& newSound = m_impl->sfxPool.back();
    newSound.setVolume(finalVolume);
    newSound.setPitch(pitch);
    newSound.play();
}

void AudioMixer::PlayMusic(const std::string& filepath, float volume, bool loop) const
{
    if (m_impl->music.openFromFile(filepath))
    {
        float finalVolume = volume * (m_impl->musicVolume / 100.0f) * (m_impl->masterVolume / 100.0f);
        m_impl->music.setVolume(finalVolume);
        m_impl->music.setLooping(loop);
        m_impl->music.play();
    }
}

void AudioMixer::StopMusic() const
{
    m_impl->music.stop();
}

void AudioMixer::SetMasterVolume(float volume) const
{
    m_impl->masterVolume = volume;
    SetMusicVolume(m_impl->musicVolume);

    for (sf::Sound& sound : m_impl->sfxPool)
    {
        sound.setVolume(m_impl->sfxVolume * (m_impl->masterVolume / 100.0f));
    }
}

void AudioMixer::SetSfxVolume(float volume) const
{
    m_impl->sfxVolume = volume;
}

void AudioMixer::SetMusicVolume(float volume) const
{
    m_impl->musicVolume = volume;
    float finalVolume = m_impl->musicVolume * (m_impl->masterVolume / 100.0f);
    m_impl->music.setVolume(finalVolume);
}

void AudioMixer::StopAll() const
{
    m_impl->sfxPool.clear();
    m_impl->music.stop();
}