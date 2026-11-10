#include "ECS/Systems/DynamicEventSystem.h"
#include "Core/GameContext.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "AudioMixer.h"
#include "Events/EventDispatcher.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "ECS/Components/EnvironmentalProjectileComponent.h"
#include "Math/Physics.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

DynamicEventSystem::DynamicEventSystem(Registry& registry, GameContext* context)
    : System(registry), mp_context(context), m_rng(std::random_device{}())
{
    m_state = ArenaEventState::Idle;
    m_stateDuration = GetNextInterval();
    m_stateTimer = m_stateDuration;
}

DynamicEventSystem::DynamicEventSystem(Registry& registry, GameContext& context)
    : DynamicEventSystem(registry, &context)
{
}

DynamicEventSystem::~DynamicEventSystem()
{
    RestoreStandardPhysics();
}

void DynamicEventSystem::OnInit()
{
    if (mp_context)
    {
        m_klaxonSfxId = mp_context->Resources.LoadResource("Resources/audio/sfx/alarm_klaxon.wav");
        m_meteorTexId = mp_context->Resources.LoadResource("Resources/sprite/hazards/hazard_meteor.png");
        m_deflectSfxId = mp_context->Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
        m_meteorExplodeSfxId = mp_context->Resources.LoadResource("Resources/audio/sfx/fireball_explode.wav");
    }
}

float DynamicEventSystem::GetNextInterval()
{
    std::uniform_real_distribution<float> dist(m_minInterval, m_maxInterval);
    return dist(m_rng);
}

void DynamicEventSystem::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!m_enabled && (m_state == ArenaEventState::Active || m_state == ArenaEventState::Warning))
    {
        StopActiveEvent();
    }
}

void DynamicEventSystem::SetIntervalRange(float minInterval, float maxInterval)
{
    m_minInterval = std::max(5.0f, minInterval);
    m_maxInterval = std::max(m_minInterval, maxInterval);
    if (m_state == ArenaEventState::Idle && m_stateTimer > m_maxInterval)
    {
        m_stateTimer = m_maxInterval;
    }
}

void DynamicEventSystem::SetWarningDuration(float duration)
{
    m_warningDuration = std::max(1.0f, duration);
}

void DynamicEventSystem::SetEventDuration(float duration)
{
    m_eventDuration = std::max(2.0f, duration);
}

void DynamicEventSystem::SetCooldownDuration(float duration)
{
    m_cooldownDuration = std::max(1.0f, duration);
}

float DynamicEventSystem::GetProgress() const noexcept
{
    if (m_stateDuration <= 0.0001f) return 0.0f;
    return std::clamp(1.0f - (m_stateTimer / m_stateDuration), 0.0f, 1.0f);
}

ArenaEventType DynamicEventSystem::SelectRandomEvent()
{
    // Uniformly pick from available event archetypes (1 to Count - 1)
    std::uniform_int_distribution<int> dist(1, static_cast<int>(ArenaEventType::Count) - 1);
    return static_cast<ArenaEventType>(dist(m_rng));
}

void DynamicEventSystem::TriggerEvent(ArenaEventType type, bool skipWarning)
{
    if (type <= ArenaEventType::None || type >= ArenaEventType::Count) return;

    m_currentEvent = type;
    if (skipWarning)
    {
        EnterState(ArenaEventState::Active);
    }
    else
    {
        EnterState(ArenaEventState::Warning);
    }
}

void DynamicEventSystem::TriggerRandomEvent(bool skipWarning)
{
    TriggerEvent(SelectRandomEvent(), skipWarning);
}

void DynamicEventSystem::StopActiveEvent()
{
    RestoreStandardPhysics();
    EnterState(ArenaEventState::Cooldown);
}

void DynamicEventSystem::ResetLifecycle()
{
    RestoreStandardPhysics();
    EnterState(ArenaEventState::Idle);
}

void DynamicEventSystem::EnterState(ArenaEventState newState)
{
    m_state = newState;

    switch (m_state)
    {
        case ArenaEventState::Idle:
            m_currentEvent = ArenaEventType::None;
            m_stateDuration = GetNextInterval();
            m_stateTimer = m_stateDuration;
            m_alarmIntensity = 0.0f;
            break;

        case ArenaEventState::Warning:
            m_stateDuration = m_warningDuration;
            m_stateTimer = m_warningDuration;
            m_warningKlaxonTimer = 0.0f;
            m_alarmIntensity = 0.75f;
            m_alarmColor = Color{255, 35, 25, 255};
            if (mp_context)
            {
                if (m_klaxonSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_klaxonSfxId, 85.0f);
                }
                mp_context->Events.Publish(ArenaEventWarningStartedEvent{m_currentEvent, m_warningDuration});
            }
            break;

        case ArenaEventState::Active:
            m_stateDuration = (m_currentEvent == ArenaEventType::TemporalFlux) ? 20.0f : m_eventDuration;
            m_stateTimer = m_stateDuration;
            m_alarmIntensity = 0.85f;
            m_alarmColor = GetArenaEventColor(m_currentEvent);
            ActivateEvent(m_currentEvent);
            if (mp_context)
            {
                if (m_klaxonSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_klaxonSfxId, 85.0f, 1.15f);
                }
                mp_context->Events.Publish(ArenaEventActivatedEvent{m_currentEvent, m_stateDuration});
            }
            break;

        case ArenaEventState::Cooldown:
            m_stateDuration = m_cooldownDuration;
            m_stateTimer = m_cooldownDuration;
            m_alarmIntensity = 0.0f;
            if (mp_context)
            {
                mp_context->Events.Publish(ArenaEventEndedEvent{m_currentEvent});
            }
            m_currentEvent = ArenaEventType::None;
            break;
    }
}

bool DynamicEventSystem::IsTemporalSliceSlow() const noexcept
{
    if (m_currentEvent != ArenaEventType::TemporalFlux || m_currentTemporalSliceIndex >= m_temporalSlices.size())
        return false;
    return m_temporalSlices[m_currentTemporalSliceIndex].IsSlow;
}

void DynamicEventSystem::GenerateTemporalSlices(float totalDuration)
{
    m_temporalSlices.clear();
    m_currentTemporalSliceIndex = 0;
    m_temporalSliceTimer = 0.0f;

    const float target = (totalDuration > 0.5f) ? totalDuration : 20.0f;
    float remaining = target;

    // Random start: 50% slow or 50% speed
    std::bernoulli_distribution startSlowDist(0.5);
    bool currentIsSlow = startSlowDist(m_rng);

    std::uniform_real_distribution<float> sliceDist(2.5f, 4.5f);

    while (remaining > 0.001f)
    {
        if (remaining <= 4.8f)
        {
            // Final slice absorbs the exact remaining duration so the total is strictly preserved
            m_temporalSlices.push_back(TemporalSlice{currentIsSlow, remaining});
            break;
        }

        float dur = sliceDist(m_rng);
        if (remaining - dur < 2.0f)
        {
            dur = remaining * 0.5f;
        }
        dur = std::round(dur * 10.0f) / 10.0f;
        dur = std::clamp(dur, 2.0f, remaining - 2.0f);

        m_temporalSlices.push_back(TemporalSlice{currentIsSlow, dur});
        remaining -= dur;
        currentIsSlow = !currentIsSlow; // Alternates between slow and speed
    }

    if (!m_temporalSlices.empty())
    {
        m_temporalSliceTimer = m_temporalSlices[0].Duration;
        ApplyTemporalSlice(0);
    }
}

void DynamicEventSystem::ApplyTemporalSlice(size_t index)
{
    if (index >= m_temporalSlices.size()) return;

    const bool isSlow = m_temporalSlices[index].IsSlow;
    if (isSlow)
    {
        m_timeScale = 0.50f;
        if (mp_context)
        {
            mp_context->Audio.FadeMusicPitch(0.70f, 0.20f);
            mp_context->Audio.SetTemporalSfxPitchMultiplier(0.70f);
        }
    }
    else
    {
        m_timeScale = 1.50f;
        if (mp_context)
        {
            mp_context->Audio.FadeMusicPitch(1.30f, 0.20f);
            mp_context->Audio.SetTemporalSfxPitchMultiplier(1.30f);
        }
    }
}

void DynamicEventSystem::ActivateEvent(ArenaEventType type)
{
    // Ensure standard physics is baseline before configuring new archetype
    m_timeScale = 1.0f;
    m_gravityForce = Vector2f{0.0f, 0.0f};
    m_arenaTiltAngle = 0.0f;
    m_cameraShakeOffset = Vector2f{0.0f, 0.0f};
    m_cameraShakeAngle = 0.0f;
    m_blackoutIntensity = 0.0f;

    switch (type)
    {
        case ArenaEventType::TemporalFlux:
            m_eventDuration = 20.0f;
            m_stateDuration = 20.0f;
            m_stateTimer = 20.0f;
            GenerateTemporalSlices(20.0f);
            break;

        case ArenaEventType::GravityShiftLeft:
            m_gravityForce = Vector2f{-420.0f, 0.0f};
            m_arenaTiltAngle = -3.5f;
            break;

        case ArenaEventType::GravityShiftRight:
            m_gravityForce = Vector2f{420.0f, 0.0f};
            m_arenaTiltAngle = 3.5f;
            break;

        case ArenaEventType::GravityShiftDown:
            m_gravityForce = Vector2f{0.0f, 380.0f};
            m_arenaTiltAngle = 0.0f;
            break;

        case ArenaEventType::CameraEarthquake:
            m_ballAgitationTimer = 0.25f;
            break;

        case ArenaEventType::SolarFlareBlackout:
            m_blackoutIntensity = 1.0f;
            m_searchlightTime = 0.0f;
            break;

        case ArenaEventType::MeteorShower:
            m_meteorSpawnTimer = 0.15f;
            break;

        default:
            break;
    }
}

void DynamicEventSystem::RestoreStandardPhysics()
{
    m_temporalSlices.clear();
    m_currentTemporalSliceIndex = 0;
    m_temporalSliceTimer = 0.0f;

    m_timeScale = 1.0f;
    m_gravityForce = Vector2f{0.0f, 0.0f};
    m_arenaTiltAngle = 0.0f;
    m_cameraShakeOffset = Vector2f{0.0f, 0.0f};
    m_cameraShakeAngle = 0.0f;
    m_blackoutIntensity = 0.0f;
    m_alarmIntensity = 0.0f;

    if (mp_context)
    {
        mp_context->Audio.ResetMusicPitch(0.25f);
        mp_context->Audio.SetTemporalSfxPitchMultiplier(1.0f);
    }

    ClearProjectiles();
}

void DynamicEventSystem::UpdateLifecycle(float dt)
{
    if (!m_enabled || dt <= 0.0f) return;

    if (m_stateTimer > 0.0f)
    {
        m_stateTimer -= dt;
    }

    switch (m_state)
    {
        case ArenaEventState::Idle:
            m_alarmIntensity = 0.0f;
            if (m_stateTimer <= 0.0f)
            {
                m_currentEvent = SelectRandomEvent();
                EnterState(ArenaEventState::Warning);
            }
            break;

        case ArenaEventState::Warning:
        {
            // High-frequency buzzer/alarm strobe pulse during warning
            const float warningPulse = std::sin(m_stateTimer * 12.0f) * 0.35f + 0.65f;
            m_alarmIntensity = warningPulse * 0.75f;
            m_alarmColor = Color{255, 35, 25, 255};

            m_warningKlaxonTimer += dt;
            if (m_warningKlaxonTimer >= 1.25f)
            {
                m_warningKlaxonTimer = 0.0f;
                if (mp_context && m_klaxonSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_klaxonSfxId, 75.0f);
                }
            }
            if (m_stateTimer <= 0.0f)
            {
                EnterState(ArenaEventState::Active);
            }
            break;
        }

        case ArenaEventState::Active:
        {
            // Update temporal flux multi-slices progression
            if (m_currentEvent == ArenaEventType::TemporalFlux && !m_temporalSlices.empty())
            {
                m_temporalSliceTimer -= dt;
                if (m_temporalSliceTimer <= 0.0f)
                {
                    if (m_currentTemporalSliceIndex + 1 < m_temporalSlices.size())
                    {
                        m_currentTemporalSliceIndex++;
                        m_temporalSliceTimer += m_temporalSlices[m_currentTemporalSliceIndex].Duration;
                        ApplyTemporalSlice(m_currentTemporalSliceIndex);
                    }
                }
            }

            m_alarmColor = GetArenaEventColor(m_currentEvent);
            const float elapsed = m_stateDuration - m_stateTimer;
            if (elapsed < 1.2f)
            {
                // Initial burst of alarm lighting that smoothly settles
                const float decay = 1.0f - (elapsed / 1.2f);
                m_alarmIntensity = decay * 0.85f;
            }
            else if (m_currentEvent == ArenaEventType::CameraEarthquake)
            {
                m_alarmIntensity = 0.35f * (std::sin(m_stateTimer * 8.0f) * 0.5f + 0.5f);
            }
            else if (m_currentEvent == ArenaEventType::SolarFlareBlackout)
            {
                m_alarmIntensity = 0.30f * (std::sin(m_stateTimer * 3.5f) * 0.5f + 0.5f);
            }
            else
            {
                m_alarmIntensity = 0.0f;
            }

            if (m_stateTimer <= 0.0f)
            {
                StopActiveEvent();
            }
            break;
        }

        case ArenaEventState::Cooldown:
            m_alarmIntensity = 0.0f;
            if (m_stateTimer <= 0.0f)
            {
                EnterState(ArenaEventState::Idle);
            }
            break;
    }
}

void DynamicEventSystem::OnUpdate(float dt)
{
    if (m_state != ArenaEventState::Active || dt <= 0.0f)
    {
        // Meteors can continue to dissipate even during cooldown if any remain
        if (!m_activeProjectiles.empty())
        {
            UpdateMeteors(dt);
        }
        return;
    }

    // 1. Directional Gravity Shift on all active balls
    if (m_gravityForce.X != 0.0f || m_gravityForce.Y != 0.0f)
    {
        m_registry.View<Transform2D, RigidBody, BallComponent>([this, dt](Entity, Transform2D&, RigidBody& rb, BallComponent&)
        {
            if (!rb.IsKinematic)
            {
                rb.Velocity += m_gravityForce * dt;
            }
        });
    }

    // 2. Camera Earthquake Vibration & Ball Agitation
    if (m_currentEvent == ArenaEventType::CameraEarthquake)
    {
        const float t = m_eventDuration - m_stateTimer;
        const float shakeX = (std::sin(t * 54.0f) * 0.65f + std::sin(t * 92.0f) * 0.35f) * 11.0f;
        const float shakeY = (std::cos(t * 48.0f) * 0.65f + std::cos(t * 96.0f) * 0.35f) * 9.0f;
        m_cameraShakeOffset = Vector2f{shakeX, shakeY};
        m_cameraShakeAngle = std::sin(t * 62.0f) * 1.15f;

        m_ballAgitationTimer -= dt;
        if (m_ballAgitationTimer <= 0.0f)
        {
            m_ballAgitationTimer = 0.55f;
            ApplyBallAgitationKicks();
        }
    }

    // 3. Solar Flare & Blackout
    if (m_currentEvent == ArenaEventType::SolarFlareBlackout)
    {
        m_searchlightTime += dt;
        // Smoothly fade in blackout in the first 0.4s
        const float elapsed = m_eventDuration - m_stateTimer;
        m_blackoutIntensity = std::clamp(elapsed / 0.4f, 0.0f, 1.0f);
    }

    // 4. Meteor Shower Projectiles
    if (m_currentEvent == ArenaEventType::MeteorShower)
    {
        m_meteorSpawnTimer -= dt;
        if (m_meteorSpawnTimer <= 0.0f)
        {
            m_meteorSpawnTimer = 0.48f;
            std::uniform_real_distribution<float> distX(-660.0f, 660.0f);
            std::uniform_real_distribution<float> distVelX(-45.0f, 45.0f);
            std::uniform_real_distribution<float> distVelY(310.0f, 410.0f);
            SpawnMeteor(Vector2f{distX(m_rng), -420.0f}, Vector2f{distVelX(m_rng), distVelY(m_rng)});
        }
        UpdateMeteors(dt);
    }
}

void DynamicEventSystem::ApplyBallAgitationKicks()
{
    std::uniform_real_distribution<float> angleDist(-0.65f, 0.65f); // ~ -37 to +37 deg kick
    std::uniform_real_distribution<float> impulseDist(60.0f, 130.0f);

    m_registry.View<Transform2D, RigidBody, BallComponent>([&](Entity, Transform2D&, RigidBody& rb, BallComponent&)
    {
        if (rb.IsKinematic) return;

        const float speed = rb.Velocity.Length();
        if (speed < 0.1f) return;

        const float currentAngle = std::atan2(rb.Velocity.Y, rb.Velocity.X);
        const float newAngle = currentAngle + angleDist(m_rng);
        const float kick = impulseDist(m_rng);

        Vector2f newDir{std::cos(newAngle), std::sin(newAngle)};
        rb.Velocity = newDir * (speed + kick * 0.15f);
    });
}

Entity DynamicEventSystem::SpawnMeteor(const Vector2f& position, const Vector2f& velocity)
{
    Entity meteor = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(meteor, Transform2D{position, 0.0f, Vector2f{1.0f, 1.0f}});
    m_registry.AddComponent<CircleCollider>(meteor, CircleCollider{16.0f, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<RigidBody>(meteor, RigidBody{velocity, 1.0f, 1.0f, true});
    m_registry.AddComponent<EnvironmentalProjectileComponent>(meteor, EnvironmentalProjectileComponent{
        .Radius = 16.0f,
        .Damage = 1,
        .IsDeflected = false,
        .Lifetime = 7.0f,
        .TrailColor = Color{255, 120, 30, 255}
    });

    if (m_meteorTexId != 0)
    {
        m_registry.AddComponent<SpriteComponent>(meteor, SpriteComponent{m_meteorTexId, Colors::White, 2});
    }

    LightSourceComponent light{};
    light.Type = LightType::Point;
    light.LightColor = Color{255, 150, 40, 255};
    light.Radius = 130.0f;
    light.Intensity = 1.35f;
    m_registry.AddComponent<LightSourceComponent>(meteor, light);

    m_activeProjectiles.push_back(meteor);
    return meteor;
}

void DynamicEventSystem::ClearProjectiles()
{
    for (Entity proj : m_activeProjectiles)
    {
        if (m_registry.IsAlive(proj))
        {
            m_registry.DestroyEntityDeferred(proj);
        }
    }
    m_activeProjectiles.clear();
}

void DynamicEventSystem::UpdateMeteors(float dt)
{
    std::erase_if(m_activeProjectiles, [this](Entity e) { return !m_registry.IsAlive(e); });

    for (Entity proj : m_activeProjectiles)
    {
        if (!m_registry.IsAlive(proj)) continue;
        if (!m_registry.HasComponent<Transform2D>(proj) || !m_registry.HasComponent<RigidBody>(proj)) continue;

        auto& trans = m_registry.GetComponent<Transform2D>(proj);
        auto& rb = m_registry.GetComponent<RigidBody>(proj);
        auto& epc = m_registry.GetComponent<EnvironmentalProjectileComponent>(proj);

        // Natural movement & atmospheric tumble
        trans.Position += rb.Velocity * dt;
        trans.Rotation += rb.Velocity.X * dt * 0.45f;
        epc.Lifetime -= dt;

        // Despawn if fallen off playfield bottom or expired
        if (trans.Position.Y > 480.0f || epc.Lifetime <= 0.0f)
        {
            m_registry.DestroyEntityDeferred(proj);
        }
    }

    CheckMeteorPaddleCollisions();
    CheckMeteorBrickCollisions();
}

void DynamicEventSystem::CheckMeteorPaddleCollisions()
{
    if (m_paddleEntity == NULL_ENTITY || !m_registry.IsAlive(m_paddleEntity)) return;
    if (!m_registry.HasComponent<Transform2D>(m_paddleEntity) || !m_registry.HasComponent<BoxCollider>(m_paddleEntity))
        return;

    const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddleEntity);
    const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddleEntity);

    for (Entity proj : m_activeProjectiles)
    {
        if (!m_registry.IsAlive(proj)) continue;
        if (!m_registry.HasComponent<Transform2D>(proj) || !m_registry.HasComponent<CircleCollider>(proj) || !m_registry.HasComponent<RigidBody>(proj))
            continue;

        auto& trans = m_registry.GetComponent<Transform2D>(proj);
        auto& col = m_registry.GetComponent<CircleCollider>(proj);
        auto& rb = m_registry.GetComponent<RigidBody>(proj);
        auto& epc = m_registry.GetComponent<EnvironmentalProjectileComponent>(proj);

        if (!epc.IsDeflected && rb.Velocity.Y > 0.0f)
        {
            if (Physics::IntersectCircleAABB(col, trans, pCol, pTrans).IsColliding)
            {
                // Player cleanly deflected the falling meteor!
                epc.IsDeflected = true;
                rb.Velocity.Y = -std::abs(rb.Velocity.Y) * 1.15f;
                const float hitOffset = (trans.Position.X - pTrans.Position.X) / 60.0f;
                rb.Velocity.X += hitOffset * 220.0f;
                epc.TrailColor = Color{80, 220, 255, 255}; // Change trail to bright cyan on deflection

                if (m_registry.HasComponent<LightSourceComponent>(proj))
                {
                    m_registry.GetComponent<LightSourceComponent>(proj).LightColor = Color{100, 230, 255, 255};
                }

                if (mp_context && m_deflectSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_deflectSfxId, 85.0f, 1.25f);
                }

                if (m_onScoreAdded)
                {
                    m_onScoreAdded(150, 1);
                }
            }
        }
    }
}

void DynamicEventSystem::CheckMeteorBrickCollisions()
{
    m_registry.View<Transform2D, BoxCollider, BrickComponent>([this](Entity bEntity, const Transform2D& bTrans, const BoxCollider& bCol, BrickComponent&)
    {
        if (!m_registry.IsAlive(bEntity)) return;

        for (Entity proj : m_activeProjectiles)
        {
            if (!m_registry.IsAlive(proj)) continue;
            if (!m_registry.HasComponent<Transform2D>(proj) || !m_registry.HasComponent<CircleCollider>(proj))
                continue;

            auto& pTrans = m_registry.GetComponent<Transform2D>(proj);
            auto& pCol = m_registry.GetComponent<CircleCollider>(proj);

            if (Physics::IntersectCircleAABB(pCol, pTrans, bCol, bTrans).IsColliding)
            {
                if (m_onBrickHit)
                {
                    m_onBrickHit(bEntity, proj);
                }

                if (mp_context && m_meteorExplodeSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_meteorExplodeSfxId, 70.0f, 1.30f);
                }

                // Detonate projectile
                m_registry.DestroyEntityDeferred(proj);
                break;
            }
        }
    });
}

void DynamicEventSystem::RenderSearchlights(GameContext& context) const
{
    if (m_blackoutIntensity <= 0.01f) return;

    // Sweeping spotlight 1: Upper arena searchlight shining down (+90 deg)
    const float angle1 = 90.0f + std::sin(m_searchlightTime * 1.3f) * 55.0f;
    const Vector2f pos1{-350.0f + std::sin(m_searchlightTime * 0.8f) * 200.0f, -420.0f};
    context.Render.DrawConeLight(pos1, 850.0f, angle1, 52.0f, Color{190, 225, 255, 255}, 1.7f * m_blackoutIntensity);

    // Sweeping spotlight 2: Right/midfield searchlight shining down (+90 deg)
    const float angle2 = 90.0f - std::cos(m_searchlightTime * 1.5f) * 50.0f;
    const Vector2f pos2{350.0f + std::cos(m_searchlightTime * 0.9f) * 200.0f, -400.0f};
    context.Render.DrawConeLight(pos2, 850.0f, angle2, 52.0f, Color{255, 220, 160, 255}, 1.7f * m_blackoutIntensity);
}

void DynamicEventSystem::RenderAlarmLights(GameContext& context) const
{
    if (m_alarmIntensity <= 0.01f) return;

    // Emergency corner siren strobes
    const float strobe = std::sin(m_stateTimer * 16.0f) * 0.5f + 0.5f;
    const float beaconIntensity = (0.50f + 0.50f * strobe) * m_alarmIntensity * 1.85f;
    const Color beaconColor = (m_state == ArenaEventState::Warning)
        ? Color{255, 95, 20, 255}
        : GetArenaEventColor(m_currentEvent);

    // Top-left and top-right siren beacons
    context.Render.DrawPointLight(Vector2f{-720.0f, -440.0f}, 480.0f, beaconColor, beaconIntensity, BlendMode::Add);
    context.Render.DrawPointLight(Vector2f{ 720.0f, -440.0f}, 480.0f, beaconColor, beaconIntensity, BlendMode::Add);

    // Bottom-left and bottom-right auxiliary beacons
    context.Render.DrawPointLight(Vector2f{-720.0f, 440.0f}, 420.0f, beaconColor, beaconIntensity * 0.75f, BlendMode::Add);
    context.Render.DrawPointLight(Vector2f{ 720.0f, 440.0f}, 420.0f, beaconColor, beaconIntensity * 0.75f, BlendMode::Add);

    // Central safety pulse during warning
    if (m_state == ArenaEventState::Warning)
    {
        context.Render.DrawPointLight(Vector2f{0.0f, -400.0f}, 550.0f, Color{255, 140, 25, 255}, beaconIntensity * 0.65f, BlendMode::Add);
    }
}

void DynamicEventSystem::RenderProjectiles(GameContext& context) const
{
    for (Entity proj : m_activeProjectiles)
    {
        if (!m_registry.IsAlive(proj) || !m_registry.HasComponent<Transform2D>(proj)) continue;

        const auto& trans = m_registry.GetComponent<Transform2D>(proj);
        const auto& epc = m_registry.GetComponent<EnvironmentalProjectileComponent>(proj);
        const auto& rb = m_registry.GetComponent<RigidBody>(proj);

        // Draw dynamic trailing wake particles
        const Vector2f dir = (rb.Velocity.Length() > 0.1f) ? (rb.Velocity.Normalized() * -1.0f) : Vector2f{0.0f, -1.0f};
        for (int i = 1; i <= 3; ++i)
        {
            const float trailDist = static_cast<float>(i) * 9.0f;
            const float trailRadius = epc.Radius * (1.0f - static_cast<float>(i) * 0.22f);
            Transform2D trailTrans{trans.Position + dir * trailDist};
            Color trailC = epc.TrailColor;
            trailC.a = static_cast<uint8_t>(200 / (i + 1));
            context.Render.DrawCircle(trailRadius, trailTrans, trailC);
        }

        // Procedural circle if sprite is not loaded
        if (!m_registry.HasComponent<SpriteComponent>(proj))
        {
            context.Render.DrawCircle(epc.Radius, trans, epc.TrailColor);
            context.Render.DrawCircleOutline(epc.Radius, trans, Colors::White, 2.0f);
        }
    }
}

void DynamicEventSystem::RenderUI(GameContext& context, uint32_t fontId) const
{
    if (m_state != ArenaEventState::Warning && m_state != ArenaEventState::Active) return;

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float centerX = viewSize.X * 0.5f;
    const float centerY = viewSize.Y * 0.5f;

    // --- 1. CENTER-SCREEN RETRO-FUTURISTIC TERMINAL OVERLAY ---
    // Prominently shown during Warning and the opening 2.5s of Active state
    const bool showCenterTerminal = (m_state == ArenaEventState::Warning) ||
                                    (m_state == ArenaEventState::Active && (m_stateDuration - m_stateTimer) < 2.5f);

    if (showCenterTerminal)
    {
        constexpr float termW = 700.0f;
        constexpr float termH = 175.0f;
        const Transform2D termTrans{Vector2f{centerX, centerY}};

        // Dark terminal console backing
        context.Render.DrawRectangle(termW, termH, termTrans, Color{8, 6, 12, 240});

        // Blinking red styling for terminal alert (4 Hz square wave with smooth glowing intensity)
        const bool isBlinkOn = (static_cast<int>(m_stateTimer * 4.0f) % 2 == 0);
        const Color redPrimary = isBlinkOn ? Color{255, 30, 30, 255} : Color{170, 20, 20, 200};
        const Color textRed    = isBlinkOn ? Color{255, 40, 40, 255} : Color{180, 25, 25, 210};

        // Terminal border outline
        context.Render.DrawRectangleOutline(termW, termH, termTrans, redPrimary, 3.0f);

        // Terminal corner bracket accents
        constexpr float bracketLen = 22.0f;
        const float left = centerX - termW * 0.5f;
        const float right = centerX + termW * 0.5f;
        const float top = centerY - termH * 0.5f;
        const float bottom = centerY + termH * 0.5f;

        // Top-left bracket
        context.Render.DrawLine(Vector2f{left, top + bracketLen}, Vector2f{left, top}, redPrimary, 4.0f);
        context.Render.DrawLine(Vector2f{left, top}, Vector2f{left + bracketLen, top}, redPrimary, 4.0f);
        // Top-right bracket
        context.Render.DrawLine(Vector2f{right, top + bracketLen}, Vector2f{right, top}, redPrimary, 4.0f);
        context.Render.DrawLine(Vector2f{right, top}, Vector2f{right - bracketLen, top}, redPrimary, 4.0f);
        // Bottom-left bracket
        context.Render.DrawLine(Vector2f{left, bottom - bracketLen}, Vector2f{left, bottom}, redPrimary, 4.0f);
        context.Render.DrawLine(Vector2f{left, bottom}, Vector2f{left + bracketLen, bottom}, redPrimary, 4.0f);
        // Bottom-right bracket
        context.Render.DrawLine(Vector2f{right, bottom - bracketLen}, Vector2f{right, bottom}, redPrimary, 4.0f);
        context.Render.DrawLine(Vector2f{right, bottom}, Vector2f{right - bracketLen, bottom}, redPrimary, 4.0f);

        // Terminal header label
        Transform2D headerTrans{Vector2f{centerX, centerY - 52.0f}};
        context.Render.DrawTextCentered("/// ARENA SYSTEM OVERRIDE : HAZARD PROTOCOL ///", fontId, 17.0f, headerTrans, Color{255, 120, 80, 240});

        // Center blinking RED event title in terminal prompt format
        std::ostringstream titleStream;
        titleStream << "> " << GetArenaEventName(m_currentEvent) << " <";
        Transform2D titleTrans{Vector2f{centerX, centerY - 8.0f}};
        context.Render.DrawTextCentered(titleStream.str(), fontId, 32.0f, titleTrans, textRed);

        // Terminal sub-status line
        std::ostringstream subStream;
        if (m_state == ArenaEventState::Warning)
        {
            subStream << "THREAT IMMINENT // T-MINUS " << std::fixed << std::setprecision(1) << m_stateTimer << "s [";
            const int barBlocks = std::clamp(static_cast<int>((1.0f - GetProgress()) * 14.0f), 0, 14);
            for (int i = 0; i < 14; ++i)
            {
                subStream << (i < barBlocks ? "=" : "-");
            }
            subStream << "] _";
        }
        else
        {
            subStream << "STATUS: ENGAGED // SIMULATION ENVIRONMENT COMPROMISED _";
        }
        Transform2D subTrans{Vector2f{centerX, centerY + 42.0f}};
        context.Render.DrawTextCentered(subStream.str(), fontId, 18.0f, subTrans, Color{255, 180, 180, 230});
    }

    // --- 2. TOP HUD COMPACT BANNER ---
    // Keeps player informed during Active gameplay without obscuring arena line-of-sight
    if (m_state == ArenaEventState::Active)
    {
        constexpr float bannerW = 640.0f;
        constexpr float bannerH = 44.0f;
        const float bannerX = centerX;
        constexpr float bannerY = 65.0f;
        const Transform2D bannerTrans{Vector2f{bannerX, bannerY}};

        const Color eventColor = GetArenaEventColor(m_currentEvent);
        const float pulse = std::sin(m_stateTimer * 6.0f) * 0.15f + 0.85f;
        Color pulseBorder = eventColor;
        pulseBorder.a = static_cast<uint8_t>(255 * pulse);

        // Dark translucent backing
        context.Render.DrawRectangle(bannerW, bannerH, bannerTrans, Color{15, 18, 26, 235});
        context.Render.DrawRectangleOutline(bannerW, bannerH, bannerTrans, pulseBorder, 3.0f);

        // Progress bar (countdown towards event expiration)
        const float remainingRatio = std::clamp(m_stateTimer / m_stateDuration, 0.0f, 1.0f);
        const float barW = bannerW * remainingRatio;
        if (barW > 2.0f)
        {
            Transform2D barTrans{Vector2f{bannerX, bannerY + bannerH * 0.5f - 3.0f}};
            context.Render.DrawRectangle(barW, 4.0f, barTrans, eventColor);
        }

        // Active Event Text: show current phase if Temporal Flux
        std::ostringstream ss;
        if (m_currentEvent == ArenaEventType::TemporalFlux && !m_temporalSlices.empty())
        {
            const bool isSlow = IsTemporalSliceSlow();
            ss << "[EVENT ACTIVE] TEMPORAL FLUX [" << (isSlow ? "SLOW PHASE" : "SPEED PHASE") << "] - "
               << std::fixed << std::setprecision(1) << m_stateTimer << "s";
        }
        else
        {
            ss << "[EVENT ACTIVE] " << GetArenaEventName(m_currentEvent) << " - "
               << std::fixed << std::setprecision(1) << m_stateTimer << "s";
        }

        Transform2D textTrans{Vector2f{bannerX, bannerY - 1.0f}};
        context.Render.DrawTextCentered(ss.str(), fontId, 20.0f, textTrans, Colors::White);
    }
}
