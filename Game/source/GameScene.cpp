#include "Scenes/GameScene.h"
#include "Scenes/PreviewScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/InputManager.h"
#include "Core/GameRules.h"
#include "ScoreManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Components/ParticleComponent.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Systems/PhysicsSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "Graphics/Renderer.h"
#include "LevelTheme.h"
#include "Events/EventDispatcher.h"
#include "Events/CollisionEvent.h"
#include "Events/CheatSubmitEvent.h"
#include "StateMachine/Transition.h"
#include "Conditions/KeyPressCondition.h"
#include "Conditions/GameConditions.h"
#include "Actions/ResetGameAction.h"
#include "Actions/NextLevelAction.h"
#include "Actions/GameOverAction.h"
#include "Actions/VictoryAction.h"
#include "ECS/Components/UI/TextInputComponent.h"
#include "AudioMixer.h"
#include "ECS/Systems/PowerUpSystem.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"
#include "Math/Physics.h"
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <queue>
#include <unordered_set>

#include "Core/Debug.h"
#include "Core/SceneManager.h"
#include "ECS/Components/TweenComponent.h"
#include "StateMachine/StateMachine.h"
#include "Generators/ILevelGenerator.h"
#include "UI/UIFactory.h"
#include "Events/GameplayEvents.h"
#include "ECS/Systems/GameFeelSystem.h"
#include "ECS/Systems/TweenSystem.h"
#include "ECS/Systems/AnimatorSystem.h"
#include "ECS/Components/AnimatorComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Systems/UISystem.h"
#include "Generators/FileLevelGenerator.h"
#include "Scenes/MenuScene.h"
#include "TweenEffects/TweenEffects.h"

void GameScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    mp_context = &context;

    PowerUpManager::Get().LoadFromPrefs();

    auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
    context.Render.SetCamera(camera);

    camera.Zoom = 0.75f;

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");
    m_crackShaderId = context.Resources.LoadResource("Resources/shaders/crack.frag");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_brickTexId = context.Resources.LoadResource("Resources/sprite/brick.png");
    m_brickCrackTexId = context.Resources.LoadResource("Resources/sprite/brick_crack.png");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_despawnSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_despawn.wav");
    m_explosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");
    m_fireballBurnSfxId = context.Resources.LoadResource("Resources/audio/sfx/fireball_burn.wav");
    m_fireballExplosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/fireball_explode.wav");
    m_countdownReadySfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_ready.wav");
    m_countdown3SfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_3.wav");
    m_countdown2SfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_2.wav");
    m_countdown1SfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_1.wav");
    m_countdownGoSfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_go.wav");
    m_countdownWhooshSfxId = context.Resources.LoadResource("Resources/audio/sfx/countdown_whoosh.wav");
    m_fireTexId = context.Resources.LoadResource("Resources/sprite/fire.png");
    m_heartTexId = context.Resources.LoadResource("Resources/sprite/heart.png");
    m_gameOverSfxId = context.Resources.LoadResource("Resources/audio/sfx/game_over.mp3");
    m_levelClearSfxId = context.Resources.LoadResource("Resources/audio/sfx/game_win.mp3");
    m_victorySfxId = context.Resources.LoadResource("Resources/audio/sfx/victory.mp3");
    m_scoreRecordedSfxId = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");
    m_lightningSfxId = context.Resources.LoadResource("Resources/audio/sfx/lightning.wav");
    m_conductionSfxId = context.Resources.LoadResource("Resources/audio/sfx/conduction.wav");

    mp_paddleSystem = m_systemManager.AddSystem<PaddleSystem>(m_registry, context);
    mp_ballSystem = m_systemManager.AddSystem<BallSystem>(m_registry, context);
    int savedTrail = PlayerPrefs::GetInt("SelectedBallTrail", static_cast<int>(TrailCosmeticId::DefaultNeon));
    if (savedTrail >= 0 && savedTrail <= static_cast<int>(TrailCosmeticId::Rainbow))
    {
        mp_ballSystem->SetBallTrailCosmetic(static_cast<TrailCosmeticId>(savedTrail));
    }
    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.AddSystem<AnimatorSystem>(m_registry);
    m_particleSystem = m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);
    m_systemManager.AddSystem<GameFeelSystem>(m_registry, context);
    m_systemManager.AddSystem<TweenSystem>(m_registry);
    m_systemManager.AddSystem<PowerUpSystem>(m_registry, context.Events);

    mp_paddleSystem->SetOnTriggerStormCallback([this](Entity)
    {
        TriggerStormStrike();
    });

    mp_paddleSystem->SetOnBallLaunchedCallback([this](Entity)
    {
        m_ballState = BallState::Active;
    });

    mp_ballSystem->SetFireTrailCallback([this](const Vector2f& pos, const Vector2f& vel, bool isFuseActive)
    {
        SpawnFireTrailParticle(pos, vel, isFuseActive);
    });

    mp_ballSystem->SetFireExplosionCallback([this](Entity b, const Vector2f& pos)
    {
        ExplodeFireBall(b, pos);
    });

    m_powerUpSub = context.Events.SubscribeScoped<PowerUpEvent>([this](const PowerUpEvent& e)
    {
        ApplyPowerUp(e.Type);
        if (m_renderGameUI && m_uiController && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
        {
            const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            m_uiController->SpawnPowerUpText(e.Type, paddleTrans.Position, m_registry.GetComponent<Camera2D>(m_camera));
        }
    });


    m_nextLevelSub = context.Events.SubscribeScoped<NextLevelEvent>([this](const NextLevelEvent&)
    {
        AdvanceToNextLevel();
    });

    m_registry.AddComponent<TweenComponent>(m_camera, TweenComponent{});
    auto& camTween = m_registry.GetComponent<TweenComponent>(m_camera);
    
    TweenEffects::CameraBreathing(camTween, m_registry, m_camera, 0.74f, 0.76f, 6.0f);
    
    m_ambientBackground.OnInit(60);
    m_bgTweenEntity = m_registry.CreateEntity();
    m_registry.AddComponent<TweenComponent>(m_bgTweenEntity, TweenComponent{});

    m_bgColor = Color{8, 10, 16, 255};
    Color c1{8, 10, 16, 255};
    Color c2{14, 12, 22, 255};
    TweenEffects::BackgroundColorShift(m_registry.GetComponent<TweenComponent>(m_bgTweenEntity), c1, c2, [this](Color c){ m_bgColor = c; }, 4.0f);

    m_postProcessParams = PostProcessParams{};
    m_currentComboMultiplier = 0;

    m_collisionSub = context.Events.SubscribeScoped<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        // 1. Check laser projectile collision with bricks
        auto& lasers = mp_ballSystem->GetLasers();
        for (auto it = lasers.begin(); it != lasers.end(); )
        {
            Entity laser = *it;
            if (!m_registry.IsAlive(laser))
            {
                it = lasers.erase(it);
                continue;
            }

            if (e.EntityA == laser || e.EntityB == laser)
            {
                Entity other = (e.EntityA == laser) ? e.EntityB : e.EntityA;

                // Lasers pass through balls and bonus capsules
                bool isBall = m_registry.HasComponent<BallComponent>(other) || (other == m_ball);
                if (isBall || m_registry.HasComponent<PowerUpComponent>(other))
                {
                    ++it;
                    continue;
                }

                if (m_registry.HasComponent<BrickComponent>(other))
                {
                    HandleBrickCollision(other, NULL_ENTITY, /*isLaser=*/true);
                    m_registry.DestroyEntityDeferred(laser);
                    it = lasers.erase(it);
                    return;
                }
                else if (other != m_paddle)
                {
                    m_registry.DestroyEntityDeferred(laser);
                    it = lasers.erase(it);
                    return;
                }
            }
            ++it;
        }

        if (m_ballState != BallState::Active) return;

        bool isBallA = m_registry.HasComponent<BallComponent>(e.EntityA) || (e.EntityA == m_ball);
        bool isBallB = m_registry.HasComponent<BallComponent>(e.EntityB) || (e.EntityB == m_ball);
        if (!isBallA && !isBallB) return;
        if (isBallA && isBallB) return;

        Entity ballEntity = isBallA ? e.EntityA : e.EntityB;
        Entity otherEntity = isBallA ? e.EntityB : e.EntityA;

        // Ignore collisions with bonus capsules, particles, or laser projectiles
        if (m_registry.HasComponent<PowerUpComponent>(otherEntity)) return;
        if (m_registry.HasComponent<ParticleComponent>(otherEntity)) return;
        if (mp_ballSystem->IsLaser(otherEntity)) return;

        bool isBrick = m_registry.HasComponent<BrickComponent>(otherEntity);
        bool isPaddle = (otherEntity == m_paddle);
        bool isBottomWall = (otherEntity == m_bottomWall);

        HandleBrickCollision(otherEntity, ballEntity, /*isLaser=*/false, /*isExplosion=*/false);

        if (isPaddle)
        {
            HandlePaddleCollision(ballEntity);
        }
        else if (isBottomWall)
        {
            int currentState = mp_state_machine ? mp_state_machine->GetCurrentState() : static_cast<int>(SceneState::Playing);
            if (!context.Rules.GetRule(Rule::Gameplay::Invincible) && currentState == static_cast<int>(SceneState::Playing))
            {
                HandleBallBottomCollision(ballEntity);
            }
        }
        else if (!isBrick && !isPaddle && !isBottomWall)
        {
            float bouncePan = 0.0f;
            if (m_registry.HasComponent<Transform2D>(ballEntity))
            {
                constexpr float kHalfPlayfieldWidth = 800.0f;
                bouncePan = std::clamp(m_registry.GetComponent<Transform2D>(ballEntity).Position.X / kHalfPlayfieldWidth, -1.0f, 1.0f);
            }
            context.Audio.PlaySfx(m_bounceSfxId, 50.0f, 1.0f, bouncePan);
        }

        if (isPaddle || (!isBrick && !isPaddle && !isBottomWall))
        {
            if (m_registry.HasComponent<Transform2D>(ballEntity) && m_registry.HasComponent<SpriteComponent>(ballEntity))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(ballEntity);
                const auto& sprite = m_registry.GetComponent<SpriteComponent>(ballEntity);
                SpawnExplosionParticles(transform.Position, sprite.Tint, 3);
            }
        }
    });

    CreateWall(0.0f, -550.0f, 2000.0f, 200.0f);
    CreateWall(-900.0f, 0.0f, 200.0f, 1200.0f);
    CreateWall(900.0f, 0.0f, 200.0f, 1200.0f);
    m_bottomWall = CreateWall(0.0f, 550.0f, 2000.0f, 200.0f);

    if (mp_ballSystem) mp_ballSystem->ClearBalls();
    m_ball = CreateBall(Vector2f{0.0f, 0.0f}, Vector2f{0.0f, 0.0f});
    m_registry.GetComponent<RigidBody>(m_ball).IsKinematic = true;

    m_paddle = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_paddle, Transform2D{Vector2f{0.0f, 300.0f}});
    m_registry.AddComponent<BoxCollider>(m_paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_paddle, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_paddle, SpriteComponent{m_paddleTexId});
    m_registry.AddComponent<TweenComponent>(m_paddle, TweenComponent{});
    m_registry.AddComponent<PaddleComponent>(m_paddle, PaddleComponent{});

    LightSourceComponent paddleLight{};
    paddleLight.Type = LightType::Cone;
    paddleLight.LightColor = Color{200, 235, 255, 255};
    paddleLight.Radius = 450.0f;
    paddleLight.Intensity = 1.3f;
    paddleLight.DirectionDeg = -90.0f; // points straight up into the field
    paddleLight.ConeAngleDeg = 80.0f;  // wide coverage beam
    paddleLight.InnerRadius = 30.0f;
    m_registry.AddComponent<LightSourceComponent>(m_paddle, paddleLight);

    m_uiController = std::make_unique<GameUIController>(m_registry, *mp_context, *this);
    m_uiController->OnInit(m_fontId, m_heartTexId, m_fireTexId, m_bounceSfxId, m_scoreRecordedSfxId);

    m_systemManager.OnInit();

    m_levelManager.Initialize("Resources/levels");
    m_levelManager.LoadFromPrefs();

    if (m_musicEnabled)
    {
        m_playlist.AddTrack(context, "Resources/audio/music/Game-1.ogg");
        m_playlist.AddTrack(context, "Resources/audio/music/Game-2.ogg");
        m_playlist.AddTrack(context, "Resources/audio/music/Game-3.ogg");
        m_playlist.AddTrack(context, "Resources/audio/music/Game-4.ogg");
        m_playlist.PlayNext(context, 1.5f);
    }
    m_lives = PlayerPrefs::GetInt("Lives", 3);

    int startLevel = PlayerPrefs::GetInt("SelectedLevel", 0);
    PlayerPrefs::SetInt("SelectedLevel", 0);
    PlayerPrefs::Save();
    LoadLevel(startLevel, false);

    mp_state_machine = std::make_unique<StateMachine<GameScene>>(this, 6);

    class PauseToCountdownCondition : public Condition<GameScene>
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

    class PauseToPlayCondition : public Condition<GameScene>
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

    class CountdownCompleteCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            return pOwner && pOwner->IsCountdownComplete();
        }
    };

    class PlayToPauseCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            if (!pOwner) return false;
            return pOwner->GetContext()->Input.IsActionTriggered(InputAction::Pause) || pOwner->GetContext()->Input.IsKeyPress(KeyCode::Escape);
        }
    };

    State<GameScene>* countdownState = mp_state_machine->CreateState(static_cast<int>(SceneState::Countdown));
    countdownState->AddTransition(new Transition<GameScene>(new CountdownCompleteCondition(), static_cast<int>(SceneState::Playing)));
    countdownState->AddTransition(new Transition<GameScene>(new PlayToPauseCondition(), static_cast<int>(SceneState::Paused)));

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new PlayToPauseCondition(), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new LevelClearedCondition<GameScene>(), static_cast<int>(SceneState::LevelTransition)));
    playingState->AddTransition(new Transition<GameScene>(new VictoryCondition<GameScene>(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new PauseToCountdownCondition(), static_cast<int>(SceneState::Countdown)));
    pauseState->AddTransition(new Transition<GameScene>(new PauseToPlayCondition(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new GameOverAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    State<GameScene>* victoryState = mp_state_machine->CreateState(static_cast<int>(SceneState::Victory));
    victoryState->AddAction(new VictoryAction<GameScene>());
    victoryState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    State<GameScene>* levelTransitionState = mp_state_machine->CreateState(static_cast<int>(SceneState::LevelTransition));
    levelTransitionState->AddAction(new NextLevelAction<GameScene>());
    levelTransitionState->AddTransition(new Transition<GameScene>(new LevelTransitionCompleteCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    context.Events.Publish(LivesChangedEvent(m_lives));
    StartCountdown();
}

void GameScene::OnUpdate(const float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;

    if (mp_state_machine)
    {
        mp_state_machine->Update();
    }

    const int currentState = mp_state_machine ? mp_state_machine->GetCurrentState() : static_cast<int>(SceneState::Playing);

    if (m_uiController)
    {
        GameUIState uiState = GameUIState::Playing;
        if (currentState == static_cast<int>(SceneState::Paused)) uiState = GameUIState::Paused;
        else if (currentState == static_cast<int>(SceneState::GameOver)) uiState = GameUIState::GameOver;
        else if (currentState == static_cast<int>(SceneState::Victory)) uiState = GameUIState::Victory;
        else if (currentState == static_cast<int>(SceneState::LevelTransition)) uiState = GameUIState::LevelTransition;
        else if (currentState == static_cast<int>(SceneState::Countdown)) uiState = GameUIState::Countdown;

        context.Events.Publish(GameStateChangedEvent(uiState));
    }

    if (m_uiController && m_uiController->IsRebinding())
    {
        UISystem::SetNavigationEnabled(false);
    }
    UISystem::OnUpdate(dt, m_registry, context);

    if (currentState == static_cast<int>(SceneState::Countdown))
    {
        if (m_countdownActive)
        {
            m_countdownTimer -= dt;

            // Trigger subtle breath whoosh as the current word/numeral fades out (last ~0.28s)
            if (m_countdownTimer <= static_cast<float>(m_countdownLastSecond - 1) + 0.28f &&
                m_countdownWhooshPlayedForSecond != m_countdownLastSecond)
            {
                m_countdownWhooshPlayedForSecond = m_countdownLastSecond;
                if (m_countdownWhooshSfxId != 0)
                {
                    context.Audio.PlaySfx(m_countdownWhooshSfxId, 45.0f);
                }
            }

            int currentSec = std::clamp(static_cast<int>(std::ceil(m_countdownTimer)), 1, 4);
            if (currentSec < m_countdownLastSecond && m_countdownTimer > 0.0f)
            {
                m_countdownLastSecond = currentSec;
                if (currentSec == 3 && m_countdown3SfxId != 0)
                {
                    context.Audio.PlaySfx(m_countdown3SfxId, 100.0f);
                }
                else if (currentSec == 2 && m_countdown2SfxId != 0)
                {
                    context.Audio.PlaySfx(m_countdown2SfxId, 100.0f);
                }
                else if (currentSec == 1 && m_countdown1SfxId != 0)
                {
                    context.Audio.PlaySfx(m_countdown1SfxId, 100.0f);
                }
            }

            if (m_countdownTimer <= 0.0f)
            {
                m_countdownTimer = 0.0f;
                m_countdownActive = false;
                m_countdownGoTimer = 0.65f;

                if (m_countdownGoSfxId != 0)
                {
                    context.Audio.PlaySfx(m_countdownGoSfxId, 100.0f);
                }

                if (mp_paddleSystem)
                {
                    mp_paddleSystem->SetInputEnabled(true);
                }

                if (mp_state_machine)
                {
                    mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
                }
                context.Events.Publish(GameStateChangedEvent(GameUIState::Playing));
            }
        }
    }

    if (m_countdownGoTimer > 0.0f)
    {
        m_countdownGoTimer -= dt;
        if (m_countdownGoTimer < 0.0f)
        {
            m_countdownGoTimer = 0.0f;
        }
    }

    if (currentState == static_cast<int>(SceneState::LevelTransition))
    {
        if (m_levelTransitionTimer > 0.0f)
        {
            m_levelTransitionTimer -= dt;
        }
    }

    if (m_uiController)
    {
        m_uiController->OnUpdate(dt);
    }

    if (currentState == static_cast<int>(SceneState::GameOver) || currentState == static_cast<int>(SceneState::Victory))
    {
        if (context.Input.IsKeyPress(KeyCode::Escape))
        {
            if (m_uiController && !m_uiController->TryConfirmNameBeforeProceeding())
            {
                return;
            }
            context.Scenes.LoadScene<MenuScene>();
            return;
        }
    }

    if (currentState == static_cast<int>(SceneState::Playing))
    {
        m_gameplayTime += dt;
        if (m_musicEnabled)
        {
            m_playlist.Update(context);
        }
        HandleInput(dt, context);
        UpdatePowerUpTimers(dt);

        // Sticky / Catch Paddle: Update caught balls and process release on input or timeout
        if (!m_stuckBalls.empty() && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
        {
            const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const auto& paddleCol = m_registry.GetComponent<BoxCollider>(m_paddle);
            const float paddleHalfW = paddleCol.GetEffectiveSize(paddleTrans.Scale).X * 0.5f;
            const float paddleHalfH = paddleCol.GetEffectiveSize(paddleTrans.Scale).Y * 0.5f;
            const float autoLaunchDelay = PowerUpManager::Get().Sticky().AutoLaunchDelay;
            const bool launchTriggered = context.Input.IsActionTriggered(InputAction::LaunchBall) ||
                                         context.Input.IsActionTriggered(InputAction::FireLaser) ||
                                         context.Input.IsKeyPress(KeyCode::Space);

            for (auto it = m_stuckBalls.begin(); it != m_stuckBalls.end(); )
            {
                if (!m_registry.IsAlive(it->Ball) || !m_registry.HasComponent<Transform2D>(it->Ball) ||
                    !m_registry.HasComponent<RigidBody>(it->Ball) || !m_registry.HasComponent<BallComponent>(it->Ball))
                {
                    it = m_stuckBalls.erase(it);
                    continue;
                }

                it->HoldTimer += dt;
                auto& ballTrans = m_registry.GetComponent<Transform2D>(it->Ball);
                auto& ballRb = m_registry.GetComponent<RigidBody>(it->Ball);
                auto& ballComp = m_registry.GetComponent<BallComponent>(it->Ball);

                float ballRadius = 20.0f;
                if (m_registry.HasComponent<CircleCollider>(it->Ball))
                {
                    ballRadius = m_registry.GetComponent<CircleCollider>(it->Ball).GetEffectiveRadius(ballTrans.Scale);
                }

                ballTrans.Position.X = paddleTrans.Position.X + it->RelativeOffsetX;
                ballTrans.Position.Y = paddleTrans.Position.Y - paddleHalfH - ballRadius - 1.0f;

                if (launchTriggered || it->HoldTimer >= autoLaunchDelay || m_stickyPaddleDuration <= 0.0f)
                {
                    float hitFactor = it->RelativeOffsetX / std::max(paddleHalfW, 1.0f);
                    hitFactor = std::clamp(hitFactor, -0.9f, 0.9f);
                    constexpr float maxAngle = 60.0f * 3.14159265f / 180.0f;
                    const float bounceAngle = hitFactor * maxAngle;

                    float speed = 640.0f;
                    if (m_tempoBallDuration > 0.0f)
                    {
                        speed = PowerUpManager::Get().Tempo().GetEffectiveFastSpeed();
                    }

                    // Rhythm Launch mechanic: If player launches manually during the sweet spot rhythm window
                    bool sweetSpotHit = false;
                    if (launchTriggered)
                    {
                        const float cycleTime = it->HoldTimer;
                        const float targetBeat = (cycleTime < 1.45f) ? 1.05f : 2.05f;
                        if (std::abs(cycleTime - targetBeat) <= 0.20f)
                        {
                            sweetSpotHit = true;
                        }
                    }

                    if (sweetSpotHit)
                    {
                        const auto& stickyCfg = PowerUpManager::Get().Sticky();
                        speed *= stickyCfg.GetEffectiveBoostMultiplier();
                        SpawnExplosionParticles(ballTrans.Position, Color{255, 235, 70, 255}, 20);
                        SpawnLightFlash(ballTrans.Position, 320.0f, Color{255, 220, 60, 255}, 1.8f, 0.35f);
                    }

                    // Move slightly above the paddle so it's fully free upon release
                    ballTrans.Position.Y = paddleTrans.Position.Y - paddleHalfH - ballRadius - 4.0f;
                    ballRb.Velocity.X = speed * std::sin(bounceAngle);
                    ballRb.Velocity.Y = -speed * std::cos(bounceAngle);
                    ballRb.IsKinematic = false;
                    ballComp.IsStuck = false;

                    if (m_bounceSfxId != 0)
                    {
                        context.Audio.PlaySfx(m_bounceSfxId, sweetSpotHit ? 100.0f : 90.0f);
                    }

                    it = m_stuckBalls.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // Bottom Barrier: safety check to catch any fast falling balls
        if (m_bottomBarrierActive && m_bottomBarrierCharges > 0 && mp_ballSystem)
        {
            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<Transform2D>(b) || !m_registry.HasComponent<RigidBody>(b))
                    continue;

                auto& bTrans = m_registry.GetComponent<Transform2D>(b);
                auto& bRb = m_registry.GetComponent<RigidBody>(b);
                if (bTrans.Position.Y >= 430.0f && bRb.Velocity.Y > 0.0f)
                {
                    HandleBallBottomCollision(b);
                }
            }
        }

        if (m_ballState == BallState::Dying)
        {
            if (m_registry.HasComponent<Transform2D>(m_ball))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(m_ball);
                SpawnBleedParticles(transform.Position);
            }
        }

        if (mp_levelGenerator)
        {
            mp_levelGenerator->Update(dt, m_registry, context);
        }
        m_scoreManager.Update(dt);
        m_levelManager.SetHighScoreForLevel(m_levelManager.GetCurrentLevelNumber(), m_scoreManager.GetScore());

        context.Events.Publish(ScoreChangedEvent(m_scoreManager.GetScore(), m_scoreManager.GetComboMultiplier()));
    }

    if (currentState != static_cast<int>(SceneState::Paused))
    {
        for (auto it = m_lightFlashes.begin(); it != m_lightFlashes.end(); )
        {
            it->Lifetime -= dt;
            if (it->Lifetime <= 0.0f)
            {
                it = m_lightFlashes.erase(it);
            }
            else
            {
                const float t = it->Lifetime / it->InitialLifetime;
                it->Intensity = it->InitialIntensity * t;
                ++it;
            }
        }
    }

    // Fireball ambient burning audio loop management
    bool hasActiveFireball = false;
    if (currentState == static_cast<int>(SceneState::Playing) && mp_ballSystem)
    {
        for (Entity b : mp_ballSystem->GetBalls())
        {
            if (m_registry.IsAlive(b) && m_registry.HasComponent<BallComponent>(b))
            {
                if (m_registry.GetComponent<BallComponent>(b).IsFireBall)
                {
                    hasActiveFireball = true;
                    break;
                }
            }
        }
    }

    if (hasActiveFireball)
    {
        if (m_fireballBurnSfxId != 0)
        {
            context.Audio.PlayLoopingSfx(m_fireballBurnSfxId, 65.0f);
        }
    }
    else
    {
        if (m_fireballBurnSfxId != 0 && context.Audio.IsLoopingSfxPlaying(m_fireballBurnSfxId))
        {
            context.Audio.StopLoopingSfx(m_fireballBurnSfxId);
        }
    }

    // 1. Dynamic background color modulation reacting to combo multipliers
    UpdateComboBackgroundColor(m_scoreManager.GetComboMultiplier());

    // 2. Update ambient background (parallax grid + floating dust particles)
    Vector2f paddlePos{0.0f, 0.0f};
    if (m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }
    Vector2f camPos{0.0f, 0.0f};
    if (m_registry.HasComponent<Camera2D>(m_camera))
    {
        camPos = m_registry.GetComponent<Camera2D>(m_camera).Position;
    }
    m_ambientBackground.OnUpdate(dt, paddlePos, camPos);

    // 3. Post-process parameter decay towards resting baseline
    constexpr float kBaseCA = 0.005f;
    constexpr float kBaseVig = 0.25f;
    const Color kBaseVigColor{0, 0, 0, 255};

    if (m_postProcessParams.ChromaticAberration > kBaseCA)
    {
        m_postProcessParams.ChromaticAberration = std::max(kBaseCA, m_postProcessParams.ChromaticAberration - dt * 0.05f);
    }
    if (m_postProcessParams.VignetteIntensity > kBaseVig)
    {
        m_postProcessParams.VignetteIntensity = std::max(kBaseVig, m_postProcessParams.VignetteIntensity - dt * 0.9f);
    }
    if (m_postProcessParams.FlashIntensity > 0.0f)
    {
        m_postProcessParams.FlashIntensity = std::max(0.0f, m_postProcessParams.FlashIntensity - dt * 1.3f);
    }

    const float decayAlpha = std::clamp(dt * 3.5f, 0.0f, 1.0f);
    m_postProcessParams.VignetteColor.r = static_cast<uint8_t>(m_postProcessParams.VignetteColor.r + (kBaseVigColor.r - m_postProcessParams.VignetteColor.r) * decayAlpha);
    m_postProcessParams.VignetteColor.g = static_cast<uint8_t>(m_postProcessParams.VignetteColor.g + (kBaseVigColor.g - m_postProcessParams.VignetteColor.g) * decayAlpha);
    m_postProcessParams.VignetteColor.b = static_cast<uint8_t>(m_postProcessParams.VignetteColor.b + (kBaseVigColor.b - m_postProcessParams.VignetteColor.b) * decayAlpha);

    const bool canControlPaddle = (currentState == static_cast<int>(SceneState::Playing) || currentState == static_cast<int>(SceneState::LevelTransition)) && !m_countdownActive;
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(canControlPaddle);
    }

    if (currentState != static_cast<int>(SceneState::Paused))
    {
        m_systemManager.OnUpdate(dt);
    }
    m_registry.ProcessDeferredCommands();
}

void GameScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    const auto& cameraComp = m_registry.GetComponent<Camera2D>(m_camera);
    context.Render.SetCamera(cameraComp);

    Vector2f paddlePos{0.0f, 0.0f};
    if (m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    // Render ambient background layer (behind gameplay entities)
    m_ambientBackground.OnRender(context.Render, m_bgColor, cameraComp.Position, paddlePos);

    m_systemManager.OnRender();

    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{800.0f, -450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{-800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{800.0f, -450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, 450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);

    // Bottom Barrier Laser Forcefield
    if (m_bottomBarrierActive && m_bottomBarrierCharges > 0)
    {
        float pulse = (std::sin(m_gameplayTime * 10.0f) + 1.0f) * 0.5f;
        Color glowColor{0, 190, 255, static_cast<uint8_t>(90 + 70 * pulse)};
        Color coreColor{140, 240, 255, static_cast<uint8_t>(200 + 55 * pulse)};
        context.Render.DrawLine(Vector2f{-800.0f, 442.0f}, Vector2f{800.0f, 442.0f}, glowColor, 6.0f);
        context.Render.DrawLine(Vector2f{-800.0f, 442.0f}, Vector2f{800.0f, 442.0f}, coreColor, 2.5f);
        context.Render.DrawCircle(7.0f, Transform2D{Vector2f{-798.0f, 442.0f}}, coreColor);
        context.Render.DrawCircle(7.0f, Transform2D{Vector2f{798.0f, 442.0f}}, coreColor);
    }

    // Sticky Paddle rhythm indicator for caught balls
    if (!m_stuckBalls.empty())
    {
        for (const auto& sb : m_stuckBalls)
        {
            if (!m_registry.IsAlive(sb.Ball) || !m_registry.HasComponent<Transform2D>(sb.Ball))
                continue;

            const auto& bTrans = m_registry.GetComponent<Transform2D>(sb.Ball);
            const float cycleTime = sb.HoldTimer;
            const float targetBeat = (cycleTime < 1.45f) ? 1.05f : 2.05f;
            const float timeDiff = std::abs(cycleTime - targetBeat);
            const bool isSweetSpot = (timeDiff <= 0.20f);

            // Expanding / shrinking rhythm ring targeting the ball
            const float ringRadius = 20.0f + std::max(0.0f, timeDiff) * 115.0f;
            const Color ringColor = isSweetSpot ? Color{255, 235, 50, 245} : Color{180, 220, 255, 160};
            context.Render.DrawCircleOutline(ringRadius, bTrans, ringColor, isSweetSpot ? -3.0f : -2.0f);

            if (isSweetSpot)
            {
                const float pulse = (std::sin(m_gameplayTime * 18.0f) + 1.0f) * 0.5f;
                context.Render.DrawCircleOutline(27.0f + 5.0f * pulse, bTrans, Color{255, 255, 120, static_cast<uint8_t>(180 + 75 * pulse)}, -2.5f);
            }
        }
    }

    // Magnet Paddle subtle aura rings and ball attraction flux lines
    if (m_magnetDuration > 0.0f && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        const auto& pt = m_registry.GetComponent<Transform2D>(m_paddle);
        float pulse = (std::sin(m_gameplayTime * 7.0f) + 1.0f) * 0.5f;
        Color magnetAura{220, 60, 255, static_cast<uint8_t>(60 + 60 * pulse)};
        context.Render.DrawCircleOutline(65.0f + 10.0f * pulse, pt, magnetAura, -2.0f);
        context.Render.DrawCircleOutline(105.0f + 15.0f * (1.0f - pulse), pt, Color{180, 40, 255, static_cast<uint8_t>(35 + 40 * (1.0f - pulse))}, -1.5f);

        if (mp_ballSystem)
        {
            const float magnetRadius = PowerUpManager::Get().Magnet().MagnetRadius;
            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<Transform2D>(b)) continue;
                const auto& bPos = m_registry.GetComponent<Transform2D>(b).Position;
                float dist = (pt.Position - bPos).Length();
                if (dist > 1.0f && dist <= magnetRadius)
                {
                    float fluxAlpha = (1.0f - dist / magnetRadius) * (0.4f + 0.35f * pulse);
                    uint8_t a = static_cast<uint8_t>(std::clamp(fluxAlpha * 255.0f, 0.0f, 255.0f));
                    context.Render.DrawLine(pt.Position, bPos, Color{230, 80, 255, a}, 2.0f);
                }
            }
        }
    }

    // Visual accents for Metal (riveted steel plate outline) and Bomb (hazard pulsing rim + core)
    m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity, const Transform2D& t, const BoxCollider& b, const BrickComponent& brick)
    {
        if (brick.IsUnbreakable)
        {
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, Color{225, 235, 250, 210}, -1.5f);
            context.Render.DrawRectangleOutline(b.Size.X - 14.0f, b.Size.Y - 10.0f, t, Color{110, 130, 155, 160}, -1.0f);
        }
        else if (brick.IsBomb)
        {
            float pulse = (std::sin(m_gameplayTime * 6.5f) + 1.0f) * 0.5f;
            Color pulseBorder{255, static_cast<uint8_t>(140 + 80 * pulse), 30, static_cast<uint8_t>(180 + 75 * pulse)};
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, pulseBorder, -2.0f);
            context.Render.DrawCircle(5.0f, t, Color{255, 250, 180, static_cast<uint8_t>(200 + 55 * pulse)});
        }
        else if (brick.IsLantern)
        {
            float pulse = (std::sin(m_gameplayTime * 4.5f) + 1.0f) * 0.5f;
            Color lanternBorder{255, 235, 120, static_cast<uint8_t>(180 + 75 * pulse)};
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, lanternBorder, -2.0f);
            context.Render.DrawCircle(6.0f, t, Color{255, 245, 180, static_cast<uint8_t>(200 + 55 * pulse)});
        }
    });

    const bool showDebug = context.Rules.GetRule(Rule::Debug::ShowCollider);
    if (showDebug)
    {
        m_registry.View<Transform2D, BoxCollider>([&](Entity, const Transform2D& t, const BoxCollider& b) {
            const Color color = b.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, color, -2.0f);
        });

        m_registry.View<Transform2D, CircleCollider>([&](Entity, const Transform2D& t, const CircleCollider& c) {
            Color color = c.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawCircleOutline(c.Radius, t, color, -2.0f);
        });
    }

    // Active lightning bolts rendering (Storm power-up) - Batched into high-performance vertex arrays
    if (!m_activeLightningBolts.empty())
    {
        std::vector<Vertex> glowVertices;
        std::vector<Vertex> coreVertices;
        glowVertices.reserve(m_activeLightningBolts.size() * 120);
        coreVertices.reserve(m_activeLightningBolts.size() * 120);

        for (const auto& bolt : m_activeLightningBolts)
        {
            float alphaRatio = std::clamp(bolt.Lifetime / bolt.MaxLifetime, 0.0f, 1.0f);
            uint8_t glowAlpha = static_cast<uint8_t>(190.0f * alphaRatio);
            uint8_t coreAlpha = static_cast<uint8_t>(255.0f * alphaRatio);

            Color glowColor{255, 235, 90, glowAlpha};
            Color coreColor{255, 255, 240, coreAlpha};

            const float glowThick = 5.5f * alphaRatio;
            const float coreThick = 2.0f * alphaRatio;

            auto pushLineQuad = [](std::vector<Vertex>& out, const Vector2f& p1, const Vector2f& p2, float thick, Color c)
            {
                Vector2f d = p2 - p1;
                float len = d.Length();
                if (len < 0.001f) return;
                Vector2f n{-d.Y / len * (thick * 0.5f), d.X / len * (thick * 0.5f)};

                out.push_back(Vertex{p1.X - n.X, p1.Y - n.Y, c, 0.0f, 0.0f});
                out.push_back(Vertex{p2.X - n.X, p2.Y - n.Y, c, 0.0f, 0.0f});
                out.push_back(Vertex{p2.X + n.X, p2.Y + n.Y, c, 0.0f, 0.0f});

                out.push_back(Vertex{p2.X + n.X, p2.Y + n.Y, c, 0.0f, 0.0f});
                out.push_back(Vertex{p1.X + n.X, p1.Y + n.Y, c, 0.0f, 0.0f});
                out.push_back(Vertex{p1.X - n.X, p1.Y - n.Y, c, 0.0f, 0.0f});
            };

            for (const auto& seg : bolt.Segments)
            {
                float jitterX = static_cast<float>((rand() % 7) - 3);
                float jitterY = static_cast<float>((rand() % 7) - 3);
                Vector2f j{jitterX, jitterY};

                pushLineQuad(glowVertices, seg.Start + j, seg.End + j, glowThick, glowColor);
                pushLineQuad(coreVertices, seg.Start, seg.End, coreThick, coreColor);
            }
        }

        if (!glowVertices.empty())
        {
            context.Render.DrawVertices(glowVertices, PrimitiveType::Triangles, 0, BlendMode::Alpha);
        }
        if (!coreVertices.empty())
        {
            context.Render.DrawVertices(coreVertices, PrimitiveType::Triangles, 0, BlendMode::Alpha);
        }
    }

    // Electrified metal bricks visual crackle
    for (const auto& eb : m_electrifiedBricks)
    {
        float ratio = std::clamp(eb.Lifetime / eb.MaxLifetime, 0.0f, 1.0f);
        uint8_t a = static_cast<uint8_t>(230.0f * ratio);
        Color arcCol{100, 225, 255, a};
        Transform2D ebTrans{eb.Position};
        context.Render.DrawRectangleOutline(104.0f, 34.0f, ebTrans, arcCol, 2.5f);
        context.Render.DrawCircle(6.0f * ratio, ebTrans, Color{220, 250, 255, a});
    }

    // 2D Dynamic Lighting Pass
    if (m_currentTheme.HasLighting())
    {
        context.Render.BeginLightingPass(m_currentTheme.AmbientLight);

        // 1. Dynamic light sources (point and cone) from ECS entities (lanterns, standard balls, etc.)
        m_registry.View<Transform2D, LightSourceComponent>([&](Entity e, const Transform2D& t, const LightSourceComponent& light)
        {
            if (e == m_paddle) return; // Handled with size-reactive multi-emission below
            if (!light.IsActive || light.Intensity <= 0.0f || light.Radius <= 0.0f) return;

            if (light.Type == LightType::Point)
            {
                context.Render.DrawPointLight(t.Position, light.Radius, light.LightColor, light.Intensity, light.Blend);
            }
            else if (light.Type == LightType::Cone)
            {
                context.Render.DrawConeLight(t.Position, light.Radius, light.DirectionDeg + t.Rotation, light.ConeAngleDeg, light.LightColor, light.Intensity, light.Blend);
            }
        });

        // 2. Extra radiant fire core for active FireBalls
        if (mp_ballSystem)
        {
            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<BallComponent>(b)) continue;
                const auto& bc = m_registry.GetComponent<BallComponent>(b);
                if (bc.IsFireBall && m_registry.HasComponent<Transform2D>(b))
                {
                    const auto& bPos = m_registry.GetComponent<Transform2D>(b).Position;
                    // White-hot inner core
                    context.Render.DrawPointLight(bPos, 85.0f, Color{255, 240, 160, 255}, 1.7f, BlendMode::Add);
                }
            }
        }

        // 3. Paddle dynamic light emission (proportional to paddle size)
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
        {
            const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const float paddleScaleX = pTrans.Scale.X;
            const float halfWidth = 60.0f * paddleScaleX; // base width is 120.0f
            const Vector2f pLeft{pTrans.Position.X - halfWidth, pTrans.Position.Y};
            const Vector2f pRight{pTrans.Position.X + halfWidth, pTrans.Position.Y};

            // A. Paddle body glow (capsule light spanning the full paddle length)
            const float bodyRadius = 110.0f * std::sqrt(paddleScaleX);
            const float bodyIntensity = 1.35f * std::clamp(paddleScaleX, 0.7f, 1.5f);
            context.Render.DrawCapsuleLight(pLeft, pRight, bodyRadius, Color{190, 235, 255, 255}, bodyIntensity, BlendMode::Add);

            // B. Paddle upward spotlight (widens and reaches further as paddle scales up)
            const float coneAngle = 65.0f + 25.0f * (paddleScaleX - 1.0f);
            const float coneRadius = 420.0f * (0.8f + 0.2f * paddleScaleX);
            const float coneIntensity = 1.3f * std::clamp(paddleScaleX, 0.7f, 1.4f);
            const Vector2f coneApex{pTrans.Position.X, pTrans.Position.Y - 10.0f};
            context.Render.DrawConeLight(coneApex, coneRadius, -90.0f, coneAngle, Color{210, 240, 255, 255}, coneIntensity, BlendMode::Add);

            if (m_stormCharges > 0 || m_stormDuration > 0.0f)
            {
                float strobe = 1.0f;
                if (PowerUpManager::Get().Storm().IsTempest() && m_stormDuration > 0.0f)
                {
                    strobe = 1.0f + 0.45f * std::sin(m_gameplayTime * 25.0f);
                }
                context.Render.DrawCapsuleLight(pLeft, pRight, bodyRadius * 1.6f, Color{255, 240, 120, 255}, bodyIntensity * 1.6f * strobe, BlendMode::Add);
            }
        }

        // 4. Decaying explosion light flashes (bomb detonations, fireball explosions)
        for (const auto& flash : m_lightFlashes)
        {
            if (flash.Intensity > 0.0f && flash.Radius > 0.0f)
            {
                context.Render.DrawPointLight(flash.Position, flash.Radius, flash.LightColor, flash.Intensity, BlendMode::Add);
            }
        }

        // 5. Lightning dynamic illumination (Storm power-up)
        for (const auto& bolt : m_activeLightningBolts)
        {
            float alphaRatio = std::clamp(bolt.Lifetime / bolt.MaxLifetime, 0.0f, 1.0f);
            if (alphaRatio <= 0.0f) continue;

            size_t step = std::max<size_t>(1, bolt.Nodes.size() / 6);
            for (size_t i = 0; i < bolt.Nodes.size(); i += step)
            {
                context.Render.DrawPointLight(bolt.Nodes[i], 240.0f, Color{255, 245, 170, 255}, 2.4f * alphaRatio, BlendMode::Add);
            }
        }

        // 5b. Electrified conductive metal bricks illumination
        for (const auto& eb : m_electrifiedBricks)
        {
            float ratio = std::clamp(eb.Lifetime / eb.MaxLifetime, 0.0f, 1.0f);
            context.Render.DrawPointLight(eb.Position, 260.0f, Color{100, 220, 255, 255}, 2.5f * ratio, BlendMode::Add);
        }

        // Bottom barrier laser illumination along the floor
        if (m_bottomBarrierActive && m_bottomBarrierCharges > 0)
        {
            context.Render.DrawCapsuleLight(Vector2f{-780.0f, 442.0f}, Vector2f{780.0f, 442.0f}, 75.0f, Color{0, 210, 255, 255}, 1.3f, BlendMode::Add);
        }

        context.Render.EndLightingPass(BlendMode::Multiply);
    }

    context.Render.ResetCamera();

    std::string gameStateStr = "PLAY";
    if (mp_state_machine)
    {
        int state = mp_state_machine->GetCurrentState();
        if (state == static_cast<int>(SceneState::Paused)) gameStateStr = "PAUSED";
        else if (state == static_cast<int>(SceneState::GameOver)) gameStateStr = "GAME OVER - PRESS SPACE TO REPLAY";
        else if (state == static_cast<int>(SceneState::Victory)) gameStateStr = "VICTORY - ALL LEVELS CLEARED! PRESS SPACE TO RESET";
        else if (state == static_cast<int>(SceneState::LevelTransition)) gameStateStr = "STAGE CLEAR! ADVANCING TO LEVEL " + std::to_string(m_levelManager.GetCurrentLevelNumber()) + "...";
        else if (state == static_cast<int>(SceneState::Countdown))
        {
            if (m_countdownActive)
                gameStateStr = (m_countdownLastSecond == 4) ? "READY..." : ("READY... " + std::to_string(m_countdownLastSecond));
            else
                gameStateStr = "READY... GO!";
        }
    }

    const bool modalOpen = (m_uiController && m_uiController->IsModalOrSubMenuOpen());
    if (showDebug && !modalOpen)
    {
        std::string debugStr = showDebug ? "ON" : "OFF";
        std::string shaderStr = context.Rules.GetRule(Rule::Graphics::EnableShader) ? "ON" : "OFF";
        std::string invStr = context.Rules.GetRule(Rule::Gameplay::Invincible) ? "ON" : "OFF";
        std::string infLivesStr = context.Rules.GetRule(Rule::Gameplay::InfiniteLives) ? "ON" : "OFF";

        std::string stats = "Level: " + std::to_string(m_levelManager.GetCurrentLevelNumber()) + " / " + std::to_string(m_levelManager.GetLevelCount()) +
                            " (Unlocked: " + std::to_string(m_levelManager.GetUnlockedLevel()) + ")" +
                            " | Theme: " + m_currentTheme.Name;
        stats += "\nLevel Best: " + std::to_string(m_levelManager.GetHighScoreForLevel(m_levelManager.GetCurrentLevelNumber())) +
                 " | Global Best: " + std::to_string(m_scoreManager.GetHighScore());
        stats += "\nDebug (G) : " + debugStr + " | Shader (F) : " + shaderStr;
        stats += "\nGod mod (I) : " + invStr + " | Infinite lives (L) : " + infLivesStr;
        stats += "\nState : " + gameStateStr;
        if (m_uiController && m_uiController->IsPowerUpTesterActive())
        {
            const auto& paddleBox = m_registry.GetComponent<BoxCollider>(m_paddle);
            const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
            stats += "\n[POWER-UP DEBUG ON] Lv:" + std::to_string(PowerUpManager::Get().GetLevel(PowerUpType::MultiBall)) +
                     " | Balls: " + std::to_string(mp_ballSystem ? mp_ballSystem->GetBallCount() : 0) +
                     " | Paddle: " + std::to_string(static_cast<int>(paddleBox.GetEffectiveSize(paddleTrans.Scale).X)) + "px" +
                     " | Laser: " + (paddleComp.HasLaser ? "ON" : "OFF") +
                     " | Keys: 1-7, P, O, R, B, U | [TAB] Hide";
        }
        else
        {
            stats += "\n[TAB] Open Power-Up Tester Tool";
        }

        DrawDefaultUI(context, "REBREAKER", stats);
    }

    if (m_renderGameUI)
    {
        if (m_uiController)
        {
            m_uiController->RenderCountdown(context);
        }

        UISystem::OnRender(m_registry, context);
        if (m_uiController)
        {
            m_uiController->OnRender(context);
        }
    }
}

uint32_t GameScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
}

PostProcessParams GameScene::GetPostProcessParams() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_postProcessParams;
    }
    return PostProcessParams{};
}

void GameScene::TriggerPostProcessFlash(float caIntensity, float vignetteIntensity, const Color& vignetteColor, float flashIntensity, const Color& flashColor)
{
    m_postProcessParams.ChromaticAberration = std::max(m_postProcessParams.ChromaticAberration, caIntensity);
    m_postProcessParams.VignetteIntensity = std::max(m_postProcessParams.VignetteIntensity, vignetteIntensity);
    m_postProcessParams.VignetteColor = vignetteColor;
    m_postProcessParams.FlashIntensity = std::max(m_postProcessParams.FlashIntensity, flashIntensity);
    m_postProcessParams.FlashColor = flashColor;
}

void GameScene::UpdateComboBackgroundColor(uint32_t comboMultiplier)
{
    if (comboMultiplier == m_currentComboMultiplier) return;
    m_currentComboMultiplier = comboMultiplier;

    Color targetColor;
    float duration = 3.5f;

    if (comboMultiplier <= 1)
    {
        targetColor = m_currentTheme.ComboColors[0];
        duration = 3.5f;
    }
    else if (comboMultiplier == 2)
    {
        targetColor = m_currentTheme.ComboColors[1];
        duration = 2.5f;
    }
    else if (comboMultiplier == 3)
    {
        targetColor = m_currentTheme.ComboColors[2];
        duration = 2.0f;
    }
    else if (comboMultiplier == 4)
    {
        targetColor = m_currentTheme.ComboColors[3];
        duration = 1.6f;
    }
    else
    {
        targetColor = m_currentTheme.ComboColors[4];
        duration = 1.2f;
    }

    if (m_registry.IsAlive(m_bgTweenEntity) && m_registry.HasComponent<TweenComponent>(m_bgTweenEntity))
    {
        auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_bgTweenEntity);
        tweenComp.Clear();
        TweenEffects::BackgroundColorShift(
            tweenComp,
            m_bgColor,
            targetColor,
            [this](Color c) { m_bgColor = c; },
            duration
        );
    }
}

void GameScene::ApplyLevelTheme(int levelNumber)
{
    m_currentTheme = LevelThemeCatalog::GetThemeForLevel(levelNumber);
    m_ambientBackground.SetTheme(m_currentTheme);

    // Apply baseline theme vignette
    m_postProcessParams.VignetteColor = m_currentTheme.VignetteBaseColor;
    m_postProcessParams.VignetteIntensity = m_currentTheme.VignetteBaseIntensity;

    // Smoothly shift to new level base background palette
    if (m_registry.IsAlive(m_bgTweenEntity) && m_registry.HasComponent<TweenComponent>(m_bgTweenEntity))
    {
        auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_bgTweenEntity);
        tweenComp.Clear();
        TweenEffects::BackgroundColorShift(
            tweenComp,
            m_currentTheme.BgBase1,
            m_currentTheme.BgBase2,
            [this](Color c) { m_bgColor = c; },
            3.5f
        );
    }
    else
    {
        m_bgColor = m_currentTheme.BgBase1;
    }

    m_currentComboMultiplier = 0;
}

Entity GameScene::CreateWall(const float x, const float y, const float w, const float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});

    return wall;
}

void GameScene::SpawnBleedParticles(const Vector2f& position)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }
    if (!m_particleSystem) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    
    // Throttle bleed particles
    if (chance(rng) > 0.75f)
    {
        std::uniform_real_distribution<float> posOffsetX(-8.0f, 8.0f);
        std::uniform_real_distribution<float> posOffsetY(-8.0f, 8.0f);
        std::uniform_real_distribution<float> velDist(-120.0f, 120.0f);
        
        Vector2f spawnPos{position.X + posOffsetX(rng), position.Y + posOffsetY(rng)};
        Vector2f vel{velDist(rng), velDist(rng)};
        
        m_particleSystem->Spawn(spawnPos, vel, 0.35f, 8.0f, Colors::Red);
    }
}

void GameScene::SpawnExplosionParticles(const Vector2f& position, const Color& color, const int count)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }
    if (!m_particleSystem) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> velDistX(-200.0f, 200.0f);
    std::uniform_real_distribution<float> velDistY(-200.0f, 200.0f);
    std::uniform_real_distribution<float> lifeDist(0.25f, 0.45f);
    std::uniform_real_distribution<float> sizeDist(3.0f, 7.0f);
    std::uniform_real_distribution<float> posOffsetX(-12.0f, 12.0f);
    std::uniform_real_distribution<float> posOffsetY(-8.0f, 8.0f);

    int actualCount = std::min(count, 12);
    for (int i = 0; i < actualCount; ++i)
    {
        Vector2f spawnPos{position.X + posOffsetX(rng), position.Y + posOffsetY(rng)};
        Vector2f vel{velDistX(rng), velDistY(rng)};
        float life = lifeDist(rng);
        float size = sizeDist(rng);

        m_particleSystem->Spawn(spawnPos, vel, life, size, color);
    }
}

void GameScene::SpawnFireTrailParticle(const Vector2f& position, const Vector2f& ballVelocity, bool isFuseActive)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }
    if (!m_particleSystem) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> offsetDist(-8.0f, 8.0f);
    std::uniform_real_distribution<float> velTurbDist(-60.0f, 60.0f);
    std::uniform_real_distribution<float> lifeDist(0.18f, isFuseActive ? 0.35f : 0.28f);
    std::uniform_real_distribution<float> sizeDist(isFuseActive ? 8.0f : 5.0f, isFuseActive ? 16.0f : 12.0f);
    std::uniform_int_distribution<int> colorPick(0, 2);

    Vector2f spawnPos{position.X + offsetDist(rng), position.Y + offsetDist(rng)};
    Vector2f pVel{-ballVelocity.X * 0.15f + velTurbDist(rng), -ballVelocity.Y * 0.15f + velTurbDist(rng)};

    Color c;
    int cp = colorPick(rng);
    if (cp == 0)      c = Color{255, 230, 70, 255};
    else if (cp == 1) c = Color{255, 120, 20, 255};
    else              c = Color{220, 40, 10, 255};

    m_particleSystem->Spawn(spawnPos, pVel, lifeDist(rng), sizeDist(rng), c);
}

void GameScene::SpawnFireExplosionParticles(const Vector2f& position, float radius, int count)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }
    if (!m_particleSystem) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(150.0f, 550.0f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.75f);
    std::uniform_real_distribution<float> sizeDist(8.0f, 22.0f);
    std::uniform_real_distribution<float> offsetDist(0.0f, radius * 0.25f);
    std::uniform_int_distribution<int> colorPick(0, 3);

    for (int i = 0; i < count; ++i)
    {
        float angle = angleDist(rng);
        float speed = speedDist(rng);
        float offset = offsetDist(rng);

        Vector2f spawnPos{
            position.X + std::cos(angle) * offset,
            position.Y + std::sin(angle) * offset
        };

        Color c;
        int cp = colorPick(rng);
        if (cp == 0)      c = Color{255, 255, 180, 255};
        else if (cp == 1) c = Color{255, 180, 30, 255};
        else if (cp == 2) c = Color{240, 50, 20, 255};
        else              c = Color{120, 30, 10, 220};

        Vector2f vel{std::cos(angle) * speed, std::sin(angle) * speed};
        m_particleSystem->Spawn(spawnPos, vel, lifeDist(rng), sizeDist(rng), c);
    }
}

void GameScene::ExplodeFireBall(Entity ballEntity, const Vector2f& explosionCenter)
{
    const auto& cfg = PowerUpManager::Get().FireBall();
    const float aoeRadius = cfg.GetEffectiveAoERadius();
    const float aoeRadiusSq = aoeRadius * aoeRadius;

    std::vector<Entity> affectedBricks;
    m_registry.View<Transform2D, BrickComponent>([&](Entity brickEntity, const Transform2D& trans, const BrickComponent&)
    {
        float dx = trans.Position.X - explosionCenter.X;
        float dy = trans.Position.Y - explosionCenter.Y;
        if (dx * dx + dy * dy <= aoeRadiusSq)
        {
            affectedBricks.push_back(brickEntity);
        }
    });

    for (Entity brickEntity : affectedBricks)
    {
        if (!m_registry.IsAlive(brickEntity) || m_registry.IsPendingDestroy(brickEntity)) continue;
        auto& brick = m_registry.GetComponent<BrickComponent>(brickEntity);
        brick.HitPoints = 0;
        HandleBrickCollision(brickEntity, NULL_ENTITY, /*isLaser=*/false, /*isExplosion=*/true);
    }

    SpawnFireExplosionParticles(explosionCenter, aoeRadius, 45);
    SpawnLightFlash(explosionCenter, 500.0f, Color{255, 170, 60, 255}, 2.2f, 0.45f);

    if (mp_context)
    {
        mp_context->Audio.TriggerDucking(0.45f, 0.5f);
        if (m_fireballExplosionSfxId != 0)
            mp_context->Audio.PlaySfx(m_fireballExplosionSfxId, 100.0f);
        else if (m_explosionSfxId != 0)
            mp_context->Audio.PlaySfx(m_explosionSfxId, 100.0f);
        else if (m_despawnSfxId != 0)
            mp_context->Audio.PlaySfx(m_despawnSfxId, 90.0f);
    }

    TweenEffects::Shake(m_registry, m_camera, 0.3f, 16.0f);
    TriggerPostProcessFlash(0.028f, 0.70f, Color{255, 120, 20, 255}, 0.28f, Color{255, 180, 50, 255});

    if (m_registry.IsAlive(ballEntity) && m_registry.HasComponent<BallComponent>(ballEntity))
    {
        auto& bc = m_registry.GetComponent<BallComponent>(ballEntity);
        bc.IsFireBall = false;
        bc.IsFuseActive = false;
        bc.FuseTimer = 0.0f;
        bc.TrailTimer = 0.0f;

        if (m_registry.HasComponent<Transform2D>(ballEntity))
        {
            float targetScale = 1.0f;
            if (m_bigBallDuration > 0.0f)
            {
                targetScale = PowerUpManager::Get().Big().GetEffectiveScale();
            }
            m_registry.GetComponent<Transform2D>(ballEntity).Scale = Vector2f{targetScale, targetScale};
        }

        if (m_registry.HasComponent<SpriteComponent>(ballEntity))
        {
            Color targetColor = Colors::White;
            if (m_tempoBallDuration > 0.0f)
            {
                targetColor = Color{210, 160, 255, 255};
            }
            m_registry.GetComponent<SpriteComponent>(ballEntity).Tint = targetColor;
        }

        if (m_registry.HasComponent<CircleCollider>(ballEntity))
        {
            m_registry.GetComponent<CircleCollider>(ballEntity).Radius = 20.0f;
        }
    }
}

void GameScene::OnDestroy(GameContext& context)
{
    if (m_fireballBurnSfxId != 0)
    {
        context.Audio.StopLoopingSfx(m_fireballBurnSfxId);
    }
    m_uiController.reset();
    mp_state_machine.reset();
    mp_levelGenerator.reset();
    m_collisionSub.Reset();
    m_powerUpSub.Reset();
    m_nextLevelSub.Reset();

    DefaultScene::OnDestroy(context);
}

int GameScene::GetCurrentLevel() const
{
    return m_levelManager.GetCurrentLevelNumber();
}

int GameScene::GetLevelCount() const
{
    return m_levelManager.GetLevelCount();
}

bool GameScene::HasNextLevel() const
{
    return m_levelManager.HasNextLevel();
}

void GameScene::LoadLevel(int levelIndex, bool preserveStats)
{
    m_levelManager.SetLevel(levelIndex);
    ApplyLevelTheme(m_levelManager.GetCurrentLevelNumber());

    if (!preserveStats)
    {
        m_scoreManager.Reset();
        m_lives = PlayerPrefs::GetInt("Lives", 3);
        m_levelStartScore = 0;
    }
    else
    {
        m_levelStartScore = m_scoreManager.GetScore();
    }

    if (mp_context)
    {
        mp_context->Events.Publish(LivesChangedEvent(m_lives));
        mp_context->Events.Publish(ScoreChangedEvent(m_scoreManager.GetScore(), m_scoreManager.GetComboMultiplier()));
    }

    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    m_registry.View<PowerUpComponent>([this](const Entity e, PowerUpComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    mp_levelGenerator = std::make_unique<FileLevelGenerator>(m_levelManager.GetCurrentLevelPath());
    if (mp_context)
    {
        m_brickCount = mp_levelGenerator->Generate(m_registry, *mp_context, m_brickTexId);
    }

    ResetBallAndPaddle();
}

void GameScene::AdvanceToNextLevel()
{
    m_scoreManager.BreakCombo();

    int curLevel = m_levelManager.GetCurrentLevelNumber();
    m_levelManager.SetHighScoreForLevel(curLevel, m_scoreManager.GetScore());
    m_levelManager.SetHighestClearedLevel(curLevel);

    int nextLevel = curLevel + 1;
    m_levelManager.UnlockLevel(nextLevel);
    m_levelManager.NextLevel();

    LoadLevel(m_levelManager.GetCurrentLevelIndex(), /*preserveStats=*/true);

    m_levelTransitionTimer = 10.0f;
}

void GameScene::StartLevelTransition()
{
    if (mp_context)
    {
        mp_context->Audio.FadeOutMusic(0.8f);
        if (m_levelClearSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_levelClearSfxId, 100.0f);
        }
    }

    AdvanceToNextLevel();
    m_levelTransitionTimer = 10.0f;
}

void GameScene::UpdateLevelTransition()
{
}

void GameScene::CompleteLevelTransition()
{
    m_levelTransitionTimer = 0.0f;
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context, 1.5f);
    }
    if (!m_countdownActive)
    {
        StartCountdown();
    }
}

void GameScene::StartCountdown()
{
    m_countdownTimer = 4.0f;
    m_countdownGoTimer = 0.0f;
    m_countdownLastSecond = 4;
    m_countdownWhooshPlayedForSecond = 0;
    m_countdownActive = true;

    if (mp_state_machine && mp_state_machine->GetCurrentState() != static_cast<int>(SceneState::Countdown))
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Countdown));
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(false);
    }
    if (mp_context)
    {
        mp_context->Events.Publish(GameStateChangedEvent(GameUIState::Countdown));
        if (m_countdownReadySfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_countdownReadySfxId, 100.0f);
        }
    }
}

bool GameScene::IsLevelTransitionComplete() const
{
    return m_levelTransitionTimer <= 0.0f || (mp_context && mp_context->Input.IsKeyPress(KeyCode::Space));
}

void GameScene::OnGameOverEnter()
{
    m_scoreManager.BreakCombo();

    if (mp_context)
    {
        // Smoothly fade out stage music and fade in game-over stinger
        mp_context->Audio.FadeToMusic("Resources/audio/music/Game-death.ogg", 0.6f, 50.0f, false);
        if (m_gameOverSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_gameOverSfxId, 100.0f);
        }
    }

    if (m_uiController)
    {
        uint32_t finalScore = m_scoreManager.GetScore();
        uint32_t earnedCredits = ProfileManager::ConvertScoreToCredits(finalScore);
        PowerUpManager::Get().AddCoins(static_cast<int>(earnedCredits));
        ProfileManager::Get().AddLifetimeScore(finalScore);
        ProfileManager::Get().AddLifetimeCredits(earnedCredits);

        m_levelManager.SetHighScoreForLevel(m_levelManager.GetCurrentLevelNumber(), finalScore);
        m_uiController->SetupGameOver(finalScore, m_levelManager.GetCurrentLevelNumber());
    }
}

void GameScene::OnGameOverUpdate()
{
}

void GameScene::OnGameOverExit()
{
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context, 1.2f);
    }
    FullReset();
}

void GameScene::OnVictoryEnter()
{
    m_scoreManager.BreakCombo();

    int curLevel = m_levelManager.GetCurrentLevelNumber();
    uint32_t finalScore = m_scoreManager.GetScore();
    uint32_t earnedCredits = ProfileManager::ConvertScoreToCredits(finalScore);
    PowerUpManager::Get().AddCoins(static_cast<int>(earnedCredits));
    ProfileManager::Get().AddLifetimeScore(finalScore);
    ProfileManager::Get().AddLifetimeCredits(earnedCredits);

    m_levelManager.SetHighScoreForLevel(curLevel, finalScore);
    m_levelManager.SetHighestClearedLevel(curLevel);

    if (mp_context)
    {
        // Smoothly fade out stage music and fade in victory stinger
        mp_context->Audio.FadeToMusic("Resources/audio/sfx/victory.mp3", 0.6f, 85.0f, false);
    }

    if (m_uiController)
    {
        m_uiController->SetupVictory(finalScore);
    }
}

void GameScene::OnVictoryUpdate()
{
}

void GameScene::OnVictoryExit()
{
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context, 1.2f);
    }
    m_levelManager.ResetToFirstLevel();
    LoadLevel(0, /*preserveStats=*/true);
    StartCountdown();
}

bool GameScene::CanUnpause() const
{
    if (m_uiController && m_uiController->IsModalOrSubMenuOpen())
    {
        return false;
    }
    return true;
}

bool GameScene::IsTypingName()
{
    return m_uiController && m_uiController->IsTypingName();
}

bool GameScene::TryConfirmNameBeforeProceeding()
{
    return m_uiController ? m_uiController->TryConfirmNameBeforeProceeding() : true;
}

void GameScene::PromptEnterName()
{
    if (m_uiController)
    {
        m_uiController->PromptEnterName();
    }
}

void GameScene::ResumeGame()
{
    if (mp_state_machine)
    {
        if (m_countdownActive)
        {
            mp_state_machine->SetState(static_cast<int>(SceneState::Countdown));
        }
        else
        {
            mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
        }
    }
}

void GameScene::OnReplayRequested()
{
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Countdown));
    }
    else
    {
        FullReset();
    }
}

void GameScene::FullReset()
{
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context, 1.0f);
    }
    m_levelManager.ResetToFirstLevel();
    LoadLevel(0, /*preserveStats=*/false);
    StartCountdown();
}

void GameScene::HandleInput(const float /*dt*/, const GameContext& context)
{
    if (context.Input.IsKeyPress(KeyCode::F))
    {
        bool shader = context.Rules.GetRule(Rule::Graphics::EnableShader);
        context.Rules.SetRule(Rule::Graphics::EnableShader, !shader);
    }
    if (context.Input.IsKeyPress(KeyCode::G))
    {
        const bool debug = context.Rules.GetRule(Rule::Debug::ShowCollider);
        context.Rules.SetRule(Rule::Debug::ShowCollider, !debug);
    }

    if (context.Input.IsKeyPress(KeyCode::I))
    {
        const bool invincible = context.Rules.GetRule(Rule::Gameplay::Invincible);
        context.Rules.SetRule(Rule::Gameplay::Invincible, !invincible);
    }

    if (context.Input.IsKeyPress(KeyCode::L))
    {
        const bool infLives = context.Rules.GetRule(Rule::Gameplay::InfiniteLives);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, !infLives);
    }

    if (context.Input.IsKeyPress(KeyCode::F1) || context.Input.IsKeyPress(KeyCode::F10))
    {
        context.Scenes.LoadScene<PreviewScene>();
        return;
    }

    bool testerActive = m_uiController && m_uiController->IsPowerUpTesterActive();

    const bool toggleTester = context.Input.IsKeyPress(KeyCode::Tab) ||
                              context.Input.IsKeyPress(KeyCode::T) ||
                              context.Input.IsJoystickButtonPress(0, 9) || // R3 (Right Stick Click)
                              context.Input.IsJoystickButtonPress(0, 6);   // Back / Select
    if (toggleTester)
    {
        if (m_uiController) m_uiController->TogglePowerUpTester();
        testerActive = m_uiController && m_uiController->IsPowerUpTesterActive();
    }

    if (testerActive && m_uiController)
    {
        // 2nd joystick (Right Stick Y) or Bumpers (LB / RB) navigation without impacting paddle
        if (context.Input.IsGamepadRightStickUpTriggered(0) || context.Input.IsGamepadLeftBumperTriggered(0))
        {
            m_uiController->NavigatePowerUpTester(-1);
        }
        else if (context.Input.IsGamepadRightStickDownTriggered(0) || context.Input.IsGamepadRightBumperTriggered(0))
        {
            m_uiController->NavigatePowerUpTester(1);
        }

        // Apply/Trigger selected power-up with Gamepad Y (Button 3) or Enter
        if (context.Input.IsJoystickButtonPress(0, 3) || context.Input.IsKeyPress(KeyCode::Enter))
        {
            m_uiController->ActivatePowerUpTesterSelected();
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Numpad1) || (testerActive && context.Input.IsKeyPress(KeyCode::Num1))) ApplyPowerUp(PowerUpType::MultiBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad2) || (testerActive && context.Input.IsKeyPress(KeyCode::Num2))) ApplyPowerUp(PowerUpType::ExpandPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad3) || (testerActive && context.Input.IsKeyPress(KeyCode::Num3))) ApplyPowerUp(PowerUpType::ShrinkPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad4) || (testerActive && context.Input.IsKeyPress(KeyCode::Num4))) ApplyPowerUp(PowerUpType::Storm);
    if (context.Input.IsKeyPress(KeyCode::Numpad5) || (testerActive && context.Input.IsKeyPress(KeyCode::Num5))) ApplyPowerUp(PowerUpType::TempoBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad6) || (testerActive && context.Input.IsKeyPress(KeyCode::Num6))) ApplyPowerUp(PowerUpType::ExtraLife);
    if (context.Input.IsKeyPress(KeyCode::Numpad7) || (testerActive && context.Input.IsKeyPress(KeyCode::Num7))) ApplyPowerUp(PowerUpType::BigBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad8) || (testerActive && context.Input.IsKeyPress(KeyCode::Num8))) ApplyPowerUp(PowerUpType::FireBall);
    if (testerActive && context.Input.IsKeyPress(KeyCode::Num9)) ApplyPowerUp(PowerUpType::StickyPaddle);
    if (testerActive && context.Input.IsKeyPress(KeyCode::Num0)) ApplyPowerUp(PowerUpType::BottomBarrier);
    if (testerActive && context.Input.IsKeyPress(KeyCode::M)) ApplyPowerUp(PowerUpType::MagnetPaddle);
    if (testerActive && context.Input.IsKeyPress(KeyCode::H)) ApplyPowerUp(PowerUpType::GhostBall);
    if ((testerActive && context.Input.IsKeyPress(KeyCode::P)))
    {
        SpawnPowerUpAtPaddle();
    }
    if (context.Input.IsKeyPress(KeyCode::Numpad9) || (testerActive && context.Input.IsKeyPress(KeyCode::O)))
    {
        SpawnAllPowerUps();
    }
    if ((testerActive && context.Input.IsKeyPress(KeyCode::R)))
    {
        ResetBallAndPaddle(true);
    }
    if (context.Input.IsKeyPress(KeyCode::Numpad0) || (testerActive && context.Input.IsKeyPress(KeyCode::B)))
    {
        RespawnBricks();
    }
    if ((testerActive || context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked)) && context.Input.IsKeyPress(KeyCode::N))
    {
        AdvanceToNextLevel();
    }
    if (testerActive && context.Input.IsKeyPress(KeyCode::U))
    {
        int curLvl = PowerUpManager::Get().GetLevel(PowerUpType::MultiBall);
        int nextLvl = (curLvl % 5) + 1;
        PowerUpManager::Get().SetLevel(PowerUpType::MultiBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::ExpandPaddle, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::ShrinkPaddle, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::Storm, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::TempoBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::ExtraLife, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::BigBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::FireBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::StickyPaddle, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::BottomBarrier, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::MagnetPaddle, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::GhostBall, nextLvl);
    }
}

void GameScene::ResetBallAndPaddle(bool smooth)
{
    // Clean up extra balls and lasers
    if (mp_ballSystem)
    {
        mp_ballSystem->ClearBalls(m_ball);
        mp_ballSystem->ClearLasers();
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->DetachBall();
    }

    if (!m_registry.IsAlive(m_ball))
    {
        m_ball = CreateBall(Vector2f{0.0f, 0.0f}, Vector2f{0.0f, 0.0f});
    }
    else if (mp_ballSystem)
    {
        mp_ballSystem->RegisterBall(m_ball);
    }

    // Clean up falling power-ups
    m_registry.View<PowerUpComponent>([this](Entity e, PowerUpComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });

    // Reset power-up durations & stacks
    m_paddleSizeDuration = 0.0f;
    m_isShrinkPaddle = false;
    m_stormDuration = 0.0f;
    m_stormCharges = 0;
    m_laserDuration = 0.0f;
    m_activeLightningBolts.clear();
    m_electrifiedBricks.clear();
    m_tempoBallDuration = 0.0f;
    m_tempoBallStacks = 0;
    m_bigBallDuration = 0.0f;
    m_stickyPaddleDuration = 0.0f;
    m_bottomBarrierDuration = 0.0f;
    m_bottomBarrierCharges = 0;
    m_bottomBarrierActive = false;
    m_magnetDuration = 0.0f;
    m_ghostBallDuration = 0.0f;
    m_ghostBallPendingExpiry = false;
    m_stuckBalls.clear();
    if (mp_context && m_fireballBurnSfxId != 0)
    {
        mp_context->Audio.StopLoopingSfx(m_fireballBurnSfxId);
    }
    if (mp_ballSystem)
    {
        mp_ballSystem->SetTempoBallActive(false);
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetLaunchSpeed(640.0f);
    }
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
    {
        UpdatePaddleSpeed();
    }
    if (m_registry.IsAlive(m_ball))
    {
        if (m_registry.HasComponent<TrailComponent>(m_ball))
        {
            auto& tc = m_registry.GetComponent<TrailComponent>(m_ball);
            tc.Clear();
            tc.Config = TrailPresets::GetPreset(mp_ballSystem ? mp_ballSystem->GetBallTrailCosmetic() : TrailCosmeticId::DefaultNeon);
        }
        if (m_registry.HasComponent<BallComponent>(m_ball))
        {
            auto& bc = m_registry.GetComponent<BallComponent>(m_ball);
            bc.IsBig = false;
            bc.IsFireBall = false;
            bc.IsFuseActive = false;
            bc.FuseTimer = 0.0f;
            bc.TrailTimer = 0.0f;
            bc.IsGhost = false;
            bc.IsStuck = false;
            bc.StuckOffsetX = 0.0f;
        }
        if (m_registry.HasComponent<CircleCollider>(m_ball))
            m_registry.GetComponent<CircleCollider>(m_ball).Radius = 20.0f;
        if (m_registry.HasComponent<Transform2D>(m_ball))
            m_registry.GetComponent<Transform2D>(m_ball).Scale = Vector2f{1.0f, 1.0f};
        if (m_registry.HasComponent<SpriteComponent>(m_ball))
            m_registry.GetComponent<SpriteComponent>(m_ball).Tint = Colors::White;
    }

    // Reset paddle size, scale, and laser status
    auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    paddleTransform.Scale = Vector2f{1.0f, 1.0f};
    paddleCollider.Size.X = 120.0f;
    if (m_registry.HasComponent<PaddleComponent>(m_paddle))
    {
        auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
        paddleComp.HasStorm = false;
        paddleComp.StormCooldown = 0.0f;
        paddleComp.HasLaser = false;
        paddleComp.HasSticky = false;
        paddleComp.HasMagnet = false;
        paddleComp.LaserCooldown = 0.0f;
    }
    if (m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }

    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
    ballRb.Velocity = Vector2f{0.0f, 0.0f};
    ballRb.IsKinematic = true;
    m_ballState = BallState::Spawning;
    ballTransform.Scale = Vector2f{0.0f, 0.0f};

    if (!m_registry.HasComponent<TweenComponent>(m_ball)) {
        m_registry.AddComponent<TweenComponent>(m_ball, TweenComponent{});
    }

    auto attachAndSpawnBall = [this]() {
        if (!m_registry.IsAlive(m_paddle) || !m_registry.IsAlive(m_ball)) return;
        auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        auto& bTrans = m_registry.GetComponent<Transform2D>(m_ball);
        bTrans.Position = Vector2f{pTrans.Position.X, pTrans.Position.Y - 40.0f};
        bTrans.Scale = Vector2f{0.0f, 0.0f};

        TweenEffects::BallIn(m_registry.GetComponent<TweenComponent>(m_ball), m_registry, m_ball, [this]() {
            m_ballState = BallState::Attached;
            if (mp_paddleSystem)
            {
                mp_paddleSystem->AttachBall(m_paddle, m_ball);
            }
        }, 0.8f);
    };

    if (smooth && m_registry.HasComponent<TweenComponent>(m_paddle))
    {
        Vector2f startPos = paddleTransform.Position;
        Vector2f targetPos{0.0f, 300.0f};
        TweenConfig<Vector2f> paddleTween;
        paddleTween.Start = startPos;
        paddleTween.End = targetPos;
        paddleTween.Duration = 0.6f;
        paddleTween.Ease = EasingFunctions::EasingType::EaseOutQuad;
        paddleTween.Setter = [this](Vector2f pos) {
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle)) {
                m_registry.GetComponent<Transform2D>(m_paddle).Position = pos;
            }
        };
        paddleTween.OnComplete = attachAndSpawnBall;
        m_registry.GetComponent<TweenComponent>(m_paddle).AddTween(paddleTween);
    }
    else
    {
        paddleTransform.Position = Vector2f{0.0f, 300.0f};
        attachAndSpawnBall();
    }
}

void GameScene::HandleDeath()
{
    if (m_ghostBallDuration > 0.0f) return;
    if (m_ballState == BallState::Dying) return;
    m_ballState = BallState::Dying;

    // Dramatic red chromatic aberration and vignette flash on ball lost
    TriggerPostProcessFlash(0.035f, 0.85f, Color{220, 20, 20, 255}, 0.35f, Color{190, 25, 25, 255});

    m_scoreManager.BreakCombo();
    mp_context->Events.Publish(BallDeathEvent(m_ball));

    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    ballRb.Velocity = Vector2f{0.0f, 0.0f};
    ballRb.IsKinematic = true;

    if (!m_registry.HasComponent<TweenComponent>(m_ball)) {
        m_registry.AddComponent<TweenComponent>(m_ball, TweenComponent{});
    }

    auto& transform = m_registry.GetComponent<Transform2D>(m_ball);

    TweenConfig<Vector2f> scaleTween;
    scaleTween.Start = transform.Scale;
    scaleTween.End = Vector2f{0.0f, 0.0f};
    scaleTween.Duration = 0.5f;
    scaleTween.Ease = EasingFunctions::EasingType::EaseInBack;

    Entity ballEntity = m_ball;
    scaleTween.Setter = [this, ballEntity](Vector2f val) {
        if (m_registry.HasComponent<Transform2D>(ballEntity)) {
            m_registry.GetComponent<Transform2D>(ballEntity).Scale = val;
        }
    };

    scaleTween.OnComplete = [this]() {
        if (m_registry.HasComponent<Transform2D>(m_ball) && m_registry.HasComponent<SpriteComponent>(m_ball)) {
            const auto& t = m_registry.GetComponent<Transform2D>(m_ball);
            const auto& s = m_registry.GetComponent<SpriteComponent>(m_ball);
            SpawnExplosionParticles(t.Position, s.Tint);
        }

        bool infiniteLives = mp_context ? mp_context->Rules.GetRule(Rule::Gameplay::InfiniteLives) : false;

        if (!infiniteLives)
        {
            if (m_uiController)
            {
                m_uiController->TriggerLifeLostAnimation(m_lives - 1);
            }
            m_lives--;
            if (mp_context)
            {
                mp_context->Events.Publish(LivesChangedEvent(m_lives));
            }
        }

        if (m_lives > 0 || infiniteLives)
        {
            ResetBallAndPaddle(true);
        }
    };

    m_registry.GetComponent<TweenComponent>(m_ball).AddTween(scaleTween);
}

void GameScene::DetonateBombBrick(Entity initialBomb)
{
    if (!m_registry.IsAlive(initialBomb) || m_registry.IsPendingDestroy(initialBomb))
        return;

    std::queue<Entity> bombQueue;
    std::unordered_set<Entity> processedBombs;

    bombQueue.push(initialBomb);
    processedBombs.insert(initialBomb);

    if (mp_context)
    {
        mp_context->Audio.TriggerDucking(0.45f, 0.5f);
        if (m_explosionSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_explosionSfxId, 100.0f);
        }
    }

    TriggerPostProcessFlash(0.025f, 0.55f, Color{255, 140, 40, 255}, 0.25f, Color{255, 60, 20, 255});
    TweenEffects::Shake(m_registry, m_camera, 0.25f, 12.0f);

    while (!bombQueue.empty())
    {
        Entity currentBomb = bombQueue.front();
        bombQueue.pop();

        if (!m_registry.IsAlive(currentBomb)) continue;

        Vector2f bombPos{0.0f, 0.0f};
        if (m_registry.HasComponent<Transform2D>(currentBomb))
        {
            bombPos = m_registry.GetComponent<Transform2D>(currentBomb).Position;
        }

        SpawnFireExplosionParticles(bombPos, 85.0f, 35);
        SpawnLightFlash(bombPos, 650.0f, Color{255, 210, 110, 255}, 2.6f, 0.55f);

        if (m_registry.HasComponent<BrickComponent>(currentBomb))
        {
            auto& bombComp = m_registry.GetComponent<BrickComponent>(currentBomb);
            m_scoreManager.AddScore(bombComp.ScoreValue);
            m_brickCount--;
            mp_context->Events.Publish(BrickHitEvent(currentBomb, true, bombComp.IsSpecial, m_scoreManager.GetComboMultiplier(), bombPos));
        }

        m_registry.DestroyEntityDeferred(currentBomb);

        // Brick spacing: Col = 110.0f (100 + 10 pad), Row = 40.0f (30 + 10 pad)
        // 3x3 grid covers dx in [-125, 125], dy in [-55, 55]
        constexpr float kMaxDx = 125.0f;
        constexpr float kMaxDy = 55.0f;

        std::vector<Entity> neighbors;
        m_registry.View<Transform2D, BrickComponent>([&](Entity otherEntity, const Transform2D& otherTrans, const BrickComponent&)
        {
            if (otherEntity == currentBomb) return;
            if (!m_registry.IsAlive(otherEntity) || m_registry.IsPendingDestroy(otherEntity)) return;
            if (processedBombs.find(otherEntity) != processedBombs.end()) return;

            float dx = std::abs(otherTrans.Position.X - bombPos.X);
            float dy = std::abs(otherTrans.Position.Y - bombPos.Y);

            if (dx <= kMaxDx && dy <= kMaxDy)
            {
                neighbors.push_back(otherEntity);
            }
        });

        for (Entity neighbor : neighbors)
        {
            if (!m_registry.IsAlive(neighbor) || m_registry.IsPendingDestroy(neighbor)) continue;

            auto& nBrick = m_registry.GetComponent<BrickComponent>(neighbor);

            // Metal / Unbreakable bricks resist bomb shockwaves
            if (nBrick.IsUnbreakable)
            {
                if (m_registry.HasComponent<Transform2D>(neighbor))
                {
                    const auto& nTrans = m_registry.GetComponent<Transform2D>(neighbor);
                    SpawnExplosionParticles(nTrans.Position, Color{200, 210, 225, 255}, 8);
                    TweenEffects::Shake(m_registry, neighbor, 0.12f, 3.0f);
                }
                continue;
            }

            // Chain reaction: If neighbor is a bomb, enqueue it without recursion!
            if (nBrick.IsBomb)
            {
                if (processedBombs.find(neighbor) == processedBombs.end())
                {
                    processedBombs.insert(neighbor);
                    bombQueue.push(neighbor);
                }
                continue;
            }

            // Destructible neighbor brick
            Vector2f nPos{0.0f, 0.0f};
            Color nColor = Colors::White;
            if (m_registry.HasComponent<Transform2D>(neighbor))
            {
                nPos = m_registry.GetComponent<Transform2D>(neighbor).Position;
            }
            if (m_registry.HasComponent<SpriteComponent>(neighbor))
            {
                nColor = m_registry.GetComponent<SpriteComponent>(neighbor).Tint;
            }

            SpawnExplosionParticles(nPos, nColor, 15);

            m_scoreManager.AddScore(nBrick.ScoreValue);
            m_brickCount--;

            mp_context->Events.Publish(BrickHitEvent(neighbor, true, nBrick.IsSpecial, m_scoreManager.GetComboMultiplier(), nPos));

            if (nBrick.IsSpecial)
            {
                SpawnPowerUp(nPos);
            }

            m_registry.DestroyEntityDeferred(neighbor);
        }
    }
}

void GameScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    if (m_registry.IsPendingDestroy(entity)) return;
    if (!m_registry.HasComponent<BrickComponent>(entity)) return;

    auto& brick = m_registry.GetComponent<BrickComponent>(entity);

    // 1. Bomb Bricks: Detonates on any strike (ball, laser, or explosion) and triggers chain reaction
    if (brick.IsBomb)
    {
        DetonateBombBrick(entity);
        return;
    }

    bool isFireBall = false;
    bool isGhostBall = false;
    if (ballEntity != NULL_ENTITY && m_registry.IsAlive(ballEntity) && m_registry.HasComponent<BallComponent>(ballEntity))
    {
        auto& ballComp = m_registry.GetComponent<BallComponent>(ballEntity);
        if (ballComp.IsFireBall)
        {
            isFireBall = true;
            if (!ballComp.IsFuseActive)
            {
                // First brick hit: start the fuse!
                ballComp.IsFuseActive = true;
                ballComp.FuseTimer = PowerUpManager::Get().FireBall().FuseDuration;
            }
        }
        if (ballComp.IsGhost)
        {
            isGhostBall = true;
        }
    }

    // 2. Metal / Unbreakable Bricks
    if (brick.IsUnbreakable || brick.Type == BrickType::Metal)
    {
        // Lightning CANNOT destroy metal bricks ("les éclaires ne peuvent pas casser les briques en métal")
        if (isLaser)
        {
            Vector2f contactPos{0.0f, 0.0f};
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
                SpawnExplosionParticles(contactPos, Color{130, 220, 255, 255}, 10);
                TweenEffects::Shake(m_registry, entity, 0.12f, 4.0f);
            }
            if (mp_context && m_bounceSfxId != 0)
            {
                mp_context->Audio.PlaySfx(m_bounceSfxId, 90.0f, 1.4f);
            }
            return;
        }

        if (isFireBall || isExplosion)
        {
            brick.HitPoints = 0;
            m_scoreManager.AddScore(brick.ScoreValue);

            Vector2f contactPos{0.0f, 0.0f};
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
                SpawnFireExplosionParticles(contactPos, 65.0f, 30);
            }

            mp_context->Events.Publish(BrickHitEvent(entity, true, false, m_scoreManager.GetComboMultiplier(), contactPos));

            if (mp_context && m_explosionSfxId != 0)
            {
                mp_context->Audio.PlaySfx(m_explosionSfxId, 100.0f);
            }

            m_registry.DestroyEntityDeferred(entity);
            return;
        }
        else
        {
            if (isGhostBall)
            {
                // Ghost Ball cannot destroy metal bricks without FireBall:
                // Phases cleanly through without damage, without bounce, without shake
                Vector2f contactPos{0.0f, 0.0f};
                if (m_registry.HasComponent<Transform2D>(entity))
                {
                    contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
                    SpawnExplosionParticles(contactPos, Color{160, 240, 255, 120}, 4);
                }
                return;
            }

            // Standard ball bounces off with spark feedback and high-pitch metallic clank
            Vector2f contactPos{0.0f, 0.0f};
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
                SpawnExplosionParticles(contactPos, Color{225, 235, 255, 255}, 8);
                TweenEffects::Shake(m_registry, entity, 0.1f, 3.0f);
            }

            if (mp_context && m_bounceSfxId != 0)
            {
                mp_context->Audio.PlaySfx(m_bounceSfxId, 100.0f, 1.25f);
            }

            mp_context->Events.Publish(BrickHitEvent(entity, false, false, m_scoreManager.GetComboMultiplier(), contactPos));
            return;
        }
    }

    // 3. Normal & Multi-Hit Bricks
    bool canDestroy = false;
    if (m_bigBallDuration > 0.0f || isFireBall || isExplosion)
    {
        canDestroy = true; // Destroyed in one hit or fatal damage
    }
    else
    {
        int effectiveDmg = std::max(1, damage);
        if (isGhostBall)
        {
            effectiveDmg = std::max(effectiveDmg, PowerUpManager::Get().Ghost().GetEffectiveDamage());
        }
        canDestroy = (brick.HitPoints <= static_cast<uint32_t>(effectiveDmg));
    }

    if (isGhostBall && !canDestroy)
    {
        // Ghost Ball cannot destroy this brick (multi-hit brick needing more damage):
        // Phases cleanly through WITHOUT damaging or breaking it!
        Vector2f contactPos{0.0f, 0.0f};
        if (m_registry.HasComponent<Transform2D>(entity))
        {
            contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
            SpawnExplosionParticles(contactPos, Color{160, 240, 255, 120}, 4);
        }
        return;
    }

    if (canDestroy)
    {
        brick.HitPoints = 0;
    }
    else
    {
        int effectiveDmg = std::max(1, damage);
        brick.HitPoints -= static_cast<uint32_t>(effectiveDmg);
    }

    bool isDestroyed = (brick.HitPoints == 0);

    if (isDestroyed)
    {
        m_scoreManager.AddScore(brick.ScoreValue);
        m_brickCount--;
    }
    else
    {
        m_scoreManager.AddScore(0);
        
        // Visual durability cracking stages for multi-hit bricks
        if (m_registry.HasComponent<SpriteComponent>(entity))
        {
            auto& sprite = m_registry.GetComponent<SpriteComponent>(entity);
            if (brick.MaxHitPoints > 1)
            {
                sprite.Shader.OverlayTextureId = m_brickCrackTexId;
                sprite.Shader.ShaderId = m_crackShaderId;

                float crackAmount = 0.5f;
                if (brick.MaxHitPoints == 2)
                {
                    crackAmount = 0.85f;
                }
                else if (brick.MaxHitPoints == 3)
                {
                    crackAmount = (brick.HitPoints == 2) ? 0.45f : 0.95f;
                }
                else
                {
                    float damageRatio = static_cast<float>(brick.MaxHitPoints - brick.HitPoints) / static_cast<float>(brick.MaxHitPoints);
                    if (damageRatio < 0.35f) crackAmount = 0.40f;
                    else if (damageRatio < 0.70f) crackAmount = 0.70f;
                    else crackAmount = 1.0f;
                }

                sprite.Shader.ShaderValue = crackAmount;
            }
            
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(entity);
                SpawnExplosionParticles(transform.Position, sprite.Tint, 6);
                TweenEffects::Shake(m_registry, entity, 0.12f, 3.5f);
            }
        }
    }

    Vector2f contactPos{0.0f, 0.0f};
    if (ballEntity != NULL_ENTITY && m_registry.HasComponent<Transform2D>(ballEntity))
    {
        contactPos = m_registry.GetComponent<Transform2D>(ballEntity).Position;
    }
    else if (m_registry.HasComponent<Transform2D>(entity))
    {
        contactPos = m_registry.GetComponent<Transform2D>(entity).Position;
    }

    mp_context->Events.Publish(BrickHitEvent(entity, isDestroyed, brick.IsSpecial, m_scoreManager.GetComboMultiplier(), contactPos));

    if (m_renderGameUI && m_scoreManager.GetComboMultiplier() > 1 && m_registry.HasComponent<Transform2D>(entity))
    {
        const auto& transform = m_registry.GetComponent<Transform2D>(entity);
        if (m_uiController && m_registry.HasComponent<Camera2D>(m_camera))
        {
            m_uiController->SpawnComboText(transform.Position, m_scoreManager.GetComboMultiplier(), m_registry.GetComponent<Camera2D>(m_camera));
        }
    }

    if (isDestroyed)
    {
        if (m_registry.HasComponent<Transform2D>(entity) && m_registry.HasComponent<SpriteComponent>(entity))
        {
            const auto& transform = m_registry.GetComponent<Transform2D>(entity);
            const auto& sprite = m_registry.GetComponent<SpriteComponent>(entity);
            
            SpawnExplosionParticles(transform.Position, sprite.Tint);

            if (brick.IsSpecial)
            {
                SpawnPowerUp(transform.Position);
            }
        }
        
        m_registry.DestroyEntityDeferred(entity);
    }
}

void GameScene::HandlePaddleCollision(Entity ballEntity)
{
    if (!m_registry.HasComponent<RigidBody>(ballEntity) || !m_registry.HasComponent<Transform2D>(ballEntity)) return;

    auto& ballRb = m_registry.GetComponent<RigidBody>(ballEntity);

    // If ball is already stuck or already moving upward (away from paddle), ignore paddle collision!
    bool isStuckAlready = false;
    if (m_registry.HasComponent<BallComponent>(ballEntity))
    {
        isStuckAlready = m_registry.GetComponent<BallComponent>(ballEntity).IsStuck;
    }
    if (isStuckAlready || ballRb.Velocity.Y <= 0.0f)
    {
        return;
    }

    m_scoreManager.BreakCombo();

    const auto& ballTransform = m_registry.GetComponent<Transform2D>(ballEntity);
    const auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);

    const float paddleHalfWidth = paddleCollider.GetEffectiveSize(paddleTransform.Scale).X * 0.5f;
    const float offset = ballTransform.Position.X - paddleTransform.Position.X;

    // Sticky Paddle: catch ball on paddle contact
    if (m_stickyPaddleDuration > 0.0f && m_registry.HasComponent<BallComponent>(ballEntity))
    {
        auto& ballComp = m_registry.GetComponent<BallComponent>(ballEntity);
        ballComp.IsStuck = true;
        ballComp.StuckOffsetX = std::clamp(offset, -paddleHalfWidth * 0.9f, paddleHalfWidth * 0.9f);
        ballRb.Velocity = Vector2f{0.0f, 0.0f};
        ballRb.IsKinematic = true;

        const float paddleHalfHeight = paddleCollider.GetEffectiveSize(paddleTransform.Scale).Y * 0.5f;
        float ballRadius = 20.0f;
        if (m_registry.HasComponent<CircleCollider>(ballEntity))
        {
            ballRadius = m_registry.GetComponent<CircleCollider>(ballEntity).GetEffectiveRadius(ballTransform.Scale);
        }

        auto& mutableBallTrans = m_registry.GetComponent<Transform2D>(ballEntity);
        mutableBallTrans.Position.X = paddleTransform.Position.X + ballComp.StuckOffsetX;
        mutableBallTrans.Position.Y = paddleTransform.Position.Y - paddleHalfHeight - ballRadius - 1.0f;

        m_stuckBalls.push_back(StuckBall{ballEntity, ballComp.StuckOffsetX, 0.0f});

        if (m_bounceSfxId != 0 && mp_context)
        {
            mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
        }
        mp_context->Events.Publish(PaddleHitEvent(m_paddle, ballEntity, mutableBallTrans.Position));
        return;
    }

    float hitFactor = offset / paddleHalfWidth;
    if (hitFactor < -1.0f) hitFactor = -1.0f;
    if (hitFactor > 1.0f) hitFactor = 1.0f;

    float speed = std::sqrt(ballRb.Velocity.X * ballRb.Velocity.X + ballRb.Velocity.Y * ballRb.Velocity.Y);
    if (m_tempoBallDuration > 0.0f)
    {
        speed = PowerUpManager::Get().Tempo().GetEffectiveFastSpeed(); // Repart a toute vitesse !
    }
    constexpr float maxAngle = 60.0f * 3.14159265f / 180.0f;
    const float bounceAngle = hitFactor * maxAngle;

    ballRb.Velocity.X = speed * std::sin(bounceAngle);
    ballRb.Velocity.Y = -speed * std::cos(bounceAngle);

    mp_context->Events.Publish(PaddleHitEvent(m_paddle, ballEntity, ballTransform.Position));
}

void GameScene::HandleBallBottomCollision(Entity ballEntity)
{
    if (m_ballState != BallState::Active) return;

    // Ghost Ball: immortal ball, cannot lose life under any circumstance!
    bool isGhost = (m_ghostBallDuration > 0.0f);
    if (!isGhost && ballEntity != NULL_ENTITY && m_registry.IsAlive(ballEntity) && m_registry.HasComponent<BallComponent>(ballEntity))
    {
        isGhost = m_registry.GetComponent<BallComponent>(ballEntity).IsGhost;
    }
    if (isGhost)
    {
        if (m_registry.IsAlive(ballEntity) && m_registry.HasComponent<RigidBody>(ballEntity) && m_registry.HasComponent<Transform2D>(ballEntity))
        {
            auto& rb = m_registry.GetComponent<RigidBody>(ballEntity);
            auto& trans = m_registry.GetComponent<Transform2D>(ballEntity);

            trans.Position.Y = 410.0f;
            float currentSpeed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
            if (currentSpeed < 450.0f) currentSpeed = 600.0f;

            float angleX = trans.Position.X / 800.0f;
            angleX = std::clamp(angleX, -0.6f, 0.6f);
            Vector2f dir{angleX, -std::sqrt(std::max(0.1f, 1.0f - angleX * angleX))};
            rb.Velocity = dir.Normalized() * currentSpeed;

            SpawnExplosionParticles(trans.Position, Color{160, 240, 255, 255}, 20);
            TriggerPostProcessFlash(0.015f, 0.35f, Color{160, 240, 255, 255});

            if (m_bounceSfxId != 0 && mp_context)
            {
                mp_context->Audio.PlaySfx(m_bounceSfxId, 100.0f, 1.35f);
            }
            return;
        }
    }

    // Bottom Barrier: safe deflection and save charge
    if (m_bottomBarrierActive && m_bottomBarrierCharges > 0)
    {
        if (m_registry.IsAlive(ballEntity) && m_registry.HasComponent<RigidBody>(ballEntity) && m_registry.HasComponent<Transform2D>(ballEntity))
        {
            auto& rb = m_registry.GetComponent<RigidBody>(ballEntity);
            auto& trans = m_registry.GetComponent<Transform2D>(ballEntity);

            trans.Position.Y = 415.0f;
            float currentSpeed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
            if (currentSpeed < 450.0f) currentSpeed = 600.0f;

            float angleX = trans.Position.X / 800.0f;
            angleX = std::clamp(angleX, -0.6f, 0.6f);
            Vector2f dir{angleX, -std::sqrt(std::max(0.1f, 1.0f - angleX * angleX))};
            rb.Velocity = dir.Normalized() * currentSpeed;

            SpawnExplosionParticles(trans.Position, Color{0, 240, 255, 255}, 25);
            TriggerPostProcessFlash(0.018f, 0.45f, Color{0, 220, 255, 255});

            if (m_bounceSfxId != 0 && mp_context)
            {
                mp_context->Audio.PlaySfx(m_bounceSfxId, 100.0f);
            }

            m_bottomBarrierCharges--;
            if (m_bottomBarrierCharges <= 0)
            {
                m_bottomBarrierActive = false;
                m_bottomBarrierDuration = 0.0f;
            }
            return;
        }
    }

    if (mp_ballSystem)
    {
        mp_ballSystem->UnregisterBall(ballEntity);
    }

    if (mp_ballSystem && mp_ballSystem->GetBallCount() > 0)
    {
        if (m_registry.HasComponent<Transform2D>(ballEntity) && m_registry.HasComponent<SpriteComponent>(ballEntity))
        {
            const auto& transform = m_registry.GetComponent<Transform2D>(ballEntity);
            const auto& sprite = m_registry.GetComponent<SpriteComponent>(ballEntity);
            SpawnExplosionParticles(transform.Position, sprite.Tint, 15);
        }
        if (mp_context) mp_context->Audio.PlaySfx(m_despawnSfxId, 80.0f);

        if (ballEntity == m_ball)
        {
            m_ball = mp_ballSystem->GetBalls().front();
        }

        m_registry.DestroyEntityDeferred(ballEntity);
    }
    else
    {
        if (mp_ballSystem) mp_ballSystem->ClearBalls();
        HandleDeath();
    }
}

Entity GameScene::CreateBall(const Vector2f& position, const Vector2f& velocity)
{
    const bool isBig = (m_bigBallDuration > 0.0f);
    const float bigScale = isBig ? PowerUpManager::Get().Big().GetEffectiveScale() : 1.0f;
    const bool isTempo = (m_tempoBallDuration > 0.0f);

    Entity b = mp_ballSystem->CreateBall(position, velocity, m_ballTexId, isBig, bigScale, isTempo);
    if (m_ghostBallDuration > 0.0f && m_registry.IsAlive(b))
    {
        if (m_registry.HasComponent<BallComponent>(b))
        {
            m_registry.GetComponent<BallComponent>(b).IsGhost = true;
        }
        if (m_registry.HasComponent<SpriteComponent>(b))
        {
            m_registry.GetComponent<SpriteComponent>(b).Tint = Color{180, 240, 255, 120};
        }
    }
    return b;
}

Color GameScene::GetPowerUpColor(PowerUpType type)
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return Color{0, 220, 255, 255};
        case PowerUpType::ExpandPaddle: return Color{50, 255, 80, 255};
        case PowerUpType::ShrinkPaddle: return Color{255, 60, 60, 255};
        case PowerUpType::Storm:        return Color{255, 235, 40, 255};
        case PowerUpType::TempoBall:    return Color{180, 70, 255, 255};
        case PowerUpType::ExtraLife:    return Color{255, 100, 180, 255};
        case PowerUpType::BigBall:      return Color{255, 120, 20, 255};
        case PowerUpType::FireBall:     return Color{255, 60, 20, 255};
        case PowerUpType::StickyPaddle: return Color{245, 175, 40, 255};
        case PowerUpType::BottomBarrier: return Color{0, 240, 255, 255};
        case PowerUpType::MagnetPaddle: return Color{220, 60, 255, 255};
        case PowerUpType::GhostBall:    return Color{160, 240, 255, 255};
        default:                        return Colors::White;
    }
}

void GameScene::SpawnPowerUp(const Vector2f& position)
{
    struct LootEntry {
        PowerUpType Type;
        int Weight;
    };
    static const LootEntry kLootTable[] = {
        { PowerUpType::MultiBall,    12 },
        { PowerUpType::ExpandPaddle, 12 },
        { PowerUpType::ShrinkPaddle,  5 },
        { PowerUpType::Storm,        10 },
        { PowerUpType::TempoBall,     8 },
        { PowerUpType::ExtraLife,     4 },
        { PowerUpType::BigBall,       9 },
        { PowerUpType::FireBall,      8 },
        { PowerUpType::StickyPaddle, 10 },
        { PowerUpType::BottomBarrier, 8 },
        { PowerUpType::MagnetPaddle,  9 },
        { PowerUpType::GhostBall,     7 }
    };

    static int totalWeight = 0;
    if (totalWeight == 0)
    {
        for (const auto& entry : kLootTable)
        {
            totalWeight += entry.Weight;
        }
    }

    int roll = rand() % totalWeight;
    PowerUpType type = PowerUpType::MultiBall;
    for (const auto& entry : kLootTable)
    {
        if (roll < entry.Weight)
        {
            type = entry.Type;
            break;
        }
        roll -= entry.Weight;
    }

    Entity capsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(capsule, Transform2D{position, 0.0f, Vector2f{0.5f, 0.7f}});
    m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 220.0f, false});

    Color color = GetPowerUpColor(type);
    m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});
}

void GameScene::TriggerStormStrike()
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<Transform2D>(m_paddle))
        return;

    const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
    float halfW = paddleCollider.GetEffectiveSize(paddleTrans.Scale).X * 0.5f;

    // Origin: top of paddle with slight random variation across width
    float startX = paddleTrans.Position.X + (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * (halfW * 0.7f);
    float startY = paddleTrans.Position.Y - 20.0f;

    ActiveLightningBolt bolt;
    bolt.Lifetime = 0.24f;
    bolt.MaxLifetime = 0.24f;

    float currentX = startX;
    float currentY = startY;
    bolt.Nodes.push_back(Vector2f{currentX, currentY});

    const float targetTopY = -430.0f;
    const auto& stormCfg = PowerUpManager::Get().Storm();
    const float strikeRadius = stormCfg.StrikeRadius;
    const int stormDmg = stormCfg.GetEffectiveDamage();

    std::unordered_set<Entity> directlyHitBricks;
    std::unordered_set<Entity> directlyHitMetalBricks;

    auto CheckBrickHitsAtPoint = [&](const Vector2f& pt)
    {
        m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity bEntity, const Transform2D& bTrans, const BoxCollider& bCol, const BrickComponent& bComp)
        {
            if (!m_registry.IsAlive(bEntity) || m_registry.IsPendingDestroy(bEntity)) return;

            Vector2f bSize = bCol.GetEffectiveSize(bTrans.Scale);
            float dx = std::abs(pt.X - bTrans.Position.X);
            float dy = std::abs(pt.Y - bTrans.Position.Y);
            if (dx <= (bSize.X * 0.5f + strikeRadius) && dy <= (bSize.Y * 0.5f + strikeRadius))
            {
                if (bComp.IsUnbreakable || bComp.Type == BrickType::Metal)
                {
                    directlyHitMetalBricks.insert(bEntity);
                }
                else
                {
                    directlyHitBricks.insert(bEntity);
                }
            }
        });
    };

    // 1. Generate main trunk from paddle up to top wall
    while (currentY > targetTopY)
    {
        float stepY = 25.0f + static_cast<float>(rand() % 35);
        float nextY = std::max(targetTopY, currentY - stepY);
        float stepX = static_cast<float>(rand() % 60) - 30.0f;
        float nextX = std::clamp(currentX + stepX, -770.0f, 770.0f);

        Vector2f p1{currentX, currentY};
        Vector2f p2{nextX, nextY};
        bolt.Segments.push_back(LightningSegment{p1, p2});
        bolt.Nodes.push_back(p2);

        CheckBrickHitsAtPoint(p2);

        // 2. Chance to generate branching fork
        if ((rand() % 100) < 45)
        {
            float branchDir = (rand() % 2 == 0) ? -1.0f : 1.0f;
            float branchX = currentX;
            float branchY = currentY;
            int branchCount = 2 + (rand() % 4);
            for (int i = 0; i < branchCount && branchY > targetTopY; ++i)
            {
                float bStepY = 15.0f + static_cast<float>(rand() % 25);
                float bNextY = std::max(targetTopY, branchY - bStepY);
                float bStepX = branchDir * (20.0f + static_cast<float>(rand() % 30));
                float bNextX = std::clamp(branchX + bStepX, -770.0f, 770.0f);

                Vector2f bp1{branchX, branchY};
                Vector2f bp2{bNextX, bNextY};
                bolt.Segments.push_back(LightningSegment{bp1, bp2});
                bolt.Nodes.push_back(bp2);

                CheckBrickHitsAtPoint(bp2);

                branchX = bNextX;
                branchY = bNextY;
            }
        }

        currentX = nextX;
        currentY = nextY;
    }

    // 3. Conductive Metal Brick Network (metal bricks conduct and electrocute 1x1 adjacent neighbors)
    std::unordered_set<Entity> electrocutedAdjacentBricks;
    if (!directlyHitMetalBricks.empty())
    {
        // Collect all metal bricks alive in the scene
        std::vector<std::pair<Entity, Vector2f>> allMetalBricks;
        m_registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& bc) {
            if (!m_registry.IsAlive(e) || m_registry.IsPendingDestroy(e)) return;
            if (bc.IsUnbreakable || bc.Type == BrickType::Metal)
            {
                allMetalBricks.push_back({e, t.Position});
            }
        });

        // BFS to propagate current across all touching/connected metal bricks
        constexpr float kMaxConnectedDistX = 115.0f; // 100 brick width + 10 padding + epsilon
        constexpr float kMaxConnectedDistY = 45.0f;  // 30 brick height + 10 padding + epsilon

        std::unordered_set<Entity> conductiveMetalChain;
        std::queue<Entity> metalQueue;
        for (Entity mb : directlyHitMetalBricks)
        {
            conductiveMetalChain.insert(mb);
            metalQueue.push(mb);
        }

        while (!metalQueue.empty())
        {
            Entity current = metalQueue.front();
            metalQueue.pop();
            if (!m_registry.HasComponent<Transform2D>(current)) continue;
            Vector2f curPos = m_registry.GetComponent<Transform2D>(current).Position;

            for (const auto& [candidate, candPos] : allMetalBricks)
            {
                if (conductiveMetalChain.count(candidate) > 0) continue;
                float dx = std::abs(curPos.X - candPos.X);
                float dy = std::abs(curPos.Y - candPos.Y);
                if (dx <= kMaxConnectedDistX && dy <= kMaxConnectedDistY)
                {
                    conductiveMetalChain.insert(candidate);
                    metalQueue.push(candidate);
                }
            }
        }

        // Create electric arcs connecting the conductive metal chain
        ActiveLightningBolt conductiveArcs;
        conductiveArcs.Lifetime = 0.32f;
        conductiveArcs.MaxLifetime = 0.32f;

        // Collect all non-metal bricks in 1x1 radius around ANY metal brick in the chain
        for (Entity mb : conductiveMetalChain)
        {
            if (!m_registry.HasComponent<Transform2D>(mb)) continue;
            Vector2f mPos = m_registry.GetComponent<Transform2D>(mb).Position;

            m_electrifiedBricks.push_back(ElectrifiedBrickEffect{mPos, 0.35f, 0.35f});
            SpawnExplosionParticles(mPos, Color{120, 235, 255, 255}, 12);
            SpawnLightFlash(mPos, 220.0f, Color{100, 225, 255, 255}, 2.4f, 0.3f);
            conductiveArcs.Nodes.push_back(mPos);

            // Connect to nearby metal bricks with conductive arc segments
            for (Entity otherMb : conductiveMetalChain)
            {
                if (otherMb <= mb) continue;
                if (!m_registry.HasComponent<Transform2D>(otherMb)) continue;
                Vector2f oPos = m_registry.GetComponent<Transform2D>(otherMb).Position;
                if (std::abs(mPos.X - oPos.X) <= kMaxConnectedDistX && std::abs(mPos.Y - oPos.Y) <= kMaxConnectedDistY)
                {
                    conductiveArcs.Segments.push_back(LightningSegment{mPos, oPos});
                }
            }

            // Find adjacent destructible/bomb bricks in 1x1 radius
            m_registry.View<Transform2D, BrickComponent>([&](Entity otherE, const Transform2D& otherT, const BrickComponent& otherBc) {
                if (!m_registry.IsAlive(otherE) || m_registry.IsPendingDestroy(otherE)) return;
                if (otherBc.IsUnbreakable || otherBc.Type == BrickType::Metal) return;

                float dx = std::abs(mPos.X - otherT.Position.X);
                float dy = std::abs(mPos.Y - otherT.Position.Y);
                if (dx <= kMaxConnectedDistX && dy <= kMaxConnectedDistY)
                {
                    electrocutedAdjacentBricks.insert(otherE);
                    conductiveArcs.Segments.push_back(LightningSegment{mPos, otherT.Position});
                    conductiveArcs.Nodes.push_back(otherT.Position);
                }
            });
        }

        if (!conductiveArcs.Segments.empty() || !conductiveArcs.Nodes.empty())
        {
            m_activeLightningBolts.push_back(std::move(conductiveArcs));
        }

        // High-voltage electrical conduction SFX
        if (mp_context && m_conductionSfxId != 0)
        {
            float zapPitch = 0.95f + static_cast<float>(rand() % 15) * 0.01f;
            mp_context->Audio.PlaySfx(m_conductionSfxId, 100.0f, zapPitch);
        }
    }

    // 4. Combine all affected bricks and apply damage
    std::unordered_set<Entity> allAffectedBricks = directlyHitBricks;
    allAffectedBricks.insert(electrocutedAdjacentBricks.begin(), electrocutedAdjacentBricks.end());

    for (Entity bEntity : allAffectedBricks)
    {
        if (!m_registry.IsAlive(bEntity) || m_registry.IsPendingDestroy(bEntity)) continue;

        Vector2f bPos{0.0f, 0.0f};
        if (m_registry.HasComponent<Transform2D>(bEntity))
        {
            bPos = m_registry.GetComponent<Transform2D>(bEntity).Position;
        }

        HandleBrickCollision(bEntity, NULL_ENTITY, /*isLaser=*/true, /*isExplosion=*/false, stormDmg);

        SpawnExplosionParticles(bPos, Color{255, 245, 120, 255}, 14);
        SpawnLightFlash(bPos, 280.0f, Color{255, 255, 170, 255}, 2.2f, 0.3f);
    }

    // Audio & Game Feel
    if (mp_context)
    {
        mp_context->Audio.TriggerDucking(0.45f, 0.5f);
        if (m_lightningSfxId != 0)
        {
            float pitch = 0.92f + static_cast<float>(rand() % 20) * 0.01f;
            mp_context->Audio.PlaySfx(m_lightningSfxId, 100.0f, pitch);
        }
    }

    // Screen-wide lightning flash & Camera Shake
    SpawnLightFlash(Vector2f{0.0f, -100.0f}, 1200.0f, Color{220, 240, 255, 255}, 1.5f, 0.16f);
    TriggerPostProcessFlash(0.025f, 0.40f, Color{240, 250, 255, 255}, 0.15f, Color{255, 240, 100, 255});
    TweenEffects::Shake(m_registry, m_camera, 0.25f, 10.0f);

    if (m_activeLightningBolts.size() >= 4)
    {
        m_activeLightningBolts.erase(m_activeLightningBolts.begin());
    }
    m_activeLightningBolts.push_back(std::move(bolt));

    // Strike consumption (only for levels 1-4; level 5 is timed tempest)
    if (m_stormCharges > 0 && !stormCfg.IsTempest())
    {
        m_stormCharges--;
        if (m_stormCharges <= 0)
        {
            m_stormCharges = 0;
            m_stormDuration = 0.0f;
            m_laserDuration = 0.0f;
            if (m_registry.IsAlive(m_paddle))
            {
                if (m_registry.HasComponent<PaddleComponent>(m_paddle))
                {
                    auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
                    paddleComp.HasStorm = false;
                    paddleComp.HasLaser = false;
                    paddleComp.StormCooldown = 0.0f;
                    paddleComp.LaserCooldown = 0.0f;
                }
                if (m_registry.HasComponent<SpriteComponent>(m_paddle) && m_paddleSizeDuration <= 0.0f && m_magnetDuration <= 0.0f && m_stickyPaddleDuration <= 0.0f)
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
                }
            }
        }
    }
}

void GameScene::FireLasers()
{
    TriggerStormStrike();
}

void GameScene::ApplyPowerUp(PowerUpType type)
{
    const auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    Color effectColor = GetPowerUpColor(type);

    SpawnExplosionParticles(paddleTransform.Position, effectColor, 20);

    if (m_registry.HasComponent<TweenComponent>(m_paddle))
    {
        auto& tween = m_registry.GetComponent<TweenComponent>(m_paddle);
        TweenEffects::Shake(tween, m_registry, m_paddle, 0.2f, 4.0f);
    }

    switch (type)
    {
        case PowerUpType::MultiBall:
        {
            const auto& cfg = PowerUpManager::Get().MultiBall();
            const size_t maxBalls = static_cast<size_t>(cfg.MaxTotalBalls);
            if (!mp_ballSystem || mp_ballSystem->GetBallCount() >= maxBalls) break;

            const int ballsToSpawn = cfg.GetBallsToSpawn();
            const float rad = cfg.SplitAngleDeg * 3.14159265f / 180.0f;
            const float cosA = std::cos(rad);
            const float sinA = std::sin(rad);

            std::vector<Entity> currentBalls = mp_ballSystem->GetBalls();
            for (Entity existingBall : currentBalls)
            {
                if (mp_ballSystem->GetBallCount() >= maxBalls) break;
                if (!m_registry.IsAlive(existingBall) || !m_registry.HasComponent<Transform2D>(existingBall))
                    continue;

                const auto& trans = m_registry.GetComponent<Transform2D>(existingBall);
                Vector2f vel{400.0f, -400.0f};
                if (m_registry.HasComponent<RigidBody>(existingBall))
                {
                    vel = m_registry.GetComponent<RigidBody>(existingBall).Velocity;
                }
                if (std::abs(vel.X) < 50.0f && std::abs(vel.Y) < 50.0f)
                {
                    vel = Vector2f{400.0f, -400.0f};
                }

                float curSpeed = std::sqrt(vel.X * vel.X + vel.Y * vel.Y);
                if (curSpeed < 450.0f) curSpeed = 600.0f;

                Vector2f vel1{vel.X * cosA - vel.Y * sinA, vel.X * sinA + vel.Y * cosA};
                Vector2f vel2{vel.X * cosA + vel.Y * sinA, -vel.X * sinA + vel.Y * cosA};
                vel1 = vel1.Normalized() * curSpeed;
                vel2 = vel2.Normalized() * curSpeed;

                CreateBall(trans.Position, vel1);
                if (ballsToSpawn > 1 && mp_ballSystem->GetBallCount() < maxBalls)
                {
                    CreateBall(trans.Position, vel2);
                }
                for (int extra = 2; extra < ballsToSpawn && mp_ballSystem->GetBallCount() < maxBalls; ++extra)
                {
                    float extraRad = rad * (extra % 2 == 0 ? (extra/2 + 1) : -(extra/2 + 1));
                    Vector2f velExtra{vel.X * std::cos(extraRad) - vel.Y * std::sin(extraRad),
                                      vel.X * std::sin(extraRad) + vel.Y * std::cos(extraRad)};
                    CreateBall(trans.Position, velExtra.Normalized() * curSpeed);
                }
            }

            if (m_ballState == BallState::Attached)
            {
                m_ballState = BallState::Active;
                if (mp_paddleSystem)
                {
                    mp_paddleSystem->LaunchBall();
                }
            }
            break;
        }
        case PowerUpType::ExpandPaddle:
        {
            m_isShrinkPaddle = false;
            UpdatePaddleSpeed();

            const auto& cfg = PowerUpManager::Get().Expand();
            auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
            auto& trans = m_registry.GetComponent<Transform2D>(m_paddle);
            
            // Base collider stays 120.0f; transform scale adapts collider automatically
            paddleCollider.Size = Vector2f{120.0f, 20.0f};
            trans.Scale.X = cfg.GetEffectiveScale();

            const float halfW = paddleCollider.GetEffectiveSize(trans.Scale).X * 0.5f;
            const float limitX = 800.0f - halfW;
            trans.Position.X = std::clamp(trans.Position.X, -limitX, limitX);

            m_paddleSizeDuration = cfg.GetEffectiveDuration();

            // Lucky drop: 20% to 50% chance to drop 1 to 3 random power-ups
            const float chance = cfg.GetEffectiveDropChance();
            const float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            if (roll <= chance)
            {
                const int dropCount = cfg.GetEffectiveDropCount();
                for (int i = 0; i < dropCount; ++i)
                {
                    const float offsetX = (dropCount > 1) ? ((static_cast<float>(i) - (dropCount - 1) * 0.5f) * 140.0f) : 0.0f;
                    const float spawnX = std::clamp(trans.Position.X + offsetX, -600.0f, 600.0f);
                    SpawnPowerUp(Vector2f{spawnX, -300.0f});
                }
            }
            break;
        }
        case PowerUpType::ShrinkPaddle:
        {
            m_isShrinkPaddle = true;
            UpdatePaddleSpeed();

            const auto& cfg = PowerUpManager::Get().Shrink();
            auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
            auto& trans = m_registry.GetComponent<Transform2D>(m_paddle);

            // Base collider stays 120.0f; transform scale adapts collider automatically
            paddleCollider.Size = Vector2f{120.0f, 20.0f};
            trans.Scale.X = cfg.GetEffectiveScale();

            const float halfW = paddleCollider.GetEffectiveSize(trans.Scale).X * 0.5f;
            const float limitX = 800.0f - halfW;
            trans.Position.X = std::clamp(trans.Position.X, -limitX, limitX);

            m_paddleSizeDuration = cfg.GetEffectiveDuration();

            // Lucky drop: 20% to 50% chance to drop 1 to 3 random power-ups
            const float chance = cfg.GetEffectiveDropChance();
            const float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            if (roll <= chance)
            {
                const int dropCount = cfg.GetEffectiveDropCount();
                for (int i = 0; i < dropCount; ++i)
                {
                    const float offsetX = (dropCount > 1) ? ((static_cast<float>(i) - (dropCount - 1) * 0.5f) * 140.0f) : 0.0f;
                    const float spawnX = std::clamp(trans.Position.X + offsetX, -600.0f, 600.0f);
                    SpawnPowerUp(Vector2f{spawnX, -300.0f});
                }
            }
            break;
        }
        case PowerUpType::Storm:
        {
            const auto& cfg = PowerUpManager::Get().Storm();
            if (cfg.IsTempest())
            {
                m_stormDuration = cfg.GetTempestDuration(); // 10.0s continuous tempest
                m_stormCharges = 999;
            }
            else
            {
                m_stormCharges += cfg.GetEffectiveStrikes();
                m_stormDuration = 0.0f;
            }

            if (m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
                paddleComp.HasStorm = true;
                paddleComp.HasLaser = true;
                paddleComp.StormCooldown = cfg.GetEffectiveCooldown();
                paddleComp.LaserCooldown = paddleComp.StormCooldown;
            }
            if (m_registry.HasComponent<SpriteComponent>(m_paddle))
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = cfg.IsTempest() ? Color{255, 220, 80, 255} : Color{255, 245, 120, 255};
            }

            m_laserDuration = m_stormDuration;
            TriggerStormStrike();
            break;
        }
        case PowerUpType::TempoBall:
        {
            const auto& cfg = PowerUpManager::Get().Tempo();
            m_tempoBallDuration = cfg.GetEffectiveDuration();
            const float fastSpeed = cfg.GetEffectiveFastSpeed();
            if (mp_ballSystem) mp_ballSystem->SetTempoBallActive(true);
            if (mp_paddleSystem) mp_paddleSystem->SetLaunchSpeed(fastSpeed);
            UpdatePaddleSpeed();
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<RigidBody>(b)) continue;
                auto& rb = m_registry.GetComponent<RigidBody>(b);
                if (rb.IsKinematic) continue;
                float currentSpeed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
                if (currentSpeed > 0.1f && rb.Velocity.Y < 0.0f)
                {
                    rb.Velocity = (rb.Velocity / currentSpeed) * fastSpeed;
                }
                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Color{210, 160, 255, 255};
                }
            }
            break;
        }
        case PowerUpType::ExtraLife:
        {
            const auto& cfg = PowerUpManager::Get().Life();
            bool infiniteLives = mp_context ? mp_context->Rules.GetRule(Rule::Gameplay::InfiniteLives) : false;
            if (!infiniteLives)
            {
                for (int i = 0; i < cfg.LivesGranted; ++i)
                {
                    if (m_lives < cfg.MaxLivesCap)
                    {
                        m_lives++;
                    }
                }
                if (mp_context)
                {
                    mp_context->Events.Publish(LivesChangedEvent(m_lives));
                }
            }
            break;
        }
        case PowerUpType::BigBall:
        {
            const auto& cfg = PowerUpManager::Get().Big();
            m_bigBallDuration = cfg.GetEffectiveDuration();
            const float bigScale = cfg.GetEffectiveScale();
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b)) continue;
                if (m_registry.HasComponent<BallComponent>(b))
                {
                    m_registry.GetComponent<BallComponent>(b).IsBig = true;
                }
                if (m_registry.HasComponent<CircleCollider>(b))
                {
                    m_registry.GetComponent<CircleCollider>(b).Radius = 20.0f; // Automatic scaling drives effective size
                }
                if (m_registry.HasComponent<Transform2D>(b))
                {
                    m_registry.GetComponent<Transform2D>(b).Scale = Vector2f{bigScale, bigScale};
                }
            }
            break;
        }
        case PowerUpType::FireBall:
        {
            const auto& cfg = PowerUpManager::Get().FireBall();
            const float fireScale = cfg.GetEffectiveScale();
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b)) continue;
                if (m_registry.HasComponent<BallComponent>(b))
                {
                    auto& bc = m_registry.GetComponent<BallComponent>(b);
                    bc.IsFireBall = true;
                    bc.IsFuseActive = false;
                    bc.FuseTimer = cfg.FuseDuration;
                    bc.TrailTimer = 0.0f;
                }
                if (m_registry.HasComponent<Transform2D>(b))
                {
                    m_registry.GetComponent<Transform2D>(b).Scale = Vector2f{fireScale, fireScale};
                }
                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Color{255, 120, 20, 255};
                }
            }
            break;
        }
        case PowerUpType::StickyPaddle:
        {
            const auto& cfg = PowerUpManager::Get().Sticky();
            m_stickyPaddleDuration = cfg.GetEffectiveDuration();
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                m_registry.GetComponent<PaddleComponent>(m_paddle).HasSticky = true;
            }
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{255, 205, 110, 255};
            }
            break;
        }
        case PowerUpType::BottomBarrier:
        {
            const auto& cfg = PowerUpManager::Get().Barrier();
            m_bottomBarrierDuration = cfg.GetEffectiveDuration();
            m_bottomBarrierCharges = cfg.GetEffectiveCharges();
            m_bottomBarrierActive = true;
            break;
        }
        case PowerUpType::MagnetPaddle:
        {
            const auto& cfg = PowerUpManager::Get().Magnet();
            m_magnetDuration = cfg.GetEffectiveDuration();
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                m_registry.GetComponent<PaddleComponent>(m_paddle).HasMagnet = true;
            }
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{235, 140, 255, 255};
            }
            break;
        }
        case PowerUpType::GhostBall:
        {
            const auto& cfg = PowerUpManager::Get().Ghost();
            m_ghostBallDuration = cfg.GetEffectiveDuration();
            m_ghostBallPendingExpiry = false;
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b)) continue;
                if (m_registry.HasComponent<BallComponent>(b))
                {
                    m_registry.GetComponent<BallComponent>(b).IsGhost = true;
                }
                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Color{180, 240, 255, 120};
                }
            }
            break;
        }
        case PowerUpType::ItemRegen:
        default:
            break;
    }
}

void GameScene::UpdatePaddleSpeed()
{
    if (!m_registry.IsAlive(m_paddle) || !m_registry.HasComponent<PaddleComponent>(m_paddle)) return;

    constexpr float kBasePaddleSpeed = 700.0f;
    float speedMultiplier = 1.0f;

    if (m_tempoBallDuration > 0.0f)
    {
        speedMultiplier += (PowerUpManager::Get().Tempo().GetEffectivePaddleSpeedBonusPercent() / 100.0f);
    }

    if (m_paddleSizeDuration > 0.0f && m_isShrinkPaddle)
    {
        speedMultiplier += (PowerUpManager::Get().Shrink().GetEffectivePaddleSpeedBonusPercent() / 100.0f);
    }

    m_registry.GetComponent<PaddleComponent>(m_paddle).Speed = kBasePaddleSpeed * speedMultiplier;
}

void GameScene::UpdatePowerUpTimers(float dt)
{
    if (m_ballState != BallState::Active) return;

    // 1. Paddle Size Duration (Expand / Shrink expires back to standard 120.0f)
    if (m_paddleSizeDuration > 0.0f)
    {
        m_paddleSizeDuration -= dt;
        if (m_paddleSizeDuration <= 0.0f)
        {
            m_paddleSizeDuration = 0.0f;
            m_isShrinkPaddle = false;
            UpdatePaddleSpeed();
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<BoxCollider>(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
            {
                auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
                auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
                paddleCollider.Size = Vector2f{120.0f, 20.0f};
                paddleTrans.Scale = Vector2f{1.0f, 1.0f};

                const float halfW = paddleCollider.GetEffectiveSize(paddleTrans.Scale).X * 0.5f;
                const float limitX = 800.0f - halfW;
                paddleTrans.Position.X = std::clamp(paddleTrans.Position.X, -limitX, limitX);
            }
        }
    }

    // 0. Active lightning bolts decay
    for (auto it = m_activeLightningBolts.begin(); it != m_activeLightningBolts.end(); )
    {
        it->Lifetime -= dt;
        if (it->Lifetime <= 0.0f)
        {
            it = m_activeLightningBolts.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Electrified metal bricks decay
    for (auto it = m_electrifiedBricks.begin(); it != m_electrifiedBricks.end(); )
    {
        it->Lifetime -= dt;
        if (it->Lifetime <= 0.0f)
        {
            it = m_electrifiedBricks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 2. Storm effects and tempest duration handling
    if (m_stormCharges > 0 || m_stormDuration > 0.0f)
    {
        if (m_stormDuration > 0.0f)
        {
            m_stormDuration -= dt;
            m_laserDuration = m_stormDuration;
            if (m_stormDuration <= 0.0f)
            {
                m_stormDuration = 0.0f;
                m_stormCharges = 0;
                m_laserDuration = 0.0f;
                if (m_registry.IsAlive(m_paddle))
                {
                    if (m_registry.HasComponent<PaddleComponent>(m_paddle))
                    {
                        auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
                        paddleComp.HasStorm = false;
                        paddleComp.HasLaser = false;
                        paddleComp.StormCooldown = 0.0f;
                        paddleComp.LaserCooldown = 0.0f;
                    }
                    if (m_registry.HasComponent<SpriteComponent>(m_paddle) && m_paddleSizeDuration <= 0.0f && m_magnetDuration <= 0.0f && m_stickyPaddleDuration <= 0.0f)
                    {
                        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
                    }
                }
            }
        }

        // Crackle spark particles from paddle while Storm is active
        if ((m_stormCharges > 0 || m_stormDuration > 0.0f) && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle) && (rand() % 100 < 35))
        {
            const auto& pt = m_registry.GetComponent<Transform2D>(m_paddle);
            SpawnExplosionParticles(Vector2f{pt.Position.X + (static_cast<float>(rand() % 100) - 50.0f), pt.Position.Y - 12.0f}, Color{255, 240, 100, 255}, 2);
        }
    }

    // 3. Tempo Ball Duration
    if (m_tempoBallDuration > 0.0f)
    {
        m_tempoBallDuration -= dt;
        if (m_tempoBallDuration <= 0.0f)
        {
            m_tempoBallDuration = 0.0f;
            m_tempoBallStacks = 0;
            if (mp_ballSystem) mp_ballSystem->SetTempoBallActive(false);
            if (mp_paddleSystem) mp_paddleSystem->SetLaunchSpeed(640.0f);
            UpdatePaddleSpeed();
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<RigidBody>(b)) continue;
                auto& rb = m_registry.GetComponent<RigidBody>(b);
                if (rb.IsKinematic) continue;
                float currentSpeed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
                if (currentSpeed > 0.1f)
                {
                    rb.Velocity = (rb.Velocity / currentSpeed) * 600.0f;
                }
                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Colors::White;
                }
                if (m_registry.HasComponent<LightSourceComponent>(b))
                {
                    m_registry.GetComponent<LightSourceComponent>(b).LightColor = Color{225, 240, 255, 255};
                }
            }
        }
        else
        {
            const auto& tempoCfg = PowerUpManager::Get().Tempo();
            UpdatePaddleSpeed();
            const float fastSpeed = tempoCfg.GetEffectiveFastSpeed();
            const float slowSpeed = tempoCfg.SlowSpeed;
            const float topY = tempoCfg.SlowZoneTopY;
            const float botY = tempoCfg.SlowZoneBottomY;

            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<RigidBody>(b) || !m_registry.HasComponent<Transform2D>(b))
                    continue;

                auto& rb = m_registry.GetComponent<RigidBody>(b);
                auto& trans = m_registry.GetComponent<Transform2D>(b);
                if (rb.IsKinematic) continue;

                float speed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
                if (speed < 0.1f) continue;

                Vector2f dir = rb.Velocity / speed;

                if (rb.Velocity.Y < 0.0f)
                {
                    // Moving towards the bricks (upward): full fast speed!
                    rb.Velocity = dir * fastSpeed;
                }
                else
                {
                    // Moving towards the paddle (downward)
                    if (trans.Position.Y < topY)
                    {
                        // Above the slow zone (brick area): keep full fast speed!
                        rb.Velocity = dir * fastSpeed;
                    }
                    else
                    {
                        // In the Slow Zone: decelerates smoothly based on Y position towards the paddle
                        float progress = std::clamp((trans.Position.Y - topY) / (botY - topY), 0.0f, 1.0f);
                        float targetSpeed = fastSpeed - progress * (fastSpeed - slowSpeed);
                        rb.Velocity = dir * targetSpeed;
                    }
                }

                if (m_registry.HasComponent<SpriteComponent>(b))
                {
                    m_registry.GetComponent<SpriteComponent>(b).Tint = Color{210, 160, 255, 255};
                }
            }
        }
    }

    // 4. Big Ball Duration (Balls shrink back to normal size)
    if (m_bigBallDuration > 0.0f)
    {
        m_bigBallDuration -= dt;
        if (m_bigBallDuration <= 0.0f)
        {
            m_bigBallDuration = 0.0f;
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            for (Entity b : balls)
            {
                if (!m_registry.IsAlive(b)) continue;
                if (m_registry.HasComponent<BallComponent>(b))
                {
                    m_registry.GetComponent<BallComponent>(b).IsBig = false;
                }
                if (m_registry.HasComponent<CircleCollider>(b))
                {
                    m_registry.GetComponent<CircleCollider>(b).Radius = 20.0f;
                }
                if (m_registry.HasComponent<Transform2D>(b))
                {
                    m_registry.GetComponent<Transform2D>(b).Scale = Vector2f{1.0f, 1.0f};
                }
            }
        }
    }

    // 5. Sticky Paddle Duration
    if (m_stickyPaddleDuration > 0.0f)
    {
        m_stickyPaddleDuration -= dt;
        if (m_stickyPaddleDuration <= 0.0f)
        {
            m_stickyPaddleDuration = 0.0f;
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                m_registry.GetComponent<PaddleComponent>(m_paddle).HasSticky = false;
            }
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle) && m_laserDuration <= 0.0f && m_magnetDuration <= 0.0f)
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
            }
        }
    }

    // 6. Bottom Barrier Duration
    if (m_bottomBarrierDuration > 0.0f)
    {
        m_bottomBarrierDuration -= dt;
        if (m_bottomBarrierDuration <= 0.0f)
        {
            m_bottomBarrierDuration = 0.0f;
            m_bottomBarrierActive = false;
            m_bottomBarrierCharges = 0;
        }
    }

    // 7. Magnet Paddle Duration & Ball Attraction
    if (m_magnetDuration > 0.0f)
    {
        m_magnetDuration -= dt;
        if (m_magnetDuration <= 0.0f)
        {
            m_magnetDuration = 0.0f;
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                m_registry.GetComponent<PaddleComponent>(m_paddle).HasMagnet = false;
            }
            if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle) && m_stormDuration <= 0.0f && m_laserDuration <= 0.0f && m_stickyPaddleDuration <= 0.0f)
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
            }
        }
        else if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle) && mp_ballSystem)
        {
            const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const auto& magnetCfg = PowerUpManager::Get().Magnet();
            const float ballPullForce = magnetCfg.GetEffectiveBallPullForce();
            const float magnetRadius = magnetCfg.MagnetRadius;

            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<Transform2D>(b) ||
                    !m_registry.HasComponent<RigidBody>(b) || !m_registry.HasComponent<BallComponent>(b))
                    continue;

                auto& ballComp = m_registry.GetComponent<BallComponent>(b);
                if (ballComp.IsStuck) continue;

                auto& ballTrans = m_registry.GetComponent<Transform2D>(b);
                auto& ballRb = m_registry.GetComponent<RigidBody>(b);

                Vector2f toPaddle = paddleTrans.Position - ballTrans.Position;
                float dist = toPaddle.Length();
                if (dist > 1.0f && dist <= magnetRadius)
                {
                    Vector2f pullDir = toPaddle / dist;
                    float currentSpeed = ballRb.Velocity.Length();
                    if (currentSpeed > 10.0f)
                    {
                        ballRb.Velocity += pullDir * (ballPullForce * dt);
                        ballRb.Velocity = ballRb.Velocity.Normalized() * currentSpeed;
                    }
                }
            }
        }
    }

    // 8. Ghost Ball Duration
    if (m_ghostBallDuration > 0.0f || m_ghostBallPendingExpiry)
    {
        if (m_ghostBallDuration > 0.0f)
        {
            m_ghostBallDuration -= dt;
            if (m_ghostBallDuration <= 0.0f)
            {
                m_ghostBallDuration = 0.0f;
                m_ghostBallPendingExpiry = true;
            }
        }

        if (m_ghostBallPendingExpiry)
        {
            m_ghostBallExpiryGraceTimer += dt;
            const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
            bool isAnyBallInBrick = false;
            if (m_ghostBallExpiryGraceTimer < 2.0f)
            {
                for (Entity b : balls)
                {
                    if (!m_registry.IsAlive(b)) continue;
                    if (!m_registry.HasComponent<BallComponent>(b) || !m_registry.GetComponent<BallComponent>(b).IsGhost) continue;
                    if (!m_registry.HasComponent<Transform2D>(b) || !m_registry.HasComponent<CircleCollider>(b)) continue;

                    const auto& ballTrans = m_registry.GetComponent<Transform2D>(b);
                    const auto& ballCircle = m_registry.GetComponent<CircleCollider>(b);

                    m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity brickEntity, const Transform2D& brickTrans, const BoxCollider& brickBox, const BrickComponent&)
                    {
                        if (isAnyBallInBrick) return;
                        if (m_registry.IsPendingDestroy(brickEntity)) return;

                        CollisionManifold m = Physics::IntersectCircleAABB(ballCircle, ballTrans, brickBox, brickTrans);
                        if (m.IsColliding)
                        {
                            isAnyBallInBrick = true;
                        }
                    });

                    if (isAnyBallInBrick) break;
                }
            }

            if (!isAnyBallInBrick)
            {
                m_ghostBallPendingExpiry = false;
                m_ghostBallExpiryGraceTimer = 0.0f;
                for (Entity b : balls)
                {
                    if (!m_registry.IsAlive(b)) continue;
                    if (m_registry.HasComponent<BallComponent>(b))
                    {
                        m_registry.GetComponent<BallComponent>(b).IsGhost = false;
                    }
                    if (m_registry.HasComponent<SpriteComponent>(b))
                    {
                        Color restoreColor = Colors::White;
                        if (m_tempoBallDuration > 0.0f) restoreColor = Color{210, 160, 255, 255};
                        if (m_registry.GetComponent<BallComponent>(b).IsFireBall) restoreColor = Color{255, 120, 20, 255};
                        m_registry.GetComponent<SpriteComponent>(b).Tint = restoreColor;
                    }
                }
            }
        }
    }

    // Update active effects indicator in PowerUpTester panel if active
    if (m_uiController && m_uiController->IsPowerUpTesterActive())
    {
        std::string status;
        if (mp_ballSystem && mp_ballSystem->GetBallCount() > 1) status += "Balls: " + std::to_string(mp_ballSystem->GetBallCount()) + "/" + std::to_string(PowerUpManager::Get().MultiBall().MaxTotalBalls) + "  ";
        if (m_paddleSizeDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Size: %.1fs  ", m_paddleSizeDuration);
            status += buf;
        }
        if (PowerUpManager::Get().Storm().IsTempest() && m_stormDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Tempest: %.1fs  ", m_stormDuration);
            status += buf;
        }
        else if (m_stormCharges > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Storm: %dx  ", m_stormCharges);
            status += buf;
        }
        else if (m_stormDuration > 0.0f || m_laserDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Storm: %.1fs  ", std::max(m_stormDuration, m_laserDuration));
            status += buf;
        }
        if (m_tempoBallDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Tempo: %.1fs  ", m_tempoBallDuration);
            status += buf;
        }
        if (m_bigBallDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Big: %.1fs  ", m_bigBallDuration);
            status += buf;
        }
        if (m_stickyPaddleDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Sticky: %.1fs  ", m_stickyPaddleDuration);
            status += buf;
        }
        if (m_bottomBarrierActive) {
            char buf[40];
            snprintf(buf, sizeof(buf), "Barrier: %dx (%.1fs)  ", m_bottomBarrierCharges, m_bottomBarrierDuration);
            status += buf;
        }
        if (m_magnetDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Magnet: %.1fs  ", m_magnetDuration);
            status += buf;
        }
        if (m_ghostBallDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Ghost: %.1fs  ", m_ghostBallDuration);
            status += buf;
        }
        const auto& balls = mp_ballSystem ? mp_ballSystem->GetBalls() : std::vector<Entity>{};
        for (Entity b : balls)
        {
            if (m_registry.IsAlive(b) && m_registry.HasComponent<BallComponent>(b))
            {
                const auto& bc = m_registry.GetComponent<BallComponent>(b);
                if (bc.IsFireBall)
                {
                    char buf[40];
                    if (bc.IsFuseActive)
                        snprintf(buf, sizeof(buf), "Fire: %.1fs  ", std::max(0.0f, bc.FuseTimer));
                    else
                        snprintf(buf, sizeof(buf), "Fire: Armed  ");
                    status += buf;
                    break;
                }
            }
        }
        if (status.empty())
        {
            status = "All Lv." + std::to_string(PowerUpManager::Get().GetLevel(PowerUpType::MultiBall)) + " | [U] Level 1-5";
        }
        if (m_uiController)
        {
            m_uiController->UpdatePowerUpTesterStatus(status);
        }
    }
}

void GameScene::SpawnPowerUpAtPaddle()
{
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        const auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
        SpawnPowerUp(Vector2f{paddleTransform.Position.X, -300.0f});
    }
}

void GameScene::SpawnAllPowerUps()
{
    for (int i = 0; i < 12; ++i)
    {
        float x = -660.0f + i * (1320.0f / 11.0f);
        PowerUpType type = static_cast<PowerUpType>(i);
        Entity capsule = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(capsule, Transform2D{Vector2f{x, -350.0f}, 0.0f, Vector2f{0.5f, 0.7f}});
        m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
        m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 220.0f, false});
        Color color = GetPowerUpColor(type);
        m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});
    }
}

void GameScene::RespawnBricks()
{
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    if (mp_levelGenerator && mp_context)
    {
        m_brickCount = mp_levelGenerator->Generate(m_registry, *mp_context, m_brickTexId);
    }
}

void GameScene::SpawnLightFlash(const Vector2f& position, float radius, const Color& color, float intensity, float duration)
{
    if (m_lightFlashes.size() >= 8) return;

    LightFlash flash;
    flash.Position = position;
    flash.Radius = radius;
    flash.LightColor = color;
    flash.Intensity = intensity;
    flash.InitialIntensity = intensity;
    flash.Lifetime = duration;
    flash.InitialLifetime = duration;
    m_lightFlashes.push_back(flash);
}




