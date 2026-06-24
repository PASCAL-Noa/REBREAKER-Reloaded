#include "Core/GameRules.h"
#include "Core/PlayerPrefs.h"
#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
#include "AudioMixer.h"
#include "Events/EventDispatcher.h"
#include "Scenes/MenuScene.h"
#include "System/Window.h"
#include "Core/InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "Core/SceneManager.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Scenes/Sandbox.h"
#include "Core/Timer.h"
#include "Core/Debug.h"
#include "AudioMixer.h"
#include "CheatManager.h"

int main()
{
    Debug::Init();
    PlayerPrefs::Load();
    Debug::Info("Engine initialized successfully.");

    WindowConfig config{};
    config.Width = 2560;
    config.Height = 1600;
    config.Mode = WindowMode::Fullscreen;
    Window window{config};
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);
    AudioMixer audioMixer(resourceManager);
    
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 100.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 100.0f);
    audioMixer.SetSfxVolume(sfxVol);
    audioMixer.SetMusicVolume(musicVol);

    EventDispatcher eventDispatcher;

    GameData gameData;
    SceneManager sceneManager;
    GameRules rules;
    Timer time(window.GetConfig().VSync ? 0 : window.GetConfig().MaxFPS);

    GameContext context{ inputManager, renderer, resourceManager, gameData, sceneManager, audioMixer, eventDispatcher, rules, time };
    
    CheatManager cheatManager(context);

    context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", true, RuleAccess::Public);
    context.Rules.RegisterRule(Rule::Graphics::EnableParticles, "Toggle Particles", true, RuleAccess::Public);
    context.Rules.RegisterRule(Rule::Debug::ShowCollider, "Toggle Hitbox", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::Invincible, "God mod", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::InfiniteLives, "Infinite lives", false, RuleAccess::Private);

    sceneManager.LoadScene<MenuScene>();

    while (window.IsOpen())
    {
        time.Update();
        gameData.FPS = static_cast<int>(time.GetFPS());

        inputManager.Update();
        if (!window.PollEvents(inputManager)) break;

        cheatManager.Update(time.GetDeltaTime(), context);

        sceneManager.Update(time.GetDeltaTime(), context);

        renderer.BeginDraw(sceneManager.GetClearColor());
        sceneManager.Render(context);
        
        cheatManager.Render(renderer);

        renderer.EndDraw(sceneManager.GetPostProcessShader());
    }

    sceneManager.Shutdown(context);

    return 0;
}