#include "ECS/Systems/AudioSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/AudioSource.h"
#include "AudioMixer.h"

AudioSystem::AudioSystem(Registry& registry, AudioMixer& mixer)
    : System(registry), m_mixer(mixer)
{
}

void AudioSystem::OnUpdate(float dt)
{
    m_registry.View<AudioSource>([this](Entity, AudioSource& audio)
    {
        if (audio.PlayRequested)
        {
            if (audio.IsMusic)
            {
                m_mixer.PlayMusic(audio.MusicPath, audio.Volume, audio.Loop);
            }
            else
            {
                m_mixer.PlaySfx(audio.SoundId, audio.Volume, audio.Pitch);
            }
            audio.PlayRequested = false;
        }

        if (audio.StopRequested)
        {
            if (audio.IsMusic)
            {
                m_mixer.StopMusic();
            }
            audio.StopRequested = false;
        }
    });
}