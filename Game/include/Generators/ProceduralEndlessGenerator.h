#pragma once

#include "Generators/ILevelGenerator.h"
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Math/Vector2.h"
#include "ECS/Components/HazardZoneComponent.h"
#include "ECS/Components/BrickComponent.h"
#include <vector>
#include <cstdint>
#include <string>

struct WaveHazardData
{
    Vector2f Position{0.0f, 0.0f};
    Vector2f Size{180.0f, 60.0f};
    HazardEffectType Effect = HazardEffectType::ScreenGlitch;
};

class ProceduralEndlessGenerator : public ILevelGenerator
{
public:
    explicit ProceduralEndlessGenerator(uint32_t seed = 0);
    ~ProceduralEndlessGenerator() override = default;

    int  Generate(Registry& registry, GameContext& context, uint32_t brickTexId) override;
    void Update(float dt, Registry& registry, GameContext& context) override;

    // Endless Wave Management
    int  GenerateWave(int waveNumber, Registry& registry, uint32_t brickTexId = 0);
    int  GenerateWave(int waveNumber, Registry& registry, GameContext& context, uint32_t brickTexId) { (void)context; return GenerateWave(waveNumber, registry, brickTexId); }
    void AdvanceWave(Registry& registry, uint32_t brickTexId = 0);
    void AdvanceWave(Registry& registry, GameContext& context, uint32_t brickTexId) { (void)context; AdvanceWave(registry, brickTexId); }
    void Reset(uint32_t seed = 0);

    // Seed API
    void SetSeed(uint32_t seed) noexcept { m_seed = seed; }
    [[nodiscard]] uint32_t GetSeed() const noexcept { return m_seed; }
    void GenerateRandomSeed() noexcept;

    // Wave Duration Mathematical Formula (Wave 1 = 40s, Wave 100 = 480s, capped at 600s max)
    [[nodiscard]] static float GetWaveTargetDuration(int waveNumber) noexcept
    {
        if (waveNumber <= 1)
        {
            return 40.0f; // Wave 1: 40s (punchy warmup)
        }
        if (waveNumber <= 100)
        {
            // Linear progression: 40s at wave 1 to 480s (8 minutes) at wave 100
            return 40.0f + (static_cast<float>(waveNumber - 1) / 99.0f) * 440.0f;
        }
        // Beyond wave 100: progressive scaling up to 600s (10 minutes max per wave)
        return std::min(600.0f, 480.0f + static_cast<float>(waveNumber - 100) * 4.0f);
    }

    // Wave & Progression queries
    [[nodiscard]] int   GetCurrentWave() const noexcept { return m_currentWave; }
    void                SetCurrentWave(int wave) noexcept { m_currentWave = std::max(1, wave); }
    [[nodiscard]] uint8_t GetArmorTier() const noexcept { return static_cast<uint8_t>(std::min(3, (m_currentWave - 1) / 2)); }
    [[nodiscard]] int   GetBreakableBricksRemaining() const noexcept { return m_breakableRemaining; }
    [[nodiscard]] int   GetWaveTargetBricks() const noexcept { return m_waveTargetBricks; }
    [[nodiscard]] int   GetWaveBricksBroken() const noexcept { return m_waveBricksBroken; }
    
    // Wave Time-based progression (Endurance mode)
    [[nodiscard]] float GetWaveTimer() const noexcept { return m_waveTimer; }
    void                SetWaveTimer(float timer) noexcept { m_waveTimer = timer; }
    [[nodiscard]] float GetCurrentWaveTargetDuration() const noexcept { return m_waveTargetDuration; }
    [[nodiscard]] float GetWaveTimeProgress() const noexcept { return (m_waveTargetDuration > 0.001f) ? std::clamp(m_waveTimer / m_waveTargetDuration, 0.0f, 1.0f) : 1.0f; }
    [[nodiscard]] bool  IsWaveTimeElapsed() const noexcept { return m_waveTimer >= m_waveTargetDuration; }
    [[nodiscard]] bool  IsWaveTargetReached() const noexcept { return m_waveTimer >= m_waveTargetDuration; }
    [[nodiscard]] bool  IsWaveClimaxActive() const noexcept { return GetWaveTimeProgress() >= 0.85f; }

    void                OnBrickDestroyed() noexcept;
    void                DecrementBreakableBricks() noexcept { OnBrickDestroyed(); }

    // Downward Creeping & Dynamic Generation Speed API
    [[nodiscard]] bool  IsCreepEnabled() const noexcept { return m_creepEnabled; }
    void                SetCreepEnabled(bool enabled) noexcept { m_creepEnabled = enabled; }
    [[nodiscard]] float GetCreepInterval() const noexcept;
    [[nodiscard]] float GetReplenishInterval() const noexcept;
    [[nodiscard]] float GetGenerationSpeedRatio() const noexcept;
    [[nodiscard]] float GetDifficultyFactor() const noexcept;
    [[nodiscard]] int   GetMinimumBoardBricks() const noexcept;
    [[nodiscard]] int   GetTargetBoardBricks() const noexcept;
    [[nodiscard]] float GetCreepTimer() const noexcept { return m_creepTimer; }
    [[nodiscard]] float GetCreepProgress() const noexcept;
    [[nodiscard]] bool  IsDangerActive() const noexcept { return m_dangerActive; }
    [[nodiscard]] float GetLowestBrickY() const noexcept { return m_lowestBrickY; }
    void                MultiplyCreepInterval(float factor) noexcept { m_creepIntervalMultiplier *= factor; }

    // Safety boundaries & generation speed caps for endless grid descent
    static constexpr float kMaxAllowedBrickY = 140.0f;     // Absolute lowest allowed brick Y (Paddle is at Y=300.0f, providing 160px safe buffer)
    static constexpr float kEmergencyVaporizeY = 180.0f;  // Instant vaporize threshold for any anomalous brick
    static constexpr float kDangerThresholdY = 110.0f;    // Visual danger pulse threshold
    static constexpr float kInitialCreepInterval = 7.5f;
    static constexpr float kMinCreepInterval = 1.6f;
    static constexpr float kInitialReplenishCooldown = 1.4f;
    static constexpr float kMinReplenishCooldown = 0.30f;

    static constexpr int   kGridCols = 12;
    static constexpr float kBrickWidth = 100.0f;
    static constexpr float kBrickHeight = 30.0f;
    static constexpr float kGridPad = 10.0f;
    static constexpr float kGridStartX = -605.0f;
    static constexpr float kGridStartY = -350.0f;
    static constexpr float kColStep = kBrickWidth + kGridPad;
    static constexpr float kRowStep = kBrickHeight + kGridPad;

    bool CanPushColumn(Registry& registry, int col) const;
    bool PushColumnTopBrick(Registry& registry, int col, uint32_t brickTexId, float spawnSpeed = -1.0f);
    int  SpawnAdaptiveBurst(Registry& registry, int desiredCount, uint32_t brickTexId);
    int  SpawnRowAtY(Registry& registry, float targetY, uint32_t brickTexId);

    // Emergency Defense
    bool CheckEmergencyBurn(Registry& registry, float thresholdY = kEmergencyVaporizeY);

    // Hazard Zones spawned by wave
    [[nodiscard]] const std::vector<WaveHazardData>& GetWaveHazards() const noexcept { return m_waveHazards; }
    [[nodiscard]] std::vector<Vector2f> GetWaveHazardPositions() const;

private:
    void StepDownward(Registry& registry);
    void SpawnTopRow(Registry& registry, uint32_t brickTexId);
    bool EvaluateProceduralCell(int col, int row, float time, int broken, int wave, BrickType& outType, bool& outSpecial) const;
    BrickType ChooseAdaptiveBrickType(Registry& registry, int col, int rowHint, bool& outSpecial) const;

    uint32_t m_seed = 1337420u;
    int   m_currentWave = 1;
    float m_waveTimer = 0.0f;
    float m_waveTargetDuration = 60.0f;
    int   m_waveTargetBricks = 56;
    int   m_waveBricksBroken = 0;
    int   m_breakableRemaining = 0;
    int   m_totalBricksBrokenSession = 0;
    float m_elapsedSurvivalTime = 0.0f;
    int   m_continuousRowIndex = 0;
    int   m_burstIndex = 0;

    bool  m_creepEnabled = true;
    float m_creepTimer = 0.0f;
    float m_creepStepCooldown = 0.0f;
    float m_creepIntervalMultiplier = 1.0f;
    float m_replenishCooldown = 0.0f;
    bool  m_dangerActive = false;
    float m_lowestBrickY = -350.0f;
    uint32_t m_cachedBrickTexId = 0;

    std::vector<WaveHazardData> m_waveHazards;
};
