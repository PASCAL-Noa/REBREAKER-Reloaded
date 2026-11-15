#include "Generators/ProceduralEndlessGenerator.h"
#include "Factories/BrickFactory.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BrickComponent.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>

ProceduralEndlessGenerator::ProceduralEndlessGenerator(uint32_t seed)
    : m_seed(seed),
      m_currentWave(1),
      m_waveTargetBricks(36),
      m_waveBricksBroken(0),
      m_breakableRemaining(0),
      m_totalBricksBrokenSession(0),
      m_elapsedSurvivalTime(0.0f),
      m_continuousRowIndex(0),
      m_creepEnabled(true),
      m_creepTimer(0.0f),
      m_creepStepCooldown(0.0f),
      m_creepIntervalMultiplier(1.0f),
      m_replenishCooldown(0.0f),
      m_dangerActive(false),
      m_lowestBrickY(-350.0f),
      m_cachedBrickTexId(0)
{
    if (m_seed == 0)
    {
        GenerateRandomSeed();
    }
}

void ProceduralEndlessGenerator::GenerateRandomSeed() noexcept
{
    std::random_device rd;
    uint32_t s1 = rd();
    uint32_t s2 = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    m_seed = s1 ^ (s2 * 1664525u + 1013904223u);
    if (m_seed == 0)
    {
        m_seed = 1337420u;
    }
}

float ProceduralEndlessGenerator::GetCreepInterval() const noexcept
{
    // Interval between downward row drops: 18.0s at Wave 1 down to 9.0s at Wave 12+
    float baseInterval = (18.0f - static_cast<float>(m_currentWave - 1) * 0.75f) * m_creepIntervalMultiplier;
    // Intra-wave creep acceleration: drops up to 35% faster near the end of the wave as time runs down
    float intraWaveFactor = 1.0f - 0.35f * GetWaveTimeProgress();
    return std::max(4.5f, baseInterval * intraWaveFactor);
}

float ProceduralEndlessGenerator::GetCreepProgress() const noexcept
{
    float interval = GetCreepInterval();
    if (interval <= 0.001f) return 0.0f;
    return std::clamp(m_creepTimer / interval, 0.0f, 1.0f);
}

void ProceduralEndlessGenerator::Reset(uint32_t seed)
{
    if (seed != 0)
    {
        m_seed = seed;
    }
    else
    {
        GenerateRandomSeed();
    }

    m_currentWave = 1;
    m_waveTimer = 0.0f;
    m_waveTargetDuration = GetWaveTargetDuration(1);
    m_waveTargetBricks = 36;
    m_waveBricksBroken = 0;
    m_breakableRemaining = 0;
    m_totalBricksBrokenSession = 0;
    m_elapsedSurvivalTime = 0.0f;
    m_continuousRowIndex = 0;
    m_creepTimer = 0.0f;
    m_creepStepCooldown = 0.0f;
    m_creepIntervalMultiplier = 1.0f;
    m_replenishCooldown = 0.0f;
    m_dangerActive = false;
    m_lowestBrickY = -350.0f;
    m_waveHazards.clear();
}

void ProceduralEndlessGenerator::OnBrickDestroyed() noexcept
{
    ++m_waveBricksBroken;
    ++m_totalBricksBrokenSession;
    if (m_breakableRemaining > 0)
    {
        --m_breakableRemaining;
    }
}

std::vector<Vector2f> ProceduralEndlessGenerator::GetWaveHazardPositions() const
{
    std::vector<Vector2f> positions;
    positions.reserve(m_waveHazards.size());
    for (const auto& h : m_waveHazards)
    {
        positions.push_back(h.Position);
    }
    return positions;
}

int ProceduralEndlessGenerator::Generate(Registry& registry, GameContext& context, uint32_t brickTexId)
{
    (void)context;
    m_cachedBrickTexId = brickTexId;
    return GenerateWave(m_currentWave, registry, brickTexId);
}

bool ProceduralEndlessGenerator::EvaluateProceduralCell(int col, int row, float time, int broken, int wave, BrickType& outType, bool& outSpecial) const
{
    const int cols = 12;
    int symCol = std::min(col, cols - 1 - col); // 0 to 5 symmetry
    float cx = static_cast<float>(symCol) / 5.0f;
    float ry = static_cast<float>(row);

    // Continuous evolving procedural fields based on survival time, broken bricks, and run seed
    float seedPhase = static_cast<float>(m_seed & 0xFFFF) * 0.0137f;
    float tPhase = time * 0.045f;
    float bPhase = static_cast<float>(broken) * 0.075f;
    float wHarmonic = static_cast<float>(wave) * 0.42f;

    // Harmonic multi-frequency noise field modulated by seed
    float density = std::sin(cx * 3.14159f * 1.5f + tPhase + wHarmonic + seedPhase) * 0.52f
                  + std::cos((ry * 0.8f + bPhase) * 1.25f + seedPhase * 0.7f) * 0.42f
                  + std::sin((cx * 2.0f + ry * 0.4f) * 2.1f + tPhase * 0.6f + seedPhase * 1.3f) * 0.28f;

    // Pseudo-random deterministic hash for cell variation with run seed
    uint32_t cellHash = (static_cast<uint32_t>(symCol) * 73856093u ^ static_cast<uint32_t>(row + 1) * 19349663u ^ static_cast<uint32_t>(wave) * 83492791u ^ (m_seed * 2654435761u));
    float cellRnd = static_cast<float>(cellHash % 1000) / 1000.0f;
    float score = density + (cellRnd - 0.5f) * 0.32f;

    // Structural density cutoff: creates organic clusters, pathways, and voids
    if (score < -0.12f)
    {
        return false;
    }

    outSpecial = false;
    if (cellRnd < 0.035f)
    {
        outType = BrickType::Special;
        outSpecial = true;
        return true;
    }

    // Outer flank metal barriers on wave 3+
    if (wave >= 3 && symCol == 0 && (row % 3 == 0) && (cellHash % 3 == 0))
    {
        outType = BrickType::Metal;
        return true;
    }

    // Volatile explosive bomb clusters
    if (score > 0.60f && (cellHash % 8 == 0))
    {
        outType = BrickType::Bomb;
        return true;
    }

    // Hard bricks scale with wave, score, and intra-wave time progress
    float intraProgress = GetWaveTimeProgress();
    float hardThreshold = std::max(0.12f, (0.45f - static_cast<float>(wave) * 0.035f) - (intraProgress * 0.15f));
    if (score > hardThreshold)
    {
        outType = BrickType::Hard;
        return true;
    }

    // Medium bricks
    if (score > 0.05f)
    {
        outType = BrickType::Medium;
        return true;
    }

    outType = BrickType::Light;
    return true;
}

int ProceduralEndlessGenerator::GenerateWave(int waveNumber, Registry& registry, uint32_t brickTexId)
{
    m_cachedBrickTexId = brickTexId;
    m_currentWave = std::max(1, waveNumber);
    m_waveTimer = 0.0f;
    m_waveTargetDuration = GetWaveTargetDuration(m_currentWave);
    m_waveTargetBricks = std::min(72, 28 + m_currentWave * 8);
    m_waveBricksBroken = 0;
    m_creepTimer = 0.0f;
    m_dangerActive = false;
    m_waveHazards.clear();

    const float bWidth = 100.0f;
    const float bHeight = 30.0f;
    const float startX = -605.0f;
    const float startY = -350.0f;
    const float pad = 10.0f;
    const int cols = 12;

    int totalRows = std::min(8, 4 + (m_currentWave / 2));
    uint8_t armor = GetArmorTier();

    int count = 0;
    m_lowestBrickY = startY;

    for (int row = 0; row < totalRows; ++row)
    {
        m_continuousRowIndex = row;
        for (int col = 0; col < cols; ++col)
        {
            BrickType type = BrickType::Light;
            bool isSpecial = false;

            bool place = EvaluateProceduralCell(col, row, m_elapsedSurvivalTime, m_totalBricksBrokenSession, m_currentWave, type, isSpecial);
            if (!place) continue;

            float x = startX + static_cast<float>(col) * (bWidth + pad);
            float y = startY + static_cast<float>(row) * (bHeight + pad);

            BrickFactory::Create(registry, x, y, type, isSpecial, brickTexId, armor);

            if (type != BrickType::Metal && type != BrickType::Special)
            {
                ++count;
            }

            if (y > m_lowestBrickY)
            {
                m_lowestBrickY = y;
            }
        }
    }

    // Ensure we have a valid breakable target matching generated count if lower
    if (count > 0 && count < m_waveTargetBricks)
    {
        m_waveTargetBricks = count;
    }

    // Procedural Hazard Zones scaling with wave difficulty and varied effects
    // Wave 1: 0 hazards (warmup)
    // Wave 2: 1 hazard (ScreenGlitch - triple illusion chromatic glitch)
    // Wave 3: 2 hazards (UltimateDrain -> drains ultimate bar!, ScreenGlitch)
    // Wave 4: 2 hazards (UltimateDrain, CameraTilt)
    // Wave 5+: 2-3 hazards cycling through all effects
    if (m_currentWave >= 2)
    {
        int hazardCount = 1;
        if (m_currentWave >= 3) hazardCount = 2;
        if (m_currentWave >= 5) hazardCount = 3;

        const HazardEffectType effectCycle[] = {
            HazardEffectType::ScreenGlitch,     // Triple-vision chromatic glitch
            HazardEffectType::UltimateDrain,    // Drains the ultimate ability bar!
            HazardEffectType::CameraTilt,
            HazardEffectType::CameraZoomPulse,
            HazardEffectType::TemporalAudioWarp,
            HazardEffectType::AllMaluses
        };

        uint32_t seed = static_cast<uint32_t>(m_currentWave * 7919 + 1337) ^ m_seed;
        auto nextRnd = [&seed]() -> float {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            return static_cast<float>(seed % 10000) / 10000.0f;
        };

        for (int h = 0; h < hazardCount; ++h)
        {
            float hx = startX + nextRnd() * static_cast<float>(cols - 1) * (bWidth + pad);
            float hy = startY + static_cast<float>(totalRows) * (bHeight + pad) + 35.0f + static_cast<float>(h) * 45.0f;

            size_t effectIndex = static_cast<size_t>((m_currentWave - 2 + h) % 6);
            HazardEffectType chosenEffect = effectCycle[effectIndex];

            WaveHazardData hazardData;
            hazardData.Position = Vector2f{hx, hy};
            hazardData.Size = Vector2f{180.0f, 55.0f};
            hazardData.Effect = chosenEffect;
            m_waveHazards.push_back(hazardData);
        }
    }

    m_breakableRemaining = count;
    return count;
}

void ProceduralEndlessGenerator::AdvanceWave(Registry& registry, uint32_t brickTexId)
{
    ++m_currentWave;
    GenerateWave(m_currentWave, registry, brickTexId);
}

void ProceduralEndlessGenerator::Update(float dt, Registry& registry, GameContext& context)
{
    (void)context;
    m_elapsedSurvivalTime += dt;
    m_waveTimer += dt;
    if (m_replenishCooldown > 0.0f)
    {
        m_replenishCooldown = std::max(0.0f, m_replenishCooldown - dt);
    }

    // 1. Recalculate lowest brick Y, highest brick Y, and count remaining breakable bricks
    int remaining = 0;
    float lowestY = -350.0f;
    float highestY = 300.0f;
    int totalBricks = 0;

    registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& transform, const BrickComponent& brick) {
        ++totalBricks;
        if (brick.Type != BrickType::Metal && brick.Type != BrickType::Special && !brick.IsUnbreakable)
        {
            ++remaining;
        }
        if (transform.Position.Y > lowestY)
        {
            lowestY = transform.Position.Y;
        }
        if (transform.Position.Y < highestY)
        {
            highestY = transform.Position.Y;
        }
    });

    if (totalBricks == 0)
    {
        lowestY = -350.0f;
        highestY = -350.0f;
    }

    m_breakableRemaining = remaining;
    m_lowestBrickY = lowestY;
    m_dangerActive = (lowestY >= kDangerThresholdY);

    // Emergency boundary defense: vaporize any anomalous brick exceeding threshold
    CheckEmergencyBurn(registry, kEmergencyVaporizeY);

    // 2. Downward Creep Mechanism
    if (m_creepEnabled && m_breakableRemaining > 0)
    {
        m_creepTimer += dt;
        if (m_creepTimer >= GetCreepInterval())
        {
            m_creepTimer = 0.0f;
            if (m_lowestBrickY + 40.0f <= kMaxAllowedBrickY)
            {
                StepDownward(registry);
                SpawnRowAtY(registry, -350.0f, m_cachedBrickTexId);
            }
            else if (highestY > -310.0f)
            {
                // Ceiling is at maximum safe descent: do not push closer to paddle!
                // Backfill top row if open space exists
                SpawnRowAtY(registry, -350.0f, m_cachedBrickTexId);
            }
        }
    }

    // 3. Fluid Dynamic Board Replenishment:
    // Player should NEVER run out of bricks ("qu'on ne soit jamais a cours de briques").
    // Case A: Board is completely empty of breakable bricks -> IMMEDIATE instant replenishment
    if (m_breakableRemaining == 0 && !IsWaveTimeElapsed())
    {
        SpawnRowAtY(registry, -350.0f, m_cachedBrickTexId);
        SpawnRowAtY(registry, -310.0f, m_cachedBrickTexId);
        SpawnRowAtY(registry, -270.0f, m_cachedBrickTexId);
        m_replenishCooldown = 0.8f;
    }
    // Case B: Breakable bricks are low (< 14) and replenishment cooldown is ready -> smoothly replenish
    else if (m_breakableRemaining < 14 && m_replenishCooldown <= 0.0f && !IsWaveTimeElapsed())
    {
        if (m_lowestBrickY + 40.0f <= kMaxAllowedBrickY && m_lowestBrickY < 30.0f)
        {
            StepDownward(registry);
            SpawnRowAtY(registry, -350.0f, m_cachedBrickTexId);
        }
        else
        {
            // Find highest unoccupied row from -350 down to -150
            const float candidateRows[] = { -350.0f, -310.0f, -270.0f, -230.0f, -190.0f, -150.0f };
            for (float rY : candidateRows)
            {
                bool rowOccupied = false;
                registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, BrickComponent&) {
                    if (std::abs(t.Position.Y - rY) < 18.0f)
                    {
                        rowOccupied = true;
                    }
                });

                if (!rowOccupied)
                {
                    SpawnRowAtY(registry, rY, m_cachedBrickTexId);
                    break;
                }
            }
        }
        m_replenishCooldown = 1.0f;
    }
}

void ProceduralEndlessGenerator::StepDownward(Registry& registry)
{
    // Strict safety check: verify no brick will ever exceed kMaxAllowedBrickY
    bool canStep = true;
    registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& transform, BrickComponent&) {
        if (transform.Position.Y + 40.0f > kMaxAllowedBrickY)
        {
            canStep = false;
        }
    });

    if (!canStep) return;

    const float stepAmount = 40.0f; // (bHeight 30.0f + pad 10.0f)
    registry.View<Transform2D, BrickComponent>([stepAmount](Entity, Transform2D& transform, BrickComponent&) {
        transform.Position.Y += stepAmount;
    });
}

int ProceduralEndlessGenerator::SpawnRowAtY(Registry& registry, float targetY, uint32_t brickTexId)
{
    const float bWidth = 100.0f;
    const float startX = -605.0f;
    const float pad = 10.0f;
    const int cols = 12;

    ++m_continuousRowIndex;
    uint8_t armor = GetArmorTier();
    int spawnedCount = 0;

    for (int col = 0; col < cols; ++col)
    {
        float x = startX + static_cast<float>(col) * (bWidth + pad);

        // Check if a brick already exists near (x, targetY)
        bool slotOccupied = false;
        registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, BrickComponent&) {
            if (std::abs(t.Position.X - x) < 40.0f && std::abs(t.Position.Y - targetY) < 18.0f)
            {
                slotOccupied = true;
            }
        });

        if (slotOccupied) continue;

        BrickType type = BrickType::Light;
        bool isSpecial = false;

        bool place = EvaluateProceduralCell(col, m_continuousRowIndex, m_elapsedSurvivalTime, m_totalBricksBrokenSession, m_currentWave, type, isSpecial);
        if (!place) continue;

        BrickFactory::Create(registry, x, targetY, type, isSpecial, brickTexId, armor);
        ++spawnedCount;

        if (type != BrickType::Metal && type != BrickType::Special)
        {
            ++m_breakableRemaining;
        }
    }

    // Safety fallback: ensure a newly spawned row contributes at least 4 breakable bricks
    if (spawnedCount < 4)
    {
        for (int col = 2; col < 10; col += 2)
        {
            float x = startX + static_cast<float>(col) * (bWidth + pad);
            bool slotOccupied = false;
            registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, BrickComponent&) {
                if (std::abs(t.Position.X - x) < 40.0f && std::abs(t.Position.Y - targetY) < 18.0f)
                {
                    slotOccupied = true;
                }
            });
            if (!slotOccupied)
            {
                BrickFactory::Create(registry, x, targetY, BrickType::Light, false, brickTexId, armor);
                ++spawnedCount;
                ++m_breakableRemaining;
            }
        }
    }

    return spawnedCount;
}

void ProceduralEndlessGenerator::SpawnTopRow(Registry& registry, uint32_t brickTexId)
{
    SpawnRowAtY(registry, -350.0f, brickTexId);
}

bool ProceduralEndlessGenerator::CheckEmergencyBurn(Registry& registry, float thresholdY)
{
    bool burned = false;
    std::vector<Entity> toDestroy;

    registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, BrickComponent&) {
        if (t.Position.Y >= thresholdY)
        {
            toDestroy.push_back(e);
            burned = true;
        }
    });

    for (Entity e : toDestroy)
    {
        registry.DestroyEntityDeferred(e);
    }

    if (burned)
    {
        registry.ProcessDeferredCommands();
    }
    return burned;
}
