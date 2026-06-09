#include "ECS/Systems/GameFeelSystem.h"

#include "AudioMixer.h"
#include "ECS/Registry.hpp"
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

    m_context.Events.Subscribe<BrickHitEvent>([this](const BrickHitEvent& e)
    {
        if (e.IsDestroyed)
        {
            m_context.Audio.PlaySfx(m_sfxBrickDestroy, 50.0f);
        }
        else
        {
            m_context.Audio.PlaySfx(m_sfxBrickHit, 50.0f);

            if (m_registry.HasComponent<TweenComponent>(e.BrickEntity) && 
                m_registry.HasComponent<Transform2D>(e.BrickEntity))
            {
                auto& tween = m_registry.GetComponent<TweenComponent>(e.BrickEntity);
                auto& transform = m_registry.GetComponent<Transform2D>(e.BrickEntity);
                TweenEffects::Shake(tween, transform, 0.15f, 5.0f);
            }
        }
    });

    m_context.Events.Subscribe<PaddleHitEvent>([this](const PaddleHitEvent& e)
    {
        m_context.Audio.PlaySfx(m_sfxPaddleHit, 50.0f);

        if (m_registry.HasComponent<TweenComponent>(e.PaddleEntity) && 
            m_registry.HasComponent<Transform2D>(e.PaddleEntity))
        {
            auto& tween = m_registry.GetComponent<TweenComponent>(e.PaddleEntity);
            auto& transform = m_registry.GetComponent<Transform2D>(e.PaddleEntity);
        }
    });

    m_context.Events.Subscribe<BallDeathEvent>([this](const BallDeathEvent& e)
    {
        m_context.Audio.PlaySfx(m_sfxBallDeath, 60.0f);
    });
}

GameFeelSystem::~GameFeelSystem()
{

}

void GameFeelSystem::OnUpdate(float dt)
{

}