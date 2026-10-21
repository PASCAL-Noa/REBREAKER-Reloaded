#include "Core/SceneManager.h"
#include "Core/GameContext.h"
#include "Graphics/Renderer.h"
#include "ECS/Systems/UISystem.h"
#include <algorithm>

SceneManager::SceneManager()
{
    m_defaultTransitionConfig = SceneTransitionConfig{
        .Type = SceneTransitionType::FadeBlack,
        .Duration = 0.4f,
        .FadeColor = Colors::Black,
        .Ease = EasingFunctions::EasingType::EaseInOutQuad
    };
    m_transitionConfig = m_defaultTransitionConfig;
}

SceneManager::~SceneManager()
{
}

void SceneManager::Shutdown(GameContext& context)
{
    if (m_currentScene)
    {
        m_currentScene->OnDestroy(context);
        m_currentScene.reset();
    }
    m_pendingSceneFactory = nullptr;
    m_transitionTween.reset();
    m_transitionPhase = SceneTransitionPhase::Idle;
    m_transitionProgress = 0.0f;
    m_midpointReached = false;
    m_uncoverCompleted = false;
    m_transitionPending = false;
    UISystem::SetNavigationEnabled(true);
}

void SceneManager::SetDefaultTransition(const SceneTransitionConfig& config)
{
    m_defaultTransitionConfig = config;
}

void SceneManager::SetDefaultTransition(SceneTransitionType type, float duration, Color color, EasingFunctions::EasingType ease)
{
    m_defaultTransitionConfig = SceneTransitionConfig{
        .Type = type,
        .Duration = duration,
        .FadeColor = color,
        .Ease = ease
    };
}

void SceneManager::StartCoveringTween(float startProgress)
{
    m_transitionPhase = SceneTransitionPhase::Covering;
    m_transitionProgress = startProgress;
    m_midpointReached = false;
    m_uncoverCompleted = false;
    UISystem::SetNavigationEnabled(false);

    const float remainingFraction = std::max(0.01f, 1.0f - startProgress);
    const float duration = (m_transitionConfig.Duration > 0.0f)
        ? (m_transitionConfig.Duration * 0.5f * remainingFraction)
        : 0.001f;

    TweenConfig<float> config{
        .Start = startProgress,
        .End = 1.0f,
        .Duration = duration,
        .Setter = [this](float val) { m_transitionProgress = val; },
        .OnComplete = [this]() {
            m_transitionProgress = 1.0f;
            m_midpointReached = true;
        },
        .Yoyo = false,
        .Ease = m_transitionConfig.Ease
    };
    m_transitionTween = std::make_unique<Tween<float>>(config);
}

void SceneManager::StartUncoveringTween()
{
    m_transitionPhase = SceneTransitionPhase::Uncovering;
    m_transitionProgress = 1.0f;
    m_midpointReached = false;
    m_uncoverCompleted = false;

    const float duration = (m_transitionConfig.Duration > 0.0f)
        ? (m_transitionConfig.Duration * 0.5f)
        : 0.001f;

    TweenConfig<float> config{
        .Start = 1.0f,
        .End = 0.0f,
        .Duration = duration,
        .Setter = [this](float val) { m_transitionProgress = val; },
        .OnComplete = [this]() {
            m_transitionProgress = 0.0f;
            m_transitionPhase = SceneTransitionPhase::Idle;
            m_uncoverCompleted = true;
        },
        .Yoyo = false,
        .Ease = m_transitionConfig.Ease
    };
    m_transitionTween = std::make_unique<Tween<float>>(config);
}

void SceneManager::Update(float dt, GameContext& context)
{
    if (m_transitionPending)
    {
        m_transitionPending = false;

        // If no scene is currently active (e.g. boot), initialize immediately
        if (!m_currentScene)
        {
            if (m_pendingSceneFactory)
            {
                m_currentScene = m_pendingSceneFactory();
                m_pendingSceneFactory = nullptr;
                m_currentScene->OnInit(context);
            }
            m_transitionPhase = SceneTransitionPhase::Idle;
            m_transitionProgress = 0.0f;
            m_transitionTween.reset();
            UISystem::SetNavigationEnabled(true);
        }
        else if (m_transitionConfig.Type == SceneTransitionType::None || m_transitionConfig.Duration <= 0.0f)
        {
            // Instant scene switch
            m_currentScene->OnDestroy(context);
            if (m_pendingSceneFactory)
            {
                m_currentScene = m_pendingSceneFactory();
                m_pendingSceneFactory = nullptr;
            }
            if (m_currentScene)
            {
                m_currentScene->OnInit(context);
            }
            m_transitionPhase = SceneTransitionPhase::Idle;
            m_transitionProgress = 0.0f;
            m_transitionTween.reset();
            UISystem::SetNavigationEnabled(true);
        }
        else
        {
            // If already covering, keep existing progress; otherwise start from current progress (handles uncovering interrupts)
            StartCoveringTween(m_transitionProgress);
        }
    }

    if (m_transitionTween)
    {
        m_transitionTween->Update(dt);
    }

    if (m_midpointReached)
    {
        m_midpointReached = false;
        m_transitionProgress = 1.0f;
        m_transitionTween.reset();

        // Screen is 100% masked: perform teardown and init safely behind overlay
        if (m_currentScene)
        {
            m_currentScene->OnDestroy(context);
            m_currentScene.reset();
        }

        if (m_pendingSceneFactory)
        {
            m_currentScene = m_pendingSceneFactory();
            m_pendingSceneFactory = nullptr;
        }

        if (m_currentScene)
        {
            m_currentScene->OnInit(context);
        }

        StartUncoveringTween();
    }

    if (m_uncoverCompleted)
    {
        m_uncoverCompleted = false;
        m_transitionTween.reset();
        UISystem::SetNavigationEnabled(true);
    }

    if (m_currentScene)
    {
        m_currentScene->OnUpdate(dt, context);
    }
}

void SceneManager::Render(GameContext& context) const
{
    if (m_currentScene)
    {
        m_currentScene->OnRender(context);
    }

    if (m_transitionPhase != SceneTransitionPhase::Idle && m_transitionConfig.Type != SceneTransitionType::None)
    {
        context.Render.DrawSceneTransition(m_transitionConfig.Type, m_transitionProgress, m_transitionConfig.FadeColor);
    }
}

uint32_t SceneManager::GetPostProcessShader() const
{
    return m_currentScene ? m_currentScene->GetPostProcessShader() : 0;
}

PostProcessParams SceneManager::GetPostProcessParams() const
{
    return m_currentScene ? m_currentScene->GetPostProcessParams() : PostProcessParams{};
}

Color SceneManager::GetClearColor() const
{
    return m_currentScene ? m_currentScene->GetClearColor() : Colors::Black;
}
