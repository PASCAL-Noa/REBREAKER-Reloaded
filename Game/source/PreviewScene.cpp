#include "Scenes/PreviewScene.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/InputManager.h"
#include "Core/SceneManager.h"
#include "Core/GameRules.h"
#include "AudioMixer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Systems/ParticleSystem.h"
#include "Generators/FileLevelGenerator.h"
#include "Graphics/Renderer.h"
#include "PowerUps/PowerUpConfig.h"
#include <filesystem>

struct PreviewItemInfo
{
    PowerUpType Type;
    const char* Name;
    const char* Badge;
    Color       CapsuleColor;
};

static const PreviewItemInfo kPreviewItems[] = {
    { PowerUpType::Storm,         "STORM THUNDER",      "STM", Color{255, 235, 40, 255} },
    { PowerUpType::FireBall,      "FIRE BALL AOE",      "FIR", Color{255, 60, 20, 255} },
    { PowerUpType::BigBall,       "BIG CRUSH BALL",     "BIG", Color{255, 120, 20, 255} },
    { PowerUpType::MultiBall,     "MULTI-BALL",         "MLB", Color{0, 220, 255, 255} },
    { PowerUpType::StickyPaddle,  "STICKY CATCH",       "STK", Color{245, 175, 40, 255} },
    { PowerUpType::BottomBarrier, "BOTTOM FORCEFIELD",  "BAR", Color{0, 240, 255, 255} },
    { PowerUpType::MagnetPaddle,  "MAGNET PADDLE",      "MAG", Color{220, 60, 255, 255} },
    { PowerUpType::GhostBall,     "GHOST PHASE BALL",   "GHO", Color{160, 240, 255, 255} },
    { PowerUpType::TempoBall,     "TEMPO BALL",         "TMP", Color{180, 70, 255, 255} },
    { PowerUpType::ExpandPaddle,  "EXPAND PADDLE",      "EXP", Color{50, 255, 80, 255} },
    { PowerUpType::ShrinkPaddle,  "SHRINK PADDLE",      "SHR", Color{255, 60, 60, 255} },
    { PowerUpType::ExtraLife,     "EXTRA LIFE",         "LIF", Color{255, 100, 180, 255} }
};
static constexpr size_t kPreviewItemCount = sizeof(kPreviewItems) / sizeof(kPreviewItems[0]);

static PowerUpType s_initialPowerUpType = PowerUpType::Storm;
static bool s_hasExplicitInitialPowerUp = false;

void PreviewScene::SetInitialPowerUp(PowerUpType type)
{
    s_initialPowerUpType = type;
    s_hasExplicitInitialPowerUp = true;
}

void PreviewScene::OnInit(GameContext& context)
{
    GameScene::OnInit(context);

    // Infinite lives & safe testing rules
    context.Rules.SetRule(Rule::Gameplay::InfiniteLives, true);
    m_lives = 99;

    // Hide HUD counter (Score : 0 and hearts) for preview scene only
    if (m_uiController)
    {
        Entity hudCanvas = m_uiController->GetMainUICanvas();
        if (hudCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(hudCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(hudCanvas).IsEnabled = false;
        }
    }

    m_currentItemIndex = 0;
    if (s_hasExplicitInitialPowerUp)
    {
        for (size_t i = 0; i < kPreviewItemCount; ++i)
        {
            if (kPreviewItems[i].Type == s_initialPowerUpType)
            {
                m_currentItemIndex = i;
                break;
            }
        }
        s_hasExplicitInitialPowerUp = false;
    }

    PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    ResetPreview();

    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetOnBallLaunchedCallback([this](Entity)
        {
            m_ballState = BallState::Active;
            OnBallLaunched();
        });
    }
}

void PreviewScene::OnUpdate(float dt, GameContext& context)
{
    // Arrow keys: Left / Right to change power-up (resets level to 1 and refreshes scene)
    if (context.Input.IsKeyPress(KeyCode::Left))
    {
        PrevPowerUp();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::Right))
    {
        NextPowerUp();
        return;
    }

    // Arrow keys: Up / Down to level up / level down (refreshes scene)
    if (context.Input.IsKeyPress(KeyCode::Up))
    {
        LevelUp();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::Down))
    {
        LevelDown();
        return;
    }

    // R key: manual refresh
    if (context.Input.IsKeyPress(KeyCode::R))
    {
        ResetPreview();
        return;
    }

    // Escape: Return to main menu
    if (context.Input.IsKeyPress(KeyCode::Escape) || context.Input.IsJoystickButtonPress(0, 6))
    {
        context.Scenes.LoadScene<MenuScene>();
        return;
    }

    // Check if the preview finished (all bricks cleared or ball dying/lost) -> instantly refresh!
    if (m_brickCount <= 0 || m_ballState == BallState::Dying)
    {
        ResetPreview();
        return;
    }

    if (mp_state_machine)
    {
        int st = mp_state_machine->GetCurrentState();
        if (st == static_cast<int>(SceneState::LevelTransition) || st == static_cast<int>(SceneState::Victory))
        {
            ResetPreview();
            return;
        }
    }

    // Ball loss check:
    // Only reset if NO balls remain in play, or if the sole remaining ball fell well below the paddle (Y > 440)
    if (m_ballState == BallState::Active && mp_ballSystem)
    {
        if (mp_ballSystem->GetBallCount() == 0 && m_capsuleSpawnedForLaunch)
        {
            ResetPreview();
            return;
        }
        else if (mp_ballSystem->GetBallCount() == 1)
        {
            const auto& balls = mp_ballSystem->GetBalls();
            if (!balls.empty() && m_registry.IsAlive(balls.front()) && m_registry.HasComponent<Transform2D>(balls.front()))
            {
                // In Ghost Ball preview, the ball is immortal and rebounds off the bottom wall
                if (m_ghostBallDuration <= 0.0f && !m_ghostBallPendingExpiry && m_registry.GetComponent<Transform2D>(balls.front()).Position.Y > 440.0f)
                {
                    ResetPreview();
                    return;
                }
            }
        }
    }

    // For Magnet Paddle preview: spawn falling capsules periodically off-center to showcase magnetic attraction
    if (kPreviewItems[m_currentItemIndex].Type == PowerUpType::MagnetPaddle && m_ballState == BallState::Active && m_capsuleSpawnedForLaunch)
    {
        m_magnetCapsuleTimer += dt;
        if (m_magnetCapsuleTimer >= 1.4f)
        {
            m_magnetCapsuleTimer = 0.0f;
            SpawnMagnetDemoCapsule();
        }
    }
    else
    {
        m_magnetCapsuleTimer = 0.0f;
    }

    // Keep preview capsule horizontally aligned with paddle center so it drops straight onto it
    if (m_previewCapsule != NULL_ENTITY && m_registry.IsAlive(m_previewCapsule))
    {
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle) &&
            m_registry.HasComponent<Transform2D>(m_previewCapsule))
        {
            const auto& pt = m_registry.GetComponent<Transform2D>(m_paddle);
            auto& ct = m_registry.GetComponent<Transform2D>(m_previewCapsule);
            ct.Position.X = pt.Position.X;
        }
    }

    // Check ball launch transition as backup
    if (m_wasBallAttached && m_ballState == BallState::Active && !m_capsuleSpawnedForLaunch)
    {
        OnBallLaunched();
    }
    m_wasBallAttached = (m_ballState == BallState::Attached);

    // Keep lives at 99
    m_lives = 99;

    GameScene::OnUpdate(dt, context);
}

void PreviewScene::OnRender(GameContext& context)
{
    // Completely disable countdown timer in preview
    m_countdownActive = false;
    m_countdownTimer = 0.0f;
    m_countdownGoTimer = 0.0f;

    GameScene::OnRender(context);

    // Overlay Header Banner showing preview info and controls (without clutter counters)
    const auto& item = kPreviewItems[m_currentItemIndex];
    int curLvl = PowerUpManager::Get().GetLevel(item.Type);
    int maxLvl = PowerUpManager::Get().GetMaxLevel(item.Type);

    // Top banner background
    context.Render.DrawRectangle(1920.0f, 66.0f, Transform2D{Vector2f{0.0f, -495.0f}}, Color{12, 16, 26, 230});
    context.Render.DrawRectangleOutline(1920.0f, 66.0f, Transform2D{Vector2f{0.0f, -495.0f}}, Color{0, 200, 255, 180}, 1.5f);

    // Item Name, Level, and live FPS (e.g. "GHOST PHASE BALL [GHO]   |   NIV. 1 / 5   |   GHOST: 4.8s   |   60 FPS")
    std::string title = std::string(item.Name) + " [" + item.Badge + "]   |   NIV. " +
                        std::to_string(curLvl) + " / " + std::to_string(maxLvl);
    if (m_ghostBallDuration > 0.0f)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "   |   GHOST: %.1fs", m_ghostBallDuration);
        title += buf;
    }
    else if (m_ghostBallPendingExpiry)
    {
        title += "   |   GHOST: SORTIE BRIQUE...";
    }
    else if (m_tempoBallDuration > 0.0f)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "   |   TEMPO: %.1fs", m_tempoBallDuration);
        title += buf;
    }
    title += "   |   " + std::to_string(context.Data.FPS) + " FPS";
    context.Render.DrawTextCentered(title, m_fontId, 28.0f, Transform2D{Vector2f{0.0f, -508.0f}}, Colors::Gold);

    // Controls reminder
    std::string instructions = "[<- / ->] Power-up  |  [^ / v] Niveau  |  [ESPACE] Lancer balle  |  [R] Reset  |  [ECHAP] Menu";
    context.Render.DrawTextCentered(instructions, m_fontId, 20.0f, Transform2D{Vector2f{0.0f, -480.0f}}, Color{180, 220, 255, 240});
}

void PreviewScene::NextPowerUp()
{
    m_currentItemIndex = (m_currentItemIndex + 1) % kPreviewItemCount;
    PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    if (m_bounceSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
    }
    ResetPreview();
}

void PreviewScene::PrevPowerUp()
{
    m_currentItemIndex = (m_currentItemIndex + kPreviewItemCount - 1) % kPreviewItemCount;
    PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    if (m_bounceSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
    }
    ResetPreview();
}

void PreviewScene::LevelUp()
{
    PowerUpType type = kPreviewItems[m_currentItemIndex].Type;
    int cur = PowerUpManager::Get().GetLevel(type);
    int maxL = PowerUpManager::Get().GetMaxLevel(type);
    if (cur < maxL)
    {
        PowerUpManager::Get().SetLevel(type, cur + 1);
        if (m_bounceSfxId != 0 && mp_context)
        {
            mp_context->Audio.PlaySfx(m_bounceSfxId, 95.0f, 1.25f);
        }
        ResetPreview();
    }
}

void PreviewScene::LevelDown()
{
    PowerUpType type = kPreviewItems[m_currentItemIndex].Type;
    int cur = PowerUpManager::Get().GetLevel(type);
    if (cur > 1)
    {
        PowerUpManager::Get().SetLevel(type, cur - 1);
        if (m_bounceSfxId != 0 && mp_context)
        {
            mp_context->Audio.PlaySfx(m_bounceSfxId, 80.0f, 0.85f);
        }
        ResetPreview();
    }
}

void PreviewScene::ResetPreview()
{
    // Clean all particles, lights, and flashes to avoid memory accumulation / frame drops
    if (m_particleSystem)
    {
        m_particleSystem->Clear();
    }
    m_activeLightningBolts.clear();
    m_electrifiedBricks.clear();
    m_lightFlashes.clear();
    m_stuckBalls.clear();

    if (m_fireballBurnSfxId != 0 && mp_context)
    {
        mp_context->Audio.StopLoopingSfx(m_fireballBurnSfxId);
    }

    // Reset all power-up durations and flags
    m_paddleSizeDuration = 0.0f;
    m_stormDuration = 0.0f;
    m_stormCharges = 0;
    m_laserDuration = 0.0f;
    m_tempoBallDuration = 0.0f;
    m_tempoBallStacks = 0;
    if (mp_ballSystem)
    {
        mp_ballSystem->SetTempoBallActive(false);
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetLaunchSpeed(640.0f);
    }
    m_bigBallDuration = 0.0f;
    m_stickyPaddleDuration = 0.0f;
    m_bottomBarrierDuration = 0.0f;
    m_bottomBarrierCharges = 0;
    m_bottomBarrierActive = false;
    m_magnetDuration = 0.0f;
    m_ghostBallDuration = 0.0f;
    m_ghostBallPendingExpiry = false;

    // Reset paddle position, scale, and size
    if (m_registry.IsAlive(m_paddle))
    {
        if (m_registry.HasComponent<Transform2D>(m_paddle))
        {
            auto& pt = m_registry.GetComponent<Transform2D>(m_paddle);
            pt.Position = Vector2f{0.0f, 300.0f};
            pt.Scale = Vector2f{1.0f, 1.0f};
        }
        if (m_registry.HasComponent<BoxCollider>(m_paddle))
        {
            m_registry.GetComponent<BoxCollider>(m_paddle).Size = Vector2f{120.0f, 20.0f};
        }
        if (m_registry.HasComponent<PaddleComponent>(m_paddle))
        {
            auto& pc = m_registry.GetComponent<PaddleComponent>(m_paddle);
            pc.Speed = 700.0f;
            pc.HasStorm = false;
            pc.HasLaser = false;
            pc.HasSticky = false;
            pc.HasMagnet = false;
            pc.StormCooldown = 0.0f;
            pc.LaserCooldown = 0.0f;
        }
        if (m_registry.HasComponent<SpriteComponent>(m_paddle))
        {
            m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
        }
    }

    // Clear existing balls and lasers
    if (mp_ballSystem)
    {
        mp_ballSystem->ClearBalls();
        mp_ballSystem->ClearLasers();
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->DetachBall();
    }

    // Clear any falling capsules
    m_registry.View<PowerUpComponent>([this](Entity e, PowerUpComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_previewCapsule = NULL_ENTITY;

    // Reload bricks cleanly:
    // 1. Storm Thunder: load dedicated conductive metal circuit (preview_storm.txt) + Level 7 theme ("STORM VAULT")
    // 2. Fire Ball: load dedicated metal bunker (preview_fireball.txt) + Level 3 theme ("SOLAR FLARE")
    // 3. Other power-ups: load Level 1 (level01.txt) + Level 1 theme ("CYBER ODYSSEY")
    PowerUpType curType = kPreviewItems[m_currentItemIndex].Type;
    std::string levelPath;
    if (curType == PowerUpType::Storm)
    {
        m_levelManager.SetLevel(6);
        ApplyLevelTheme(7);
        const std::string stormLevel = "Resources/levels/preview_storm.txt";
        std::error_code ec;
        if (std::filesystem::exists(stormLevel, ec))
        {
            levelPath = stormLevel;
        }
        else
        {
            levelPath = m_levelManager.GetCurrentLevelPath();
        }
    }
    else if (curType == PowerUpType::FireBall)
    {
        m_levelManager.SetLevel(2);
        ApplyLevelTheme(3);
        const std::string fireballLevel = "Resources/levels/preview_fireball.txt";
        std::error_code ec;
        if (std::filesystem::exists(fireballLevel, ec))
        {
            levelPath = fireballLevel;
        }
        else
        {
            levelPath = m_levelManager.GetCurrentLevelPath();
        }
    }
    else if (curType == PowerUpType::GhostBall)
    {
        m_levelManager.SetLevel(3);
        ApplyLevelTheme(4); // AMETHYST VOID
        const std::string ghostLevel = "Resources/levels/preview_ghost.txt";
        std::error_code ec;
        if (std::filesystem::exists(ghostLevel, ec))
        {
            levelPath = ghostLevel;
        }
        else
        {
            levelPath = m_levelManager.GetCurrentLevelPath();
        }
    }
    else
    {
        m_levelManager.SetLevel(0);
        ApplyLevelTheme(1);
        levelPath = m_levelManager.GetCurrentLevelPath();
    }

    m_magnetCapsuleTimer = 0.0f;

    m_registry.View<BrickComponent>([this](Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    mp_levelGenerator = std::make_unique<FileLevelGenerator>(levelPath);
    if (mp_context)
    {
        m_brickCount = mp_levelGenerator->Generate(m_registry, *mp_context, m_brickTexId);
    }

    // Create a fresh single ball and attach it
    m_ball = CreateBall(Vector2f{0.0f, 260.0f}, Vector2f{0.0f, 0.0f});
    if (m_registry.IsAlive(m_ball))
    {
        if (m_registry.HasComponent<Transform2D>(m_ball))
        {
            m_registry.GetComponent<Transform2D>(m_ball).Scale = Vector2f{1.0f, 1.0f};
        }
        if (m_registry.HasComponent<RigidBody>(m_ball))
        {
            auto& rb = m_registry.GetComponent<RigidBody>(m_ball);
            rb.Velocity = Vector2f{0.0f, 0.0f};
            rb.IsKinematic = true;
        }
    }
    m_ballState = BallState::Attached;
    if (mp_paddleSystem && m_registry.IsAlive(m_paddle) && m_registry.IsAlive(m_ball))
    {
        mp_paddleSystem->AttachBall(m_paddle, m_ball);
    }

    // Bypass countdown
    m_countdownActive = false;
    m_countdownTimer = 0.0f;
    m_countdownGoTimer = 0.0f;
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
    }

    m_capsuleSpawnedForLaunch = false;
    m_wasBallAttached = true;
    m_lives = 99;

    // Hide HUD counter in preview
    if (m_uiController)
    {
        Entity hudCanvas = m_uiController->GetMainUICanvas();
        if (hudCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(hudCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(hudCanvas).IsEnabled = false;
        }
    }
}

void PreviewScene::SpawnPowerUp(const Vector2f& position)
{
    // For Magnet Paddle preview: let destroyed bricks drop extra capsules to showcase magnetic pull!
    if (kPreviewItems[m_currentItemIndex].Type == PowerUpType::MagnetPaddle)
    {
        int typeIndex = rand() % kPreviewItemCount;
        PowerUpType type = kPreviewItems[typeIndex].Type;
        Color color = kPreviewItems[typeIndex].CapsuleColor;

        Entity capsule = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(capsule, Transform2D{position, 0.0f, Vector2f{0.5f, 0.7f}});
        m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
        m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 210.0f, false});
        m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});
    }
}

void PreviewScene::ApplyPowerUp(PowerUpType type)
{
    GameScene::ApplyPowerUp(type);

    // If currently previewing Magnet, keep magnet continuously refreshed whenever any capsule is caught
    if (kPreviewItems[m_currentItemIndex].Type == PowerUpType::MagnetPaddle)
    {
        const auto& magnetCfg = PowerUpManager::Get().Magnet();
        m_magnetDuration = magnetCfg.GetEffectiveDuration();
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
        {
            auto& pc = m_registry.GetComponent<PaddleComponent>(m_paddle);
            pc.HasMagnet = true;
        }
    }
}

void PreviewScene::SpawnMagnetDemoCapsule()
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<Transform2D>(m_paddle)) return;

    // Pick an offset X to clearly demonstrate the curved gravitational pull into the paddle
    const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    float offsetDir = (rand() % 2 == 0) ? -1.0f : 1.0f;
    float offsetX = offsetDir * (130.0f + static_cast<float>(rand() % 250));
    float spawnX = std::clamp(paddleTrans.Position.X + offsetX, -650.0f, 650.0f);
    float spawnY = -270.0f - static_cast<float>(rand() % 70);

    // Variety of colorful capsules falling
    int typeIndex = rand() % kPreviewItemCount;
    PowerUpType type = kPreviewItems[typeIndex].Type;
    Color color = kPreviewItems[typeIndex].CapsuleColor;

    Entity capsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(capsule, Transform2D{Vector2f{spawnX, spawnY}, 0.0f, Vector2f{0.5f, 0.7f}});
    m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 210.0f, false});
    m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});
}

void PreviewScene::SpawnPreviewCapsule()
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<Transform2D>(m_paddle)) return;

    const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    PowerUpType type = kPreviewItems[m_currentItemIndex].Type;

    if (m_previewCapsule != NULL_ENTITY && m_registry.IsAlive(m_previewCapsule))
    {
        m_registry.DestroyEntityDeferred(m_previewCapsule);
        m_registry.ProcessDeferredCommands();
    }

    // Spawn capsule directly above paddle
    Vector2f pos{ paddleTrans.Position.X, -100.0f };

    m_previewCapsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_previewCapsule, Transform2D{pos, 0.0f, Vector2f{0.5f, 0.7f}});
    m_registry.AddComponent<BoxCollider>(m_previewCapsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<PowerUpComponent>(m_previewCapsule, PowerUpComponent{type, 260.0f, false});
    Color color = kPreviewItems[m_currentItemIndex].CapsuleColor;
    m_registry.AddComponent<SpriteComponent>(m_previewCapsule, SpriteComponent{m_brickTexId, color});
}

void PreviewScene::OnBallLaunched()
{
    if (m_capsuleSpawnedForLaunch) return;
    m_capsuleSpawnedForLaunch = true;
    SpawnPreviewCapsule();
}
