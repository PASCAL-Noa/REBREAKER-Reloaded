#pragma once
#include "ECS/System.h"
#include "ECS/Entity.h"
#include "Events/ArenaEvents.h"
#include "Math/Vector2.h"
#include <vector>
#include <functional>
#include <random>

struct GameContext;

class DynamicEventSystem : public System
{
public:
    explicit DynamicEventSystem(Registry& registry, GameContext* context = nullptr);
    DynamicEventSystem(Registry& registry, GameContext& context);
    ~DynamicEventSystem() override;

    void OnInit() override;
    void OnUpdate(float dt) override;

    // Real wall-clock lifecycle update (for accurate event durations and warnings)
    void UpdateLifecycle(float realDt);

    // Rendering passes
    void RenderSearchlights(GameContext& context) const;
    void RenderAlarmLights(GameContext& context) const;
    void RenderProjectiles(GameContext& context) const;
    void RenderUI(GameContext& context, uint32_t fontId) const;

    // Configuration & Stage Rules
    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    void SetIntervalRange(float minInterval, float maxInterval);
    [[nodiscard]] float GetMinInterval() const noexcept { return m_minInterval; }
    [[nodiscard]] float GetMaxInterval() const noexcept { return m_maxInterval; }

    void SetWarningDuration(float duration);
    [[nodiscard]] float GetWarningDuration() const noexcept { return m_warningDuration; }

    void SetEventDuration(float duration);
    [[nodiscard]] float GetEventDuration() const noexcept { return m_eventDuration; }

    void SetCooldownDuration(float duration);
    [[nodiscard]] float GetCooldownDuration() const noexcept { return m_cooldownDuration; }

    // Manual Event Control (Sandbox / Debug / Testing)
    void TriggerEvent(ArenaEventType type, bool skipWarning = false);
    void TriggerRandomEvent(bool skipWarning = false);
    void StopActiveEvent();
    void ResetLifecycle();
    void RestoreStandardPhysics();

    // Lifecycle Queries
    [[nodiscard]] ArenaEventState GetState() const noexcept { return m_state; }
    [[nodiscard]] ArenaEventType  GetCurrentEventType() const noexcept { return m_currentEvent; }
    [[nodiscard]] float           GetStateTimer() const noexcept { return m_stateTimer; }
    [[nodiscard]] float           GetStateDuration() const noexcept { return m_stateDuration; }
    [[nodiscard]] float           GetTimeRemaining() const noexcept { return m_stateTimer; }
    [[nodiscard]] float           GetProgress() const noexcept;
    [[nodiscard]] bool            IsEventActive() const noexcept { return m_state == ArenaEventState::Active; }
    [[nodiscard]] bool            IsWarningActive() const noexcept { return m_state == ArenaEventState::Warning; }

    // Physical Alterations
    [[nodiscard]] float           GetTimeScale() const noexcept { return m_timeScale; }
    [[nodiscard]] const Vector2f& GetGravityForce() const noexcept { return m_gravityForce; }
    [[nodiscard]] float           GetArenaTiltAngle() const noexcept { return m_arenaTiltAngle; }
    [[nodiscard]] const Vector2f& GetCameraShakeOffset() const noexcept { return m_cameraShakeOffset; }
    [[nodiscard]] float           GetCameraShakeAngle() const noexcept { return m_cameraShakeAngle; }
    [[nodiscard]] float           GetBlackoutIntensity() const noexcept { return m_blackoutIntensity; }
    [[nodiscard]] float           GetAlarmIntensity() const noexcept { return m_alarmIntensity; }
    [[nodiscard]] Color           GetAlarmColor() const noexcept { return m_alarmColor; }

    // Temporal Flux Multi-variation
    struct TemporalSlice
    {
        bool  IsSlow = false;
        float Duration = 0.0f;
    };

    [[nodiscard]] bool                             IsTemporalSliceSlow() const noexcept;
    [[nodiscard]] size_t                           GetTemporalSliceCount() const noexcept { return m_temporalSlices.size(); }
    [[nodiscard]] size_t                           GetCurrentTemporalSliceIndex() const noexcept { return m_currentTemporalSliceIndex; }
    [[nodiscard]] float                            GetTemporalSliceTimeRemaining() const noexcept { return m_temporalSliceTimer; }
    [[nodiscard]] const std::vector<TemporalSlice>& GetTemporalSlices() const noexcept { return m_temporalSlices; }

    // Projectile Management (Meteor Shower)
    Entity SpawnMeteor(const Vector2f& position, const Vector2f& velocity);
    void   ClearProjectiles();
    [[nodiscard]] const std::vector<Entity>& GetActiveProjectiles() const noexcept { return m_activeProjectiles; }
    [[nodiscard]] size_t                     GetActiveProjectileCount() const noexcept { return m_activeProjectiles.size(); }

    void SetPaddleEntity(Entity paddle) noexcept { m_paddleEntity = paddle; }
    void SetBrickCollisionCallback(std::function<void(Entity brick, Entity projectile)> cb) { m_onBrickHit = std::move(cb); }
    void SetScoreCallback(std::function<void(uint32_t score, uint32_t mult)> cb) { m_onScoreAdded = std::move(cb); }

private:
    void EnterState(ArenaEventState newState);
    void ActivateEvent(ArenaEventType type);
    void GenerateTemporalSlices(float totalDuration = 20.0f);
    void ApplyTemporalSlice(size_t index);
    void ApplyBallAgitationKicks();
    void UpdateMeteors(float dt);
    void CheckMeteorPaddleCollisions();
    void CheckMeteorBrickCollisions();
    ArenaEventType SelectRandomEvent();
    float GetNextInterval();

    GameContext*       mp_context = nullptr;
    bool               m_enabled = true;
    ArenaEventState    m_state = ArenaEventState::Idle;
    ArenaEventType     m_currentEvent = ArenaEventType::None;

    float              m_stateTimer = 0.0f;
    float              m_stateDuration = 0.0f;
    float              m_minInterval = 25.0f;
    float              m_maxInterval = 40.0f;
    float              m_warningDuration = 3.0f;
    float              m_eventDuration = 10.0f;
    float              m_cooldownDuration = 8.0f;

    // Physical Alterations
    float              m_timeScale = 1.0f;
    Vector2f           m_gravityForce{0.0f, 0.0f};
    float              m_arenaTiltAngle = 0.0f;
    Vector2f           m_cameraShakeOffset{0.0f, 0.0f};
    float              m_cameraShakeAngle = 0.0f;
    float              m_blackoutIntensity = 0.0f;
    float              m_alarmIntensity = 0.0f;
    Color              m_alarmColor{255, 30, 30, 255};

    // Archetype Internal Timers & State
    float              m_ballAgitationTimer = 0.0f;
    float              m_meteorSpawnTimer = 0.0f;
    float              m_searchlightTime = 0.0f;
    float              m_warningKlaxonTimer = 0.0f;

    // Temporal Flux Multi-variation state
    std::vector<TemporalSlice> m_temporalSlices;
    size_t                     m_currentTemporalSliceIndex = 0;
    float                      m_temporalSliceTimer = 0.0f;

    // Audio & Sprite IDs
    uint32_t           m_klaxonSfxId = 0;
    uint32_t           m_meteorTexId = 0;
    uint32_t           m_deflectSfxId = 0;
    uint32_t           m_meteorExplodeSfxId = 0;

    Entity             m_paddleEntity = NULL_ENTITY;
    std::vector<Entity> m_activeProjectiles;

    std::function<void(Entity, Entity)>   m_onBrickHit;
    std::function<void(uint32_t, uint32_t)> m_onScoreAdded;

    std::mt19937       m_rng;
};
