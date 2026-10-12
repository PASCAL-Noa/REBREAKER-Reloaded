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

    m_bassem = context.Resources.LoadResource("Resources/audio/sfx/ToDelete/bassem.mp3");
    m_putine = context.Resources.LoadResource("Resources/audio/sfx/ToDelete/putine.mp3");
    m_electric_zoo = context.Resources.LoadResource("Resources/audio/sfx/ToDelete/electric-zoo.mp3");
    m_faaah = context.Resources.LoadResource("Resources/audio/sfx/ToDelete/faaah.mp3");
    m_brickHit = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");
    m_combo = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");

    m_systemManager.AddSystem<AudioSystem>(m_registry, context.Audio);

    m_musicEntity = m_registry.CreateEntity();
    auto& music = m_registry.AddComponent<AudioSource>(m_musicEntity);
    music.MusicPath = "Resources/audio/music/Game-1.ogg";
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

    // Master Volume Controls
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

    // Horizontal Stereo Panning Controls
    if (input.IsKeyDown(KeyCode::Left) || input.IsKeyDown(KeyCode::Q))
    {
        m_pan = std::clamp(m_pan - 1.5f * dt, -1.0f, 1.0f);
    }
    if (input.IsKeyDown(KeyCode::Right) || input.IsKeyDown(KeyCode::D))
    {
        m_pan = std::clamp(m_pan + 1.5f * dt, -1.0f, 1.0f);
    }
    if (input.IsKeyPress(KeyCode::J)) m_pan = -1.0f; // Quick Left
    if (input.IsKeyPress(KeyCode::K)) m_pan = 0.0f;  // Quick Center
    if (input.IsKeyPress(KeyCode::L)) m_pan = 1.0f;  // Quick Right

    // Voice Limiter Test: rapid spam trigger with [SPACE]
    if (input.IsKeyDown(KeyCode::Space))
    {
        context.Audio.PlaySfx(m_brickHit, 100.0f, 1.0f, m_pan);
        m_spamCount++;
    }

    // Individual SFX triggers using stereo panning
    if (m_sfxCooldown <= 0.0f)
    {
        if (input.IsKeyDown(KeyCode::Num1))
        {
            PlaySound(m_bassem, m_pan);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num2))
        {
            PlaySound(m_putine, m_pan);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num3))
        {
            PlaySound(m_electric_zoo, m_pan);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num4))
        {
            PlaySound(m_faaah, m_pan);
            m_sfxCooldown = 0.2f;
        }
        else if (input.IsKeyDown(KeyCode::Num5))
        {
            context.Audio.PlaySfx(m_brickHit, 100.0f, 1.0f, m_pan);
            m_sfxCooldown = 0.15f;
        }
        else if (input.IsKeyDown(KeyCode::Num6))
        {
            context.Audio.PlaySfx(m_combo, 100.0f, 1.0f, m_pan);
            m_sfxCooldown = 0.15f;
        }
    }

    auto& music = m_registry.GetComponent<AudioSource>(m_musicEntity);
    if (input.IsKeyDown(KeyCode::M)) music.PlayRequested = true;
    if (input.IsKeyDown(KeyCode::P)) music.StopRequested = true;

    m_systemManager.OnUpdate(dt);
}

void SampleAudio::PlaySound(uint32_t soundId, float pan)
{
    auto& sfx = m_registry.GetComponent<AudioSource>(m_sfxEntity);
    sfx.SoundId = soundId;
    sfx.PlayRequested = true;
}

void SampleAudio::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    std::string panDesc;
    if (m_pan < -0.3f)      panDesc = " [LEFT]";
    else if (m_pan > 0.3f)  panDesc = " [RIGHT]";
    else                    panDesc = " [CENTER]";

    char panStr[32];
    std::snprintf(panStr, sizeof(panStr), "%.2f", m_pan);

    std::string uiText = "[UP/DOWN] Volume : " + std::to_string(static_cast<int>(m_currentVolume)) + "%\n";
    uiText += "[LEFT/RIGHT or J/K/L] Stereo Pan : " + std::string(panStr) + panDesc + "\n";
    uiText += "[SPACE (Hold)] Spam Brick Hit (Voice Limiter Test, spam triggers: " + std::to_string(m_spamCount) + ")\n";
    uiText += "[1-4] SFX Fun (Bassem, Putine, Zoo, Faaah)\n";
    uiText += "[5] SFX Brick Hit | [6] SFX Combo (Panned)\n";
    uiText += "[M] Play Musique | [P] Stop Musique";

    DrawDefaultUI(context, "SAMPLE : AUDIO", uiText);
}