#pragma once

#include "System/Window.h"
#include "Core/InputManager.h"
#include "Resources/ResourceManager.h"
#include "Graphics/Renderer.h"
#include "AudioMixer.h"
#include "Events/EventDispatcher.h"
#include "Core/GameData.h"
#include "Core/SceneManager.h"
#include "Core/GameRules.h"
#include "Core/Timer.h"
#include "Core/GameContext.h"
#include "CheatManager.h"
#include "UI/AchievementToastOverlay.h"

class Application
{
public:
    explicit Application(const WindowConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run();
    bool RunSmokeTest();

private:
    void Init();

    Window m_window;
    InputManager m_inputManager;
    ResourceManager m_resourceManager;
    Renderer m_renderer;
    AudioMixer m_audioMixer;
    EventDispatcher m_eventDispatcher;
    GameData m_gameData;
    SceneManager m_sceneManager;
    GameRules m_rules;
    Timer m_time;

    GameContext m_context;
    CheatManager m_cheatManager;
    AchievementToastOverlay m_achievementToast;
};
