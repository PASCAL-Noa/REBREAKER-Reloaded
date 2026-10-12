#include "ECS/Systems/BallSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "PowerUps/PowerUpConfig.h"
#include "Data/Color.h"
#include <algorithm>
#include <cmath>

BallSystem::BallSystem(Registry& registry, GameContext& context)
    : System(registry), m_context(context)
{
}

void BallSystem::OnUpdate(float dt)
{
    UpdateBallPhysics(dt);
    UpdateFireBalls(dt);
    UpdateLasers(dt);
}

void BallSystem::SetPlayfieldBounds(float minX, float maxX, float minY, float maxY)
{
    m_boundMinX = minX;
    m_boundMaxX = maxX;
    m_boundMinY = minY;
    m_boundMaxY = maxY;
}

Entity BallSystem::CreateBall(const Vector2f& position, const Vector2f& velocity, uint32_t textureId, bool isBig, float bigScale, bool isTempo)
{
    Entity ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(ball, Transform2D{position});
    m_registry.AddComponent<CircleCollider>(ball, CircleCollider{m_ballRadius, Vector2f{0.0f, 0.0f}, false});
    m_registry.AddComponent<RigidBody>(ball, RigidBody{velocity, 1.0f, 1.0f, false});
    m_registry.AddComponent<SpriteComponent>(ball, SpriteComponent{textureId});
    m_registry.AddComponent<TweenComponent>(ball, TweenComponent{});

    BallComponent ballComp{};
    if (isBig)
    {
        ballComp.IsBig = true;
        m_registry.GetComponent<Transform2D>(ball).Scale = Vector2f{bigScale, bigScale};
    }
    if (isTempo)
    {
        m_registry.GetComponent<SpriteComponent>(ball).Tint = Color{210, 160, 255, 255};
    }
    m_registry.AddComponent<BallComponent>(ball, ballComp);

    m_balls.push_back(ball);
    return ball;
}

void BallSystem::RegisterBall(Entity ball)
{
    if (std::find(m_balls.begin(), m_balls.end(), ball) == m_balls.end())
    {
        m_balls.push_back(ball);
    }
}

void BallSystem::UnregisterBall(Entity ball)
{
    std::erase(m_balls, ball);
}

void BallSystem::ClearBalls(Entity preserveBall)
{
    for (Entity b : m_balls)
    {
        if (b != preserveBall && m_registry.IsAlive(b))
        {
            m_registry.DestroyEntityDeferred(b);
        }
    }
    m_balls.clear();
    if (preserveBall != NULL_ENTITY && m_registry.IsAlive(preserveBall))
    {
        m_balls.push_back(preserveBall);
    }
}

bool BallSystem::HasActiveBalls() const
{
    return std::any_of(m_balls.begin(), m_balls.end(), [this](Entity b) {
        return m_registry.IsAlive(b);
    });
}

void BallSystem::RegisterLaser(Entity laser)
{
    if (std::find(m_lasers.begin(), m_lasers.end(), laser) == m_lasers.end())
    {
        m_lasers.push_back(laser);
    }
}

void BallSystem::UnregisterLaser(Entity laser)
{
    std::erase(m_lasers, laser);
}

void BallSystem::ClearLasers()
{
    for (Entity laser : m_lasers)
    {
        if (m_registry.IsAlive(laser))
        {
            m_registry.DestroyEntityDeferred(laser);
        }
    }
    m_lasers.clear();
}

bool BallSystem::IsLaser(Entity entity) const
{
    return std::find(m_lasers.begin(), m_lasers.end(), entity) != m_lasers.end();
}

void BallSystem::UpdateBallPhysics(float /*dt*/)
{
    std::erase_if(m_balls, [this](Entity b) { return !m_registry.IsAlive(b); });

    const float minX = m_boundMinX + m_ballRadius;
    const float maxX = m_boundMaxX - m_ballRadius;
    const float minY = m_boundMinY + m_ballRadius;

    float minSpeed = (m_isTempoActive) ? 280.0f : 450.0f;
    if (m_customMinSpeed > 0.0f)
    {
        minSpeed = m_customMinSpeed;
    }
    constexpr float MAX_BALL_SPEED = 900.0f;
    constexpr float MIN_VERTICAL_SPEED = 120.0f;

    for (Entity ballEntity : m_balls)
    {
        if (!m_registry.IsAlive(ballEntity)) continue;
        if (!m_registry.HasComponent<Transform2D>(ballEntity) || !m_registry.HasComponent<RigidBody>(ballEntity))
            continue;

        auto& trans = m_registry.GetComponent<Transform2D>(ballEntity);
        auto& rb = m_registry.GetComponent<RigidBody>(ballEntity);
        if (rb.IsKinematic) continue;

        // Hard boundary safety clamp & bounce for all active balls
        if (trans.Position.X < minX)
        {
            trans.Position.X = minX;
            if (rb.Velocity.X < 0.0f) rb.Velocity.X = std::abs(rb.Velocity.X);
        }
        else if (trans.Position.X > maxX)
        {
            trans.Position.X = maxX;
            if (rb.Velocity.X > 0.0f) rb.Velocity.X = -std::abs(rb.Velocity.X);
        }

        if (trans.Position.Y < minY)
        {
            trans.Position.Y = minY;
            if (rb.Velocity.Y < 0.0f) rb.Velocity.Y = std::abs(rb.Velocity.Y);
        }

        // Enforce minimum & maximum speed
        float speed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
        if (speed < 0.1f)
        {
            rb.Velocity = Vector2f{300.0f, -400.0f}.Normalized() * minSpeed;
        }
        else if (speed < minSpeed)
        {
            rb.Velocity = (rb.Velocity / speed) * minSpeed;
        }
        else if (speed > MAX_BALL_SPEED)
        {
            rb.Velocity = (rb.Velocity / speed) * MAX_BALL_SPEED;
        }

        // Prevent balls from getting trapped in near-horizontal trajectories
        if (std::abs(rb.Velocity.Y) < MIN_VERTICAL_SPEED)
        {
            rb.Velocity.Y = (rb.Velocity.Y >= 0.0f) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
            float currentSpd = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
            if (currentSpd > 0.1f)
            {
                rb.Velocity = (rb.Velocity / currentSpd) * speed;
            }
        }
    }
}

void BallSystem::UpdateLasers(float /*dt*/)
{
    std::erase_if(m_lasers, [this](Entity laser) {
        if (!m_registry.IsAlive(laser)) return true;
        if (m_registry.HasComponent<Transform2D>(laser))
        {
            const auto& trans = m_registry.GetComponent<Transform2D>(laser);
            if (trans.Position.Y < -460.0f)
            {
                m_registry.DestroyEntityDeferred(laser);
                return true;
            }
        }
        return false;
    });
}

void BallSystem::UpdateFireBalls(float dt)
{
    const auto& fireCfg = PowerUpManager::Get().FireBall();

    for (Entity b : m_balls)
    {
        if (!m_registry.IsAlive(b) || !m_registry.HasComponent<BallComponent>(b)) continue;
        auto& bc = m_registry.GetComponent<BallComponent>(b);
        if (!bc.IsFireBall) continue;

        auto& bTrans = m_registry.GetComponent<Transform2D>(b);
        Vector2f vel{0.0f, 0.0f};
        if (m_registry.HasComponent<RigidBody>(b))
        {
            vel = m_registry.GetComponent<RigidBody>(b).Velocity;
        }

        // Emit dynamic trail particles
        bc.TrailTimer += dt;
        const float trailInterval = bc.IsFuseActive ? 0.015f : 0.03f;
        while (bc.TrailTimer >= trailInterval)
        {
            bc.TrailTimer -= trailInterval;
            if (m_onFireTrail)
            {
                m_onFireTrail(bTrans.Position, vel, bc.IsFuseActive);
            }
        }

        // If the fuse has started (first brick was pierced)
        if (bc.IsFuseActive)
        {
            bc.FuseTimer -= dt;

            // Visual warning: fast pulsating intensity and tint
            if (m_registry.HasComponent<SpriteComponent>(b))
            {
                auto& spr = m_registry.GetComponent<SpriteComponent>(b);
                float blink = std::sin((fireCfg.FuseDuration - bc.FuseTimer) * 22.0f);
                if (blink > 0.0f)
                {
                    spr.Tint = Color{255, 240, 80, 255};
                }
                else
                {
                    spr.Tint = Color{255, 50, 10, 255};
                }
            }

            if (bc.FuseTimer <= 0.0f)
            {
                if (m_onFireExplode)
                {
                    m_onFireExplode(b, bTrans.Position);
                }
            }
        }
    }
}
