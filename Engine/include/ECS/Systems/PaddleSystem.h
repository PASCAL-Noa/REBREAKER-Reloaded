#pragma once
#include "ECS/System.h"
#include "ECS/Entity.h"
#include "Core/GameContext.h"
#include "Math/Vector2.h"
#include <functional>

class PaddleSystem : public System
{
public:
    explicit PaddleSystem(Registry& registry, GameContext& context);
    ~PaddleSystem() override = default;

    void OnUpdate(float dt) override;

    // Paddle Input Control
    void SetInputEnabled(bool enabled) { m_inputEnabled = enabled; }
    [[nodiscard]] bool IsInputEnabled() const { return m_inputEnabled; }

    // Playfield Boundaries
    void SetBoundaryLimit(float limitX) { m_playfieldLimitX = limitX; }
    [[nodiscard]] float GetBoundaryLimit() const { return m_playfieldLimitX; }

    // Sticky Ball Mechanics
    void AttachBall(Entity paddle, Entity ball);
    void AttachBall(Entity ball);
    void LaunchBall();
    void DetachBall();
    [[nodiscard]] bool   IsBallAttached() const { return m_isBallAttached; }
    [[nodiscard]] Entity GetAttachedBall() const { return m_attachedBall; }
    [[nodiscard]] Entity GetAttachedPaddle() const { return m_attachedPaddle; }

    void SetLaunchSpeed(float speed) { m_launchSpeed = speed; }
    [[nodiscard]] float GetLaunchSpeed() const { return m_launchSpeed; }

    // Callbacks
    void SetOnFireLaserCallback(std::function<void(Entity paddle)> cb) { m_onFireLaser = std::move(cb); }
    void SetOnBallLaunchedCallback(std::function<void(Entity ball)> cb) { m_onBallLaunched = std::move(cb); }

private:
    GameContext& m_context;
    bool         m_inputEnabled = true;
    float        m_playfieldLimitX = 800.0f;

    bool         m_isBallAttached = false;
    Entity       m_attachedPaddle = NULL_ENTITY;
    Entity       m_attachedBall = NULL_ENTITY;
    float        m_launchSpeed = 640.0f;

    std::function<void(Entity)> m_onFireLaser;
    std::function<void(Entity)> m_onBallLaunched;
};
