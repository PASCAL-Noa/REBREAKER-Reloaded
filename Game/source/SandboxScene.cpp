#include "Scenes/SandboxScene.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "Core/SceneManager.h"
#include "AudioMixer.h"
#include "Factories/BrickFactory.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"
#include "Graphics/Renderer.h"
#include "UI/UIFactory.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "ECS/Systems/BallSystem.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include <algorithm>

namespace
{
    constexpr PowerUpType kAllPowerUps[] = {
        PowerUpType::MultiBall,
        PowerUpType::ExpandPaddle,
        PowerUpType::ShrinkPaddle,
        PowerUpType::Storm,
        PowerUpType::TempoBall,
        PowerUpType::ExtraLife,
        PowerUpType::BigBall,
        PowerUpType::FireBall,
        PowerUpType::StickyPaddle,
        PowerUpType::BottomBarrier,
        PowerUpType::MagnetPaddle,
        PowerUpType::GhostBall,
        PowerUpType::GlitchMalus
    };
    constexpr int kPowerUpCount = sizeof(kAllPowerUps) / sizeof(kAllPowerUps[0]);

    constexpr UltimateAbilityType kAllUltimates[] = {
        UltimateAbilityType::PlasmaOrb,
        UltimateAbilityType::DiscoBall,
        UltimateAbilityType::PowerUpRain
    };
    constexpr int kUltimateCount = sizeof(kAllUltimates) / sizeof(kAllUltimates[0]);

    constexpr PlaygroundLayout kAllLayouts[] = {
        PlaygroundLayout::TacticalMix,
        PlaygroundLayout::MetalChains,
        PlaygroundLayout::BombClusters,
        PlaygroundLayout::RegeneratingItemLab
    };
    constexpr int kLayoutCount = sizeof(kAllLayouts) / sizeof(kAllLayouts[0]);

    constexpr HazardEffectType kAllMaluses[] = {
        HazardEffectType::AllMaluses,
        HazardEffectType::ScreenGlitch,
        HazardEffectType::CameraTilt,
        HazardEffectType::CameraZoomPulse,
        HazardEffectType::TemporalAudioWarp
    };
    constexpr int kMalusCount = sizeof(kAllMaluses) / sizeof(kAllMaluses[0]);

    constexpr LightingMode kAllLightingModes[] = {
        LightingMode::Lit,
        LightingMode::Atmospheric,
        LightingMode::PitchBlack
    };
    constexpr int kLightingModeCount = sizeof(kAllLightingModes) / sizeof(kAllLightingModes[0]);

    const char* GetPowerUpDisplayName(PowerUpType type)
    {
        switch (type)
        {
            case PowerUpType::MultiBall:     return "MULTI-BALL";
            case PowerUpType::ExpandPaddle:  return "EXPAND PADDLE";
            case PowerUpType::ShrinkPaddle:  return "SHRINK PADDLE";
            case PowerUpType::Storm:         return "STORM THUNDER";
            case PowerUpType::TempoBall:     return "TEMPO BALL";
            case PowerUpType::ExtraLife:     return "EXTRA LIFE";
            case PowerUpType::BigBall:       return "BIG BALL";
            case PowerUpType::FireBall:      return "FIRE BALL";
            case PowerUpType::StickyPaddle:  return "STICKY PADDLE";
            case PowerUpType::BottomBarrier: return "LASER BARRIER";
            case PowerUpType::MagnetPaddle:  return "MAGNET PADDLE";
            case PowerUpType::GhostBall:     return "GHOST BALL";
            case PowerUpType::GlitchMalus:   return "GLITCH MALUS";
            default:                         return "POWER-UP";
        }
    }

    Color GetPowerUpDisplayColor(PowerUpType type)
    {
        switch (type)
        {
            case PowerUpType::MultiBall:     return Color{0, 140, 195, 255};
            case PowerUpType::ExpandPaddle:  return Color{35, 145, 65, 255};
            case PowerUpType::ShrinkPaddle:  return Color{175, 45, 45, 255};
            case PowerUpType::Storm:         return Color{195, 155, 25, 255};
            case PowerUpType::TempoBall:     return Color{130, 50, 190, 255};
            case PowerUpType::ExtraLife:     return Color{185, 55, 130, 255};
            case PowerUpType::BigBall:       return Color{205, 90, 20, 255};
            case PowerUpType::FireBall:      return Color{215, 50, 15, 255};
            case PowerUpType::StickyPaddle:  return Color{185, 130, 25, 255};
            case PowerUpType::BottomBarrier: return Color{0, 165, 210, 255};
            case PowerUpType::MagnetPaddle:  return Color{165, 45, 195, 255};
            case PowerUpType::GhostBall:     return Color{75, 155, 195, 255};
            case PowerUpType::GlitchMalus:   return Color{255, 40, 100, 255};
            default:                         return Color{50, 60, 80, 255};
        }
    }

    const char* GetUltimateDisplayName(UltimateAbilityType type)
    {
        switch (type)
        {
            case UltimateAbilityType::PlasmaOrb:   return "PLASMA ORB";
            case UltimateAbilityType::DiscoBall:   return "DISCO BALL";
            case UltimateAbilityType::PowerUpRain: return "POWER-UP RAIN";
            default:                               return "ULTIMATE";
        }
    }

    const char* GetLightingModeName(LightingMode mode)
    {
        switch (mode)
        {
            case LightingMode::Lit:         return "LIGHT: LIT";
            case LightingMode::Atmospheric: return "LIGHT: DIM";
            case LightingMode::PitchBlack:  return "LIGHT: DARK";
            default:                        return "LIGHT";
        }
    }

    const char* GetMalusDisplayName(HazardEffectType type)
    {
        switch (type)
        {
            case HazardEffectType::AllMaluses:        return "MALUS: ALL COMBINED";
            case HazardEffectType::ScreenGlitch:      return "MALUS: SCREEN GLITCH";
            case HazardEffectType::CameraTilt:        return "MALUS: CAMERA TILT";
            case HazardEffectType::CameraZoomPulse:   return "MALUS: ZOOM PULSE";
            case HazardEffectType::TemporalAudioWarp: return "MALUS: AUDIO WARP";
            default:                                  return "MALUS";
        }
    }

    Color GetMalusDisplayColor(HazardEffectType type)
    {
        switch (type)
        {
            case HazardEffectType::AllMaluses:        return Color{220, 35, 75, 255};
            case HazardEffectType::ScreenGlitch:      return Color{195, 30, 115, 255};
            case HazardEffectType::CameraTilt:        return Color{165, 45, 155, 255};
            case HazardEffectType::CameraZoomPulse:   return Color{135, 45, 185, 255};
            case HazardEffectType::TemporalAudioWarp: return Color{95, 55, 185, 255};
            default:                                  return Color{200, 40, 80, 255};
        }
    }
}

SandboxScene::SandboxScene()
{
    m_lives = 99;
    m_musicEnabled = true;
    m_audioEnabled = true;
}

void SandboxScene::OnInit(GameContext& context)
{
    GameScene::OnInit(context);

    m_lives = 99;
    m_safetyBarrierEnabled = true;

    // Apply baseline theme and lighting
    SetLightingMode(LightingMode::Lit);

    // Initial equipped ultimate sync
    UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();
    for (int i = 0; i < kUltimateCount; ++i)
    {
        if (kAllUltimates[i] == equipped)
        {
            m_selectedUltimateIdx = i;
            break;
        }
    }

    // Load initial showcase layout
    LoadPlaygroundLayout(PlaygroundLayout::TacticalMix);

    // Build Streamer Deck UI
    CreateStreamDeckUI(context);
    UpdateSandboxUIDisplay();
}

void SandboxScene::OnUpdate(float dt, GameContext& context)
{
    GameScene::OnUpdate(dt, context);
    HandleSandboxInput(context);

    // Keep invincible laser floor active when enabled
    if (m_safetyBarrierEnabled)
    {
        m_bottomBarrierActive = true;
        m_bottomBarrierDuration = 999999.0f;
        m_bottomBarrierCharges = 999999;
    }

    // Dynamic arena events real-time UI timer update
    if (mp_eventSystem && (mp_eventSystem->IsEventActive() || mp_eventSystem->IsWarningActive()))
    {
        UpdateSandboxUIDisplay();
    }

    // Auto-respawn safety: never lock the player if all balls are lost
    if (mp_ballSystem && mp_ballSystem->GetBalls().empty())
    {
        RespawnBall();
    }
}

void SandboxScene::OnRender(GameContext& context)
{
    GameScene::OnRender(context);
}

void SandboxScene::OnDestroy(GameContext& context)
{
    GameScene::OnDestroy(context);
}

void SandboxScene::OnGameOverEnter()
{
    // Prevent Game Over in Sandbox: smooth auto-recovery
    RespawnBall();
    m_lives = 99;
}

void SandboxScene::OnVictoryEnter()
{
    // Auto-refill bricks on stage clear without exiting
    RespawnCurrentLayout();
}

void SandboxScene::SetLightingMode(LightingMode mode)
{
    m_lightingMode = mode;
    switch (m_lightingMode)
    {
        case LightingMode::Lit:
            m_currentTheme.AmbientLight = Color{255, 255, 255, 255};
            break;
        case LightingMode::Atmospheric:
            m_currentTheme.AmbientLight = Color{45, 50, 75, 255};
            break;
        case LightingMode::PitchBlack:
            m_currentTheme.AmbientLight = Color{0, 0, 0, 255};
            break;
    }
    UpdateSandboxUIDisplay();
}

void SandboxScene::NextLightingMode()
{
    int nextMode = (static_cast<int>(m_lightingMode) + 1) % kLightingModeCount;
    SetLightingMode(static_cast<LightingMode>(nextMode));
}

void SandboxScene::PreviousLightingMode()
{
    int prevMode = (static_cast<int>(m_lightingMode) - 1 + kLightingModeCount) % kLightingModeCount;
    SetLightingMode(static_cast<LightingMode>(prevMode));
}

void SandboxScene::ToggleLightingMode()
{
    NextLightingMode();
}

void SandboxScene::SetSafetyBarrier(bool enabled)
{
    m_safetyBarrierEnabled = enabled;
    if (m_safetyBarrierEnabled)
    {
        m_bottomBarrierActive = true;
        m_bottomBarrierDuration = 999999.0f;
        m_bottomBarrierCharges = 999999;
    }
    else
    {
        m_bottomBarrierActive = false;
        m_bottomBarrierDuration = 0.0f;
        m_bottomBarrierCharges = 0;
    }
    UpdateSandboxUIDisplay();
}

void SandboxScene::ToggleSafetyBarrier()
{
    SetSafetyBarrier(!m_safetyBarrierEnabled);
}

void SandboxScene::RespawnBall()
{
    ResetBallAndPaddle(true);
}

const char* SandboxScene::GetCurrentLayoutName() const noexcept
{
    switch (m_currentLayout)
    {
        case PlaygroundLayout::TacticalMix:         return "TACTICAL MIX";
        case PlaygroundLayout::MetalChains:         return "METAL CHAINS";
        case PlaygroundLayout::BombClusters:        return "BOMB CLUSTERS";
        case PlaygroundLayout::RegeneratingItemLab: return "REGEN ITEM LAB";
        default:                                    return "CUSTOM";
    }
}

void SandboxScene::ClearAllBricks()
{
    m_preDiscoBrickTints.clear();
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.View<HazardZoneComponent>([this](const Entity e, HazardZoneComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
    m_brickCount = 0;
}

void SandboxScene::LoadPlaygroundLayout(PlaygroundLayout layout)
{
    m_currentLayout = layout;
    ClearAllBricks();

    constexpr float startX = -550.0f;
    constexpr float startY = -330.0f;
    constexpr float stepX = 100.0f;
    constexpr float stepY = 38.0f;
    constexpr int cols = 12;

    auto spawnAt = [this, startX, startY, stepX, stepY](int col, int row, BrickType type, bool isSpecial) {
        float x = startX + col * stepX;
        float y = startY + row * stepY;
        BrickFactory::Create(m_registry, x, y, type, isSpecial, m_brickTexId);
        m_brickCount++;
    };

    switch (layout)
    {
        case PlaygroundLayout::MetalChains:
        {
            for (int col = 0; col < cols; ++col)
            {
                if (col % 3 == 2)
                    spawnAt(col, 0, BrickType::Lantern, false);
                else
                    spawnAt(col, 0, BrickType::Metal, false);

                if (col % 3 == 0)
                    spawnAt(col, 1, BrickType::Metal, false);
                else if (col % 2 == 1)
                    spawnAt(col, 1, BrickType::Medium, false);

                if (col % 3 == 1)
                    spawnAt(col, 2, BrickType::Lantern, false);
                else
                    spawnAt(col, 2, BrickType::Metal, false);

                spawnAt(col, 3, (col % 2 == 0) ? BrickType::Light : BrickType::Hard, false);
            }
            break;
        }

        case PlaygroundLayout::BombClusters:
        {
            for (int col = 0; col < cols; ++col)
            {
                for (int row = 0; row < 3; ++row)
                {
                    bool isClusterA = (col >= 1 && col <= 3);
                    bool isClusterB = (col >= 5 && col <= 6);
                    bool isClusterC = (col >= 8 && col <= 10);

                    if (isClusterA || isClusterB || isClusterC)
                    {
                        if (row == 1 && (col == 2 || col == 5 || col == 9))
                            spawnAt(col, row, BrickType::Lantern, false);
                        else
                            spawnAt(col, row, BrickType::Bomb, false);
                    }
                    else
                    {
                        spawnAt(col, row, (row == 0) ? BrickType::Medium : BrickType::Hard, false);
                    }
                }
            }
            break;
        }

        case PlaygroundLayout::RegeneratingItemLab:
        {
            for (int col = 0; col < cols; ++col)
            {
                spawnAt(col, 0, BrickType::Special, true);
                if (col % 4 == 0)
                    spawnAt(col, 1, BrickType::Lantern, false);
                else
                    spawnAt(col, 1, BrickType::Special, true);

                spawnAt(col, 2, (col % 2 == 0) ? BrickType::Special : BrickType::Metal, (col % 2 == 0));
            }
            break;
        }

        case PlaygroundLayout::TacticalMix:
        default:
        {
            for (int col = 0; col < cols; ++col)
            {
                if (col == 0 || col == 11)
                    spawnAt(col, 0, BrickType::Lantern, false);
                else if (col == 1 || col == 10)
                    spawnAt(col, 0, BrickType::Bomb, false);
                else if (col >= 3 && col <= 8)
                    spawnAt(col, 0, BrickType::Metal, false);
                else
                    spawnAt(col, 0, BrickType::Hard, false);

                if (col >= 4 && col <= 7)
                    spawnAt(col, 1, BrickType::Special, true);
                else if (col == 2 || col == 9)
                    spawnAt(col, 1, BrickType::Metal, false);
                else
                    spawnAt(col, 1, BrickType::Medium, false);

                if (col % 3 == 1)
                    spawnAt(col, 2, BrickType::Bomb, false);
                else if (col % 3 == 2)
                    spawnAt(col, 2, BrickType::Lantern, false);
                else
                    spawnAt(col, 2, BrickType::Light, false);
            }
            break;
        }
    }

    UpdateSandboxUIDisplay();
}

void SandboxScene::NextPlaygroundLayout()
{
    int next = (static_cast<int>(m_currentLayout) + 1) % kLayoutCount;
    LoadPlaygroundLayout(static_cast<PlaygroundLayout>(next));
}

void SandboxScene::PreviousPlaygroundLayout()
{
    int prev = (static_cast<int>(m_currentLayout) - 1 + kLayoutCount) % kLayoutCount;
    LoadPlaygroundLayout(static_cast<PlaygroundLayout>(prev));
}

void SandboxScene::RespawnCurrentLayout()
{
    LoadPlaygroundLayout(m_currentLayout);
}

void SandboxScene::NextPowerUp()
{
    m_selectedPowerUpIdx = (m_selectedPowerUpIdx + 1) % kPowerUpCount;
    UpdateSandboxUIDisplay();
}

void SandboxScene::PreviousPowerUp()
{
    m_selectedPowerUpIdx = (m_selectedPowerUpIdx - 1 + kPowerUpCount) % kPowerUpCount;
    UpdateSandboxUIDisplay();
}

void SandboxScene::TriggerSelectedPowerUp()
{
    PowerUpType type = kAllPowerUps[m_selectedPowerUpIdx];
    if (m_spawnCapsuleMode)
    {
        Vector2f spawnPos{0.0f, -320.0f};
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
        {
            spawnPos.X = m_registry.GetComponent<Transform2D>(m_paddle).Position.X;
        }
        SpawnPowerUpCapsule(type, spawnPos);
    }
    else
    {
        ApplyPowerUp(type);
    }
}

void SandboxScene::SpawnPowerUpCapsule(PowerUpType type, const Vector2f& position)
{
    Entity capsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(capsule, Transform2D{position, 0.0f, Vector2f{1.0f, 1.0f}});
    m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{36.0f, 36.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 220.0f, false});
    uint32_t orbTex = GetPowerUpOrbTexture(type);
    Color color = (orbTex != m_brickTexId) ? Colors::White : GetPowerUpColor(type);
    m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{orbTex, color});
}

void SandboxScene::ClearAllPowerUpEffects()
{
    m_stormDuration = 0.0f;
    m_stormCharges = 0;
    m_activeLightningBolts.clear();
    m_paddleSizeDuration = 0.0f;
    m_isShrinkPaddle = false;
    m_tempoBallDuration = 0.0f;
    m_tempoBallStacks = 0;
    m_tempoAudioDistortionDuration = 0.0f;
    RestoreTemporalAudio(0.05f);
    m_bigBallDuration = 0.0f;
    m_stickyPaddleDuration = 0.0f;
    m_magnetDuration = 0.0f;
    m_ghostBallDuration = 0.0f;
    m_ghostBallPendingExpiry = false;

    if (m_registry.IsAlive(m_paddle))
    {
        if (m_registry.HasComponent<PaddleComponent>(m_paddle))
        {
            auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
            paddleComp.HasSticky = false;
            paddleComp.HasMagnet = false;
        }
        if (m_registry.HasComponent<BoxCollider>(m_paddle))
        {
            m_registry.GetComponent<BoxCollider>(m_paddle).Size.X = 120.0f;
        }
        if (m_registry.HasComponent<SpriteComponent>(m_paddle))
        {
            m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
        }
    }

    if (mp_ballSystem)
    {
        mp_ballSystem->ClearLasers();
        for (Entity b : mp_ballSystem->GetBalls())
        {
            if (m_registry.IsAlive(b))
            {
                if (m_registry.HasComponent<BallComponent>(b))
                {
                    auto& ball = m_registry.GetComponent<BallComponent>(b);
                    ball.IsFireBall = false;
                    ball.IsGhost = false;
                    ball.IsBig = false;
                    ball.IsStuck = false;
                    ball.StuckOffsetX = 0.0f;
                }
                if (m_registry.HasComponent<CircleCollider>(b))
                {
                    m_registry.GetComponent<CircleCollider>(b).Radius = 10.0f;
                }
                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Colors::White;
                }
            }
        }
    }
}

void SandboxScene::MaximizeAllPowerUpLevels()
{
    for (PowerUpType t : kAllPowerUps)
    {
        PowerUpManager::Get().SetLevel(t, 5);
    }
}

void SandboxScene::NextUltimate()
{
    m_selectedUltimateIdx = (m_selectedUltimateIdx + 1) % kUltimateCount;
    ProfileManager::Get().SetEquippedUltimate(kAllUltimates[m_selectedUltimateIdx]);
    UpdateSandboxUIDisplay();
}

void SandboxScene::PreviousUltimate()
{
    m_selectedUltimateIdx = (m_selectedUltimateIdx - 1 + kUltimateCount) % kUltimateCount;
    ProfileManager::Get().SetEquippedUltimate(kAllUltimates[m_selectedUltimateIdx]);
    UpdateSandboxUIDisplay();
}

void SandboxScene::FireSelectedUltimate()
{
    ProfileManager::Get().SetEquippedUltimate(kAllUltimates[m_selectedUltimateIdx]);
    m_ultimateCharge = m_ultimateMaxCharge;
    m_ultimateReady = true;
    TryTriggerUltimate();
}

void SandboxScene::ClearActiveUltimatesDebug()
{
    ClearActiveUltimates();
    m_ultimateCharge = 0.0f;
    m_ultimateReady = false;
    m_ultimateReadyPulseTime = 0.0f;
}

void SandboxScene::NextMalus()
{
    m_selectedMalusIdx = (m_selectedMalusIdx + 1) % kMalusCount;
    UpdateSandboxUIDisplay();
}

void SandboxScene::PreviousMalus()
{
    m_selectedMalusIdx = (m_selectedMalusIdx - 1 + kMalusCount) % kMalusCount;
    UpdateSandboxUIDisplay();
}

void SandboxScene::TriggerSelectedMalus()
{
    HazardEffectType effect = kAllMaluses[m_selectedMalusIdx];
    switch (effect)
    {
        case HazardEffectType::ScreenGlitch:
            TriggerScreenGlitch(1.2f, 0.85f);
            break;
        case HazardEffectType::CameraTilt:
            TriggerCameraTilt(1.2f, 2.5f, 2.2f);
            break;
        case HazardEffectType::CameraZoomPulse:
            TriggerCameraZoomPulse(1.2f, 2.5f, 0.05f);
            break;
        case HazardEffectType::TemporalAudioWarp:
            TriggerTemporalAudioDistortion(0.88f, 0.15f, 1.2f);
            break;
        case HazardEffectType::AllMaluses:
        default:
            TriggerVisualMalus(1.2f, true, true, true);
            TriggerTemporalAudioDistortion(0.88f, 0.15f, 1.2f);
            break;
    }
}

void SandboxScene::SpawnSelectedHazardZone()
{
    HazardEffectType effect = kAllMaluses[m_selectedMalusIdx];
    SpawnHazardZone(Vector2f{0.0f, -100.0f}, Vector2f{240.0f, 70.0f}, effect, 1.4f);
}

void SandboxScene::ClearActiveMaluses()
{
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);

    // Destroy all active hazard zones in the sandbox
    m_registry.View<HazardZoneComponent>([this](const Entity e, HazardZoneComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });

    // Destroy any falling debuff capsules in the sandbox
    m_registry.View<PowerUpComponent>([this](const Entity e, const PowerUpComponent& p) {
        if (p.IsDebuff) m_registry.DestroyEntityDeferred(e);
    });

    m_registry.ProcessDeferredCommands();
}

static constexpr std::array<ArenaEventType, 7> kAllArenaEvents = {
    ArenaEventType::TemporalFlux,
    ArenaEventType::GravityShiftLeft,
    ArenaEventType::GravityShiftRight,
    ArenaEventType::GravityShiftDown,
    ArenaEventType::CameraEarthquake,
    ArenaEventType::SolarFlareBlackout,
    ArenaEventType::MeteorShower
};

void SandboxScene::NextArenaEvent()
{
    int idx = 0;
    for (size_t i = 0; i < kAllArenaEvents.size(); ++i)
    {
        if (kAllArenaEvents[i] == m_selectedArenaEventType) { idx = static_cast<int>(i); break; }
    }
    m_selectedArenaEventType = kAllArenaEvents[(idx + 1) % kAllArenaEvents.size()];
    UpdateSandboxUIDisplay();
}

void SandboxScene::PreviousArenaEvent()
{
    int idx = 0;
    for (size_t i = 0; i < kAllArenaEvents.size(); ++i)
    {
        if (kAllArenaEvents[i] == m_selectedArenaEventType) { idx = static_cast<int>(i); break; }
    }
    m_selectedArenaEventType = kAllArenaEvents[(idx - 1 + static_cast<int>(kAllArenaEvents.size())) % kAllArenaEvents.size()];
    UpdateSandboxUIDisplay();
}

void SandboxScene::TriggerSelectedArenaEvent()
{
    if (mp_eventSystem)
    {
        mp_eventSystem->TriggerEvent(m_selectedArenaEventType, false);
    }
    UpdateSandboxUIDisplay();
}

void SandboxScene::ClearActiveArenaEvent()
{
    if (mp_eventSystem)
    {
        mp_eventSystem->StopActiveEvent();
    }
    UpdateSandboxUIDisplay();
}

void SandboxScene::ToggleAutoArenaEvents()
{
    if (mp_eventSystem)
    {
        mp_eventSystem->SetEnabled(!mp_eventSystem->IsEnabled());
    }
    UpdateSandboxUIDisplay();
}

const char* SandboxScene::GetSelectedArenaEventName() const noexcept
{
    return GetArenaEventName(m_selectedArenaEventType);
}

void SandboxScene::TogglePalette()
{
    m_paletteVisible = !m_paletteVisible;
    if (m_sandboxCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_sandboxCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_sandboxCanvas).IsEnabled = m_paletteVisible;
    }
    if (m_miniToggleCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_miniToggleCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_miniToggleCanvas).IsEnabled = !m_paletteVisible;
    }
}

void SandboxScene::HandleSandboxInput(const GameContext& context)
{
    // [TAB]: Toggle Streamer Deck console
    if (context.Input.IsKeyPress(KeyCode::Tab))
    {
        TogglePalette();
    }

    // [ESC]: Back to Main Menu
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);
        context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.45f);
        return;
    }

    // Direct keyboard hotkeys
    if (context.Input.IsKeyPress(KeyCode::L)) ToggleLightingMode();
    if (context.Input.IsKeyPress(KeyCode::K)) ToggleSafetyBarrier();
    if (context.Input.IsKeyPress(KeyCode::R)) RespawnBall();
    if (context.Input.IsKeyPress(KeyCode::J)) NextPlaygroundLayout();
    if (context.Input.IsKeyPress(KeyCode::B)) RespawnCurrentLayout();
    if (context.Input.IsKeyPress(KeyCode::C)) ClearAllBricks();
    if (context.Input.IsKeyPress(KeyCode::U)) FireSelectedUltimate();
    if (context.Input.IsKeyPress(KeyCode::G)) TriggerSelectedMalus();
    if (context.Input.IsKeyPress(KeyCode::H)) SpawnSelectedHazardZone();
    if (context.Input.IsKeyPress(KeyCode::M)) NextMalus();
    if (context.Input.IsKeyPress(KeyCode::N)) PreviousMalus();
    if (context.Input.IsKeyPress(KeyCode::X)) ClearActiveMaluses();

    // Arena Events shortcuts
    if (context.Input.IsKeyPress(KeyCode::E)) TriggerSelectedArenaEvent();
    if (context.Input.IsKeyPress(KeyCode::T)) NextArenaEvent();
    if (context.Input.IsKeyPress(KeyCode::Y)) ToggleAutoArenaEvents();

    // Arrows to cycle power-ups / layouts
    if (context.Input.IsKeyPress(KeyCode::Left))  PreviousPowerUp();
    if (context.Input.IsKeyPress(KeyCode::Right)) NextPowerUp();
    if (context.Input.IsKeyPress(KeyCode::Enter)) TriggerSelectedPowerUp();
}

void SandboxScene::CreateStreamDeckUI(const GameContext& context)
{
    // 1. Mini Bottom-Right Toggle Button (shown by default in bottom-right corner)
    m_miniToggleCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_miniToggleCanvas, CanvasComponent{.IsEnabled = !m_paletteVisible});

    UIFactory::CreateButton(m_registry, m_miniToggleCanvas, ButtonDescriptor{
        .Text = "DEBUG",
        .OnClick = [this]() { TogglePalette(); },
        .Position = {-75.0f, -32.0f},
        .Size = {120.0f, 38.0f},
        .DefaultColor = Color{25, 40, 65, 230},
        .HoverColor = Color{40, 70, 115, 255},
        .PressedColor = Color{15, 25, 45, 255},
        .TextColor = Colors::Gold,
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .AnchorPoint = Anchor::BottomRight,
        .IgnoreMenuNavigation = true
    });

    // 2. Main Stream Deck Controller Console
    m_sandboxCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_sandboxCanvas, CanvasComponent{.IsEnabled = m_paletteVisible});

    constexpr float deckW = 860.0f;
    constexpr float deckH = 550.0f;
    constexpr float deckY = 5.0f;

    // Dark sleek console body
    UIFactory::CreatePanel(m_registry, m_sandboxCanvas, PanelDescriptor{
        .Position = {0.0f, deckY},
        .Size = {deckW, deckH},
        .Tint = Color{15, 19, 29, 245},
        .AnchorPoint = Anchor::Center,
        .ZOrder = 10
    });

    // Console Top Bar / Bevel
    UIFactory::CreatePanel(m_registry, m_sandboxCanvas, PanelDescriptor{
        .Position = {0.0f, deckY - deckH * 0.5f + 21.0f},
        .Size = {deckW, 42.0f},
        .Tint = Color{24, 30, 48, 255},
        .AnchorPoint = Anchor::Center,
        .ZOrder = 11
    });

    // Console Header Title
    UIFactory::CreateText(m_registry, m_sandboxCanvas, TextDescriptor{
        .Text = "STREAM DECK // DEBUG & TESTING LAB",
        .Position = {-170.0f, deckY - deckH * 0.5f + 21.0f},
        .FontId = m_fontId,
        .FontSize = 21.0f,
        .Tint = Colors::Gold,
        .AnchorPoint = Anchor::Center,
        .ZOrder = 12
    });

    // Top Right Close Button
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CLOSE",
        .OnClick = [this]() { TogglePalette(); },
        .Position = {375.0f, deckY - deckH * 0.5f + 21.0f},
        .Size = {80.0f, 30.0f},
        .DefaultColor = Color{85, 30, 35, 255},
        .HoverColor = Color{125, 45, 50, 255},
        .PressedColor = Color{50, 20, 25, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    constexpr float padH = 38.0f;

    // ==========================================
    // CHANNEL 1: POWER-UP SPINNER & TRIGGER
    // ==========================================
    constexpr float row1Y = deckY - 180.0f;

    // Previous Power-Up Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousPowerUp(); },
        .Position = {-375.0f, row1Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Power-Up LCD Display & Click-Trigger
    m_btnPowerUpLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MULTI-BALL (LV.1)",
        .OnClick = [this]() { TriggerSelectedPowerUp(); },
        .Position = {-185.0f, row1Y},
        .Size = {310.0f, padH},
        .DefaultColor = Color{0, 140, 195, 255},
        .HoverColor = Color{0, 180, 245, 255},
        .PressedColor = Color{0, 95, 135, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Power-Up Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextPowerUp(); },
        .Position = {5.0f, row1Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Trigger Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "TRIGGER",
        .OnClick = [this]() { TriggerSelectedPowerUp(); },
        .Position = {90.0f, row1Y},
        .Size = {100.0f, padH},
        .DefaultColor = Color{30, 125, 75, 255},
        .HoverColor = Color{45, 175, 105, 255},
        .PressedColor = Color{20, 80, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Mode Toggle Pad (Instant Apply vs Drop Capsule)
    m_btnSpawnMode = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MODE: INSTANT",
        .OnClick = [this]() {
            ToggleSpawnCapsuleMode();
            UpdateSandboxUIDisplay();
        },
        .Position = {205.0f, row1Y},
        .Size = {110.0f, padH},
        .DefaultColor = Color{35, 120, 75, 255},
        .HoverColor = Color{50, 160, 100, 255},
        .PressedColor = Color{20, 70, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Max Power-Up Levels
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MAX",
        .OnClick = [this]() {
            MaximizeAllPowerUpLevels();
            UpdateSandboxUIDisplay();
        },
        .Position = {295.0f, row1Y},
        .Size = {58.0f, padH},
        .DefaultColor = Color{120, 85, 25, 255},
        .HoverColor = Color{165, 120, 35, 255},
        .PressedColor = Color{75, 55, 15, 255},
        .TextColor = Colors::Gold,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Clear Active Buffs
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CLEAR",
        .OnClick = [this]() { ClearAllPowerUpEffects(); },
        .Position = {365.0f, row1Y},
        .Size = {68.0f, padH},
        .DefaultColor = Color{65, 65, 75, 255},
        .HoverColor = Color{90, 90, 105, 255},
        .PressedColor = Color{40, 40, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // ==========================================
    // CHANNEL 2: ULTIMATE ABILITIES MODULE
    // ==========================================
    constexpr float row2Y = deckY - 118.0f;

    // Previous Ultimate Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousUltimate(); },
        .Position = {-375.0f, row2Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Ultimate LCD Display
    m_btnUltimateLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "ULT: PLASMA ORB",
        .OnClick = [this]() { FireSelectedUltimate(); },
        .Position = {-185.0f, row2Y},
        .Size = {310.0f, padH},
        .DefaultColor = Color{95, 45, 145, 255},
        .HoverColor = Color{135, 65, 195, 255},
        .PressedColor = Color{60, 25, 95, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Ultimate Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextUltimate(); },
        .Position = {5.0f, row2Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Fire Ultimate Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "FIRE ULTIMATE",
        .OnClick = [this]() { FireSelectedUltimate(); },
        .Position = {110.0f, row2Y},
        .Size = {140.0f, padH},
        .DefaultColor = Color{135, 40, 150, 255},
        .HoverColor = Color{180, 55, 200, 255},
        .PressedColor = Color{85, 25, 95, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Cancel Ultimate Effect Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CANCEL",
        .OnClick = [this]() { ClearActiveUltimatesDebug(); },
        .Position = {230.0f, row2Y},
        .Size = {84.0f, padH},
        .DefaultColor = Color{75, 40, 75, 255},
        .HoverColor = Color{105, 55, 105, 255},
        .PressedColor = Color{45, 25, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Max Ultimate Level Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MAX LVL",
        .OnClick = [this]() {
            ProfileManager::Get().SetUltimateLevel(kAllUltimates[m_selectedUltimateIdx], 5);
            UpdateSandboxUIDisplay();
        },
        .Position = {330.0f, row2Y},
        .Size = {100.0f, padH},
        .DefaultColor = Color{110, 75, 25, 255},
        .HoverColor = Color{150, 105, 35, 255},
        .PressedColor = Color{65, 45, 15, 255},
        .TextColor = Colors::Gold,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // ==========================================
    // CHANNEL 3: MALUS & SCREEN DISRUPTION MODULE
    // ==========================================
    constexpr float row3Y = deckY - 56.0f;

    // Previous Malus Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousMalus(); },
        .Position = {-375.0f, row3Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Malus LCD Display & Direct Trigger
    m_btnMalusLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MALUS: ALL COMBINED",
        .OnClick = [this]() { TriggerSelectedMalus(); },
        .Position = {-185.0f, row3Y},
        .Size = {310.0f, padH},
        .DefaultColor = Color{220, 35, 75, 255},
        .HoverColor = Color{255, 55, 95, 255},
        .PressedColor = Color{160, 25, 55, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Malus Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextMalus(); },
        .Position = {5.0f, row3Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Trigger Malus Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "TRIGGER",
        .OnClick = [this]() { TriggerSelectedMalus(); },
        .Position = {90.0f, row3Y},
        .Size = {100.0f, padH},
        .DefaultColor = Color{180, 35, 75, 255},
        .HoverColor = Color{225, 50, 95, 255},
        .PressedColor = Color{130, 25, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Spawn Hazard Zone Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "HAZARD ZONE",
        .OnClick = [this]() { SpawnSelectedHazardZone(); },
        .Position = {215.0f, row3Y},
        .Size = {130.0f, padH},
        .DefaultColor = Color{140, 30, 90, 255},
        .HoverColor = Color{185, 45, 120, 255},
        .PressedColor = Color{95, 20, 60, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Clear Malus Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CLEAR",
        .OnClick = [this]() { ClearActiveMaluses(); },
        .Position = {365.0f, row3Y},
        .Size = {68.0f, padH},
        .DefaultColor = Color{65, 65, 75, 255},
        .HoverColor = Color{90, 90, 105, 255},
        .PressedColor = Color{40, 40, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // ==========================================
    // CHANNEL 4: DYNAMIC ARENA EVENTS MODULE
    // ==========================================
    constexpr float row4Y = deckY + 6.0f;

    // Previous Event Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousArenaEvent(); },
        .Position = {-375.0f, row4Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Event LCD Display & Direct Trigger
    m_btnArenaEventLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "EVENT: TEMPORAL FLUX (SLOW)",
        .OnClick = [this]() { TriggerSelectedArenaEvent(); },
        .Position = {-185.0f, row4Y},
        .Size = {310.0f, padH},
        .DefaultColor = Color{140, 80, 210, 255},
        .HoverColor = Color{175, 110, 250, 255},
        .PressedColor = Color{95, 45, 150, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Event Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextArenaEvent(); },
        .Position = {5.0f, row4Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Trigger Event Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "TRIGGER",
        .OnClick = [this]() { TriggerSelectedArenaEvent(); },
        .Position = {90.0f, row4Y},
        .Size = {100.0f, padH},
        .DefaultColor = Color{180, 75, 35, 255},
        .HoverColor = Color{225, 100, 50, 255},
        .PressedColor = Color{120, 45, 20, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Auto Events Toggle Pad
    m_btnAutoEventsPad = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "AUTO: ON",
        .OnClick = [this]() { ToggleAutoArenaEvents(); },
        .Position = {215.0f, row4Y},
        .Size = {130.0f, padH},
        .DefaultColor = Color{40, 140, 80, 255},
        .HoverColor = Color{55, 175, 100, 255},
        .PressedColor = Color{25, 90, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Clear Event Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CLEAR",
        .OnClick = [this]() { ClearActiveArenaEvent(); },
        .Position = {365.0f, row4Y},
        .Size = {68.0f, padH},
        .DefaultColor = Color{65, 65, 75, 255},
        .HoverColor = Color{90, 90, 105, 255},
        .PressedColor = Color{40, 40, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // ==========================================
    // CHANNEL 5: PLAYGROUND LAYOUTS & BRICKS
    // ==========================================
    constexpr float row5Y = deckY + 68.0f;

    // Previous Layout Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousPlaygroundLayout(); },
        .Position = {-375.0f, row5Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Layout LCD Display
    m_btnLayoutLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "LAYOUT: TACTICAL MIX",
        .OnClick = [this]() { NextPlaygroundLayout(); },
        .Position = {-185.0f, row5Y},
        .Size = {310.0f, padH},
        .DefaultColor = Color{40, 75, 115, 255},
        .HoverColor = Color{60, 105, 165, 255},
        .PressedColor = Color{25, 50, 80, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Layout Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextPlaygroundLayout(); },
        .Position = {5.0f, row5Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Refill Bricks Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "REFILL BRICKS",
        .OnClick = [this]() { RespawnCurrentLayout(); },
        .Position = {110.0f, row5Y},
        .Size = {140.0f, padH},
        .DefaultColor = Color{40, 95, 55, 255},
        .HoverColor = Color{55, 135, 75, 255},
        .PressedColor = Color{25, 60, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Clear Bricks Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "CLEAR",
        .OnClick = [this]() { ClearAllBricks(); },
        .Position = {230.0f, row5Y},
        .Size = {84.0f, padH},
        .DefaultColor = Color{110, 40, 40, 255},
        .HoverColor = Color{150, 55, 55, 255},
        .PressedColor = Color{70, 25, 25, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 14.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // ==========================================
    // CHANNEL 6: ENVIRONMENT & SAFETY & TOOLS
    // ==========================================
    constexpr float row6Y = deckY + 130.0f;

    // Previous Light Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this]() { PreviousLightingMode(); },
        .Position = {-375.0f, row6Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Lighting LCD Display
    m_btnLightingLCD = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "LIGHT: LIT",
        .OnClick = [this]() { NextLightingMode(); },
        .Position = {-235.0f, row6Y},
        .Size = {210.0f, padH},
        .DefaultColor = Color{45, 55, 75, 255},
        .HoverColor = Color{65, 80, 110, 255},
        .PressedColor = Color{30, 35, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Next Light Arrow
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this]() { NextLightingMode(); },
        .Position = {-100.0f, row6Y},
        .Size = {44.0f, padH},
        .DefaultColor = Color{35, 42, 60, 255},
        .HoverColor = Color{55, 68, 95, 255},
        .PressedColor = Color{20, 25, 35, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Laser Floor Safety Barrier Pad
    m_btnBarrierPad = UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "LASER FLOOR: ACTIVE",
        .OnClick = [this]() { ToggleSafetyBarrier(); },
        .Position = {45.0f, row6Y},
        .Size = {195.0f, padH},
        .DefaultColor = Color{0, 140, 185, 255},
        .HoverColor = Color{0, 185, 245, 255},
        .PressedColor = Color{0, 90, 120, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Ball Respawn Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "BALL RESPAWN",
        .OnClick = [this]() { RespawnBall(); },
        .Position = {210.0f, row6Y},
        .Size = {120.0f, padH},
        .DefaultColor = Color{35, 90, 110, 255},
        .HoverColor = Color{50, 125, 155, 255},
        .PressedColor = Color{25, 60, 75, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 15.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Main Menu Exit Pad
    UIFactory::CreateButton(m_registry, m_sandboxCanvas, ButtonDescriptor{
        .Text = "MENU",
        .OnClick = [&context]() {
            context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);
            context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.45f);
        },
        .Position = {330.0f, row6Y},
        .Size = {100.0f, padH},
        .DefaultColor = Color{110, 35, 40, 255},
        .HoverColor = Color{150, 50, 55, 255},
        .PressedColor = Color{75, 25, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .AnchorPoint = Anchor::Center,
        .IgnoreMenuNavigation = true,
        .ZOrder = 12
    });

    // Bottom ergonomic hint line
    UIFactory::CreateText(m_registry, m_sandboxCanvas, TextDescriptor{
        .Text = "STREAM DECK CONTROLLER  |  TOGGLE [TAB]  |  CLICK PADS TO INTERACT",
        .Position = {0.0f, deckY + 195.0f},
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .Tint = Color{170, 195, 230, 190},
        .AnchorPoint = Anchor::Center,
        .ZOrder = 12
    });
}

void SandboxScene::UpdateSandboxUIDisplay()
{
    // Update Power-Up LCD Pad
    if (m_btnPowerUpLCD != NULL_ENTITY)
    {
        PowerUpType pt = kAllPowerUps[m_selectedPowerUpIdx];
        int lvl = PowerUpManager::Get().GetLevel(pt);
        std::string label = std::string(GetPowerUpDisplayName(pt)) + " (LV." + std::to_string(lvl) + ")";
        Color col = GetPowerUpDisplayColor(pt);

        if (m_registry.HasComponent<TextComponent>(m_btnPowerUpLCD))
        {
            m_registry.GetComponent<TextComponent>(m_btnPowerUpLCD).Text = label;
        }
        if (m_registry.HasComponent<ButtonComponent>(m_btnPowerUpLCD))
        {
            auto& btn = m_registry.GetComponent<ButtonComponent>(m_btnPowerUpLCD);
            btn.DefaultColor = col;
            btn.HoverColor = Color{
                static_cast<uint8_t>(std::min(255, col.r + 35)),
                static_cast<uint8_t>(std::min(255, col.g + 35)),
                static_cast<uint8_t>(std::min(255, col.b + 35)),
                255
            };
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnPowerUpLCD))
        {
            m_registry.GetComponent<PanelComponent>(m_btnPowerUpLCD).Tint = col;
        }
    }

    // Update Mode Toggle Pad
    if (m_btnSpawnMode != NULL_ENTITY)
    {
        if (m_registry.HasComponent<TextComponent>(m_btnSpawnMode))
        {
            m_registry.GetComponent<TextComponent>(m_btnSpawnMode).Text =
                m_spawnCapsuleMode ? "MODE: DROP" : "MODE: INSTANT";
        }
        Color modeCol = m_spawnCapsuleMode ? Color{150, 95, 20, 255} : Color{35, 120, 75, 255};
        if (m_registry.HasComponent<ButtonComponent>(m_btnSpawnMode))
        {
            m_registry.GetComponent<ButtonComponent>(m_btnSpawnMode).DefaultColor = modeCol;
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnSpawnMode))
        {
            m_registry.GetComponent<PanelComponent>(m_btnSpawnMode).Tint = modeCol;
        }
    }

    // Update Ultimate LCD Pad
    if (m_btnUltimateLCD != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_btnUltimateLCD))
    {
        UltimateAbilityType ut = kAllUltimates[m_selectedUltimateIdx];
        int ultLvl = ProfileManager::Get().GetUltimateLevel(ut);
        std::string ultLabel = std::string("ULT: ") + GetUltimateDisplayName(ut) + " (MK-" + std::to_string(ultLvl) + ")";
        m_registry.GetComponent<TextComponent>(m_btnUltimateLCD).Text = ultLabel;
    }

    // Update Malus LCD Pad
    if (m_btnMalusLCD != NULL_ENTITY)
    {
        HazardEffectType mt = kAllMaluses[m_selectedMalusIdx];
        if (m_registry.HasComponent<TextComponent>(m_btnMalusLCD))
        {
            m_registry.GetComponent<TextComponent>(m_btnMalusLCD).Text = GetMalusDisplayName(mt);
        }
        Color mCol = GetMalusDisplayColor(mt);
        if (m_registry.HasComponent<ButtonComponent>(m_btnMalusLCD))
        {
            auto& btn = m_registry.GetComponent<ButtonComponent>(m_btnMalusLCD);
            btn.DefaultColor = mCol;
            btn.HoverColor = Color{
                static_cast<uint8_t>(std::min(255, mCol.r + 35)),
                static_cast<uint8_t>(std::min(255, mCol.g + 35)),
                static_cast<uint8_t>(std::min(255, mCol.b + 35)),
                255
            };
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnMalusLCD))
        {
            m_registry.GetComponent<PanelComponent>(m_btnMalusLCD).Tint = mCol;
        }
    }

    // Update Arena Event LCD Pad
    if (m_btnArenaEventLCD != NULL_ENTITY)
    {
        std::string status;
        if (mp_eventSystem)
        {
            if (mp_eventSystem->IsEventActive())
            {
                std::ostringstream ss;
                ss << "[ACTIVE] " << GetArenaEventName(mp_eventSystem->GetCurrentEventType()) << " (" << std::fixed << std::setprecision(1) << mp_eventSystem->GetTimeRemaining() << "s)";
                status = ss.str();
            }
            else if (mp_eventSystem->IsWarningActive())
            {
                std::ostringstream ss;
                ss << "[WARN] " << GetArenaEventName(mp_eventSystem->GetCurrentEventType()) << " (" << std::fixed << std::setprecision(1) << mp_eventSystem->GetTimeRemaining() << "s)";
                status = ss.str();
            }
            else
            {
                status = std::string("EVENT: ") + GetArenaEventName(m_selectedArenaEventType);
            }
        }
        else
        {
            status = std::string("EVENT: ") + GetArenaEventName(m_selectedArenaEventType);
        }

        if (m_registry.HasComponent<TextComponent>(m_btnArenaEventLCD))
        {
            m_registry.GetComponent<TextComponent>(m_btnArenaEventLCD).Text = status;
        }
        Color eCol = GetArenaEventColor(m_selectedArenaEventType);
        if (m_registry.HasComponent<ButtonComponent>(m_btnArenaEventLCD))
        {
            auto& btn = m_registry.GetComponent<ButtonComponent>(m_btnArenaEventLCD);
            btn.DefaultColor = eCol;
            btn.HoverColor = Color{
                static_cast<uint8_t>(std::min(255, eCol.r + 35)),
                static_cast<uint8_t>(std::min(255, eCol.g + 35)),
                static_cast<uint8_t>(std::min(255, eCol.b + 35)),
                255
            };
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnArenaEventLCD))
        {
            m_registry.GetComponent<PanelComponent>(m_btnArenaEventLCD).Tint = eCol;
        }
    }

    // Update Auto Events Toggle Pad
    if (m_btnAutoEventsPad != NULL_ENTITY)
    {
        const bool autoOn = mp_eventSystem ? mp_eventSystem->IsEnabled() : true;
        if (m_registry.HasComponent<TextComponent>(m_btnAutoEventsPad))
        {
            m_registry.GetComponent<TextComponent>(m_btnAutoEventsPad).Text =
                autoOn ? "AUTO: ON" : "AUTO: OFF";
        }
        Color aCol = autoOn ? Color{40, 140, 80, 255} : Color{120, 45, 50, 255};
        if (m_registry.HasComponent<ButtonComponent>(m_btnAutoEventsPad))
        {
            m_registry.GetComponent<ButtonComponent>(m_btnAutoEventsPad).DefaultColor = aCol;
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnAutoEventsPad))
        {
            m_registry.GetComponent<PanelComponent>(m_btnAutoEventsPad).Tint = aCol;
        }
    }

    // Update Layout LCD Pad
    if (m_btnLayoutLCD != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_btnLayoutLCD))
    {
        m_registry.GetComponent<TextComponent>(m_btnLayoutLCD).Text = std::string("LAYOUT: ") + GetCurrentLayoutName();
    }

    // Update Lighting LCD Pad
    if (m_btnLightingLCD != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_btnLightingLCD))
    {
        m_registry.GetComponent<TextComponent>(m_btnLightingLCD).Text = GetLightingModeName(m_lightingMode);
    }

    // Update Barrier Pad
    if (m_btnBarrierPad != NULL_ENTITY)
    {
        if (m_registry.HasComponent<TextComponent>(m_btnBarrierPad))
        {
            m_registry.GetComponent<TextComponent>(m_btnBarrierPad).Text =
                m_safetyBarrierEnabled ? "LASER FLOOR: ACTIVE" : "LASER FLOOR: OFF";
        }
        Color barrierCol = m_safetyBarrierEnabled ? Color{0, 140, 185, 255} : Color{70, 35, 40, 255};
        if (m_registry.HasComponent<ButtonComponent>(m_btnBarrierPad))
        {
            m_registry.GetComponent<ButtonComponent>(m_btnBarrierPad).DefaultColor = barrierCol;
        }
        if (m_registry.HasComponent<PanelComponent>(m_btnBarrierPad))
        {
            m_registry.GetComponent<PanelComponent>(m_btnBarrierPad).Tint = barrierCol;
        }
    }
}
