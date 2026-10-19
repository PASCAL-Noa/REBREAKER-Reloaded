#include "Scenes/GameScene.h"
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

    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");
    m_crackShaderId = context.Resources.LoadResource("Resources/shaders/crack.frag");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_brickTexId = context.Resources.LoadResource("Resources/sprite/brick.png");
    m_brickCrackTexId = context.Resources.LoadResource("Resources/sprite/brick_crack.png");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_despawnSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_despawn.wav");
    m_explosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");
    m_fireTexId = context.Resources.LoadResource("Resources/sprite/fire.png");
    m_heartTexId = context.Resources.LoadResource("Resources/sprite/heart.png");
    m_gameOverSfxId = context.Resources.LoadResource("Resources/audio/sfx/game_over.mp3");
    m_levelClearSfxId = context.Resources.LoadResource("Resources/audio/sfx/game_win.mp3");
    m_victorySfxId = context.Resources.LoadResource("Resources/audio/sfx/victory.mp3");
    m_scoreRecordedSfxId = context.Resources.LoadResource("Resources/audio/sfx/combo.wav");



    mp_paddleSystem = m_systemManager.AddSystem<PaddleSystem>(m_registry, context);
    mp_ballSystem = m_systemManager.AddSystem<BallSystem>(m_registry, context);
    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.AddSystem<AnimatorSystem>(m_registry);
    m_particleSystem = m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);
    m_systemManager.AddSystem<GameFeelSystem>(m_registry, context);
    m_systemManager.AddSystem<TweenSystem>(m_registry);
    m_systemManager.AddSystem<PowerUpSystem>(m_registry, context.Events);

    mp_paddleSystem->SetOnFireLaserCallback([this](Entity)
    {
        FireLasers();
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
        if (m_uiController && m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
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

    m_uiController = std::make_unique<GameUIController>(m_registry, *mp_context, *this);
    m_uiController->OnInit(m_fontId, m_heartTexId, m_fireTexId, m_bounceSfxId, m_scoreRecordedSfxId);

    m_systemManager.OnInit();

    m_levelManager.Initialize("Resources/levels");
    m_levelManager.LoadFromPrefs();

    m_playlist.AddTrack(context, "Resources/audio/music/Game-1.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-2.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-3.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-4.ogg");
    m_playlist.PlayNext(context, 1.5f);
    m_lives = PlayerPrefs::GetInt("Lives", 3);

    int startLevel = PlayerPrefs::GetInt("SelectedLevel", 0);
    PlayerPrefs::SetInt("SelectedLevel", 0);
    PlayerPrefs::Save();
    LoadLevel(startLevel, false);

    mp_state_machine = std::make_unique<StateMachine<GameScene>>(this, 5);

    class PauseToPlayCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            if (!pOwner || !pOwner->CanUnpause()) return false;
            return pOwner->GetContext()->Input.IsActionTriggered(InputAction::Pause) ||
                   pOwner->GetContext()->Input.IsKeyPress(KeyCode::Escape) ||
                   pOwner->GetContext()->Input.IsJoystickButtonPress(0, 1);
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

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new PlayToPauseCondition(), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new LevelClearedCondition<GameScene>(), static_cast<int>(SceneState::LevelTransition)));
    playingState->AddTransition(new Transition<GameScene>(new VictoryCondition<GameScene>(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new PauseToPlayCondition(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new GameOverAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* victoryState = mp_state_machine->CreateState(static_cast<int>(SceneState::Victory));
    victoryState->AddAction(new VictoryAction<GameScene>());
    victoryState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* levelTransitionState = mp_state_machine->CreateState(static_cast<int>(SceneState::LevelTransition));
    levelTransitionState->AddAction(new NextLevelAction<GameScene>());
    levelTransitionState->AddTransition(new Transition<GameScene>(new LevelTransitionCompleteCondition<GameScene>(), static_cast<int>(SceneState::Playing)));

    mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
    context.Events.Publish(GameStateChangedEvent(GameUIState::Playing));
    context.Events.Publish(LivesChangedEvent(m_lives));
}

void GameScene::OnUpdate(const float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;

    m_gameplayTime += dt;
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

        context.Events.Publish(GameStateChangedEvent(uiState));
    }

    if (m_uiController && m_uiController->IsRebinding())
    {
        UISystem::SetNavigationEnabled(false);
    }
    UISystem::OnUpdate(dt, m_registry, context);

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
            context.Scenes.LoadScene<MenuScene>();
            return;
        }
    }

    if (currentState == static_cast<int>(SceneState::Playing))
    {
        m_playlist.Update(context);
        HandleInput(dt, context);
        UpdatePowerUpTimers(dt);

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

    const bool canControlPaddle = (currentState == static_cast<int>(SceneState::Playing) || currentState == static_cast<int>(SceneState::LevelTransition));
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

    context.Render.ResetCamera();

    std::string gameStateStr = "PLAY";
    if (mp_state_machine)
    {
        int state = mp_state_machine->GetCurrentState();
        if (state == static_cast<int>(SceneState::Paused)) gameStateStr = "PAUSED";
        else if (state == static_cast<int>(SceneState::GameOver)) gameStateStr = "GAME OVER - PRESS SPACE TO REPLAY";
        else if (state == static_cast<int>(SceneState::Victory)) gameStateStr = "VICTORY - ALL LEVELS CLEARED! PRESS SPACE TO RESET";
        else if (state == static_cast<int>(SceneState::LevelTransition)) gameStateStr = "STAGE CLEAR! ADVANCING TO LEVEL " + std::to_string(m_levelManager.GetCurrentLevelNumber()) + "...";
    }

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

    const bool modalOpen = (m_uiController && m_uiController->IsModalOrSubMenuOpen());
    if (!modalOpen)
    {
        DrawDefaultUI(context, "REBREAKER", stats);
    }
    UISystem::OnRender(m_registry, context);
    if (m_uiController)
    {
        m_uiController->OnRender(context);
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

    if (mp_context)
    {
        mp_context->Audio.TriggerDucking(0.45f, 0.5f);
        if (m_explosionSfxId != 0)
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
        m_levelManager.SetHighScoreForLevel(m_levelManager.GetCurrentLevelNumber(), m_scoreManager.GetScore());
        m_uiController->SetupGameOver(m_scoreManager.GetScore(), m_levelManager.GetCurrentLevelNumber());
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
    m_levelManager.SetHighScoreForLevel(curLevel, m_scoreManager.GetScore());
    m_levelManager.SetHighestClearedLevel(curLevel);

    if (mp_context)
    {
        // Smoothly fade out stage music and fade in victory stinger
        mp_context->Audio.FadeToMusic("Resources/audio/sfx/victory.mp3", 0.6f, 85.0f, false);
    }

    if (m_uiController)
    {
        m_uiController->SetupVictory(m_scoreManager.GetScore());
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

void GameScene::ResumeGame()
{
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
    }
}

void GameScene::OnReplayRequested()
{
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
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
    if (context.Input.IsKeyPress(KeyCode::Numpad4) || (testerActive && context.Input.IsKeyPress(KeyCode::Num4))) ApplyPowerUp(PowerUpType::LaserPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad5) || (testerActive && context.Input.IsKeyPress(KeyCode::Num5))) ApplyPowerUp(PowerUpType::TempoBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad6) || (testerActive && context.Input.IsKeyPress(KeyCode::Num6))) ApplyPowerUp(PowerUpType::ExtraLife);
    if (context.Input.IsKeyPress(KeyCode::Numpad7) || (testerActive && context.Input.IsKeyPress(KeyCode::Num7))) ApplyPowerUp(PowerUpType::BigBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad8) || (testerActive && context.Input.IsKeyPress(KeyCode::Num8))) ApplyPowerUp(PowerUpType::FireBall);
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
        PowerUpManager::Get().SetLevel(PowerUpType::LaserPaddle, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::TempoBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::ExtraLife, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::BigBall, nextLvl);
        PowerUpManager::Get().SetLevel(PowerUpType::FireBall, nextLvl);
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
    m_laserDuration = 0.0f;
    m_tempoBallDuration = 0.0f;
    m_tempoBallStacks = 0;
    m_bigBallDuration = 0.0f;
    if (mp_ballSystem)
    {
        mp_ballSystem->SetTempoBallActive(false);
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetLaunchSpeed(640.0f);
    }
    if (m_registry.IsAlive(m_ball))
    {
        if (m_registry.HasComponent<BallComponent>(m_ball))
        {
            auto& bc = m_registry.GetComponent<BallComponent>(m_ball);
            bc.IsBig = false;
            bc.IsFireBall = false;
            bc.IsFuseActive = false;
            bc.FuseTimer = 0.0f;
            bc.TrailTimer = 0.0f;
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
        paddleComp.HasLaser = false;
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

void GameScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion)
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
    }

    // 2. Metal / Unbreakable Bricks: Impenetrable to standard balls, destroyable only via FireBall or laser piercing
    if (brick.IsUnbreakable)
    {
        if (isFireBall || isLaser || isExplosion)
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
    if (m_bigBallDuration > 0.0f || isFireBall || isLaser || isExplosion || brick.HitPoints <= 1)
    {
        brick.HitPoints = 0; // Destroyed in one hit or fatal damage
    }
    else
    {
        brick.HitPoints--;
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

    if (m_scoreManager.GetComboMultiplier() > 1 && m_registry.HasComponent<Transform2D>(entity))
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
    m_scoreManager.BreakCombo();

    if (!m_registry.HasComponent<RigidBody>(ballEntity) || !m_registry.HasComponent<Transform2D>(ballEntity)) return;

    auto& ballRb = m_registry.GetComponent<RigidBody>(ballEntity);
    const auto& ballTransform = m_registry.GetComponent<Transform2D>(ballEntity);
    const auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);

    const float paddleHalfWidth = paddleCollider.GetEffectiveSize(paddleTransform.Scale).X * 0.5f;
    const float offset = ballTransform.Position.X - paddleTransform.Position.X;

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

    return mp_ballSystem->CreateBall(position, velocity, m_ballTexId, isBig, bigScale, isTempo);
}

Color GameScene::GetPowerUpColor(PowerUpType type)
{
    switch (type)
    {
        case PowerUpType::MultiBall:    return Color{0, 220, 255, 255};
        case PowerUpType::ExpandPaddle: return Color{50, 255, 80, 255};
        case PowerUpType::ShrinkPaddle: return Color{255, 60, 60, 255};
        case PowerUpType::LaserPaddle:  return Color{255, 200, 0, 255};
        case PowerUpType::TempoBall:    return Color{180, 70, 255, 255};
        case PowerUpType::ExtraLife:    return Color{255, 100, 180, 255};
        case PowerUpType::BigBall:      return Color{255, 120, 20, 255};
        case PowerUpType::FireBall:     return Color{255, 60, 20, 255};
        default:                        return Colors::White;
    }
}

void GameScene::SpawnPowerUp(const Vector2f& position)
{
    static int nextTypeIndex = 0;
    PowerUpType type = static_cast<PowerUpType>(nextTypeIndex % 8);
    nextTypeIndex++;

    Entity capsule = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(capsule, Transform2D{position, 0.0f, Vector2f{0.5f, 0.7f}});
    m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
    m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{type, 220.0f, false});

    Color color = GetPowerUpColor(type);
    m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});
}

void GameScene::FireLasers()
{
    const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
    float halfW = paddleCollider.GetEffectiveSize(paddleTrans.Scale).X * 0.4f;
    float projSpeed = PowerUpManager::Get().Laser().ProjectileSpeed;

    for (float offset : {-halfW, halfW})
    {
        Entity laser = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(laser, Transform2D{Vector2f{paddleTrans.Position.X + offset, paddleTrans.Position.Y - 25.0f}});
        m_registry.AddComponent<BoxCollider>(laser, BoxCollider{Vector2f{10.0f, 25.0f}, Vector2f{0.0f, 0.0f}, false, true});
        m_registry.AddComponent<RigidBody>(laser, RigidBody{Vector2f{0.0f, -projSpeed}, 1.0f, 1.0f, false});
        m_registry.AddComponent<SpriteComponent>(laser, SpriteComponent{m_fireTexId, Color{255, 180, 0, 255}});
        if (mp_ballSystem)
        {
            mp_ballSystem->RegisterLaser(laser);
        }
    }
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
            break;
        }
        case PowerUpType::ShrinkPaddle:
        {
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
            break;
        }
        case PowerUpType::LaserPaddle:
        {
            const auto& cfg = PowerUpManager::Get().Laser();
            if (m_registry.HasComponent<PaddleComponent>(m_paddle))
            {
                auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
                paddleComp.HasLaser = true;
            }
            if (m_registry.HasComponent<SpriteComponent>(m_paddle))
            {
                m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{255, 220, 100, 255};
            }
            m_laserDuration = cfg.GetEffectiveDuration();
            FireLasers();
            break;
        }
        case PowerUpType::TempoBall:
        {
            const auto& cfg = PowerUpManager::Get().Tempo();
            m_tempoBallDuration = cfg.GetEffectiveDuration();
            if (mp_ballSystem) mp_ballSystem->SetTempoBallActive(true);
            if (mp_paddleSystem) mp_paddleSystem->SetLaunchSpeed(850.0f);
            const float fastSpeed = cfg.GetEffectiveFastSpeed();
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
    }
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

    // 2. Laser Duration (Laser paddle expires back to normal)
    if (m_laserDuration > 0.0f)
    {
        m_laserDuration -= dt;
        if (m_laserDuration <= 0.0f)
        {
            m_laserDuration = 0.0f;
            if (m_registry.IsAlive(m_paddle))
            {
                if (m_registry.HasComponent<PaddleComponent>(m_paddle))
                {
                    auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
                    paddleComp.HasLaser = false;
                    paddleComp.LaserCooldown = 0.0f;
                }
                if (m_registry.HasComponent<SpriteComponent>(m_paddle))
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
                }
            }
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
            }
        }
        else
        {
            const auto& tempoCfg = PowerUpManager::Get().Tempo();
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
        if (m_laserDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Laser: %.1fs  ", m_laserDuration);
            status += buf;
        }
        if (m_tempoBallDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Tempo Ball: %.1fs  ", m_tempoBallDuration);
            status += buf;
        }
        if (m_bigBallDuration > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Big Ball: %.1fs  ", m_bigBallDuration);
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
    for (int i = 0; i < 8; ++i)
    {
        float x = -630.0f + i * 180.0f;
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



