#pragma once
#include "Scene.h"
#include "Data/Color.h"
#include "Core/SceneTransition.h"
#include "ECS/Tween.h"

#include <memory>
#include <functional>
#include <tuple>
#include <utility>

class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    void Shutdown(GameContext& context);

    void SetDefaultTransition(const SceneTransitionConfig& config);
    void SetDefaultTransition(SceneTransitionType type, float duration = 0.4f, Color color = Colors::Black, EasingFunctions::EasingType ease = EasingFunctions::EasingType::EaseInOutQuad);
    [[nodiscard]] const SceneTransitionConfig& GetDefaultTransition() const noexcept { return m_defaultTransitionConfig; }

    [[nodiscard]] SceneTransitionPhase GetTransitionPhase() const noexcept { return m_transitionPhase; }
    [[nodiscard]] float GetTransitionProgress() const noexcept { return m_transitionProgress; }
    [[nodiscard]] bool IsTransitionActive() const noexcept { return m_transitionPhase != SceneTransitionPhase::Idle; }
    [[nodiscard]] SceneTransitionType GetCurrentTransitionType() const noexcept { return m_transitionConfig.Type; }

    template <typename T, typename... Args>
    void LoadScene(Args&&... args);

    template <typename T, typename... Args>
    void LoadSceneWithTransition(const SceneTransitionConfig& config, Args&&... args);

    template <typename T, typename... Args>
    void LoadSceneWithTransition(SceneTransitionType type, float duration = 0.4f, Color color = Colors::Black, EasingFunctions::EasingType ease = EasingFunctions::EasingType::EaseInOutQuad, Args&&... args);

    template <typename T, typename... Args>
    void LoadSceneInstant(Args&&... args);

    void Update(float dt, GameContext& context);
    void Render(GameContext& context) const;

    [[nodiscard]] uint32_t GetPostProcessShader() const;
    [[nodiscard]] PostProcessParams GetPostProcessParams() const;
    [[nodiscard]] Color GetClearColor() const;

private:
    void StartCoveringTween(float startProgress);
    void StartUncoveringTween();

    std::unique_ptr<Scene> m_currentScene = nullptr;
    std::function<std::unique_ptr<Scene>()> m_pendingSceneFactory = nullptr;

    SceneTransitionConfig m_defaultTransitionConfig;
    SceneTransitionConfig m_transitionConfig;
    SceneTransitionPhase m_transitionPhase = SceneTransitionPhase::Idle;
    float m_transitionProgress = 0.0f;
    std::unique_ptr<Tween<float>> m_transitionTween = nullptr;
    bool m_midpointReached = false;
    bool m_uncoverCompleted = false;
    bool m_transitionPending = false;
};

template <typename T, typename... Args>
void SceneManager::LoadScene(Args&&... args)
{
    LoadSceneWithTransition<T>(m_defaultTransitionConfig, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void SceneManager::LoadSceneWithTransition(SceneTransitionType type, float duration, Color color, EasingFunctions::EasingType ease, Args&&... args)
{
    SceneTransitionConfig config{
        .Type = type,
        .Duration = duration,
        .FadeColor = color,
        .Ease = ease
    };
    LoadSceneWithTransition<T>(config, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void SceneManager::LoadSceneWithTransition(const SceneTransitionConfig& config, Args&&... args)
{
    auto tupleArgs = std::make_tuple(std::forward<Args>(args)...);
    m_pendingSceneFactory = [tupleArgs = std::move(tupleArgs)]() mutable {
        return std::apply([](auto&&... unpacked) {
            return std::make_unique<T>(std::forward<decltype(unpacked)>(unpacked)...);
        }, std::move(tupleArgs));
    };

    m_transitionConfig = config;
    m_transitionPending = true;
}

template <typename T, typename... Args>
void SceneManager::LoadSceneInstant(Args&&... args)
{
    SceneTransitionConfig config{
        .Type = SceneTransitionType::None,
        .Duration = 0.0f,
        .FadeColor = Colors::Black,
        .Ease = EasingFunctions::EasingType::Linear
    };
    LoadSceneWithTransition<T>(config, std::forward<Args>(args)...);
}
