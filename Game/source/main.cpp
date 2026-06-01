#include "Scenes/MenuScene.h"
#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include "System/Window.h"
#include "InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "Core/SceneManager.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Scenes/Sandbox.h"
#include "Timer.h"
#include "Core/Debug.h"

int main()
{
    Debug::Init();
    Debug::Info("Engine initialized successfully.");

    Window window{};
    InputManager inputManager;
    ResourceManager resourceManager;
    Renderer renderer(window, resourceManager);

    GameData gameData;
    SceneManager sceneManager;
    GameContext context{ inputManager, renderer, resourceManager, gameData, sceneManager };

    sceneManager.LoadScene<MenuScene>();

    Timer time(window.GetConfig().VSync ? 0 : window.GetConfig().MaxFPS);

    while (window.IsOpen())
    {
        time.Update();
        gameData.FPS = static_cast<int>(time.GetFPS());

        inputManager.Update();
        if (!window.PollEvents(inputManager)) break;

        sceneManager.Update(time.GetDeltaTime(), context);

        renderer.BeginDraw();
        sceneManager.Render(context);
        renderer.EndDraw(sceneManager.GetPostProcessShader());
    }

    sceneManager.Shutdown(context);

    return 0;
}