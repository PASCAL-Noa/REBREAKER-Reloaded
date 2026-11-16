#pragma once

#include "Scenes/GameScene.h"
#include "Generators/ProceduralEndlessGenerator.h"
#include "Endless/RogueliteDraft.h"
#include <vector>
#include <string>

class EndlessScene : public GameScene
{
public:
    EndlessScene();
    ~EndlessScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    // Progression Overrides: Endless Mode runs infinitely without level clearing
    [[nodiscard]] bool HasNextLevel() const override { return false; }
    void OnGameOverEnter() override;
    void OnGameOverExit() override;
    void OnVictoryEnter() override;

    // Post processing override to ensure modals are never deformed
    [[nodiscard]] PostProcessParams GetPostProcessParams() const override;

    // Paddle modifiers overrides
    [[nodiscard]] float GetBasePaddleScaleX() const override { return m_paddleWidthMultiplier; }
    [[nodiscard]] float GetBasePaddleSpeedMultiplier() const override { return m_paddleSpeedMultiplier; }

    // Endless Metrics Queries
    [[nodiscard]] float GetSurvivalTime() const noexcept { return m_survivalTime; }
    [[nodiscard]] std::string GetSurvivalTimeFormatted() const;
    [[nodiscard]] int   GetWavesCleared() const noexcept { return m_wavesCleared; }
    [[nodiscard]] int   GetCurrentWave() const noexcept { return m_endlessGenerator.GetCurrentWave(); }
    [[nodiscard]] float GetScoreMultiplier() const noexcept { return m_endlessScoreMultiplier; }

    // Seed API
    void SetEndlessSeed(uint32_t seed) noexcept { m_endlessGenerator.SetSeed(seed); }
    [[nodiscard]] uint32_t GetEndlessSeed() const noexcept { return m_endlessGenerator.GetSeed(); }

    // Draft API
    void TriggerDraft(DraftType type = DraftType::PeriodicSupply);
    void RerollDraft();
    void SelectDraftCard(int index);
    [[nodiscard]] bool IsDraftActive() const noexcept { return m_draftActive; }
    [[nodiscard]] DraftType GetActiveDraftType() const noexcept { return m_activeDraftType; }
    [[nodiscard]] int  GetAvailableRerolls() const noexcept { return m_remainingRerolls; }
    [[nodiscard]] const std::vector<RogueliteCard>& GetCurrentDraftCards() const noexcept { return m_currentDraftCards; }

    // Perk Applicator Hook
    void ApplyPerk(RoguelitePerkType type);

    // Gameplay modifiers
    void AddScoreMultiplier(float bonus) noexcept { m_endlessScoreMultiplier += bonus; }
    void AddBallDamageBonus(int bonus) noexcept { m_ballDamageBonus += bonus; }
    void SetLightningRod(bool active) noexcept { m_lightningRodActive = active; }
    void SetPlasmaField(bool active) noexcept { m_plasmaFieldActive = active; }
    void SetMagneticTether(bool active) noexcept { m_magneticTetherActive = active; }
    void SetOverdriveCapacitor(bool active) noexcept { m_overdriveCapacitorActive = active; }
    void SetShieldSiphon(bool active) noexcept { m_shieldSiphonActive = active; }
    void TriggerVortexPulse();
    void MultiplyPaddleWidth(float factor);
    void MultiplyPaddleSpeed(float factor);

    // Wave hazards & lighting
    void ClearHazardZones();
    void ApplyWaveTheme(int wave);

protected:
    void RenderSceneSpecificHUD(GameContext& context) override;
    void HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false, int damage = 1) override;
    void RenderEndlessHUD(GameContext& context) const;
    void RenderDraftModal(GameContext& context) const;
    void RenderTripleVisionPaddle(GameContext& context);
    void CheckWaveClear(GameContext& context);

private:
    ProceduralEndlessGenerator m_endlessGenerator;

    // Endurance metrics
    float m_survivalTime = 0.0f;
    int   m_wavesCleared = 0;
    float m_endlessScoreMultiplier = 1.0f;
    float m_nextMinuteDraftTime = 60.0f;
    int   m_consecutiveWavesForShield = 0;

    // Roguelite Perks state
    int   m_ballDamageBonus = 0;
    bool  m_lightningRodActive = false;
    bool  m_plasmaFieldActive = false;
    bool  m_magneticTetherActive = false;
    bool  m_overdriveCapacitorActive = false;
    bool  m_shieldSiphonActive = false;
    bool  m_midWaveHazardSpawned = false;
    float m_paddleWidthMultiplier = 1.0f;
    float m_paddleSpeedMultiplier = 1.0f;
    float m_basePaddleWidth = 160.0f;
    float m_basePaddleSpeed = 900.0f;

    // Draft modal state
    bool      m_draftActive = false;
    DraftType m_activeDraftType = DraftType::PeriodicSupply;
    int       m_remainingRerolls = 0;
    int       m_selectedDraftIndex = 0;
    float     m_draftAnimProgress = 0.0f;
    std::vector<RogueliteCard> m_currentDraftCards;
    uint32_t  m_draftRndSeed = 987654321;

    // Wave Clear celebration banner
    bool  m_waveClearBannerActive = false;
    float m_waveClearBannerTimer = 0.0f;
    int   m_justClearedWaveNumber = 1;

    // Emergency Burnout cooldown
    float m_emergencyBurnCooldown = 0.0f;

    // Re-entrancy guard for brick collisions
    bool  m_isHandlingCollision = false;

    // Bottom barrier entity
    Entity m_bottomBarrierEntity = NULL_ENTITY;
};
