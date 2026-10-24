#include "ECS/Systems/PaddleSystem.h"
#include "ECS/Registry.hpp"
#include "Core/InputManager.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "PowerUps/PowerUpConfig.h"
#include <algorithm>
#include <cmath>

PaddleSystem::PaddleSystem(Registry& registry, GameContext& context)
    : System(registry), m_context(context)
{
}

void PaddleSystem::OnUpdate(float dt)
{
    m_registry.View<PaddleComponent, Transform2D, BoxCollider>([&](Entity paddleEntity, PaddleComponent& paddleComp, Transform2D& paddleTrans, const BoxCollider& paddleCollider)
    {
        // 1. Paddle input handling
        if (m_inputEnabled)
        {
            const float speed = paddleComp.Speed > 0.0f ? paddleComp.Speed : 700.0f;

            // Keyboard / Digital remapped controls
            float moveDir = 0.0f;
            if (m_context.Input.IsActionActive(InputAction::MoveLeft))
            {
                moveDir -= 1.0f;
            }
            if (m_context.Input.IsActionActive(InputAction::MoveRight))
            {
                moveDir += 1.0f;
            }

            // Analog Gamepad Left Stick / D-Pad
            const float gamepadX = m_context.Input.GetGamepadMoveX();
            if (std::abs(gamepadX) > 0.05f)
            {
                moveDir = gamepadX;
            }

            paddleTrans.Position.X += moveDir * speed * dt;
        }

        // 2. Playfield boundary constraints
        const float halfPaddleWidth = paddleCollider.GetEffectiveSize(paddleTrans.Scale).X * 0.5f;
        const float limitX = m_playfieldLimitX - halfPaddleWidth;
        paddleTrans.Position.X = std::clamp(paddleTrans.Position.X, -limitX, limitX);

        // 3. Storm cooldown and automatic firing
        if (paddleComp.HasStorm || paddleComp.HasLaser)
        {
            float& cd = paddleComp.HasStorm ? paddleComp.StormCooldown : paddleComp.LaserCooldown;
            cd -= dt;
            if (m_inputEnabled && !m_isBallAttached && cd <= 0.0f)
            {
                if (m_onTriggerStorm)
                {
                    m_onTriggerStorm(paddleEntity);
                }
                else if (m_onFireLaser)
                {
                    m_onFireLaser(paddleEntity);
                }
                cd = PowerUpManager::Get().Storm().GetEffectiveCooldown();
            }
        }

        // 4. Attached ball mechanics
        if (m_isBallAttached && m_attachedBall != NULL_ENTITY)
        {
            if (m_registry.IsAlive(m_attachedBall) && m_registry.HasComponent<Transform2D>(m_attachedBall))
            {
                auto& ballTransform = m_registry.GetComponent<Transform2D>(m_attachedBall);
                float ballRadius = 20.0f;
                if (m_registry.HasComponent<CircleCollider>(m_attachedBall))
                {
                    ballRadius = m_registry.GetComponent<CircleCollider>(m_attachedBall).GetEffectiveRadius(ballTransform.Scale);
                }
                const float paddleHalfH = paddleCollider.GetEffectiveSize(paddleTrans.Scale).Y * 0.5f;
                ballTransform.Position = Vector2f{paddleTrans.Position.X, paddleTrans.Position.Y - paddleHalfH - ballRadius - 2.0f};

                if (m_inputEnabled && m_context.Input.IsActionTriggered(InputAction::LaunchBall))
                {
                    LaunchBall();
                }
            }
            else
            {
                m_isBallAttached = false;
                m_attachedBall = NULL_ENTITY;
            }
        }
    });
}

void PaddleSystem::AttachBall(Entity paddle, Entity ball)
{
    m_attachedPaddle = paddle;
    m_attachedBall = ball;
    m_isBallAttached = true;

    if (m_registry.IsAlive(ball) && m_registry.HasComponent<RigidBody>(ball))
    {
        auto& ballRb = m_registry.GetComponent<RigidBody>(ball);
        ballRb.Velocity = Vector2f{0.0f, 0.0f};
        ballRb.IsKinematic = true;
    }
}

void PaddleSystem::AttachBall(Entity ball)
{
    AttachBall(m_attachedPaddle, ball);
}

void PaddleSystem::LaunchBall()
{
    if (!m_isBallAttached || m_attachedBall == NULL_ENTITY) return;

    if (m_registry.IsAlive(m_attachedBall) && m_registry.HasComponent<RigidBody>(m_attachedBall))
    {
        auto& ballRb = m_registry.GetComponent<RigidBody>(m_attachedBall);
        Vector2f launchDir = Vector2f{500.0f, -400.0f}.Normalized();
        ballRb.Velocity = launchDir * m_launchSpeed;
        ballRb.IsKinematic = false;
    }

    Entity launchedBall = m_attachedBall;
    m_isBallAttached = false;
    m_attachedBall = NULL_ENTITY;

    if (m_onBallLaunched)
    {
        m_onBallLaunched(launchedBall);
    }
}

void PaddleSystem::DetachBall()
{
    m_isBallAttached = false;
    m_attachedBall = NULL_ENTITY;
}
