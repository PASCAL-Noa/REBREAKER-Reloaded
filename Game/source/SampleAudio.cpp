#include "Scenes/SampleAudio.h"
#include "ECS/Components/AudioSource.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "Resources/ResourceManager.h"
#include "AudioMixer.h"
#include "ECS/Systems/AudioSystem.h"
#include <algorithm>

void SampleAudio::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_bassem = context.Resources.LoadResource("Resources/Audio/Sound/ToDelete/bassem.mp3");
    m_putine = context.Resources.LoadResource("Resources/Audio/Sound/ToDelete/putine.mp3");
    m_electric_zoo = context.Resources.LoadResource("Resources/Audio/Sound/ToDelete/electric-zoo.mp3");
    m_faaah = context.Resources.LoadResource("Resources/Audio/Sound/ToDelete/faaah.mp3");

    m_systemManager.AddSystem<AudioSystem>(m_registry, context.Audio);

    m_musicEntity = m_registry.CreateEntity();
    auto& music = m_registry.AddComponent<AudioSource>(m_musicEntity);
    music.MusicPath = "Resources/Audio/Music/Game-1.ogg";
    music.IsMusic = true;
    music.Loop = true;
    music.PlayRequested = true;

    m_sfxEntity = m_registry.CreateEntity();
    m_registry.AddComponent<AudioSource>(m_sfxEntity);

    m_systemManager.OnInit();
}

void SampleAudio::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    if (m_sfxCooldown > 0.0f) m_sfxCooldown -= dt;

    const InputManager& input = context.Input;

    if (input.IsKeyDown(KeyCode::Up) || input.IsKeyDown(KeyCode::Z))
    {
        m_currentVolume = std::min(m_currentVolume + 50.0f * dt, 100.0f);
        context.Audio.SetMasterVolume(m_currentVolume);
    }
    if (input.IsKeyDown(KeyCode::Down) || input.IsKeyDown(KeyCode::S))
    {
        m_currentVolume = std::max(m_currentVolume - 50.0f * dt, 0.0f);
        context.Audio.SetMasterVolume(m_currentVolume);
    }

    if (m_sfxCooldown <= 0.0f)
    {
        if (input.IsKeyDown(KeyCode::Num1))
        {
            PlaySound(m_bassem);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num2))
        {
            PlaySound(m_putine);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num3))
        {
            PlaySound(m_electric_zoo);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num4))
        {
            PlaySound(m_faaah);
            m_sfxCooldown = 0.2f;
        }
    }

    auto& music = m_registry.GetComponent<AudioSource>(m_musicEntity);
    if (input.IsKeyDown(KeyCode::M)) music.PlayRequested = true;
    if (input.IsKeyDown(KeyCode::P)) music.StopRequested = true;

    m_systemManager.OnUpdate(dt);
}

void SampleAudio::PlaySound(uint32_t soundId)
{
    auto& sfx = m_registry.GetComponent<AudioSource>(m_sfxEntity);
    sfx.SoundId = soundId;
    sfx.PlayRequested = true;
}

void SampleAudio::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    std::string uiText = "[UP/DOWN] Volume : " + std::to_string(static_cast<int>(m_currentVolume)) + "%\n";
    uiText += "[1] SFX : Bassem\n";
    uiText += "[2] SFX : Putine\n";
    uiText += "[3] SFX : Electric Zoo\n";
    uiText += "[4] SFX : Faaah\n";
    uiText += "[M] Play Musique | [P] Stop Musique";

    DrawDefaultUI(context, "SAMPLE AUDIO", uiText);
}