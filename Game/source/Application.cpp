#include "Application.h"
#include "Core/PlayerPrefs.h"
#include "Scenes/MenuScene.h"

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
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 100.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 100.0f);
    m_audioMixer.SetSfxVolume(sfxVol);
    m_audioMixer.SetMusicVolume(musicVol);

    // Register global rules
    m_context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", true, RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Graphics::EnableParticles, "Toggle Particles", true, RuleAccess::Public);
    m_context.Rules.RegisterRule(Rule::Debug::ShowCollider, "Toggle Hitbox", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::Invincible, "God mod", false, RuleAccess::Private);
    m_context.Rules.RegisterRule(Rule::Gameplay::InfiniteLives, "Infinite lives", false, RuleAccess::Private);

    m_sceneManager.LoadScene<MenuScene>();
}

void Application::Run()
{
    while (m_window.IsOpen())
    {
        m_time.Update();
        m_gameData.FPS = static_cast<int>(m_time.GetFPS());

        m_inputManager.Update();
        if (!m_window.PollEvents(m_inputManager)) break;

        m_cheatManager.Update(m_time.GetDeltaTime(), m_context);

        m_sceneManager.Update(m_time.GetDeltaTime(), m_context);

        m_renderer.BeginDraw(m_sceneManager.GetClearColor());
        m_sceneManager.Render(m_context);
        
        m_cheatManager.Render(m_renderer);

        m_renderer.EndDraw(m_sceneManager.GetPostProcessShader());
    }

    m_sceneManager.Shutdown(m_context);
}
