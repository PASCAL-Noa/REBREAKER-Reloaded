#include "Generators/ProceduralEndlessGenerator.h"
#include "Factories/BrickFactory.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/BoxCollider.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>

ProceduralEndlessGenerator::ProceduralEndlessGenerator(uint32_t seed)
    : m_seed(seed),
      m_currentWave(1),
      m_waveTargetBricks(56),
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
    // Smooth progressive acceleration driven by wave progression, survival time, and intra-wave pressure
    const float waveDrop = static_cast<float>(m_currentWave - 1) * 0.55f;
    const float timeDrop = m_elapsedSurvivalTime * 0.014f;
    const float baseInterval = kInitialCreepInterval - waveDrop - timeDrop;
    // Intra-wave creep acceleration: drops up to 35% faster near the end of the wave as time runs down
    const float intraWaveFactor = 1.0f - 0.35f * GetWaveTimeProgress();
    const float rawInterval = std::max(kMinCreepInterval, baseInterval * intraWaveFactor);
    return rawInterval * m_creepIntervalMultiplier;
}

float ProceduralEndlessGenerator::GetReplenishInterval() const noexcept
{
    const float waveDrop = static_cast<float>(m_currentWave - 1) * 0.11f;
    const float timeDrop = m_elapsedSurvivalTime * 0.0028f;
    return std::max(kMinReplenishCooldown, kInitialReplenishCooldown - waveDrop - timeDrop);
}

float ProceduralEndlessGenerator::GetGenerationSpeedRatio() const noexcept
{
    const float waveDrop = static_cast<float>(m_currentWave - 1) * 0.55f;
    const float timeDrop = m_elapsedSurvivalTime * 0.014f;
    const float baseInterval = kInitialCreepInterval - waveDrop - timeDrop;
    const float intraWaveFactor = 1.0f - 0.35f * GetWaveTimeProgress();
    const float rawInterval = std::max(kMinCreepInterval, baseInterval * intraWaveFactor);
    const float span = kInitialCreepInterval - kMinCreepInterval;
    if (span <= 0.001f) return 1.0f;
    return std::clamp((kInitialCreepInterval - rawInterval) / span, 0.0f, 1.0f);
}

float ProceduralEndlessGenerator::GetDifficultyFactor() const noexcept
{
    const float waveFactor = std::clamp(static_cast<float>(m_currentWave - 1) / 8.0f, 0.0f, 1.0f);
    const float timeFactor = std::clamp(m_elapsedSurvivalTime / 300.0f, 0.0f, 1.0f);
    return std::clamp(waveFactor * 0.65f + timeFactor * 0.35f + GetWaveTimeProgress() * 0.15f, 0.0f, 1.0f);
}

int ProceduralEndlessGenerator::GetMinimumBoardBricks() const noexcept
{
    return 28 + static_cast<int>(std::round(26.0f * GetDifficultyFactor()));
}

int ProceduralEndlessGenerator::GetTargetBoardBricks() const noexcept
{
    return 58 + static_cast<int>(std::round(38.0f * GetDifficultyFactor()));
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
    m_waveTargetBricks = 56;
    m_waveBricksBroken = 0;
    m_breakableRemaining = 0;
    m_totalBricksBrokenSession = 0;
    m_elapsedSurvivalTime = 0.0f;
    m_continuousRowIndex = 0;
    m_burstIndex = 0;
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
    const int cols = kGridCols;
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

    // Structural density cutoff: denser formations with intentional pathways
    if (score < -0.34f)
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

    // Volatile explosive bomb clusters (and early explosive tension on Wave 1)
    if ((score > 0.60f && (cellHash % 8 == 0)) || (wave == 1 && row == 1 && symCol == 2))
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

BrickType ProceduralEndlessGenerator::ChooseAdaptiveBrickType(Registry& registry, int col, int rowHint, bool& outSpecial) const
{
    BrickType type = BrickType::Light;
    outSpecial = false;

    if (!EvaluateProceduralCell(col, rowHint, m_elapsedSurvivalTime, m_totalBricksBrokenSession, m_currentWave, type, outSpecial))
    {
        const float diff = GetDifficultyFactor();
        const uint32_t h = (static_cast<uint32_t>(col + 1) * 73856093u)
                         ^ (static_cast<uint32_t>(rowHint + 7) * 19349663u)
                         ^ (static_cast<uint32_t>(m_currentWave) * 83492791u)
                         ^ (m_seed * 2654435761u);
        const float roll = static_cast<float>(h % 1000) / 1000.0f;

        if (roll < 0.045f)
        {
            outSpecial = true;
            return BrickType::Special;
        }
        if (roll < 0.11f)
        {
            return BrickType::Bomb;
        }
        if (roll < 0.22f + 0.35f * diff)
        {
            return BrickType::Hard;
        }
        if (roll < 0.55f + 0.20f * diff)
        {
            return BrickType::Medium;
        }
        return BrickType::Light;
    }

    // Prevent more than 1 unbreakable Metal brick in the same column so columns never jam permanently
    if (type == BrickType::Metal)
    {
        const float colX = kGridStartX + static_cast<float>(col) * kColStep;
        int metalInCol = 0;
        registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent& brick) {
            if (std::abs(t.Position.X - colX) < 90.0f && brick.Type == BrickType::Metal)
            {
                ++metalInCol;
            }
        });
        if (metalInCol >= 1)
        {
            return BrickType::Hard;
        }
    }

    return type;
}

int ProceduralEndlessGenerator::GenerateWave(int waveNumber, Registry& registry, uint32_t brickTexId)
{
    m_cachedBrickTexId = brickTexId;
    m_currentWave = std::max(1, waveNumber);
    m_waveTimer = 0.0f;
    m_waveTargetDuration = GetWaveTargetDuration(m_currentWave);
    m_waveTargetBricks = GetTargetBoardBricks();
    m_waveBricksBroken = 0;
    m_creepTimer = 0.0f;
    m_burstIndex = 0;
    m_dangerActive = false;
    m_waveHazards.clear();

    const uint8_t armor = GetArmorTier();
    const float descendSpeed = 340.0f + 200.0f * GetGenerationSpeedRatio();
    const float seedPhase = static_cast<float>(m_seed & 0xFFFF) * 0.021f;
    const float diff = GetDifficultyFactor();

    // Sculpted per-column heights (pillars, arches, valleys) rather than rigid flat rows
    const int baseDepth = 5 + static_cast<int>(std::round(3.5f * diff));
    int colDepth[kGridCols]{};
    int maxDepth = 0;

    for (int col = 0; col < kGridCols; ++col)
    {
        const float normX = static_cast<float>(col) / static_cast<float>(kGridCols - 1);
        const float waveShape = std::sin(normX * 3.14159f * (1.2f + static_cast<float>(m_currentWave % 3)) + seedPhase) * 1.8f;
        const uint32_t colHash = (static_cast<uint32_t>(col + 11) * 2654435761u) ^ (static_cast<uint32_t>(m_currentWave) * 83492791u) ^ m_seed;
        const int jitter = static_cast<int>(colHash % 3) - 1; // -1, 0, +1
        int depth = std::clamp(baseDepth + static_cast<int>(std::round(waveShape)) + jitter, 4, 10);

        while (kGridStartY + static_cast<float>(depth - 1) * kRowStep > kMaxAllowedBrickY && depth > 1)
        {
            --depth;
        }
        colDepth[col] = depth;
        if (depth > maxDepth)
        {
            maxDepth = depth;
        }
    }

    int count = 0;
    m_lowestBrickY = kGridStartY;

    auto spawnAtCell = [&](int col, int row, BrickType type, bool isSpecial) {
        const float x = kGridStartX + static_cast<float>(col) * kColStep;
        const float y = kGridStartY + static_cast<float>(row) * kRowStep;

        Entity b = BrickFactory::Create(registry, x, y, type, isSpecial, brickTexId, armor);
        if (registry.IsAlive(b) && registry.HasComponent<Transform2D>(b) && registry.HasComponent<BrickComponent>(b))
        {
            auto& bComp = registry.GetComponent<BrickComponent>(b);
            auto& bTrans = registry.GetComponent<Transform2D>(b);
            bComp.IsDescending = true;
            bComp.DescendTargetY = y;
            bComp.DescendSpeed = descendSpeed;
            bTrans.Position.Y = std::min(-475.0f, y - 340.0f);
            if (registry.HasComponent<BoxCollider>(b))
            {
                registry.GetComponent<BoxCollider>(b).IsTrigger = true;
            }
        }

        if (type != BrickType::Metal && type != BrickType::Special)
        {
            ++count;
        }
        if (y > m_lowestBrickY)
        {
            m_lowestBrickY = y;
        }
    };

    for (int col = 0; col < kGridCols; ++col)
    {
        for (int row = 0; row < colDepth[col]; ++row)
        {
            m_continuousRowIndex = std::max(m_continuousRowIndex, row);
            BrickType type = BrickType::Light;
            bool isSpecial = false;

            const bool place = EvaluateProceduralCell(col, row, m_elapsedSurvivalTime, m_totalBricksBrokenSession, m_currentWave, type, isSpecial);
            if (!place) continue;

            // Keep column Metal bricks capped at 1 even during initial wave sculpting
            if (type == BrickType::Metal && row > 0)
            {
                type = BrickType::Hard;
            }
            spawnAtCell(col, row, type, isSpecial);
        }
    }

    // Guarantee minimum breakable brick density for Wave start (no sparse/boring starts)
    const int minInitialBricks = std::max(42, GetMinimumBoardBricks() + 12);
    if (count < minInitialBricks)
    {
        for (int row = 0; row < 10 && count < minInitialBricks; ++row)
        {
            for (int col = 0; col < kGridCols && count < minInitialBricks; ++col)
            {
                const float x = kGridStartX + static_cast<float>(col) * kColStep;
                const float y = kGridStartY + static_cast<float>(row) * kRowStep;
                if (y > kMaxAllowedBrickY) continue;

                bool occupied = false;
                registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent& bComp) {
                    const float effY = bComp.IsDescending ? bComp.DescendTargetY : t.Position.Y;
                    if (std::abs(t.Position.X - x) < 40.0f && std::abs(effY - y) < 18.0f)
                    {
                        occupied = true;
                    }
                });
                if (occupied) continue;

                bool isSpecial = false;
                BrickType fillType = ChooseAdaptiveBrickType(registry, col, row + 17, isSpecial);
                if (fillType == BrickType::Metal || fillType == BrickType::Special)
                {
                    fillType = BrickType::Light;
                    isSpecial = false;
                }
                spawnAtCell(col, row, fillType, isSpecial);
            }
        }
    }

    // Ensure we have a valid breakable target matching generated count if lower
    if (count > 0 && count < m_waveTargetBricks)
    {
        m_waveTargetBricks = count;
    }

    // Procedural Hazard Zones scaling with wave difficulty and varied effects
    if (m_currentWave >= 2)
    {
        int hazardCount = 1;
        if (m_currentWave >= 3) hazardCount = 2;
        if (m_currentWave >= 5) hazardCount = 3;

        const HazardEffectType effectCycle[] = {
            HazardEffectType::ScreenGlitch,     // Triple-vision chromatic glitch
            HazardEffectType::SpeedSurge,       // Kinetic acceleration shockwave
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
            float hx = kGridStartX + nextRnd() * static_cast<float>(kGridCols - 1) * kColStep;
            float hy = kGridStartY + static_cast<float>(std::min(maxDepth, 8)) * kRowStep + 30.0f + static_cast<float>(h) * 40.0f;
            hy = std::min(hy, kMaxAllowedBrickY + 35.0f);

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

bool ProceduralEndlessGenerator::CanPushColumn(Registry& registry, int col) const
{
    if (col < 0 || col >= kGridCols) return false;

    const float colX = kGridStartX + static_cast<float>(col) * kColStep;
    struct ColBrick { float EffY; };
    std::vector<ColBrick> colBricks;

    registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent& brick) {
        if (std::abs(t.Position.X - colX) < 90.0f)
        {
            const float effY = brick.IsDescending ? brick.DescendTargetY : t.Position.Y;
            colBricks.push_back(ColBrick{effY});
        }
    });

    if (colBricks.empty()) return true;

    std::sort(colBricks.begin(), colBricks.end(), [](const ColBrick& a, const ColBrick& b) {
        return a.EffY < b.EffY;
    });

    // If the top slot (-350) is free, inserting a brick at top does not push any existing brick
    if (colBricks.front().EffY >= kGridStartY + kRowStep - 12.0f)
    {
        return true;
    }

    // Dry-run contiguous downward cascade in this column
    float prevNewY = kGridStartY;
    for (const auto& cb : colBricks)
    {
        if (cb.EffY < prevNewY + kRowStep - 12.0f)
        {
            const float newY = std::max(cb.EffY + kRowStep, prevNewY + kRowStep);
            if (newY > kMaxAllowedBrickY)
            {
                return false;
            }
            prevNewY = newY;
        }
        else
        {
            // Gap below prevNewY absorbs the push
            break;
        }
    }

    return true;
}

bool ProceduralEndlessGenerator::PushColumnTopBrick(Registry& registry, int col, uint32_t brickTexId, float spawnSpeed)
{
    if (col < 0 || col >= kGridCols) return false;

    const float colX = kGridStartX + static_cast<float>(col) * kColStep;
    struct ColBrick
    {
        Entity Id;
        float  EffY;
        float  CurrentY;
    };
    std::vector<ColBrick> colBricks;

    registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& brick) {
        if (std::abs(t.Position.X - colX) < 90.0f)
        {
            const float effY = brick.IsDescending ? brick.DescendTargetY : t.Position.Y;
            colBricks.push_back(ColBrick{e, effY, t.Position.Y});
        }
    });

    std::sort(colBricks.begin(), colBricks.end(), [](const ColBrick& a, const ColBrick& b) {
        return a.EffY < b.EffY;
    });

    // Determine which bricks in this column are pushed downward by the incoming top brick
    struct PushOp
    {
        Entity Id;
        float  NewY;
    };
    std::vector<PushOp> pushOps;

    if (!colBricks.empty() && colBricks.front().EffY < kGridStartY + kRowStep - 12.0f)
    {
        float prevNewY = kGridStartY;
        for (const auto& cb : colBricks)
        {
            if (cb.EffY < prevNewY + kRowStep - 12.0f)
            {
                const float newY = std::max(cb.EffY + kRowStep, prevNewY + kRowStep);
                if (newY > kMaxAllowedBrickY)
                {
                    return false; // This column is at maximum safe depth
                }
                pushOps.push_back(PushOp{cb.Id, newY});
                prevNewY = newY;
            }
            else
            {
                break;
            }
        }
    }

    const float effectiveSpawnSpeed = (spawnSpeed > 0.0f)
        ? spawnSpeed
        : (320.0f + 220.0f * GetGenerationSpeedRatio());
    const float pushSpeed = std::max(260.0f, effectiveSpawnSpeed * 0.85f);

    // 1. Push existing connected bricks in this column smoothly downward
    for (const auto& op : pushOps)
    {
        if (registry.IsAlive(op.Id) && registry.HasComponent<BrickComponent>(op.Id))
        {
            auto& bComp = registry.GetComponent<BrickComponent>(op.Id);
            bComp.IsDescending = true;
            bComp.DescendTargetY = op.NewY;
            bComp.DescendSpeed = std::max(bComp.DescendSpeed, pushSpeed);
        }
    }

    // 2. Stack spawnStartY cleanly above any brick still descending near the top edge
    float spawnStartY = -475.0f;
    for (const auto& cb : colBricks)
    {
        if (cb.CurrentY <= -430.0f)
        {
            spawnStartY = std::min(spawnStartY, cb.CurrentY - (kBrickHeight + 12.0f));
        }
    }

    // 3. Spawn new brick at the top of this column
    ++m_continuousRowIndex;
    bool isSpecial = false;
    const BrickType type = ChooseAdaptiveBrickType(registry, col, m_continuousRowIndex, isSpecial);
    const uint8_t armor = GetArmorTier();

    Entity newBrick = BrickFactory::Create(registry, colX, kGridStartY, type, isSpecial, brickTexId, armor);
    if (registry.IsAlive(newBrick) && registry.HasComponent<BrickComponent>(newBrick))
    {
        auto& bComp = registry.GetComponent<BrickComponent>(newBrick);
        bComp.IsDescending = true;
        bComp.DescendTargetY = kGridStartY;
        bComp.DescendSpeed = effectiveSpawnSpeed;
    }
    if (registry.IsAlive(newBrick) && registry.HasComponent<Transform2D>(newBrick))
    {
        registry.GetComponent<Transform2D>(newBrick).Position.Y = spawnStartY;
    }
    if (registry.IsAlive(newBrick) && registry.HasComponent<BoxCollider>(newBrick))
    {
        registry.GetComponent<BoxCollider>(newBrick).IsTrigger = true;
    }

    if (type != BrickType::Metal && type != BrickType::Special)
    {
        ++m_breakableRemaining;
    }

    return true;
}

int ProceduralEndlessGenerator::SpawnAdaptiveBurst(Registry& registry, int desiredCount, uint32_t brickTexId)
{
    if (desiredCount <= 0) return 0;

    ++m_burstIndex;
    const int pattern = (m_burstIndex + static_cast<int>(m_seed & 7u)) % 5;
    const float sweepPhase = static_cast<float>(m_burstIndex) * 0.9f;

    int colCount[kGridCols]{};
    bool colCanPush[kGridCols]{};
    int usedInBurst[kGridCols]{};

    for (int c = 0; c < kGridCols; ++c)
    {
        const float colX = kGridStartX + static_cast<float>(c) * kColStep;
        registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent&) {
            if (std::abs(t.Position.X - colX) < 90.0f)
            {
                ++colCount[c];
            }
        });
        colCanPush[c] = CanPushColumn(registry, c);
    }

    int spawned = 0;
    for (int i = 0; i < desiredCount; ++i)
    {
        int bestCol = -1;
        float bestScore = -1e9f;

        for (int c = 0; c < kGridCols; ++c)
        {
            if (!colCanPush[c]) continue;

            const float distFromCenter = std::abs(static_cast<float>(c) - 5.5f);
            float affinity = 0.0f;

            switch (pattern)
            {
            case 0: // Twin flanks / pincer
                affinity = distFromCenter * 0.55f;
                break;
            case 1: // Central spear
                affinity = (5.5f - distFromCenter) * 0.55f;
                break;
            case 2: // Alternating vertical pillars / comb
                affinity = (((c + m_burstIndex) % 2) == 0) ? 2.4f : -0.6f;
                break;
            case 3: // Sweeping diagonal crest
                affinity = std::cos(static_cast<float>(c) * 0.65f - sweepPhase) * 2.0f;
                break;
            case 4: // Anti-camp: heavily prioritize cleared/sparsest columns
            default:
                affinity = 2.0f - static_cast<float>(colCount[c]) * 0.35f;
                break;
            }

            const uint32_t h = (static_cast<uint32_t>(c + 3) * 73856093u)
                             ^ (static_cast<uint32_t>(m_burstIndex + i) * 19349663u)
                             ^ m_seed;
            const float jitter = (static_cast<float>(h % 100) / 100.0f) * 0.9f;

            const float score = affinity
                              - static_cast<float>(colCount[c]) * 0.35f
                              - static_cast<float>(usedInBurst[c]) * 1.65f
                              + jitter;

            if (score > bestScore)
            {
                bestScore = score;
                bestCol = c;
            }
        }

        if (bestCol < 0) break;

        if (PushColumnTopBrick(registry, bestCol, brickTexId))
        {
            ++spawned;
            ++colCount[bestCol];
            ++usedInBurst[bestCol];
            colCanPush[bestCol] = CanPushColumn(registry, bestCol);
        }
        else
        {
            colCanPush[bestCol] = false;
        }
    }

    return spawned;
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
        const float effectiveY = brick.IsDescending ? brick.DescendTargetY : transform.Position.Y;
        if (effectiveY > lowestY)
        {
            lowestY = effectiveY;
        }
        if (effectiveY < highestY)
        {
            highestY = effectiveY;
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

    const float diff = GetDifficultyFactor();
    const int minBricks = GetMinimumBoardBricks();
    const int targetBricks = GetTargetBoardBricks();

    // 2. Column-Aware Periodic Push & Adaptive Non-Line Spawning
    if (m_creepEnabled && m_breakableRemaining > 0)
    {
        m_creepTimer += dt;
        if (m_creepTimer >= GetCreepInterval())
        {
            m_creepTimer = 0.0f;
            int pulseBricks = 3 + static_cast<int>(std::round(4.0f * diff)); // 3 at start -> 7 at high difficulty
            if (m_breakableRemaining < targetBricks)
            {
                pulseBricks += 2;
            }
            else if (m_breakableRemaining > targetBricks + 16)
            {
                pulseBricks = 2;
            }
            SpawnAdaptiveBurst(registry, pulseBricks, m_cachedBrickTexId);
        }
    }

    // 3. Guaranteed Minimum Density & Anti-Boredom Replenishment
    if (m_breakableRemaining == 0 && !IsWaveTimeElapsed())
    {
        const int starterBurst = std::max(minBricks, 32 + static_cast<int>(std::round(16.0f * diff)));
        SpawnAdaptiveBurst(registry, starterBurst, m_cachedBrickTexId);
        m_replenishCooldown = GetReplenishInterval();
    }
    else if (m_replenishCooldown <= 0.0f && !IsWaveTimeElapsed())
    {
        if (m_breakableRemaining < minBricks)
        {
            const int deficit = std::max(4, minBricks - m_breakableRemaining);
            const int burstCount = std::clamp(deficit, 4, 8 + static_cast<int>(std::round(6.0f * diff)));
            SpawnAdaptiveBurst(registry, burstCount, m_cachedBrickTexId);
            m_replenishCooldown = GetReplenishInterval();
        }
        else if (m_breakableRemaining < targetBricks - 12)
        {
            const int adaptiveCount = 2 + static_cast<int>(std::round(4.0f * diff));
            SpawnAdaptiveBurst(registry, adaptiveCount, m_cachedBrickTexId);
            m_replenishCooldown = GetReplenishInterval();
        }
    }
}

void ProceduralEndlessGenerator::StepDownward(Registry& registry)
{
    const float stepAmount = kRowStep;
    const float pushSpeed = std::max(260.0f, (320.0f + 220.0f * GetGenerationSpeedRatio()) * 0.85f);
    std::vector<Entity> stepped;

    for (int col = 0; col < kGridCols; ++col)
    {
        const float colX = kGridStartX + static_cast<float>(col) * kColStep;
        bool colCanStep = true;
        std::vector<std::pair<Entity, float>> colEntities;

        registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& transform, const BrickComponent& brick) {
            if (std::abs(transform.Position.X - colX) < 90.0f)
            {
                const float effY = brick.IsDescending ? brick.DescendTargetY : transform.Position.Y;
                if (effY + stepAmount > kMaxAllowedBrickY)
                {
                    colCanStep = false;
                }
                colEntities.emplace_back(e, effY);
            }
        });

        if (!colCanStep) continue;

        for (const auto& [e, effY] : colEntities)
        {
            if (std::find(stepped.begin(), stepped.end(), e) != stepped.end()) continue;
            stepped.push_back(e);

            if (registry.HasComponent<BrickComponent>(e))
            {
                auto& bComp = registry.GetComponent<BrickComponent>(e);
                bComp.IsDescending = true;
                bComp.DescendTargetY = effY + stepAmount;
                bComp.DescendSpeed = std::max(bComp.DescendSpeed, pushSpeed);
            }
        }
    }
}

int ProceduralEndlessGenerator::SpawnRowAtY(Registry& registry, float targetY, uint32_t brickTexId)
{
    const float bWidth = kBrickWidth;
    const float startX = kGridStartX;
    const float pad = kGridPad;
    const int cols = kGridCols;

    ++m_continuousRowIndex;
    uint8_t armor = GetArmorTier();
    int spawnedCount = 0;
    const float descendSpeed = 320.0f + 220.0f * GetGenerationSpeedRatio();

    auto startDescent = [&](Entity e) {
        if (registry.HasComponent<BrickComponent>(e))
        {
            auto& brickComp = registry.GetComponent<BrickComponent>(e);
            brickComp.IsDescending = true;
            brickComp.DescendTargetY = targetY;
            brickComp.DescendSpeed = descendSpeed;
        }
        if (registry.HasComponent<Transform2D>(e))
        {
            registry.GetComponent<Transform2D>(e).Position.Y = std::min(-475.0f, targetY - 170.0f);
        }
        if (registry.HasComponent<BoxCollider>(e))
        {
            registry.GetComponent<BoxCollider>(e).IsTrigger = true;
        }
    };

    for (int col = 0; col < cols; ++col)
    {
        float x = startX + static_cast<float>(col) * (bWidth + pad);

        // Check if a brick already exists near (x, targetY)
        bool slotOccupied = false;
        registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent& brick) {
            const float effY = brick.IsDescending ? brick.DescendTargetY : t.Position.Y;
            if (std::abs(t.Position.X - x) < 40.0f && std::abs(effY - targetY) < 18.0f)
            {
                slotOccupied = true;
            }
        });

        if (slotOccupied) continue;

        BrickType type = BrickType::Light;
        bool isSpecial = false;

        bool place = EvaluateProceduralCell(col, m_continuousRowIndex, m_elapsedSurvivalTime, m_totalBricksBrokenSession, m_currentWave, type, isSpecial);
        if (!place) continue;

        Entity e = BrickFactory::Create(registry, x, targetY, type, isSpecial, brickTexId, armor);
        startDescent(e);
        ++spawnedCount;

        if (type != BrickType::Metal && type != BrickType::Special)
        {
            ++m_breakableRemaining;
        }
    }

    // Safety fallback: ensure a newly spawned row contributes at least 6 breakable bricks
    if (spawnedCount < 6)
    {
        for (int col = 1; col < 11 && spawnedCount < 6; ++col)
        {
            float x = startX + static_cast<float>(col) * (bWidth + pad);
            bool slotOccupied = false;
            registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, const BrickComponent& brick) {
                const float effY = brick.IsDescending ? brick.DescendTargetY : t.Position.Y;
                if (std::abs(t.Position.X - x) < 40.0f && std::abs(effY - targetY) < 18.0f)
                {
                    slotOccupied = true;
                }
            });
            if (!slotOccupied)
            {
                Entity e = BrickFactory::Create(registry, x, targetY, BrickType::Light, false, brickTexId, armor);
                startDescent(e);
                ++spawnedCount;
                ++m_breakableRemaining;
            }
        }
    }

    return spawnedCount;
}

void ProceduralEndlessGenerator::SpawnTopRow(Registry& registry, uint32_t brickTexId)
{
    SpawnRowAtY(registry, kGridStartY, brickTexId);
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
