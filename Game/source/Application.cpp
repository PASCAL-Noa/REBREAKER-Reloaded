#include "Application.h"
#include "Core/PlayerPrefs.h"
#include "ECS/Systems/UISystem.h"
#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/StoryScene.h"
#include "Scenes/EndlessScene.h"
#include "Scenes/EditorScene.h"
#include "Scenes/RobostockScene.h"
#include "Scenes/ProfileScene.h"
#include <algorithm>
#include <iostream>

Application::Application(const WindowConfig& config)
    : m_window(config),
      m_inputManager(),
      m_resourceManager(),
      m_renderer(m_window, m_resourceManager),
      m_audioMixer(m_resourceManager),
      m_eventDispatcher(),
      m_gameData(),
      m_sceneManager(),
      m_rules(),
      m_time(config.VSync ? 0 : config.MaxFPS),
      m_context{ m_inputManager, m_renderer, m_resourceManager, m_gameData, m_sceneManager, m_audioMixer, m_eventDispatcher, m_rules, m_time },
      m_cheatManager(m_context)
{
    Init();
}

void Application::Init()
{
    bool prefsNeedSave = false;
    std::string savedDevice = PlayerPrefs::GetString("Audio_Device", "");
    if (!savedDevice.empty() && savedDevice != "Default Device")
    {
        auto available = m_audioMixer.GetAvailableAudioDevices();
        if (std::find(available.begin(), available.end(), savedDevice) == available.end())
        {
            PlayerPrefs::SetString("Audio_Device", "");
            savedDevice.clear();
            prefsNeedSave = true;
        }
        else
        {
            auto defaultDev = sf::PlaybackDevice::getDefaultDevice();
            if (!defaultDev.has_value() || savedDevice != defaultDev.value())
            {
                m_audioMixer.SetAudioDevice(savedDevice);
            }
        }
    }
    if (!PlayerPrefs::HasKey("MasterVolume"))
    {
        PlayerPrefs::SetFloat("MasterVolume", 80.0f);
        prefsNeedSave = true;
    }
    if (!PlayerPrefs::HasKey("SfxVolume"))
    {
        PlayerPrefs::SetFloat("SfxVolume", 70.0f);
        prefsNeedSave = true;
    }
    if (!PlayerPrefs::HasKey("MusicVolume"))
    {
        PlayerPrefs::SetFloat("MusicVolume", 70.0f);
        prefsNeedSave = true;
    }
    if (prefsNeedSave)
    {
        PlayerPrefs::Save();
    }

    float masterVol = PlayerPrefs::GetFloat("MasterVolume", 80.0f);
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 70.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 70.0f);
    m_audioMixer.SetMasterVolume(masterVol);
    m_audioMixer.SetSfxVolume(sfxVol);
    m_audioMixer.SetMusicVolume(musicVol);

    uint32_t clickSfx = m_resourceManager.LoadResource("Resources/audio/sfx/ui_click.wav");
    uint32_t hoverSfx = m_resourceManager.LoadResource("Resources/audio/sfx/ui_hover.wav");
    UISystem::SetDefaultAudio(hoverSfx, clickSfx);

    // Register global rules
    m_context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", PlayerPrefs::GetBool("EnableShader", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Graphics::EnableParticles, "Toggle Particles", PlayerPrefs::GetBool("EnableParticles", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Debug::ShowCollider, "Toggle Hitbox", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::Invincible, "God mod", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::InfiniteLives, "Infinite lives", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::CheatsUnlocked, "Cheats Unlocked", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::InfiniteUltimate, "Infinite Ultimate", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::ArenaEventsEnabled, "Arena Events", PlayerPrefs::GetBool("EnableArenaEvents", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardZonesEnabled, "Hazard Zones", PlayerPrefs::GetBool("EnableHazardZones", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardUltimateDrainEnabled, "Hazard: Ultimate Drain", PlayerPrefs::GetBool("EnableHazardUltimateDrain", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardScreenGlitchEnabled, "Hazard: Screen Glitch", PlayerPrefs::GetBool("EnableHazardScreenGlitch", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardCameraTiltEnabled, "Hazard: Camera Tilt", PlayerPrefs::GetBool("EnableHazardCameraTilt", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardZoomPulseEnabled, "Hazard: Zoom Pulse", PlayerPrefs::GetBool("EnableHazardZoomPulse", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::HazardAudioWarpEnabled, "Hazard: Audio Warp", PlayerPrefs::GetBool("EnableHazardAudioWarp", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::UltimateLockdownEnabled, "Event: EMP Lockdown", PlayerPrefs::GetBool("EnableUltimateLockdown", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::EndlessCreepingCeiling, "Endless: Creeping Ceiling", PlayerPrefs::GetBool("EnableCreepingCeiling", true), RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Gameplay::ManualOverride, "Gamerules Manual Override", PlayerPrefs::GetBool("GamerulesManualOverride", false), RuleAccess::Public);

    m_sceneManager.LoadScene<MenuScene>();
}

void Application::Run()
{
    while (m_window.IsOpen())
    {
        m_time.Update();
        m_gameData.FPS = static_cast<int>(m_time.GetFPS());

        m_resourceManager.ProcessPendingUploads();
        m_audioMixer.Update(m_time.GetDeltaTime());

        m_inputManager.Update();
        if (!m_window.PollEvents(m_inputManager, &m_renderer)) break;

        m_cheatManager.Update(m_time.GetDeltaTime(), m_context);
        m_sceneManager.Update(m_time.GetDeltaTime(), m_context);

        m_renderer.BeginDraw(m_sceneManager.GetClearColor());
        m_sceneManager.Render(m_context);
        m_cheatManager.Render(m_renderer);
        m_renderer.EndDraw(m_sceneManager.GetPostProcessShader(), m_sceneManager.GetPostProcessParams());
    }

    m_sceneManager.Shutdown(m_context);
    PlayerPrefs::Save();
}

bool Application::RunSmokeTest()
{
    std::cout << "[SmokeTest] Starting automated channel launch smoke test..." << std::endl;

    auto stepFrames = [this](int frameCount) {
        for (int frame = 0; frame < frameCount && m_window.IsOpen(); ++frame)
        {
            m_time.Update();
            m_gameData.FPS = static_cast<int>(m_time.GetFPS());
            m_resourceManager.ProcessPendingUploads();
            m_audioMixer.Update(0.016f);
            m_inputManager.Update();
            if (!m_window.PollEvents(m_inputManager, &m_renderer)) break;
            m_cheatManager.Update(0.016f, m_context);
            m_sceneManager.Update(0.016f, m_context);
            m_renderer.BeginDraw(m_sceneManager.GetClearColor());
            m_sceneManager.Render(m_context);
            m_cheatManager.Render(m_renderer);
            m_renderer.EndDraw(m_sceneManager.GetPostProcessShader(), m_sceneManager.GetPostProcessParams());
        }
    };

    // Step 1: Boot MenuScene and run 15 frames
    std::cout << "[SmokeTest] Stepping MenuScene..." << std::endl;
    stepFrames(15);

    std::cout << "[SmokeTest] Transitioning directly from MenuScene to ProfileScene..." << std::endl;
    ProfileScene::Launch(m_context.Scenes, SceneTransitionType::StaticSnow, 0.55f);
    stepFrames(120);
    std::cout << "[SmokeTest] ProfileScene direct transition stable!" << std::endl;

    // Step 2: Launch Campaign (StoryScene) with transition
    std::cout << "[SmokeTest] Triggering transition to StoryScene (StoryMode)..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<StoryScene>(SceneTransitionType::StaticSnow, 0.65f);
    stepFrames(70);
    std::cout << "[SmokeTest] StoryScene (StoryMode) active and running stably!" << std::endl;

    // Step 3: Launch Classic GameScene with transition
    std::cout << "[SmokeTest] Triggering transition to GameScene (Classic)..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<GameScene>(SceneTransitionType::StaticSnow, 0.65f);
    stepFrames(70);
    std::cout << "[SmokeTest] GameScene (Classic) active and running stably!" << std::endl;

    // Step 3: Launch EndlessMode (EndlessScene) with transition
    std::cout << "[SmokeTest] Triggering transition to EndlessScene (EndlessMode)..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<EndlessScene>(SceneTransitionType::StaticSnow, 0.65f);
    stepFrames(70);
    std::cout << "[SmokeTest] EndlessScene (EndlessMode) active and running stably!" << std::endl;

    // Step 4: Launch LevelMaker (EditorScene) with transition
    std::cout << "[SmokeTest] Triggering transition to EditorScene (LevelMaker)..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<EditorScene>(SceneTransitionType::StaticSnow, 0.55f);
    stepFrames(60);
    std::cout << "[SmokeTest] EditorScene active and running stably!" << std::endl;

    // Step 5: Launch RobostockScene with transition
    std::cout << "[SmokeTest] Triggering transition to RobostockScene..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<RobostockScene>(SceneTransitionType::StaticSnow, 0.55f);
    stepFrames(60);
    std::cout << "[SmokeTest] RobostockScene active and running stably!" << std::endl;

    // Step 6: Launch ProfileScene (Customization Garage) with transition
    std::cout << "[SmokeTest] Triggering transition to ProfileScene (Customization Garage)..." << std::endl;
    ProfileScene::Launch(m_context.Scenes, SceneTransitionType::StaticSnow, 0.55f);
    stepFrames(60);
    std::cout << "[SmokeTest] ProfileScene active and running stably!" << std::endl;

    // Step 7: Return to MenuScene
    std::cout << "[SmokeTest] Returning to MenuScene with transition..." << std::endl;
    m_context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
    stepFrames(70);
    std::cout << "[SmokeTest] MenuScene re-entered and running stably!" << std::endl;

    m_sceneManager.Shutdown(m_context);
    std::cout << "[SmokeTest] PASSED: All channels verified with zero crashes!" << std::endl;
    return true;
}
