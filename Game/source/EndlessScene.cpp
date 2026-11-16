#include "Scenes/EndlessScene.h"
#include "Core/ProfileManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/HazardZoneComponent.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include "PowerUps/PowerUpConfig.h"
#include "AudioMixer.h"
#include "LevelTheme.h"
#include "StateMachine/Transition.h"
#include "StateMachine/Condition.h"
#include "Conditions/GameConditions.h"
#include "Actions/GameOverAction.h"
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <cmath>

EndlessScene::EndlessScene()
    : m_survivalTime(0.0f),
      m_wavesCleared(0),
      m_endlessScoreMultiplier(1.0f),
      m_nextMinuteDraftTime(60.0f),
      m_consecutiveWavesForShield(0),
      m_ballDamageBonus(0),
      m_lightningRodActive(false),
      m_plasmaFieldActive(false),
      m_magneticTetherActive(false),
      m_paddleWidthMultiplier(1.0f),
      m_paddleSpeedMultiplier(1.0f),
      m_basePaddleWidth(160.0f),
      m_basePaddleSpeed(900.0f),
      m_draftActive(false),
      m_activeDraftType(DraftType::PeriodicSupply),
      m_remainingRerolls(0),
      m_selectedDraftIndex(0),
      m_draftAnimProgress(0.0f),
      m_waveClearBannerActive(false),
      m_waveClearBannerTimer(0.0f),
      m_justClearedWaveNumber(1),
      m_emergencyBurnCooldown(0.0f)
{
}

void EndlessScene::OnInit(GameContext& context)
{
    // 1. Initialize base GameScene
    GameScene::OnInit(context);

    // 2. Initialize Workshop Endless Perks
    int rerollPerk = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls);
    m_remainingRerolls = rerollPerk;

    int scorePerk = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ScoreMultiplier);
    m_endlessScoreMultiplier = 1.0f + static_cast<float>(scorePerk) * 0.10f;

    // Always restart new games with 3 red hearts
    m_lives = 3;
    context.Events.Publish(LivesChangedEvent(m_lives));

    int shieldPerk = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ShieldRecovery);
    if (shieldPerk > 0)
    {
        ApplyPowerUp(PowerUpType::BottomBarrier);
    }

    // 3. Clear any leftover campaign bricks and old hazard zones
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
    ClearHazardZones();

    // 4. Setup Procedural Generator for Endless Mode with randomized seed
    m_endlessGenerator.Reset(0);
    m_draftRndSeed = m_endlessGenerator.GetSeed() ^ 0x5DEECE66u;
    m_brickCount = m_endlessGenerator.Generate(m_registry, context, m_brickTexId);
    ApplyWaveTheme(1);

    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetLaunchSpeed(700.0f);
    }

    // 5. Calibrate Dynamic Event System for Endless progression
    if (mp_eventSystem)
    {
        mp_eventSystem->ResetLifecycle();
        mp_eventSystem->SetIntervalRange(35.0f, 50.0f);
    }

    // 6. Spawn initial wave hazard zones if any (with diverse effects)
    for (const auto& hazard : m_endlessGenerator.GetWaveHazards())
    {
        SpawnHazardZone(hazard.Position, hazard.Size, hazard.Effect, 2.0f);
    }

    m_survivalTime = 0.0f;
    m_wavesCleared = 0;
    m_nextMinuteDraftTime = 60.0f;
    m_draftActive = false;
    m_activeDraftType = DraftType::PeriodicSupply;

    // 7. Reconfigure State Machine specifically for Endless Mode:
    // Endless mode runs continuously: NO LevelCleared or Victory transitions!
    class EndlessPauseToCountdownCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            if (!pOwner || !pOwner->CanUnpause()) return false;
            if (!pOwner->IsCountdownActive()) return false;
            return pOwner->GetContext()->Input.IsActionTriggered(InputAction::Pause) ||
                   pOwner->GetContext()->Input.IsKeyPress(KeyCode::Escape) ||
                   pOwner->GetContext()->Input.IsJoystickButtonPress(0, 1);
        }
    };

    class EndlessPauseToPlayCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            if (!pOwner || !pOwner->CanUnpause()) return false;
            if (pOwner->IsCountdownActive()) return false;
            return pOwner->GetContext()->Input.IsActionTriggered(InputAction::Pause) ||
                   pOwner->GetContext()->Input.IsKeyPress(KeyCode::Escape) ||
                   pOwner->GetContext()->Input.IsJoystickButtonPress(0, 1);
        }
    };

    class EndlessCountdownCompleteCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            return pOwner && pOwner->IsCountdownComplete();
        }
    };

    class EndlessPlayToPauseCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            if (!pOwner) return false;
            return pOwner->GetContext()->Input.IsActionTriggered(InputAction::Pause) ||
                   pOwner->GetContext()->Input.IsKeyPress(KeyCode::Escape);
        }
    };

    mp_state_machine = std::make_unique<StateMachine<GameScene>>(this, 6);

    State<GameScene>* countdownState = mp_state_machine->CreateState(static_cast<int>(SceneState::Countdown));
    countdownState->AddTransition(new Transition<GameScene>(new EndlessCountdownCompleteCondition(), static_cast<int>(SceneState::Playing)));
    countdownState->AddTransition(new Transition<GameScene>(new EndlessPlayToPauseCondition(), static_cast<int>(SceneState::Paused)));

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new EndlessPlayToPauseCondition(), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new EndlessPauseToCountdownCondition(), static_cast<int>(SceneState::Countdown)));
    pauseState->AddTransition(new Transition<GameScene>(new EndlessPauseToPlayCondition(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new GameOverAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    StartCountdown();
}

void EndlessScene::OnDestroy(GameContext& context)
{
    ClearHazardZones();
    GameScene::OnDestroy(context);
}

void EndlessScene::ClearHazardZones()
{
    m_registry.View<HazardZoneComponent>([this](Entity e, HazardZoneComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
}

void EndlessScene::ApplyWaveTheme(int wave)
{
    m_currentTheme = LevelThemeCatalog::GetThemeForLevel(wave);
    m_ambientBackground.SetTheme(m_currentTheme);
    m_postProcessParams.VignetteColor = m_currentTheme.VignetteBaseColor;
    m_postProcessParams.VignetteIntensity = m_currentTheme.VignetteBaseIntensity;
    m_bgColor = m_currentTheme.BgBase1;
}

std::string EndlessScene::GetSurvivalTimeFormatted() const
{
    int minutes = static_cast<int>(m_survivalTime) / 60;
    int seconds = static_cast<int>(m_survivalTime) % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    return std::string(buffer);
}

void EndlessScene::TriggerDraft(DraftType type)
{
    m_draftActive = true;
    m_activeDraftType = type;
    m_selectedDraftIndex = 0;
    m_draftAnimProgress = 0.0f;
    m_draftTimer = kDraftMaxDuration;
    m_draftRndSeed = static_cast<uint32_t>(m_survivalTime * 1000.0f) + static_cast<uint32_t>(m_wavesCleared * 100);

    // Cancel visual maluses & distortions during card draft so panel is clean and readable
    m_screenGlitchDuration = 0.0f;
    m_cameraZoomPulseDuration = 0.0f;
    m_cameraTiltDuration = 0.0f;
    m_postProcessParams.GlitchIntensity = 0.0f;
    m_postProcessParams.FlashIntensity = 0.0f;
    m_postProcessParams.ChromaticAberration = 0.002f;

    m_currentDraftCards = RogueliteDraftManager::GenerateDraft(3, type, m_draftRndSeed);

    if (m_audioEnabled && m_levelClearSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_levelClearSfxId, 80.0f);
    }
}

void EndlessScene::RerollDraft()
{
    if (m_remainingRerolls <= 0) return;
    --m_remainingRerolls;
    m_draftRndSeed += 1337;
    m_currentDraftCards = RogueliteDraftManager::GenerateDraft(3, m_activeDraftType, m_draftRndSeed);

    if (m_audioEnabled && m_countdownReadySfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_countdownReadySfxId, 60.0f);
    }
}

void EndlessScene::SelectDraftCard(int index)
{
    if (index < 0 || index >= static_cast<int>(m_currentDraftCards.size())) return;

    const auto& card = m_currentDraftCards[index];

    ApplyPerk(card.Type);

    if (m_audioEnabled && m_scoreRecordedSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_scoreRecordedSfxId, 70.0f);
    }

    m_draftActive = false;
}

void EndlessScene::ApplyPerk(RoguelitePerkType type)
{
    switch (type)
    {
    case RoguelitePerkType::TitanPaddle:
        MultiplyPaddleWidth(1.20f);
        break;
    case RoguelitePerkType::HyperDrive:
        MultiplyPaddleSpeed(1.15f);
        break;
    case RoguelitePerkType::ReinforcedHull:
        ApplyPowerUp(PowerUpType::ExtraLife);
        break;
    case RoguelitePerkType::KineticCore:
        AddScoreMultiplier(0.25f);
        ApplyPowerUp(PowerUpType::TempoBall);
        break;
    case RoguelitePerkType::HeavyImpact:
        AddBallDamageBonus(1);
        break;
    case RoguelitePerkType::ThermalRounds:
        ApplyPowerUp(PowerUpType::FireBall);
        break;
    case RoguelitePerkType::LightningRod:
        SetLightningRod(true);
        break;
    case RoguelitePerkType::SplitStream:
        ApplyPowerUp(PowerUpType::MultiBall);
        break;
    case RoguelitePerkType::PlasmaField:
        SetPlasmaField(true);
        break;
    case RoguelitePerkType::MagneticTether:
        ApplyPowerUp(PowerUpType::MagnetPaddle);
        SetMagneticTether(true);
        break;
    case RoguelitePerkType::EmergencyShield:
        ApplyPowerUp(PowerUpType::BottomBarrier);
        break;
    case RoguelitePerkType::InstantOvercharge:
        SetUltimateCharge(100.0f);
        break;
    case RoguelitePerkType::JackpotCredits:
        PowerUpManager::Get().AddCoins(250);
        ProfileManager::Get().AddLifetimeCredits(250);
        break;
    case RoguelitePerkType::ScoreFrenzy:
        AddScoreMultiplier(0.25f);
        break;
    case RoguelitePerkType::ChronoStasis:
        m_endlessGenerator.MultiplyCreepInterval(1.30f);
        ApplyPowerUp(PowerUpType::TempoBall);
        break;
    case RoguelitePerkType::OverdriveCapacitor:
        SetOverdriveCapacitor(true);
        break;
    case RoguelitePerkType::ShieldSiphon:
        SetShieldSiphon(true);
        break;
    case RoguelitePerkType::VortexPulse:
        TriggerVortexPulse();
        break;
    default:
        break;
    }
}

void EndlessScene::TriggerVortexPulse()
{
    ClearHazardZones();
    TriggerPostProcessFlash(0.04f, 0.45f, Color{0, 200, 255, 255}, 0.2f, Color{100, 230, 255, 255});
    if (m_audioEnabled && m_bounceSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f, 1.25f);
    }
}

void EndlessScene::MultiplyPaddleWidth(float factor)
{
    m_paddleWidthMultiplier *= factor;
    if (m_paddle != NULL_ENTITY && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        auto& t = m_registry.GetComponent<Transform2D>(m_paddle);
        t.Scale.X = m_paddleWidthMultiplier;
    }
}

void EndlessScene::MultiplyPaddleSpeed(float factor)
{
    m_paddleSpeedMultiplier *= factor;
    UpdatePaddleSpeed();
}

void EndlessScene::OnUpdate(float dt, GameContext& context)
{
    const int currentState = mp_state_machine ? mp_state_machine->GetCurrentState() : static_cast<int>(SceneState::Playing);
    if (currentState != static_cast<int>(SceneState::Playing))
    {
        // When Paused, Countdown, or GameOver: update base GameScene (menus, UI controller) and return
        GameScene::OnUpdate(dt, context);
        return;
    }

    // 1. Handle Draft Modal Interactivity (Pauses physical action & limits choice time to 10s max)
    if (m_draftActive)
    {
        // Check for pause request while in draft modal
        if (context.Input.IsActionTriggered(InputAction::Pause) || context.Input.IsKeyPress(KeyCode::Escape))
        {
            if (mp_state_machine)
            {
                mp_state_machine->SetState(static_cast<int>(SceneState::Paused));
                return;
            }
        }

        // 10s max timer to select boost
        m_draftTimer -= dt;
        if (m_draftTimer <= 0.0f)
        {
            m_draftTimer = 0.0f;
            SelectDraftCard(m_selectedDraftIndex);
            return;
        }

        m_draftAnimProgress = std::min(1.0f, m_draftAnimProgress + 8.0f * dt);

        // Keyboard & Gamepad Selection
        if (context.Input.IsKeyPress(KeyCode::Left) || context.Input.IsKeyPress(KeyCode::A) || context.Input.IsMenuLeftTriggered())
        {
            m_selectedDraftIndex = (m_selectedDraftIndex > 0) ? (m_selectedDraftIndex - 1) : 2;
            if (m_bounceSfxId != 0 && m_audioEnabled) PlaySfx(m_bounceSfxId, 25.0f, 1.4f);
        }
        else if (context.Input.IsKeyPress(KeyCode::Right) || context.Input.IsKeyPress(KeyCode::D) || context.Input.IsMenuRightTriggered())
        {
            m_selectedDraftIndex = (m_selectedDraftIndex + 1) % 3;
            if (m_bounceSfxId != 0 && m_audioEnabled) PlaySfx(m_bounceSfxId, 25.0f, 1.4f);
        }
        else if (context.Input.IsKeyPress(KeyCode::Num1) || context.Input.IsKeyPress(KeyCode::Numpad1))
        {
            SelectDraftCard(0);
            return;
        }
        else if (context.Input.IsKeyPress(KeyCode::Num2) || context.Input.IsKeyPress(KeyCode::Numpad2))
        {
            SelectDraftCard(1);
            return;
        }
        else if (context.Input.IsKeyPress(KeyCode::Num3) || context.Input.IsKeyPress(KeyCode::Numpad3))
        {
            SelectDraftCard(2);
            return;
        }
        else if (context.Input.IsKeyPress(KeyCode::Enter) || context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsMenuSelectTriggered())
        {
            SelectDraftCard(m_selectedDraftIndex);
            return;
        }
        else if (context.Input.IsKeyPress(KeyCode::R))
        {
            RerollDraft();
        }

        // Mouse click navigation in screen-space
        const auto mousePosPair = context.Input.GetMousePosition();
        const Vector2f mousePos{mousePosPair.first, mousePosPair.second};

        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const float screenCenterX = viewSize.X * 0.5f;
        const float screenCenterY = viewSize.Y * 0.5f;

        const float cardWidth = 340.0f;
        const float cardHeight = 430.0f;
        const float cardSpacing = 370.0f;
        const float cardCenterY = screenCenterY + 15.0f;

        const bool isMouseClicked = context.Input.IsMouseButtonPress(MouseButton::Left) || context.Input.IsMouseButtonRelease(MouseButton::Left);

        for (int i = 0; i < 3; ++i)
        {
            float cx = screenCenterX + static_cast<float>(i - 1) * cardSpacing;
            float cy = cardCenterY;

            if (std::abs(mousePos.X - cx) <= cardWidth * 0.5f &&
                std::abs(mousePos.Y - cy) <= cardHeight * 0.5f)
            {
                if (m_selectedDraftIndex != i)
                {
                    m_selectedDraftIndex = i;
                    if (m_bounceSfxId != 0 && m_audioEnabled) PlaySfx(m_bounceSfxId, 20.0f, 1.5f);
                }

                if (isMouseClicked)
                {
                    SelectDraftCard(i);
                    return;
                }
            }
        }

        // Check Reroll button click area
        const float rerollBtnY = cardCenterY + cardHeight * 0.5f + 40.0f;
        if (std::abs(mousePos.X - screenCenterX) <= 150.0f &&
            std::abs(mousePos.Y - rerollBtnY) <= 25.0f && isMouseClicked)
        {
            RerollDraft();
        }

        return; // STRICTLY FREEZE GAMEPLAY & PHYSICS DURING DRAFT! Player CANNOT lose!
    }

    // 2. Base GameScene Update (handles physics, balls, paddle, collisions during active gameplay)
    GameScene::OnUpdate(dt, context);

    // 3. Anti-AFK Check: Only advance wave survival timer if ball is in flight
    const bool isBallAttached = (mp_paddleSystem && mp_paddleSystem->IsBallAttached());
    if (!isBallAttached)
    {
        m_survivalTime += dt;
        m_endlessGenerator.Update(dt, m_registry, context);
    }

    // 3. Wave Clear Banner Animation
    if (m_waveClearBannerActive)
    {
        m_waveClearBannerTimer -= dt;
        if (m_waveClearBannerTimer <= 0.0f)
        {
            m_waveClearBannerActive = false;
        }
    }

    // 4. Emergency Defense Burn (Triggered automatically or when danger is critical)
    if (m_emergencyBurnCooldown > 0.0f)
    {
        m_emergencyBurnCooldown -= dt;
    }
    if (m_endlessGenerator.IsDangerActive() && m_emergencyBurnCooldown <= 0.0f)
    {
        if (m_endlessGenerator.CheckEmergencyBurn(m_registry, 300.0f))
        {
            m_emergencyBurnCooldown = 8.0f;
            TriggerPostProcessFlash(0.04f, 0.6f, Colors::Red, 0.4f, Colors::Orange);
            if (m_bounceSfxId != 0 && m_audioEnabled) PlaySfx(m_bounceSfxId, 100.0f, 0.5f);
        }
    }

    // 5. Dynamic Intra-Wave Hazard Injection at 50% wave progress
    if (m_endlessGenerator.GetWaveTimeProgress() >= 0.5f && !m_midWaveHazardSpawned)
    {
        m_midWaveHazardSpawned = true;
        float spawnX = static_cast<float>((m_wavesCleared * 137) % 600) - 300.0f;
        SpawnHazardZone(Vector2f{spawnX, -30.0f}, Vector2f{200.0f, 60.0f}, HazardEffectType::SpeedSurge, 2.5f);
        TriggerPostProcessFlash(0.02f, 0.35f, Color{0, 200, 255, 255}, 0.15f, Color{100, 230, 255, 255});
    }

    // 6. Continuous Dynamic Ball Pacing:
    // Prevent slow sluggish balls and boring idle loops during Endless play
    m_registry.View<RigidBody, BallComponent>([](Entity, RigidBody& rb, BallComponent&) {
        float speedSq = rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y;
        if (speedSq < 600.0f * 600.0f && speedSq > 1.0f)
        {
            float speed = std::sqrt(speedSq);
            rb.Velocity = (rb.Velocity / speed) * 640.0f;
        }
    });

    // Climax Rush in final 15% of wave time:
    // Extra acceleration for high-tension climax sprint
    if (m_endlessGenerator.IsWaveClimaxActive())
    {
        m_registry.View<RigidBody, BallComponent>([](Entity, RigidBody& rb, BallComponent&) {
            float speedSq = rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y;
            if (speedSq < 720.0f * 720.0f && speedSq > 1.0f)
            {
                float speed = std::sqrt(speedSq);
                rb.Velocity = (rb.Velocity / speed) * 740.0f;
            }
        });
    }

    // 7. Check Wave Progression (Quotas & Grid clears)
    CheckWaveClear(context);

    // 8. Check Periodic 1-Minute Survival Supply Draft
    if (m_survivalTime >= m_nextMinuteDraftTime && !m_draftActive)
    {
        m_nextMinuteDraftTime += 60.0f;
        TriggerDraft(DraftType::PeriodicSupply);
    }

    // 9. Dynamic Event Scaling with Wave Level
    if (mp_eventSystem)
    {
        float minInterval = std::max(16.0f, 35.0f - static_cast<float>(m_wavesCleared) * 1.5f);
        float maxInterval = minInterval + 12.0f;
        mp_eventSystem->SetIntervalRange(minInterval, maxInterval);
    }
}

void EndlessScene::CheckWaveClear(GameContext& context)
{
    if (m_endlessGenerator.IsWaveTimeElapsed() && !m_draftActive)
    {
        ++m_wavesCleared;
        m_justClearedWaveNumber = m_wavesCleared;
        m_waveClearBannerActive = true;
        m_waveClearBannerTimer = 2.5f;
        m_midWaveHazardSpawned = false;

        // Wave clear score bonus
        uint32_t bonus = static_cast<uint32_t>(1000.0f * static_cast<float>(m_wavesCleared) * m_endlessScoreMultiplier);
        m_scoreManager.AddScore(bonus);
        context.Events.Publish(ScoreChangedEvent(m_scoreManager.GetScore(), m_scoreManager.GetComboMultiplier()));

        if (m_levelClearSfxId != 0 && m_audioEnabled)
        {
            PlaySfx(m_levelClearSfxId, 90.0f);
        }

        // Clean up remaining bricks from current wave before generating new wave
        m_registry.View<BrickComponent>([this](Entity e, BrickComponent&) {
            m_registry.DestroyEntityDeferred(e);
        });
        m_registry.ProcessDeferredCommands();

        // 1. CLEAR OLD HAZARD ZONES! (Guarantees zones never accumulate)
        ClearHazardZones();

        // 2. Advance to next procedural wave
        m_endlessGenerator.AdvanceWave(m_registry, m_brickTexId);
        m_brickCount = m_endlessGenerator.GetBreakableBricksRemaining();

        // 3. Shift visual lighting / theme for new wave
        ApplyWaveTheme(m_endlessGenerator.GetCurrentWave());

        // 4. Spawn wave hazard zones with distinct effects!
        for (const auto& hazard : m_endlessGenerator.GetWaveHazards())
        {
            SpawnHazardZone(hazard.Position, hazard.Size, hazard.Effect, 2.0f);
        }

        // Periodic Shield Recovery perk check
        int shieldPerk = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ShieldRecovery);
        if (shieldPerk > 0)
        {
            int interval = std::max(1, 6 - shieldPerk);
            if (m_wavesCleared % interval == 0)
            {
                ApplyPowerUp(PowerUpType::BottomBarrier);
            }
        }

        // User request: Don't give a draft on every wave! Only every 3 waves (Milestones: Wave 3, 6, 9...)
        if (m_wavesCleared % 3 == 0)
        {
            TriggerDraft(DraftType::WaveClear);
        }
    }
}

void EndlessScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    if (entity == NULL_ENTITY || !m_registry.IsAlive(entity) || !m_registry.HasComponent<BrickComponent>(entity))
    {
        return;
    }

    if (m_isHandlingCollision)
    {
        GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, damage);
        if (m_registry.IsPendingDestroy(entity))
        {
            m_endlessGenerator.OnBrickDestroyed();
        }
        return;
    }

    struct CollisionGuard {
        bool& flag;
        CollisionGuard(bool& f) : flag(f) { flag = true; }
        ~CollisionGuard() { flag = false; }
    } guard(m_isHandlingCollision);

    int finalDamage = damage;
    if (ballEntity != NULL_ENTITY && !isLaser && !isExplosion)
    {
        finalDamage += m_ballDamageBonus;
        // Overdrive Capacitor: +1 damage during wave climax
        if (m_overdriveCapacitorActive && m_endlessGenerator.IsWaveClimaxActive())
        {
            finalDamage += 1;
        }
    }

    // Tesla Discharge Synergy (Only on direct ball impact, strictly preventing recursive lightning storms)
    if (m_lightningRodActive && ballEntity != NULL_ENTITY && !isLaser && !isExplosion && (std::rand() % 4 == 0))
    {
        TriggerStormStrike();
    }

    // Plasma Field Synergy (Only on direct ball impact)
    if (m_plasmaFieldActive && ballEntity != NULL_ENTITY && !isLaser && !isExplosion && m_registry.HasComponent<BrickComponent>(entity))
    {
        const auto& brick = m_registry.GetComponent<BrickComponent>(entity);
        if (brick.Type == BrickType::Hard || brick.Type == BrickType::Metal)
        {
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                SpawnExplosionParticles(m_registry.GetComponent<Transform2D>(entity).Position, Color{255, 120, 240, 255}, 16);
            }
        }
    }

    int scoreBefore = m_scoreManager.GetScore();
    GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, finalDamage);
    int scoreDelta = m_scoreManager.GetScore() - scoreBefore;

    // Apply Endless Score Multiplier to all brick scores
    if (scoreDelta > 0 && m_endlessScoreMultiplier > 1.0f)
    {
        uint32_t bonusScore = static_cast<uint32_t>(static_cast<float>(scoreDelta) * (m_endlessScoreMultiplier - 1.0f));
        if (bonusScore > 0)
        {
            m_scoreManager.AddScore(bonusScore);
            if (mp_context)
            {
                mp_context->Events.Publish(ScoreChangedEvent(m_scoreManager.GetScore(), m_scoreManager.GetComboMultiplier()));
            }
        }
    }

    // Only count as destroyed if the brick was actually destroyed by this hit!
    // (Prevents brick counter from incrementing on paddle hits or non-fatal strikes)
    if (m_registry.IsPendingDestroy(entity))
    {
        m_endlessGenerator.OnBrickDestroyed();
        // Shield Siphon perk: 20% chance to restore +5% Ultimate charge on destroying bricks
        if (m_shieldSiphonActive && (std::rand() % 5 == 0))
        {
            AddUltimateCharge(5.0f);
        }
    }
}

void EndlessScene::OnGameOverEnter()
{
    // Record Endless Run Stats
    ProfileManager::Get().RecordEndlessRun(m_survivalTime, m_wavesCleared, m_scoreManager.GetScore());

    GameScene::OnGameOverEnter();
}

void EndlessScene::OnGameOverExit()
{
    // Full reset specifically for Endless Mode: restart with a fresh seed, 3 red hearts, wave 1
    m_survivalTime = 0.0f;
    m_wavesCleared = 0;
    m_scoreManager.Reset();
    m_lives = 3; // Restart with 3 red hearts!
    m_endlessScoreMultiplier = 1.0f + static_cast<float>(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ScoreMultiplier)) * 0.10f;
    m_remainingRerolls = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls);
    m_consecutiveWavesForShield = 0;
    m_nextMinuteDraftTime = 60.0f;
    m_draftActive = false;
    m_waveClearBannerActive = false;
    m_emergencyBurnCooldown = 0.0f;
    m_ballDamageBonus = 0;
    m_lightningRodActive = false;
    m_plasmaFieldActive = false;
    m_magneticTetherActive = false;
    m_paddleWidthMultiplier = 1.0f;
    m_paddleSpeedMultiplier = 1.0f;

    // Fresh procedural seed
    m_endlessGenerator.Reset(0);
    m_draftRndSeed = m_endlessGenerator.GetSeed() ^ 0x5DEECE66u;

    // Clear bricks and hazard zones
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
    ClearHazardZones();

    if (mp_context)
    {
        m_brickCount = m_endlessGenerator.Generate(m_registry, *mp_context, m_brickTexId);
        ApplyWaveTheme(1);
        mp_context->Events.Publish(LivesChangedEvent(3));
        mp_context->Events.Publish(ScoreChangedEvent(0, 1));
        if (m_playlist.GetTrackCount() > 0)
        {
            m_playlist.PlayNext(*mp_context, 1.0f);
        }
    }

    int shieldPerk = ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ShieldRecovery);
    if (shieldPerk > 0)
    {
        ApplyPowerUp(PowerUpType::BottomBarrier);
    }

    if (mp_eventSystem)
    {
        mp_eventSystem->ResetLifecycle();
        mp_eventSystem->SetIntervalRange(35.0f, 50.0f);
    }

    for (const auto& hazard : m_endlessGenerator.GetWaveHazards())
    {
        SpawnHazardZone(hazard.Position, hazard.Size, hazard.Effect, 2.0f);
    }

    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.1f);
    ResetBallAndPaddle(true);
    StartCountdown();
}

void EndlessScene::OnVictoryEnter()
{
    // Endless mode runs infinitely; fallback to GameScene
    GameScene::OnVictoryEnter();
}

PostProcessParams EndlessScene::GetPostProcessParams() const
{
    PostProcessParams params = GameScene::GetPostProcessParams();
    if (m_draftActive)
    {
        // Keep atmospheric shaders active (CRT scanlines, curvature, soft vignette)
        // without disabling shaders, while silencing disruptive glitch tearing, flash whitewash, and CA splits
        params.GlitchIntensity = 0.0f;
        params.ChromaticAberration = 0.002f;
        params.FlashIntensity = 0.0f;
        params.AlarmIntensity = 0.0f;
        params.ScanlineFlicker = 0.0f;
    }
    return params;
}

void EndlessScene::RenderSceneSpecificHUD(GameContext& context)
{
    RenderEndlessHUD(context);
}

void EndlessScene::OnRender(GameContext& context)
{
    GameScene::OnRender(context);

    const bool isPaused = IsPaused();
    const bool isModalOpen = (m_uiController && m_uiController->IsModalOrSubMenuOpen());

    if (!isPaused && !isModalOpen)
    {
        RenderTripleVisionPaddle(context);
    }

    if (m_draftActive)
    {
        RenderDraftModal(context);
    }
}

void EndlessScene::RenderTripleVisionPaddle(GameContext& context)
{
    // Only render chromatic ghost paddles when ScreenGlitch hazard is actively running
    if (m_screenGlitchDuration <= 0.0f) return;
    if (m_paddle == NULL_ENTITY || !m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<Transform2D>(m_paddle)) return;

    // Set world camera so it's not rendered in top-left screen-space
    const auto& cameraComp = m_registry.GetComponent<Camera2D>(m_camera);
    context.Render.SetCamera(cameraComp);

    const auto& trans = m_registry.GetComponent<Transform2D>(m_paddle);
    float paddleW = 120.0f * trans.Scale.X;
    float paddleH = 20.0f * trans.Scale.Y;

    float offset = 7.0f;
    uint8_t alpha = 85;

    // Cyan chromatic ghost (left)
    context.Render.DrawRectangle(paddleW, paddleH, Transform2D{Vector2f{trans.Position.X - offset, trans.Position.Y}}, Color{0, 240, 255, alpha});
    // Magenta chromatic ghost (right)
    context.Render.DrawRectangle(paddleW, paddleH, Transform2D{Vector2f{trans.Position.X + offset, trans.Position.Y}}, Color{255, 30, 180, alpha});

    // Reset back to UI camera
    context.Render.ResetCamera();
}

void EndlessScene::RenderEndlessHUD(GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    auto formatSeconds = [](float seconds) -> std::string {
        int totalSec = std::max(0, static_cast<int>(seconds));
        int mins = totalSec / 60;
        int secs = totalSec % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
        return std::string(buf);
    };

    float waveTime = m_endlessGenerator.GetWaveTimer();
    float waveTarget = m_endlessGenerator.GetCurrentWaveTargetDuration();
    int wavePct = static_cast<int>(m_endlessGenerator.GetWaveTimeProgress() * 100.0f);

    // Top-center Endurance HUD Banner with Seed display & Time progression (Brick counter removed per user request)
    std::string hudText = "WAVE " + std::to_string(m_endlessGenerator.GetCurrentWave()) +
                          "  |  TIME: " + formatSeconds(waveTime) + " / " + formatSeconds(waveTarget) + " (" + std::to_string(wavePct) + "%)" +
                          "  |  SURVIVAL: " + GetSurvivalTimeFormatted() +
                          "  |  SCORE: x" + std::to_string(m_endlessScoreMultiplier).substr(0, 4) +
                          "  |  SEED: #" + std::to_string(m_endlessGenerator.GetSeed());

    context.Render.DrawTextCentered(hudText, m_fontId, 22.0f, Transform2D{Vector2f{screenCenterX, 55.0f}}, Colors::Gold);

    // EMP Ultimate Lockdown Alert
    if (mp_eventSystem && mp_eventSystem->IsUltimateBlocked())
    {
        Color empCol{0, 240, 255, static_cast<uint8_t>(180 + 75.0f * std::sin(m_survivalTime * 8.0f))};
        context.Render.DrawTextCentered("[!] EMP LOCKDOWN : ULTIMATE ABILITIES BLOCKED (45s) [!]", m_fontId, 20.0f, Transform2D{Vector2f{screenCenterX, 85.0f}}, empCol);
    }
    // Danger Warning if bricks descend too close
    else if (m_endlessGenerator.IsDangerActive())
    {
        Color warnCol{255, 40, 40, static_cast<uint8_t>(180 + 75.0f * std::sin(m_survivalTime * 6.0f))};
        context.Render.DrawTextCentered("[!] WARNING : CEILING COLLAPSE IMMINENT [!]", m_fontId, 22.0f, Transform2D{Vector2f{screenCenterX, 85.0f}}, warnCol);
    }
    // Climax Rush Warning in final 15% of wave time
    else if (m_endlessGenerator.IsWaveClimaxActive())
    {
        Color climaxCol{255, 140, 20, static_cast<uint8_t>(190 + 65.0f * std::sin(m_survivalTime * 10.0f))};
        context.Render.DrawTextCentered("[!] WAVE CLIMAX - FINAL SURGE ENGAGED [!]", m_fontId, 20.0f, Transform2D{Vector2f{screenCenterX, 85.0f}}, climaxCol);
    }

    // Wave Clear Banner overlay
    if (m_waveClearBannerActive)
    {
        float bannerAlpha = std::min(1.0f, m_waveClearBannerTimer / 0.5f);
        Color bannerCol{255, 230, 100, static_cast<uint8_t>(255 * bannerAlpha)};
        context.Render.DrawTextCentered("WAVE " + std::to_string(m_justClearedWaveNumber) + " CLEARED!", m_fontId, 48.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 140.0f}}, bannerCol);
    }
}

void EndlessScene::RenderDraftModal(GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    // Dark translucent backdrop
    context.Render.DrawRectangle(viewSize.X, viewSize.Y, Transform2D{Vector2f{screenCenterX, screenCenterY}}, Color{5, 8, 14, 215});

    // Header Title & Subtitle based on DraftType
    if (m_activeDraftType == DraftType::WaveClear)
    {
        context.Render.DrawTextCentered("WAVE CLEARED : ELITE UPGRADE", m_fontId, 36.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 290.0f}}, Colors::Gold);
        context.Render.DrawTextCentered("ELITE REWARD FOR SURVIVING THE WAVE - ENHANCED RARITY", m_fontId, 18.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 252.0f}}, Color{255, 220, 130, 230});
    }
    else
    {
        context.Render.DrawTextCentered("TACTICAL SUPPLY : 1-MINUTE SURVIVAL", m_fontId, 36.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 290.0f}}, Color{80, 220, 255, 255});
        context.Render.DrawTextCentered("SURVIVAL MILESTONE REACHED - SELECT A TACTICAL UPGRADE", m_fontId, 18.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 252.0f}}, Color{160, 210, 240, 220});
    }

    // 10s Timer Banner & Progress Bar
    const float timerRatio = std::clamp(m_draftTimer / kDraftMaxDuration, 0.0f, 1.0f);
    int secondsRemaining = static_cast<int>(std::ceil(m_draftTimer));
    char timerBuf[48];
    std::snprintf(timerBuf, sizeof(timerBuf), "CHOOSE YOUR BOOST  [ %02d s ]", secondsRemaining);

    Color timerTextColor = (m_draftTimer <= 3.0f)
        ? Color{255, 70, 70, static_cast<uint8_t>(200 + 55.0f * std::sin(m_draftTimer * 16.0f))}
        : (m_draftTimer <= 6.0f ? Colors::Gold : Color{80, 230, 255, 255});

    context.Render.DrawTextCentered(timerBuf, m_fontId, 19.0f, Transform2D{Vector2f{screenCenterX, screenCenterY - 222.0f}}, timerTextColor);

    // Timer Bar
    const float barTotalWidth = 460.0f;
    const float barHeight = 6.0f;
    const float barCenterY = screenCenterY - 204.0f;
    context.Render.DrawRectangle(barTotalWidth + 4.0f, barHeight + 4.0f, Transform2D{Vector2f{screenCenterX, barCenterY}}, Color{15, 22, 35, 220});
    context.Render.DrawRectangleOutline(barTotalWidth + 4.0f, barHeight + 4.0f, Transform2D{Vector2f{screenCenterX, barCenterY}}, Color{60, 85, 120, 200}, 1.5f);
    if (timerRatio > 0.0f)
    {
        float filledW = barTotalWidth * timerRatio;
        float filledCenterX = screenCenterX - (barTotalWidth - filledW) * 0.5f;
        Color barFillColor = (m_draftTimer <= 3.0f) ? Color{255, 60, 60, 255} : (m_draftTimer <= 6.0f ? Color{255, 200, 40, 255} : Color{0, 210, 255, 255});
        context.Render.DrawRectangle(filledW, barHeight, Transform2D{Vector2f{filledCenterX, barCenterY}}, barFillColor);
    }

    const float cardWidth = 340.0f;
    const float cardHeight = 430.0f;
    const float cardSpacing = 370.0f;
    const float cardCenterY = screenCenterY + 15.0f;

    for (size_t i = 0; i < m_currentDraftCards.size(); ++i)
    {
        const auto& card = m_currentDraftCards[i];
        float cx = screenCenterX + static_cast<float>(static_cast<int>(i) - 1) * cardSpacing;
        float cy = cardCenterY;
        bool isSelected = (static_cast<int>(i) == m_selectedDraftIndex);

        // Card Border & Background
        Color bgCol = isSelected ? Color{32, 48, 78, 255} : Color{18, 26, 42, 255};
        Color borderCol = isSelected ? Colors::Gold : card.RarityColor;

        context.Render.DrawRectangle(cardWidth, cardHeight, Transform2D{Vector2f{cx, cy}}, bgCol);
        context.Render.DrawRectangleOutline(cardWidth, cardHeight, Transform2D{Vector2f{cx, cy}}, borderCol, isSelected ? 4.0f : 2.0f);

        // Rarity Tag (top)
        context.Render.DrawTextCentered(card.RarityName, m_fontId, 18.0f, Transform2D{Vector2f{cx, cy - 175.0f}}, card.RarityColor);

        // Category Tag
        context.Render.DrawTextCentered(card.Category, m_fontId, 14.0f, Transform2D{Vector2f{cx, cy - 150.0f}}, Color{160, 180, 210, 200});

        // Title
        context.Render.DrawTextCentered(card.Title, m_fontId, 24.0f, Transform2D{Vector2f{cx, cy - 90.0f}}, card.CardColor);

        // Separator line
        context.Render.DrawRectangle(cardWidth - 50.0f, 2.0f, Transform2D{Vector2f{cx, cy - 55.0f}}, card.RarityColor);

        // Description (with word wrapping)
        auto wrapDescription = [](const std::string& text, size_t maxChars) -> std::vector<std::string>
        {
            std::vector<std::string> lines;
            std::istringstream stream(text);
            std::string word;
            std::string currentLine;
            while (stream >> word)
            {
                if (!currentLine.empty() && currentLine.length() + 1 + word.length() > maxChars)
                {
                    lines.push_back(currentLine);
                    currentLine = word;
                }
                else
                {
                    if (!currentLine.empty()) currentLine += " ";
                    currentLine += word;
                }
            }
            if (!currentLine.empty()) lines.push_back(currentLine);
            return lines;
        };

        auto descLines = wrapDescription(card.Description, 26);
        float lineY = cy - 15.0f;
        for (const auto& line : descLines)
        {
            context.Render.DrawTextCentered(line, m_fontId, 16.0f, Transform2D{Vector2f{cx, lineY}}, Color{220, 230, 245, 240});
            lineY += 24.0f;
        }

        // Selection prompt / Key shortcut
        if (isSelected)
        {
            context.Render.DrawRectangle(cardWidth - 40.0f, 40.0f, Transform2D{Vector2f{cx, cy + 165.0f}}, Colors::Gold);
            context.Render.DrawTextCentered("SELECT [ ENTER ]", m_fontId, 18.0f, Transform2D{Vector2f{cx, cy + 165.0f}}, Colors::Black);
        }
        else
        {
            std::string keyPrompt = "[ " + std::to_string(i + 1) + " ] SELECT";
            context.Render.DrawTextCentered(keyPrompt, m_fontId, 16.0f, Transform2D{Vector2f{cx, cy + 165.0f}}, Color{120, 150, 180, 220});
        }
    }

    // Rerolls footer button
    std::string rerollText = "REROLL  [R]  (" + std::to_string(m_remainingRerolls) + " REMAINING)";
    Color rerollCol = (m_remainingRerolls > 0) ? Colors::Cyan : Color{100, 110, 125, 200};
    const float rerollBtnY = cardCenterY + cardHeight * 0.5f + 40.0f;
    context.Render.DrawRectangle(320.0f, 42.0f, Transform2D{Vector2f{screenCenterX, rerollBtnY}}, Color{16, 24, 38, 240});
    context.Render.DrawRectangleOutline(320.0f, 42.0f, Transform2D{Vector2f{screenCenterX, rerollBtnY}}, rerollCol, 2.0f);
    context.Render.DrawTextCentered(rerollText, m_fontId, 16.0f, Transform2D{Vector2f{screenCenterX, rerollBtnY}}, rerollCol);
}
