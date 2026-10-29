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
    m_sfxPowerUpStorm = context.Resources.LoadResource("Resources/audio/sfx/lightning.wav");
    m_sfxPowerUpLaser = context.Resources.LoadResource("Resources/audio/sfx/powerup_laser.wav");
    m_sfxPowerUpFireball = context.Resources.LoadResource("Resources/audio/sfx/powerup_fireball.wav");
    m_sfxPowerUpBuff = context.Resources.LoadResource("Resources/audio/sfx/powerup_buff.wav");
    m_sfxPowerUpShrink = context.Resources.LoadResource("Resources/audio/sfx/powerup_shrink.wav");
    m_sfxPowerUpLife = context.Resources.LoadResource("Resources/audio/sfx/powerup_life.wav");
    m_sfxPowerUpTempo = context.Resources.LoadResource("Resources/audio/sfx/powerup_tempo.wav");
    m_sfxCombo = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");


    m_brickHitSub = m_context.Events.SubscribeScoped<BrickHitEvent>([this](const BrickHitEvent& e)
    {
        if (!m_audioEnabled) return;
        if (!m_registry.IsAlive(e.BrickEntity)) return;

        float pitch = 1.0f + (e.Combo * 0.05f);
        constexpr float kHalfPlayfieldWidth = 800.0f;
        float pan = std::clamp(e.ContactPoint.X / kHalfPlayfieldWidth, -1.0f, 1.0f);

        m_context.Audio.PlaySfx(m_sfxBrickHit, 85.0f, pitch, pan);

        if (e.IsDestroyed)
        {
            m_context.Audio.PlaySfx(m_sfxBrickDestroy, 100.0f, pitch, pan);
            m_context.Audio.PlaySfx(m_sfxCombo, 100.0f, pitch - 0.5f, pan);
        }
        else
        {
            if (m_registry.HasComponent<TweenComponent>(e.BrickEntity) &&
                m_registry.HasComponent<Transform2D>(e.BrickEntity))
            {
                auto& tween = m_registry.GetComponent<TweenComponent>(e.BrickEntity);
                TweenEffects::Shake(tween, m_registry, e.BrickEntity, 0.15f, 5.0f);
            }

            if (e.Combo > 1)
            {
                m_context.Audio.PlaySfx(m_sfxCombo, 100.0f, pitch - 0.5f, pan);
            }
        }
    });

    m_paddleHitSub = m_context.Events.SubscribeScoped<PaddleHitEvent>([this](const PaddleHitEvent& e)
    {
        if (!m_audioEnabled) return;
        if (!m_registry.IsAlive(e.PaddleEntity)) return;

        constexpr float kHalfPlayfieldWidth = 800.0f;
        float pan = std::clamp(e.ContactPoint.X / kHalfPlayfieldWidth, -1.0f, 1.0f);
        m_context.Audio.PlaySfx(m_sfxPaddleHit, 100.0f, 1.0f, pan);
    });

    m_ballDeathSub = m_context.Events.SubscribeScoped<BallDeathEvent>([this](const BallDeathEvent& e)
    {
        if (!m_audioEnabled) return;
        if (!m_registry.IsAlive(e.BallEntity)) return;

        m_context.Audio.PlaySfx(m_sfxBallDeath, 100.0f);
        m_registry.View<Camera2D, TweenComponent>([&](Entity entity, Camera2D&, TweenComponent& tween) {
            TweenEffects::Shake(tween, m_registry, entity, 0.4f, 15.0f);
        });
    });

    m_powerUpSub = m_context.Events.SubscribeScoped<PowerUpEvent>([this](const PowerUpEvent& e)
    {
        if (!m_audioEnabled) return;
        if (e.PowerUpEntity != NULL_ENTITY && !m_registry.IsAlive(e.PowerUpEntity)) return;

        uint32_t sfx = (m_sfxPowerUpBuff != 0) ? m_sfxPowerUpBuff : m_sfxPowerUp;
        float pitch = 1.0f;
        float volume = 100.0f;

        switch (e.Type)
        {
            case PowerUpType::Storm:
                sfx = (m_sfxPowerUpStorm != 0) ? m_sfxPowerUpStorm : ((m_sfxPowerUpLaser != 0) ? m_sfxPowerUpLaser : m_sfxPowerUp);
                break;
            case PowerUpType::FireBall:
                sfx = (m_sfxPowerUpFireball != 0) ? m_sfxPowerUpFireball : m_sfxPowerUp;
                break;
            case PowerUpType::ShrinkPaddle:
                sfx = (m_sfxPowerUpShrink != 0) ? m_sfxPowerUpShrink : m_sfxPowerUp;
                break;
            case PowerUpType::ExtraLife:
                sfx = (m_sfxPowerUpLife != 0) ? m_sfxPowerUpLife : m_sfxPowerUp;
                break;
            case PowerUpType::TempoBall:
                sfx = (m_sfxPowerUpTempo != 0) ? m_sfxPowerUpTempo : m_sfxPowerUp;
                break;
            case PowerUpType::MultiBall:
                sfx = (m_sfxPowerUpBuff != 0) ? m_sfxPowerUpBuff : m_sfxPowerUp;
                pitch = 1.15f;
                break;
            case PowerUpType::BigBall:
                sfx = (m_sfxPowerUpBuff != 0) ? m_sfxPowerUpBuff : m_sfxPowerUp;
                pitch = 0.85f;
                break;
            case PowerUpType::ExpandPaddle:
            default:
                sfx = (m_sfxPowerUpBuff != 0) ? m_sfxPowerUpBuff : m_sfxPowerUp;
                pitch = 1.0f;
                break;
        }

        m_context.Audio.PlaySfx(sfx, volume, pitch);
    });

}

void GameFeelSystem::OnUpdate(float /*dt*/)
{
}