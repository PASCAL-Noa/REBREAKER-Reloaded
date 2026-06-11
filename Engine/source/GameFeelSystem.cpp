#include "ECS/Systems/GameFeelSystem.h"

#include "AudioMixer.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Camera2D.h"
#include "Events/GameplayEvents.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Components/Transform2D.h"
#include "Events/EventDispatcher.h"
#include "Resources/ResourceManager.h"
#include "TweenEffects/TweenEffects.h"

GameFeelSystem::GameFeelSystem(Registry& registry, GameContext& context)
    : System(registry), m_context(context)
{
    m_sfxBrickHit = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");
    m_sfxBrickDestroy = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");
    m_sfxPaddleHit = context.Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
    m_sfxBallDeath = context.Resources.LoadResource("Resources/audio/sfx/ball_despawn.wav");
    m_sfxPowerUp = context.Resources.LoadResource("Resources/audio/sfx/powerup.wav");
    m_sfxCombo = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");

    m_context.Events.Subscribe<BrickHitEvent>([this](const BrickHitEvent& e)
    {
        float pitch = 1.0f + (e.Combo * 0.05f);

        m_context.Audio.PlaySfx(m_sfxBrickHit, 50.0f, pitch);

        if (e.IsDestroyed)
        {
            m_context.Audio.PlaySfx(m_sfxBrickDestroy, 80.0f, pitch);
            m_context.Audio.PlaySfx(m_sfxCombo, 80.0f, pitch - 0.5f);
        }
        else
        {
            if (m_registry.HasComponent<TweenComponent>(e.BrickEntity) &&
                m_registry.HasComponent<Transform2D>(e.BrickEntity))
            {
                auto& tween = m_registry.GetComponent<TweenComponent>(e.BrickEntity);
                auto& transform = m_registry.GetComponent<Transform2D>(e.BrickEntity);
                TweenEffects::Shake(tween, transform, 0.15f, 5.0f);
            }

            if (e.Combo > 1)
            {
                m_context.Audio.PlaySfx(m_sfxCombo, 80.0f, pitch - 0.5f);
            }
        }
    });

    m_context.Events.Subscribe<PaddleHitEvent>([this](const PaddleHitEvent& e)
    {
        m_context.Audio.PlaySfx(m_sfxPaddleHit, 80.0f);
    });

    m_context.Events.Subscribe<BallDeathEvent>([this](const BallDeathEvent& e)
    {
        m_context.Audio.PlaySfx(m_sfxBallDeath, 80.0f);
        m_registry.View<Camera2D, TweenComponent>([&](Entity, Camera2D& cam, TweenComponent& tween) {
        TweenEffects::Shake(tween, cam, 0.4f, 15.0f);
    });
    });
}


void GameFeelSystem::OnUpdate(float dt)
{
}