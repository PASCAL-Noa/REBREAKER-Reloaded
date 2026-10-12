#pragma once
#include "ECS/System.h"
#include "ECS/Entity.h"
#include "Core/GameContext.h"
#include "Math/Vector2.h"
#include <vector>
#include <functional>

class BallSystem : public System
{
public:
    explicit BallSystem(Registry& registry, GameContext& context);
    ~BallSystem() override = default;

    void OnUpdate(float dt) override;

    // Ball Lifecycle Management
    Entity CreateBall(const Vector2f& position, const Vector2f& velocity, uint32_t textureId, bool isBig = false, float bigScale = 1.0f, bool isTempo = false);
    void   RegisterBall(Entity ball);
    void   UnregisterBall(Entity ball);
    void   ClearBalls(Entity preserveBall = NULL_ENTITY);
    [[nodiscard]] const std::vector<Entity>& GetBalls() const { return m_balls; }
    [[nodiscard]] std::vector<Entity>&       GetBalls() { return m_balls; }
    [[nodiscard]] size_t                     GetBallCount() const { return m_balls.size(); }
    [[nodiscard]] bool                       HasActiveBalls() const;

    // Laser Lifecycle & Updates
    void   RegisterLaser(Entity laser);
    void   UnregisterLaser(Entity laser);
    void   ClearLasers();
    [[nodiscard]] const std::vector<Entity>& GetLasers() const { return m_lasers; }
    [[nodiscard]] std::vector<Entity>&       GetLasers() { return m_lasers; }
    [[nodiscard]] bool                       IsLaser(Entity entity) const;

    // Speed Clamping & Trajectory Configuration
    void   SetTempoBallActive(bool active) { m_isTempoActive = active; }
    [[nodiscard]] bool IsTempoBallActive() const { return m_isTempoActive; }

    void   SetMinSpeed(float speed) { m_customMinSpeed = speed; }
    void   ResetMinSpeed() { m_customMinSpeed = -1.0f; }

    void   SetPlayfieldBounds(float minX, float maxX, float minY, float maxY = 450.0f);
    void   SetBallRadius(float radius) { m_ballRadius = radius; }

    // Dynamic Effects Callbacks
    void   SetFireTrailCallback(std::function<void(const Vector2f& pos, const Vector2f& vel, bool isFuseActive)> cb) { m_onFireTrail = std::move(cb); }
    void   SetFireExplosionCallback(std::function<void(Entity ball, const Vector2f& pos)> cb) { m_onFireExplode = std::move(cb); }

private:
    void   UpdateBallPhysics(float dt);
    void   UpdateLasers(float dt);
    void   UpdateFireBalls(float dt);

    GameContext&        m_context;
    std::vector<Entity> m_balls;
    std::vector<Entity> m_lasers;

    bool  m_isTempoActive = false;
    float m_customMinSpeed = -1.0f;

    float m_boundMinX = -800.0f;
    float m_boundMaxX = 800.0f;
    float m_boundMinY = -450.0f;
    float m_boundMaxY = 450.0f;
    float m_ballRadius = 20.0f;

    std::function<void(const Vector2f&, const Vector2f&, bool)> m_onFireTrail;
    std::function<void(Entity, const Vector2f&)>                m_onFireExplode;
};
