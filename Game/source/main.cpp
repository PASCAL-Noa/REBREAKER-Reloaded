#include "Core/GameRules.h"
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

int main()
{
    Debug::Init();
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
    EventDispatcher eventDispatcher;

    GameData gameData;
    SceneManager sceneManager;
    GameRules rules;
    Timer time(window.GetConfig().VSync ? 0 : window.GetConfig().MaxFPS);

    GameContext context{ inputManager, renderer, resourceManager, gameData, sceneManager, audioMixer, eventDispatcher, rules, time };

    sceneManager.LoadScene<MenuScene>();

    while (window.IsOpen())
    {
        time.Update();
        gameData.FPS = static_cast<int>(time.GetFPS());

        inputManager.Update();
        if (!window.PollEvents(inputManager)) break;

        sceneManager.Update(time.GetDeltaTime(), context);

        renderer.BeginDraw(sceneManager.GetClearColor());
        sceneManager.Render(context);
        renderer.EndDraw(sceneManager.GetPostProcessShader());
    }

    sceneManager.Shutdown(context);

    return 0;
}