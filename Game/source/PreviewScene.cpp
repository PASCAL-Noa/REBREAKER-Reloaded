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
#include "ECS/Components/TweenComponent.h"
#include "ECS/Systems/ParticleSystem.h"
#include "Generators/FileLevelGenerator.h"
#include "Graphics/Renderer.h"
#include "PowerUps/PowerUpConfig.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

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

static UltimateAbilityType s_initialUltimateType = UltimateAbilityType::PlasmaOrb;
static int s_initialUltimateLevel = 1;
static bool s_hasExplicitInitialUltimate = false;

PreviewScene::PreviewScene()
{
    m_musicEnabled = false;
    m_audioEnabled = false;
    m_renderGameUI = false;
}

void PreviewScene::SetInitialPowerUp(PowerUpType type)
{
    s_initialPowerUpType = type;
    s_hasExplicitInitialPowerUp = true;
    s_hasExplicitInitialUltimate = false;
}

void PreviewScene::SetInitialUltimate(UltimateAbilityType type, int level)
{
    s_initialUltimateType = type;
    s_initialUltimateLevel = std::clamp(level, 1, 5);
    s_hasExplicitInitialUltimate = true;
    s_hasExplicitInitialPowerUp = false;
}

void PreviewScene::SetEmbeddedMode(bool embedded)
{
    m_isEmbeddedMode = embedded;
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(!embedded);
    }
    if (m_isEmbeddedMode && m_registry.IsAlive(m_camera))
    {
        if (m_registry.HasComponent<TweenComponent>(m_camera))
        {
            m_registry.RemoveComponent<TweenComponent>(m_camera);
        }
        if (m_registry.HasComponent<Camera2D>(m_camera))
        {
            auto& cam = m_registry.GetComponent<Camera2D>(m_camera);
            cam.Position = Vector2f{0.0f, -20.0f};
            cam.Zoom = 1.35f;
            cam.Rotation = 0.0f;
        }
    }
}

void PreviewScene::SetAudioEnabled(bool enabled)
{
    GameScene::SetAudioEnabled(enabled);
    if (!enabled && mp_context && m_fireballBurnSfxId != 0)
    {
        mp_context->Audio.StopLoopingSfx(m_fireballBurnSfxId);
    }
}

void PreviewScene::SetMusicEnabled(bool enabled)
{
    bool wasEnabled = m_musicEnabled;
    GameScene::SetMusicEnabled(enabled);
    if (wasEnabled && !enabled && mp_context)
    {
        std::string cur = mp_context->Audio.GetCurrentMusicPath();
        if (cur.find("disco_ultimate") != std::string::npos ||
            cur.find("plasma_ultimate") != std::string::npos ||
            cur.find("lucky_drop_ultimate") != std::string::npos)
        {
            mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
        }
    }
}

void PreviewScene::RestorePostUltimateMusic()
{
    if (!mp_context || !m_musicEnabled) return;
    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
}

void PreviewScene::SetPreviewPowerUp(PowerUpType type, int level)
{
    m_previewMode = PreviewMode::PowerUp;
    m_previewLevel = std::clamp(level, 1, PowerUpManager::Get().GetMaxLevel(type));
    for (size_t i = 0; i < kPreviewItemCount; ++i)
    {
        if (kPreviewItems[i].Type == type)
        {
            m_currentItemIndex = i;
            break;
        }
    }
    PowerUpManager::Get().SetLevelTransient(type, m_previewLevel);
    ResetPreview();
}

void PreviewScene::SetPreviewUltimate(UltimateAbilityType type, int level)
{
    m_previewMode = PreviewMode::Ultimate;
    m_currentUltimate = type;
    m_previewLevel = std::clamp(level, 1, 5);
    ResetPreview();
}

void PreviewScene::TogglePreviewMode()
{
    if (m_previewMode == PreviewMode::PowerUp)
    {
        m_previewMode = PreviewMode::Ultimate;
        m_previewLevel = ProfileManager::Get().GetUltimateLevel(m_currentUltimate);
    }
    else
    {
        m_previewMode = PreviewMode::PowerUp;
        m_previewLevel = PowerUpManager::Get().GetLevel(kPreviewItems[m_currentItemIndex].Type);
    }
    ResetPreview();
}

void PreviewScene::UpdateAutopilot(float dt)
{
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(false);
    }

    // 1. Auto-launch ball when attached
    if (m_ballState == BallState::Attached)
    {
        m_autoLaunchTimer += dt;
        if (m_autoLaunchTimer >= 0.45f)
        {
            m_autoLaunchTimer = 0.0f;
            m_ballState = BallState::Active;
            if (mp_paddleSystem)
            {
                mp_paddleSystem->LaunchBall();
            }
        }
    }
    else
    {
        m_autoLaunchTimer = 0.0f;
    }

    // 2. Autopilot paddle positioning
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        float targetX = paddleTrans.Position.X;

        // Find the lowest falling ball that could threaten paddle miss
        float lowestFallingBallY = -9999.0f;
        float lowestFallingBallX = paddleTrans.Position.X;
        bool hasFallingBall = false;

        float lowestAnyBallY = -9999.0f;
        float lowestAnyBallX = paddleTrans.Position.X;
        bool hasAnyBall = false;

        if (mp_ballSystem)
        {
            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (m_registry.IsAlive(b) && m_registry.HasComponent<Transform2D>(b))
                {
                    const auto& bt = m_registry.GetComponent<Transform2D>(b);
                    if (bt.Position.Y > lowestAnyBallY)
                    {
                        lowestAnyBallY = bt.Position.Y;
                        lowestAnyBallX = bt.Position.X;
                        hasAnyBall = true;
                    }

                    if (m_registry.HasComponent<RigidBody>(b))
                    {
                        const auto& rb = m_registry.GetComponent<RigidBody>(b);
                        if (rb.Velocity.Y > 0.0f && bt.Position.Y > lowestFallingBallY)
                        {
                            lowestFallingBallY = bt.Position.Y;
                            lowestFallingBallX = bt.Position.X;
                            hasFallingBall = true;
                        }
                    }
                }
            }
        }

        if (!hasAnyBall && m_registry.IsAlive(m_ball) && m_registry.HasComponent<Transform2D>(m_ball))
        {
            const auto& bt = m_registry.GetComponent<Transform2D>(m_ball);
            lowestAnyBallY = bt.Position.Y;
            lowestAnyBallX = bt.Position.X;
            hasAnyBall = true;
            if (m_registry.HasComponent<RigidBody>(m_ball))
            {
                const auto& rb = m_registry.GetComponent<RigidBody>(m_ball);
                if (rb.Velocity.Y > 0.0f)
                {
                    lowestFallingBallY = bt.Position.Y;
                    lowestFallingBallX = bt.Position.X;
                    hasFallingBall = true;
                }
            }
        }

        // Ball preservation rule:
        // If a ball is falling towards paddle and getting below Y=60 (paddle is ~300),
        // we MUST prioritize protecting the ball!
        const bool isBallCritical = (hasFallingBall && lowestFallingBallY > 60.0f);

        // Find best falling power-up capsule
        Entity targetCapsule = NULL_ENTITY;
        float lowestCapsuleY = -9999.0f;
        m_registry.View<Transform2D, PowerUpComponent>([&](Entity e, const Transform2D& ct, const PowerUpComponent&)
        {
            if (ct.Position.Y > lowestCapsuleY && ct.Position.Y < paddleTrans.Position.Y + 30.0f)
            {
                lowestCapsuleY = ct.Position.Y;
                targetCapsule = e;
            }
        });

        if (!isBallCritical && targetCapsule != NULL_ENTITY && m_registry.IsAlive(targetCapsule) && m_registry.HasComponent<Transform2D>(targetCapsule))
        {
            // Catch falling power-up capsule while ball is safe!
            targetX = m_registry.GetComponent<Transform2D>(targetCapsule).Position.X;
        }
        else if (hasFallingBall)
        {
            targetX = lowestFallingBallX;

            // In Disco Ball preview: actively steer reflection towards unactivated big balls
            if (m_previewMode == PreviewMode::Ultimate && m_currentUltimate == UltimateAbilityType::DiscoBall)
            {
                for (const auto& db : m_discoBigBalls)
                {
                    if (!db.IsIndependent)
                    {
                        // Aim paddle so reflection angle directs ball towards db.Position.X
                        float offset = std::clamp((paddleTrans.Position.X - db.Position.X) * 0.18f, -40.0f, 40.0f);
                        targetX += offset;
                        break;
                    }
                }
            }
        }
        else if (hasAnyBall)
        {
            targetX = lowestAnyBallX;
        }

        // Fast & responsive paddle tracking
        float diff = targetX - paddleTrans.Position.X;
        float maxSpeed = 1150.0f;
        float step = std::clamp(diff, -maxSpeed * dt, maxSpeed * dt);
        paddleTrans.Position.X += step;
        paddleTrans.Position.X = std::clamp(paddleTrans.Position.X, -680.0f, 680.0f);
    }
}

void PreviewScene::OnInit(GameContext& context)
{
    m_musicEnabled = false;
    m_audioEnabled = false;
    GameScene::OnInit(context);

    if (!m_isEmbeddedMode)
    {
        m_musicEnabled = true;
        m_audioEnabled = true;
    }

    // Infinite lives & safe testing rules
    context.Rules.SetRule(Rule::Gameplay::InfiniteLives, true);
    m_lives = 99;

    // Isolate preview state machine: strictly Playing state without LevelTransition or Victory transitions
    mp_state_machine = std::make_unique<StateMachine<GameScene>>(this, 6);
    mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    mp_state_machine->SetState(static_cast<int>(SceneState::Playing));

    // Hide HUD counter in preview
    if (m_uiController)
    {
        Entity hudCanvas = m_uiController->GetMainUICanvas();
        if (hudCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(hudCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(hudCanvas).IsEnabled = false;
        }
    }

    if (s_hasExplicitInitialUltimate)
    {
        m_previewMode = PreviewMode::Ultimate;
        m_currentUltimate = s_initialUltimateType;
        m_previewLevel = s_initialUltimateLevel;
        s_hasExplicitInitialUltimate = false;
    }
    else if (s_hasExplicitInitialPowerUp)
    {
        m_previewMode = PreviewMode::PowerUp;
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
    else
    {
        m_currentItemIndex = 0;
        m_previewMode = PreviewMode::PowerUp;
    }

    if (m_previewMode == PreviewMode::PowerUp)
    {
        PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    }

    ResetPreview();

    if (mp_paddleSystem)
    {
        if (m_isEmbeddedMode)
        {
            mp_paddleSystem->SetInputEnabled(false);
        }
        mp_paddleSystem->SetOnBallLaunchedCallback([this](Entity)
        {
            m_ballState = BallState::Active;
            OnBallLaunched();
        });
    }

    if (m_isEmbeddedMode && m_registry.IsAlive(m_camera) && m_registry.HasComponent<Camera2D>(m_camera))
    {
        auto& cam = m_registry.GetComponent<Camera2D>(m_camera);
        cam.Position = Vector2f{0.0f, -20.0f};
        cam.Zoom = 1.35f;
    }
}

void PreviewScene::StartCountdown()
{
    m_countdownActive = false;
    m_countdownTimer = 0.0f;
    m_countdownGoTimer = 0.0f;
}

void PreviewScene::OnDestroy(GameContext& context)
{
    if (m_fireballBurnSfxId != 0 && mp_context)
    {
        mp_context->Audio.StopLoopingSfx(m_fireballBurnSfxId);
    }
    m_uiController.reset();
    mp_state_machine.reset();
    mp_levelGenerator.reset();
    m_collisionSub.Reset();
    m_powerUpSub.Reset();
    m_nextLevelSub.Reset();

    Scene::OnDestroy(context);
}

void PreviewScene::OnUpdate(float dt, GameContext& context)
{
    if (!m_isEmbeddedMode)
    {
        if (context.Input.IsKeyPress(KeyCode::Tab))
        {
            TogglePreviewMode();
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::Left))
        {
            PrevItem();
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::Right))
        {
            NextItem();
            return;
        }
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
        if (context.Input.IsKeyPress(KeyCode::R))
        {
            ResetPreview();
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::Escape) || context.Input.IsJoystickButtonPress(0, 6))
        {
            context.Scenes.LoadScene<MenuScene>();
            return;
        }
    }

    if (m_resetTransitionTimer > 0.0f)
    {
        m_resetTransitionTimer = std::max(0.0f, m_resetTransitionTimer - dt);
    }

    // USER REQUIREMENT:
    // PowerUpRain preview must refresh every 20s because it is too long
    if (m_previewMode == PreviewMode::Ultimate && m_currentUltimate == UltimateAbilityType::PowerUpRain)
    {
        m_powerUpRainPreviewTimer += dt;
        if (m_powerUpRainPreviewTimer >= 20.0f)
        {
            m_powerUpRainPreviewTimer = 0.0f;
            ResetPreview();
            return;
        }
    }

    // Magnet Paddle preview extra capsules
    if (m_previewMode == PreviewMode::PowerUp && kPreviewItems[m_currentItemIndex].Type == PowerUpType::MagnetPaddle &&
        m_ballState == BallState::Active && m_capsuleSpawnedForLaunch && m_magnetDuration > 0.0f)
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

    // Keep preview capsule horizontally aligned with paddle center for smooth collection
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

    m_lives = 99;

    // Sticky Paddle autopilot sweet-spot release
    if (m_isEmbeddedMode && !m_stuckBalls.empty())
    {
        for (auto& sb : m_stuckBalls)
        {
            if (sb.HoldTimer >= 1.05f)
            {
                sb.HoldTimer = PowerUpManager::Get().Sticky().AutoLaunchDelay;
            }
        }
    }

    // Update base gameplay
    GameScene::OnUpdate(dt, context);

    // Autopilot AI continuously plays the demonstration in both embedded and fullscreen mode
    UpdateAutopilot(dt);

    // Disco Ball preview: guide balls toward unactivated dancing big balls to unlock all instruments & Frenzy
    if (m_previewMode == PreviewMode::Ultimate && m_currentUltimate == UltimateAbilityType::DiscoBall)
    {
        const DiscoBigBall* targetDb = nullptr;
        float closestDistSq = 9999999.0f;
        for (const auto& db : m_discoBigBalls)
        {
            if (!db.IsIndependent)
            {
                float d = db.Position.X * db.Position.X + db.Position.Y * db.Position.Y;
                if (d < closestDistSq)
                {
                    closestDistSq = d;
                    targetDb = &db;
                }
            }
        }

        if (targetDb)
        {
            m_registry.View<BallComponent, Transform2D, RigidBody>([&](Entity, BallComponent&, const Transform2D& bt, RigidBody& rb)
            {
                if (rb.Velocity.Y < 0.0f) // ball moving upwards towards big balls
                {
                    Vector2f toTarget = targetDb->Position - bt.Position;
                    if (toTarget.Length() > 25.0f)
                    {
                        Vector2f desiredDir = toTarget.Normalized();
                        float currentSpeed = rb.Velocity.Length();
                        rb.Velocity = (rb.Velocity + desiredDir * (420.0f * dt)).Normalized() * currentSpeed;
                    }
                }
            });
        }
    }

    // Ball loss safety bounce in preview: ensure autopilot never loses the ball
    m_registry.View<BallComponent, Transform2D, RigidBody>([&](Entity, BallComponent&, Transform2D& bt, RigidBody& rb)
    {
        if (bt.Position.Y > 380.0f && rb.Velocity.Y > 0.0f)
        {
            rb.Velocity.Y = -std::abs(rb.Velocity.Y);
            bt.Position.Y = 378.0f;
        }
    });

    if (m_isEmbeddedMode)
    {
        if (m_registry.IsAlive(m_camera) && m_registry.HasComponent<Camera2D>(m_camera))
        {
            auto& cam = m_registry.GetComponent<Camera2D>(m_camera);
            cam.Position = Vector2f{0.0f, -20.0f};
            cam.Zoom = 1.35f;
            cam.Rotation = 0.0f;
        }
    }

    if (m_extraLifeTimer > 0.0f)
    {
        m_extraLifeTimer -= dt;
    }

    // USER REQUIREMENT:
    // If the playing AI manages to finish the preview level before the effect has finished,
    // we let the effect finish before resetting the scene.
    const bool isLevelFinished = (m_brickCount <= 0);
    const bool effectActive = IsEffectActive();

    if (isLevelFinished)
    {
        // Keep state machine in Playing so rendering, physics, and visual effects do not halt
        if (mp_state_machine)
        {
            int st = mp_state_machine->GetCurrentState();
            if (st != static_cast<int>(SceneState::Playing))
            {
                mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
            }
        }

        if (effectActive)
        {
            // Effect is still running: let it play out fully!
            m_autoResetTimer = 0.0f;
        }
        else
        {
            // Effect has finished after clearing the level: wait a short comfortable moment then reset cleanly
            m_autoResetTimer += dt;
            if (m_autoResetTimer >= 0.85f)
            {
                ResetPreview();
                return;
            }
        }
    }
    else
    {
        // Level not finished yet: normal auto-reset once the active power-up / ultimate effect expires
        bool hasActivated = (m_previewMode == PreviewMode::Ultimate) ? m_ultimateActivated : m_powerUpActivated;
        if (hasActivated)
        {
            if (!effectActive)
            {
                m_autoResetTimer += dt;
                if (m_autoResetTimer >= 0.70f)
                {
                    ResetPreview();
                    return;
                }
            }
            else
            {
                m_autoResetTimer = 0.0f;
            }
        }
    }

    // Ball loss check: only reset if no effect is currently running
    if (m_ballState == BallState::Active && mp_ballSystem)
    {
        if (mp_ballSystem->GetBallCount() == 0 && m_capsuleSpawnedForLaunch)
        {
            if (!effectActive)
            {
                m_autoResetTimer += dt;
                if (m_autoResetTimer >= 0.5f)
                {
                    ResetPreview();
                    return;
                }
            }
        }
        else if (mp_ballSystem->GetBallCount() == 1)
        {
            const auto& balls = mp_ballSystem->GetBalls();
            if (!balls.empty() && m_registry.IsAlive(balls.front()) && m_registry.HasComponent<Transform2D>(balls.front()))
            {
                if (m_ghostBallDuration <= 0.0f && !m_ghostBallPendingExpiry && m_registry.GetComponent<Transform2D>(balls.front()).Position.Y > 440.0f)
                {
                    if (!effectActive)
                    {
                        ResetPreview();
                        return;
                    }
                }
            }
        }
    }
}

bool PreviewScene::IsEffectActive()
{
    if (m_previewMode == PreviewMode::Ultimate)
    {
        return IsUltimateEffectActive();
    }
    return IsPowerUpEffectActive();
}

bool PreviewScene::IsUltimateEffectActive()
{
    switch (m_currentUltimate)
    {
    case UltimateAbilityType::PlasmaOrb:
        return !m_plasmaOrbs.empty() || (m_plasmaActiveTimer > 0.0f);

    case UltimateAbilityType::DiscoBall:
        return (m_discoTimer > 0.0f) || !m_discoBigBalls.empty() || !m_discoExplosionAnims.empty()
            || (m_discoGrandFinaleBannerTimer > 0.0f) || !m_soundWaves.empty();

    case UltimateAbilityType::PowerUpRain:
    {
        if (m_powerUpRainActiveTimer > 0.0f || !m_powerUpRainQueue.empty())
            return true;
        bool hasCapsules = false;
        m_registry.View<PowerUpComponent>([&](Entity, PowerUpComponent&) {
            hasCapsules = true;
        });
        if (hasCapsules) return true;
        return IsAnyPowerUpEffectActive();
    }
    default:
        return false;
    }
}

bool PreviewScene::IsAnyPowerUpEffectActive()
{
    if (m_bigBallDuration > 0.0f) return true;
    if (m_stormDuration > 0.0f || m_stormCharges > 0 || !m_activeLightningBolts.empty()) return true;
    if (m_stickyPaddleDuration > 0.0f || !m_stuckBalls.empty()) return true;
    if (m_bottomBarrierDuration > 0.0f && (m_bottomBarrierActive || m_bottomBarrierCharges > 0)) return true;
    if (m_magnetDuration > 0.0f) return true;
    if (m_ghostBallDuration > 0.0f || m_ghostBallPendingExpiry) return true;
    if (m_tempoBallDuration > 0.0f) return true;
    if (m_paddleSizeDuration > 0.0f) return true;
    if (mp_ballSystem && mp_ballSystem->GetBallCount() > 1) return true;

    if (mp_ballSystem)
    {
        for (Entity b : mp_ballSystem->GetBalls())
        {
            if (m_registry.IsAlive(b) && m_registry.HasComponent<BallComponent>(b))
            {
                if (m_registry.GetComponent<BallComponent>(b).IsFireBall) return true;
                if (m_registry.GetComponent<BallComponent>(b).IsGhost) return true;
            }
        }
    }
    return false;
}

bool PreviewScene::IsPowerUpEffectActive()
{
    PowerUpType type = kPreviewItems[m_currentItemIndex].Type;
    switch (type)
    {
        case PowerUpType::Storm:
            return (m_stormDuration > 0.0f) || (m_stormCharges > 0) || !m_activeLightningBolts.empty();

        case PowerUpType::FireBall:
        {
            if (mp_ballSystem)
            {
                const auto& balls = mp_ballSystem->GetBalls();
                for (Entity b : balls)
                {
                    if (m_registry.IsAlive(b) && m_registry.HasComponent<BallComponent>(b))
                    {
                        if (m_registry.GetComponent<BallComponent>(b).IsFireBall)
                            return true;
                    }
                }
            }
            return false;
        }

        case PowerUpType::BigBall:
            return m_bigBallDuration > 0.0f;

        case PowerUpType::MultiBall:
            return mp_ballSystem && mp_ballSystem->GetBallCount() > 1;

        case PowerUpType::StickyPaddle:
            return (m_stickyPaddleDuration > 0.0f) || !m_stuckBalls.empty();

        case PowerUpType::BottomBarrier:
            return (m_bottomBarrierDuration > 0.0f) && (m_bottomBarrierActive || m_bottomBarrierCharges > 0);

        case PowerUpType::MagnetPaddle:
            return m_magnetDuration > 0.0f;

        case PowerUpType::GhostBall:
        {
            if (m_ghostBallDuration > 0.0f || m_ghostBallPendingExpiry)
                return true;
            if (mp_ballSystem)
            {
                const auto& balls = mp_ballSystem->GetBalls();
                for (Entity b : balls)
                {
                    if (m_registry.IsAlive(b) && m_registry.HasComponent<BallComponent>(b))
                    {
                        if (m_registry.GetComponent<BallComponent>(b).IsGhost)
                            return true;
                    }
                }
            }
            return false;
        }

        case PowerUpType::TempoBall:
            return m_tempoBallDuration > 0.0f;

        case PowerUpType::ExpandPaddle:
        case PowerUpType::ShrinkPaddle:
            return m_paddleSizeDuration > 0.0f;

        case PowerUpType::ExtraLife:
            return m_extraLifeTimer > 0.0f;

        default:
            return false;
    }
}

void PreviewScene::OnRender(GameContext& context)
{
    m_countdownActive = false;
    m_countdownTimer = 0.0f;
    m_countdownGoTimer = 0.0f;

    GameScene::OnRender(context);

    if (m_isEmbeddedMode)
    {
        return;
    }

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float bannerW = 1040.0f;
    const float bannerH = 110.0f;
    const Vector2f bannerPos{viewSize.X * 0.5f, 65.0f};

    context.Render.DrawRectangle(bannerW, bannerH, Transform2D{bannerPos}, Color{12, 16, 28, 230});
    context.Render.DrawRectangleOutline(bannerW, bannerH, Transform2D{bannerPos}, Color{45, 95, 175, 230}, 2.5f);

    constexpr float cw = 18.0f;
    float halfW = bannerW * 0.5f;
    float halfH = bannerH * 0.5f;
    Color bracketCol{255, 215, 60, 240};
    context.Render.DrawLine(Vector2f{bannerPos.X - halfW, bannerPos.Y - halfH}, Vector2f{bannerPos.X - halfW + cw, bannerPos.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X - halfW, bannerPos.Y - halfH}, Vector2f{bannerPos.X - halfW, bannerPos.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X + halfW, bannerPos.Y - halfH}, Vector2f{bannerPos.X + halfW - cw, bannerPos.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X + halfW, bannerPos.Y - halfH}, Vector2f{bannerPos.X + halfW, bannerPos.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X - halfW, bannerPos.Y + halfH}, Vector2f{bannerPos.X - halfW + cw, bannerPos.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X - halfW, bannerPos.Y + halfH}, Vector2f{bannerPos.X - halfW, bannerPos.Y + halfH - cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X + halfW, bannerPos.Y + halfH}, Vector2f{bannerPos.X + halfW - cw, bannerPos.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{bannerPos.X + halfW, bannerPos.Y + halfH}, Vector2f{bannerPos.X + halfW, bannerPos.Y + halfH - cw}, bracketCol, 2.5f);

    if (m_previewMode == PreviewMode::Ultimate)
    {
        std::string ultName = ProfileManager::GetUltimateName(m_currentUltimate);
        std::string ultCat = ProfileManager::GetUltimateCategory(m_currentUltimate);
        std::string tierInfo = ProfileManager::GetUltimateTierInfo(m_currentUltimate, m_previewLevel);

        std::string title = "[ PREVIEW ULTIME ]  " + ultName + "  -  NIV. " + std::to_string(m_previewLevel) + " / 5";
        std::string sub = ultCat + "  |  " + tierInfo;
        std::string controls = "[GAUCHE/DROITE] CHANGER ULTIME  |  [HAUT/BAS] NIVEAU  |  [TAB] MODE POWER-UPS  |  [R] RESET  |  [ESC] RETOUR";

        context.Render.DrawTextCentered(title, m_fontId, 28.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y - 26.0f}}, Colors::Gold);
        context.Render.DrawTextCentered(sub, m_fontId, 20.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y + 6.0f}}, Color{120, 210, 255, 235});
        context.Render.DrawTextCentered(controls, m_fontId, 16.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y + 34.0f}}, Color{180, 200, 230, 210});
    }
    else
    {
        const auto& item = kPreviewItems[m_currentItemIndex];
        int maxLvl = PowerUpManager::Get().GetMaxLevel(item.Type);
        std::string title = "[ PREVIEW POWER-UP ]  " + std::string(item.Name) + "  -  NIV. " + std::to_string(m_previewLevel) + " / " + std::to_string(maxLvl);
        std::string controls = "[GAUCHE/DROITE] CHANGER POWER-UP  |  [HAUT/BAS] NIVEAU  |  [TAB] MODE ULTIMES  |  [R] RESET  |  [ESC] RETOUR";

        context.Render.DrawTextCentered(title, m_fontId, 28.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y - 20.0f}}, item.CapsuleColor);
        context.Render.DrawTextCentered(controls, m_fontId, 17.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y + 22.0f}}, Color{180, 200, 230, 210});
    }

    if (m_brickCount <= 0 && IsEffectActive())
    {
        float pulse = 0.5f + 0.5f * std::sin(m_gameplayTime * 8.0f);
        uint8_t a = static_cast<uint8_t>(180 + pulse * 75);
        context.Render.DrawTextCentered("[ NIVEAU TERMINE - FINALISATION DE L'EFFET ... ]", m_fontId, 24.0f,
            Transform2D{Vector2f{viewSize.X * 0.5f, bannerPos.Y + 80.0f}}, Color{255, 225, 60, a});
    }
}

void PreviewScene::NextPowerUp()
{
    NextItem();
}

void PreviewScene::PrevPowerUp()
{
    PrevItem();
}

void PreviewScene::NextItem()
{
    if (m_previewMode == PreviewMode::PowerUp)
    {
        m_currentItemIndex = (m_currentItemIndex + 1) % kPreviewItemCount;
        m_previewLevel = 1;
        PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    }
    else
    {
        int next = (static_cast<int>(m_currentUltimate) + 1) % 3;
        m_currentUltimate = static_cast<UltimateAbilityType>(next);
        m_previewLevel = ProfileManager::Get().GetUltimateLevel(m_currentUltimate);
    }
    if (mp_context && m_bounceSfxId != 0 && !m_isEmbeddedMode)
    {
        mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
    }
    ResetPreview();
}

void PreviewScene::PrevItem()
{
    if (m_previewMode == PreviewMode::PowerUp)
    {
        m_currentItemIndex = (m_currentItemIndex + kPreviewItemCount - 1) % kPreviewItemCount;
        m_previewLevel = 1;
        PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, 1);
    }
    else
    {
        int prev = (static_cast<int>(m_currentUltimate) + 2) % 3;
        m_currentUltimate = static_cast<UltimateAbilityType>(prev);
        m_previewLevel = ProfileManager::Get().GetUltimateLevel(m_currentUltimate);
    }
    if (mp_context && m_bounceSfxId != 0 && !m_isEmbeddedMode)
    {
        mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
    }
    ResetPreview();
}

void PreviewScene::LevelUp()
{
    int maxLevel = 5;
    if (m_previewMode == PreviewMode::PowerUp)
    {
        maxLevel = PowerUpManager::Get().GetMaxLevel(kPreviewItems[m_currentItemIndex].Type);
    }
    else
    {
        maxLevel = ProfileManager::Get().GetUltimateMaxLevel(m_currentUltimate);
    }

    if (m_previewLevel < maxLevel)
    {
        m_previewLevel++;
        if (m_previewMode == PreviewMode::PowerUp)
        {
            PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, m_previewLevel);
        }
        if (mp_context && m_bounceSfxId != 0 && !m_isEmbeddedMode)
        {
            mp_context->Audio.PlaySfx(m_bounceSfxId, 95.0f, 1.25f);
        }
        ResetPreview();
    }
}

void PreviewScene::LevelDown()
{
    if (m_previewLevel > 1)
    {
        m_previewLevel--;
        if (m_previewMode == PreviewMode::PowerUp)
        {
            PowerUpManager::Get().SetLevel(kPreviewItems[m_currentItemIndex].Type, m_previewLevel);
        }
        if (mp_context && m_bounceSfxId != 0 && !m_isEmbeddedMode)
        {
            mp_context->Audio.PlaySfx(m_bounceSfxId, 80.0f, 0.85f);
        }
        ResetPreview();
    }
}

void PreviewScene::TryTriggerUltimate()
{
    if (m_previewMode == PreviewMode::Ultimate)
    {
        m_ultimateActivated = true;
        switch (m_currentUltimate)
        {
        case UltimateAbilityType::PlasmaOrb:
            ExecutePlasmaOrb(m_previewLevel);
            break;
        case UltimateAbilityType::DiscoBall:
            ExecuteDiscoBall(m_previewLevel);
            break;
        case UltimateAbilityType::PowerUpRain:
            ExecutePowerUpRain(m_previewLevel);
            break;
        default:
            break;
        }
    }
    m_ultimateCharge = 0.0f;
    m_ultimateReady = false;
}

void PreviewScene::ResetPreview()
{
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

    ClearActiveUltimates();

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
    m_isShrinkPaddle = false;
    UpdatePaddleSpeed();

    m_powerUpActivated = false;
    m_ultimateActivated = false;
    m_autoResetTimer = 0.0f;
    m_extraLifeTimer = 0.0f;
    m_powerUpRainPreviewTimer = 0.0f;
    m_resetTransitionTimer = 0.38f;
    if (m_onResetCallback)
    {
        m_onResetCallback();
    }
    m_scoreManager.Reset();

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

    if (mp_ballSystem)
    {
        mp_ballSystem->ClearBalls();
        mp_ballSystem->ClearLasers();
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->DetachBall();
    }

    m_registry.View<PowerUpComponent>([this](Entity e, PowerUpComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_previewCapsule = NULL_ENTITY;

    std::string levelPath;
    if (m_previewMode == PreviewMode::Ultimate)
    {
        switch (m_currentUltimate)
        {
        case UltimateAbilityType::PlasmaOrb:
            m_levelManager.SetLevel(1);
            ApplyLevelTheme(2);
            levelPath = "Resources/levels/preview_plasma.txt";
            break;
        case UltimateAbilityType::DiscoBall:
            m_levelManager.SetLevel(4);
            ApplyLevelTheme(5);
            levelPath = "Resources/levels/preview_disco.txt";
            break;
        case UltimateAbilityType::PowerUpRain:
            m_levelManager.SetLevel(0);
            ApplyLevelTheme(1);
            levelPath = "Resources/levels/preview_poweruprain.txt";
            break;
        default:
            levelPath = "Resources/levels/level01.txt";
            break;
        }
    }
    else
    {
        PowerUpType curType = kPreviewItems[m_currentItemIndex].Type;
        if (curType == PowerUpType::Storm)
        {
            m_levelManager.SetLevel(6);
            ApplyLevelTheme(7);
            const std::string stormLevel = "Resources/levels/preview_storm.txt";
            std::error_code ec;
            levelPath = std::filesystem::exists(stormLevel, ec) ? stormLevel : m_levelManager.GetCurrentLevelPath();
        }
        else if (curType == PowerUpType::FireBall)
        {
            m_levelManager.SetLevel(2);
            ApplyLevelTheme(3);
            const std::string fireballLevel = "Resources/levels/preview_fireball.txt";
            std::error_code ec;
            levelPath = std::filesystem::exists(fireballLevel, ec) ? fireballLevel : m_levelManager.GetCurrentLevelPath();
        }
        else if (curType == PowerUpType::GhostBall)
        {
            m_levelManager.SetLevel(3);
            ApplyLevelTheme(4);
            const std::string ghostLevel = "Resources/levels/preview_ghost.txt";
            std::error_code ec;
            levelPath = std::filesystem::exists(ghostLevel, ec) ? ghostLevel : m_levelManager.GetCurrentLevelPath();
        }
        else
        {
            m_levelManager.SetLevel(0);
            ApplyLevelTheme(1);
            levelPath = m_levelManager.GetCurrentLevelPath();
        }
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
    GameScene::SpawnPowerUp(position);
}

void PreviewScene::ApplyPowerUp(PowerUpType type)
{
    GameScene::ApplyPowerUp(type);

    if (m_previewMode == PreviewMode::PowerUp && type == kPreviewItems[m_currentItemIndex].Type)
    {
        m_powerUpActivated = true;
        if (type == PowerUpType::ExtraLife)
        {
            m_extraLifeTimer = 2.0f;
        }
    }
}

void PreviewScene::SpawnMagnetDemoCapsule()
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<Transform2D>(m_paddle)) return;

    const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    float offsetDir = (rand() % 2 == 0) ? -1.0f : 1.0f;
    float offsetX = offsetDir * (130.0f + static_cast<float>(rand() % 250));
    float spawnX = std::clamp(paddleTrans.Position.X + offsetX, -650.0f, 650.0f);
    float spawnY = -270.0f - static_cast<float>(rand() % 70);

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

    if (m_previewMode == PreviewMode::PowerUp)
    {
        SpawnPreviewCapsule();
    }
    else if (m_previewMode == PreviewMode::Ultimate)
    {
        m_ultimateActivated = true;
        switch (m_currentUltimate)
        {
        case UltimateAbilityType::PlasmaOrb:
            ExecutePlasmaOrb(m_previewLevel);
            break;
        case UltimateAbilityType::DiscoBall:
            ExecuteDiscoBall(m_previewLevel);
            break;
        case UltimateAbilityType::PowerUpRain:
            ExecutePowerUpRain(m_previewLevel);
            break;
        default:
            break;
        }
    }
}
