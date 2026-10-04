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



    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.AddSystem<AnimatorSystem>(m_registry);
    m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);
    m_systemManager.AddSystem<GameFeelSystem>(m_registry, context);
    m_systemManager.AddSystem<TweenSystem>(m_registry);
    m_systemManager.AddSystem<PowerUpSystem>(m_registry, context.Events);

    m_powerUpSub = context.Events.SubscribeScoped<PowerUpEvent>([this](const PowerUpEvent& e)
    {
        ApplyPowerUp(e.Type);
    });

    m_nextLevelSub = context.Events.SubscribeScoped<NextLevelEvent>([this](const NextLevelEvent&)
    {
        AdvanceToNextLevel();
    });

    m_registry.AddComponent<TweenComponent>(m_camera, TweenComponent{});
    auto& camTween = m_registry.GetComponent<TweenComponent>(m_camera);
    
    TweenEffects::CameraBreathing(camTween, m_registry, m_camera, 0.74f, 0.76f, 6.0f);
    
    Color c1{20, 20, 30, 255};
    Color c2{30, 20, 40, 255};
    TweenEffects::BackgroundColorShift(camTween, c1, c2, [this](Color c){ m_bgColor = c; }, 5.0f);

    TweenConfig<float> alphaTween;
    alphaTween.Start = 0.0f;
    alphaTween.End = 255.0f;
    alphaTween.Duration = 1.0f;
    alphaTween.Ease = EasingFunctions::EasingType::EaseInOutQuad;
    alphaTween.Setter = [this](float val) {
        m_heartsAlpha = val;
    };
    camTween.AddTween(alphaTween);

    m_collisionSub = context.Events.SubscribeScoped<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        // 1. Check laser projectile collision with bricks
        for (auto it = m_lasers.begin(); it != m_lasers.end(); )
        {
            Entity laser = *it;
            if (!m_registry.IsAlive(laser))
            {
                it = m_lasers.erase(it);
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
                    HandleBrickCollision(other);
                    m_registry.DestroyEntityDeferred(laser);
                    it = m_lasers.erase(it);
                    return;
                }
                else if (other != m_paddle)
                {
                    m_registry.DestroyEntityDeferred(laser);
                    it = m_lasers.erase(it);
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
        for (Entity laser : m_lasers) { if (otherEntity == laser) return; }

        bool isBrick = m_registry.HasComponent<BrickComponent>(otherEntity);
        bool isPaddle = (otherEntity == m_paddle);
        bool isBottomWall = (otherEntity == m_bottomWall);

        HandleBrickCollision(otherEntity, ballEntity);

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
            context.Audio.PlaySfx(m_bounceSfxId, 50.0f);
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

    m_balls.clear();
    m_ball = CreateBall(Vector2f{0.0f, 0.0f}, Vector2f{0.0f, 0.0f});
    m_registry.GetComponent<RigidBody>(m_ball).IsKinematic = true;

    m_paddle = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_paddle, Transform2D{Vector2f{0.0f, 300.0f}});
    m_registry.AddComponent<BoxCollider>(m_paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_paddle, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_paddle, SpriteComponent{m_paddleTexId});
    m_registry.AddComponent<TweenComponent>(m_paddle, TweenComponent{});
    m_registry.AddComponent<PaddleComponent>(m_paddle, PaddleComponent{});

    CreateUILayout(context);
    m_textFeedback = std::make_unique<TextFeedback>(m_registry, *mp_context, m_fontId, m_fireTexId);
    m_textFeedback->OnInit(m_uiCanvas);

    m_systemManager.OnInit();

    m_levelManager.Initialize("Resources/levels");
    m_levelManager.LoadFromPrefs();

    m_playlist.AddTrack(context, "Resources/audio/music/Game-1.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-2.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-3.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-4.ogg");
    m_lives = PlayerPrefs::GetInt("Lives", 3);

    LoadLevel(0, false);

    mp_state_machine = std::make_unique<StateMachine<GameScene>>(this, 5);

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new LevelClearedCondition<GameScene>(), static_cast<int>(SceneState::LevelTransition)));
    playingState->AddTransition(new Transition<GameScene>(new VictoryCondition<GameScene>(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Playing)));

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

    CreatePauseMenu(context);
    CreateAudioTab(context);
    CreateRenderTab(context);
    CreateInputsTab(context);
    CreateGamerulesTab(context);
    CreateCheatsTab(context);
    CreateSettingsLayout(context);
    CreatePowerUpTesterUI(context);
    CreateGameOverMenu(context);
    CreateLevelClearMenu(context);
    CreateVictoryMenu(context);
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

    if (m_pauseCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_pauseCanvas))
    {
        bool isSettingsOpen = false;
        if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas))
        {
            isSettingsOpen = m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled;
        }

        m_registry.GetComponent<CanvasComponent>(m_pauseCanvas).IsEnabled = (currentState == static_cast<int>(SceneState::Paused) && !isSettingsOpen);
        
        if (m_cheatsTabBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
        {
            m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
        }
        
        if (currentState != static_cast<int>(SceneState::Paused) && m_settingsLayoutCanvas != NULL_ENTITY)
        {
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
            if (m_activeTabCanvas != NULL_ENTITY) {
                m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
                m_activeTabCanvas = NULL_ENTITY;
            }
        }
    }

    if (m_powerUpTesterCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_powerUpTesterCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_powerUpTesterCanvas).IsEnabled = (m_powerUpTesterActive && currentState == static_cast<int>(SceneState::Playing));
    }

    if (m_gameOverCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_gameOverCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_gameOverCanvas).IsEnabled =
            (currentState == static_cast<int>(SceneState::GameOver) || currentState == static_cast<int>(SceneState::Victory));
    }

    if (m_levelClearCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelClearCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_levelClearCanvas).IsEnabled = (currentState == static_cast<int>(SceneState::LevelTransition));
    }

    if (m_victoryCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_victoryCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_victoryCanvas).IsEnabled = false;
    }

    UISystem::OnUpdate(dt, m_registry, context);
    
    if (currentState == static_cast<int>(SceneState::LevelTransition))
    {
        if (m_levelTransitionTimer > 0.0f)
        {
            m_levelTransitionTimer -= dt;
        }

        if (m_levelClearTimerText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearTimerText))
        {
            int remainingSecs = static_cast<int>(std::ceil(std::max(0.0f, m_levelTransitionTimer)));
            m_registry.GetComponent<TextComponent>(m_levelClearTimerText).Text =
                "NEXT STAGE IN " + std::to_string(remainingSecs) + "s (OR PRESS SPACE)";
        }
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

        if (m_scoreTextEntity != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_scoreTextEntity) && m_registry.HasComponent<RectTransform>(m_scoreTextEntity))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(m_scoreTextEntity);
            auto& textRect = m_registry.GetComponent<RectTransform>(m_scoreTextEntity);
            textComp.Text = "Score : " + std::to_string(m_scoreManager.GetScore());
            
            Vector2f textSize = context.Render.GetTextSize(textComp.Text, textComp.FontId, textComp.FontSize);
            m_textFeedback->SetFlamePosition(textRect, textSize.X + 40.0f, 50.0f);
        }

        m_textFeedback->OnUpdate(dt, m_scoreManager.GetComboMultiplier(), m_scoreManager.GetScore());
    }

    if (currentState != static_cast<int>(SceneState::Paused))
    {
        m_systemManager.OnUpdate(dt);

        // Hard boundary safety clamp & bounce for all active balls (guarantees no escaping or tunneling walls)
        for (Entity ballEntity : m_balls)
        {
            if (!m_registry.IsAlive(ballEntity)) continue;
            if (!m_registry.HasComponent<Transform2D>(ballEntity) || !m_registry.HasComponent<RigidBody>(ballEntity))
                continue;

            auto& trans = m_registry.GetComponent<Transform2D>(ballEntity);
            auto& rb = m_registry.GetComponent<RigidBody>(ballEntity);
            if (rb.IsKinematic) continue;

            constexpr float radius = 20.0f;
            constexpr float minX = -800.0f + radius;
            constexpr float maxX = 800.0f - radius;
            constexpr float minY = -450.0f + radius;

            if (trans.Position.X < minX)
            {
                trans.Position.X = minX;
                if (rb.Velocity.X < 0.0f) rb.Velocity.X = std::abs(rb.Velocity.X);
            }
            else if (trans.Position.X > maxX)
            {
                trans.Position.X = maxX;
                if (rb.Velocity.X > 0.0f) rb.Velocity.X = -std::abs(rb.Velocity.X);
            }

            if (trans.Position.Y < minY)
            {
                trans.Position.Y = minY;
                if (rb.Velocity.Y < 0.0f) rb.Velocity.Y = std::abs(rb.Velocity.Y);
            }

            // Enforce minimum & maximum speed (especially critical during MultiBall collisions)
            float speed = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
            float minSpeed = (m_tempoBallDuration > 0.0f) ? 280.0f : 450.0f;
            constexpr float MAX_BALL_SPEED = 900.0f;

            if (speed < 0.1f)
            {
                rb.Velocity = Vector2f{300.0f, -400.0f}.Normalized() * minSpeed;
            }
            else if (speed < minSpeed)
            {
                rb.Velocity = (rb.Velocity / speed) * minSpeed;
            }
            else if (speed > MAX_BALL_SPEED)
            {
                rb.Velocity = (rb.Velocity / speed) * MAX_BALL_SPEED;
            }

            // Prevent balls from getting trapped in near-horizontal trajectories
            constexpr float MIN_VERTICAL_SPEED = 120.0f;
            if (std::abs(rb.Velocity.Y) < MIN_VERTICAL_SPEED)
            {
                rb.Velocity.Y = (rb.Velocity.Y >= 0.0f) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
                float currentSpd = std::sqrt(rb.Velocity.X * rb.Velocity.X + rb.Velocity.Y * rb.Velocity.Y);
                if (currentSpd > 0.1f)
                {
                    rb.Velocity = (rb.Velocity / currentSpd) * speed;
                }
            }
        }

        std::erase_if(m_lasers, [this](Entity laser) {
            if (!m_registry.IsAlive(laser)) return true;
            if (m_registry.HasComponent<Transform2D>(laser))
            {
                const auto& trans = m_registry.GetComponent<Transform2D>(laser);
                if (trans.Position.Y < -460.0f)
                {
                    m_registry.DestroyEntityDeferred(laser);
                    return true;
                }
            }
            return false;
        });
    }
    m_registry.ProcessDeferredCommands();
}

void GameScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_registry.GetComponent<Camera2D>(m_camera));
    m_systemManager.OnRender();

    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{800.0f, -450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, -450.0f}, Vector2f{-800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{800.0f, -450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(Vector2f{-800.0f, 450.0f}, Vector2f{800.0f, 450.0f}, Colors::White, 2.0f);


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
        if (state == static_cast<int>(SceneState::Paused)) gameStateStr = "PAUSE";
        else if (state == static_cast<int>(SceneState::GameOver)) gameStateStr = "GAME OVER - ESPACE POUR REJOUER";
        else if (state == static_cast<int>(SceneState::Victory)) gameStateStr = "VICTOIRE - TOUS LES NIVEAUX TERMINES ! ESPACE POUR REINITIALISER";
        else if (state == static_cast<int>(SceneState::LevelTransition)) gameStateStr = "NIVEAU TERMINE ! PASSAGE AU NIVEAU " + std::to_string(m_levelManager.GetCurrentLevelNumber()) + "...";
    }

    std::string debugStr = showDebug ? "ON" : "OFF";
    std::string shaderStr = context.Rules.GetRule(Rule::Graphics::EnableShader) ? "ON" : "OFF";
    std::string invStr = context.Rules.GetRule(Rule::Gameplay::Invincible) ? "ON" : "OFF";
    std::string infLivesStr = context.Rules.GetRule(Rule::Gameplay::InfiniteLives) ? "ON" : "OFF";

    std::string stats = "Niveau : " + std::to_string(m_levelManager.GetCurrentLevelNumber()) + " / " + std::to_string(m_levelManager.GetLevelCount()) +
                        " (Debloques : " + std::to_string(m_levelManager.GetUnlockedLevel()) + ")";
    stats += "\nRecord Niveau : " + std::to_string(m_levelManager.GetHighScoreForLevel(m_levelManager.GetCurrentLevelNumber())) +
             " | Record Global : " + std::to_string(m_scoreManager.GetHighScore());
    stats += "\nDebug (G) : " + debugStr + " | Shader (F) : " + shaderStr;
    stats += "\nGod mod (I) : " + invStr + " | Infinite lives (L) : " + infLivesStr;
    stats += "\nState : " + gameStateStr;
    if (m_powerUpTesterActive)
    {
        const auto& paddleBox = m_registry.GetComponent<BoxCollider>(m_paddle);
        const auto& paddleTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        const auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
        stats += "\n[POWER-UP DEBUG ON] Lv:" + std::to_string(PowerUpManager::Get().GetLevel(PowerUpType::MultiBall)) +
                 " | Balls: " + std::to_string(m_balls.size()) +
                 " | Paddle: " + std::to_string(static_cast<int>(paddleBox.GetEffectiveSize(paddleTrans.Scale).X)) + "px" +
                 " | Laser: " + (paddleComp.HasLaser ? "ON" : "OFF") +
                 " | Keys: 1-7, P, O, R, B, U | [TAB] Hide";
    }
    else
    {
        stats += "\n[TAB] Open Power-Up Tester Tool";
    }

    DrawDefaultUI(context, "REBREAKER", stats);
    UISystem::OnRender(m_registry, context);
}

uint32_t GameScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
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

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    
    // Throttle bleed particles to avoid flooding ECS
    if (chance(rng) > 0.75f)
    {
        std::uniform_real_distribution<float> posOffsetX(-8.0f, 8.0f);
        std::uniform_real_distribution<float> posOffsetY(-8.0f, 8.0f);
        std::uniform_real_distribution<float> velDist(-120.0f, 120.0f);
        
        Entity p = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(p, Transform2D{Vector2f{position.X + posOffsetX(rng), position.Y + posOffsetY(rng)}});
        
        ParticleComponent particle;
        particle.Velocity = Vector2f{velDist(rng), velDist(rng)};
        particle.Life = 0.35f;
        particle.MaxLife = 0.35f;
        particle.Size = 8.0f;
        particle.Tint = Colors::Red; 
        
        m_registry.AddComponent<ParticleComponent>(p, particle);
    }
}

void GameScene::SpawnExplosionParticles(const Vector2f& position, const Color& color, const int count)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }

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
        Entity p = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(p, Transform2D{Vector2f{position.X + posOffsetX(rng), position.Y + posOffsetY(rng)}});
        
        ParticleComponent particle;
        particle.Velocity = Vector2f{velDistX(rng), velDistY(rng)};
        particle.Life = lifeDist(rng);
        particle.MaxLife = particle.Life;
        particle.Size = sizeDist(rng);
        particle.Tint = color; 
        
        m_registry.AddComponent<ParticleComponent>(p, particle);
    }
}

void GameScene::SpawnFireTrailParticle(const Vector2f& position, const Vector2f& ballVelocity, bool isFuseActive)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> offsetDist(-8.0f, 8.0f);
    std::uniform_real_distribution<float> velTurbDist(-60.0f, 60.0f);
    std::uniform_real_distribution<float> lifeDist(0.18f, isFuseActive ? 0.35f : 0.28f);
    std::uniform_real_distribution<float> sizeDist(isFuseActive ? 8.0f : 5.0f, isFuseActive ? 16.0f : 12.0f);
    std::uniform_int_distribution<int> colorPick(0, 2);

    Entity p = m_registry.CreateEntity();
    Vector2f spawnPos{position.X + offsetDist(rng), position.Y + offsetDist(rng)};
    m_registry.AddComponent<Transform2D>(p, Transform2D{spawnPos});

    Vector2f pVel{-ballVelocity.X * 0.15f + velTurbDist(rng), -ballVelocity.Y * 0.15f + velTurbDist(rng)};

    Color c;
    int cp = colorPick(rng);
    if (cp == 0)      c = Color{255, 230, 70, 255};
    else if (cp == 1) c = Color{255, 120, 20, 255};
    else              c = Color{220, 40, 10, 255};

    ParticleComponent particle;
    particle.Velocity = pVel;
    particle.Life = lifeDist(rng);
    particle.MaxLife = particle.Life;
    particle.Size = sizeDist(rng);
    particle.Tint = c;

    m_registry.AddComponent<ParticleComponent>(p, particle);
}

void GameScene::SpawnFireExplosionParticles(const Vector2f& position, float radius, int count)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }

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

        Entity p = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(p, Transform2D{spawnPos});

        Color c;
        int cp = colorPick(rng);
        if (cp == 0)      c = Color{255, 255, 180, 255};
        else if (cp == 1) c = Color{255, 180, 30, 255};
        else if (cp == 2) c = Color{240, 50, 20, 255};
        else              c = Color{120, 30, 10, 220};

        ParticleComponent particle;
        particle.Velocity = Vector2f{std::cos(angle) * speed, std::sin(angle) * speed};
        particle.Life = lifeDist(rng);
        particle.MaxLife = particle.Life;
        particle.Size = sizeDist(rng);
        particle.Tint = c;

        m_registry.AddComponent<ParticleComponent>(p, particle);
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
        HandleBrickCollision(brickEntity, NULL_ENTITY);
    }

    SpawnFireExplosionParticles(explosionCenter, aoeRadius, 45);

    if (mp_context)
    {
        if (m_explosionSfxId != 0)
            mp_context->Audio.PlaySfx(m_explosionSfxId, 100.0f);
        else if (m_despawnSfxId != 0)
            mp_context->Audio.PlaySfx(m_despawnSfxId, 90.0f);
    }

    TweenEffects::Shake(m_registry, m_camera, 0.3f, 16.0f);

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
    m_textFeedback.reset();
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

    for (size_t i = 0; i < m_heartEntities.size(); ++i)
    {
        if (m_registry.HasComponent<RectTransform>(m_heartEntities[i]))
        {
            m_registry.GetComponent<RectTransform>(m_heartEntities[i]).IsActive = (static_cast<int>(i) < m_lives);
        }
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

    int nextLevel = curLevel + 1;
    m_levelManager.UnlockLevel(nextLevel);
    m_levelManager.NextLevel();

    LoadLevel(m_levelManager.GetCurrentLevelIndex(), /*preserveStats=*/true);

    m_levelTransitionTimer = 10.0f;
}

void GameScene::StartLevelTransition()
{
    int clearedLevel = m_levelManager.GetCurrentLevelNumber();
    uint32_t currentScore = m_scoreManager.GetScore();

    if (mp_context)
    {
        mp_context->Audio.StopMusic();
        if (m_levelClearSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_levelClearSfxId, 100.0f);
        }
    }

    AdvanceToNextLevel();

    if (m_levelClearTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearTitleText))
    {
        m_registry.GetComponent<TextComponent>(m_levelClearTitleText).Text =
            "STAGE " + std::to_string(clearedLevel) + " COMPLETED!";
    }

    if (m_levelClearStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearStatsText))
    {
        m_registry.GetComponent<TextComponent>(m_levelClearStatsText).Text =
            "Accumulated Score: " + std::to_string(currentScore) + "  |  Lives: " + std::to_string(m_lives);
    }

    m_levelTransitionTimer = 10.0f;
}

void GameScene::UpdateLevelTransition()
{
}

void GameScene::CompleteLevelTransition()
{
    m_levelTransitionTimer = 0.0f;
    if (m_levelClearCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelClearCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_levelClearCanvas).IsEnabled = false;
    }
    if (mp_context)
    {
        m_playlist.PlayNext(*mp_context);
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
        mp_context->Audio.StopMusic();
        if (m_gameOverSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_gameOverSfxId, 100.0f);
        }
        mp_context->Audio.PlayMusic("Resources/audio/music/Game-death.ogg", 40.0f, false);
    }

    m_hasSubmittedScore = false;
    m_enteredPlayerName = "";
    uint32_t finalScore = m_scoreManager.GetScore();
    int finalLevel = m_levelManager.GetCurrentLevelNumber();

    if (m_gameOverTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverTitleText))
    {
        auto& titleComp = m_registry.GetComponent<TextComponent>(m_gameOverTitleText);
        titleComp.Text = "GAME OVER";
        titleComp.Tint = Color{240, 50, 50, 255};
    }

    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_replayBtn))
    {
        m_registry.GetComponent<TextComponent>(m_replayBtn).Text = "REPLAY [SPACE]";
    }

    if (m_gameOverScoreText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverScoreText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverScoreText).Text =
            "FINAL SCORE: " + std::to_string(finalScore) + "  |  STAGE REACHED: " + std::to_string(finalLevel);
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = "ENTER YOUR PSEUDO [PRESS ENTER OR SAVE]:";
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = "";
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    RefreshLeaderboardUI();
}

void GameScene::OnGameOverUpdate()
{
}

void GameScene::OnGameOverExit()
{
    if (mp_context)
    {
        mp_context->Audio.StopMusic();
        m_playlist.PlayNext(*mp_context);
    }
    FullReset();
}

void GameScene::OnVictoryEnter()
{
    m_scoreManager.BreakCombo();

    if (mp_context)
    {
        mp_context->Audio.StopMusic();
        if (m_victorySfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_victorySfxId, 100.0f);
        }
    }

    m_hasSubmittedScore = false;
    m_enteredPlayerName = "";
    uint32_t finalScore = m_scoreManager.GetScore();

    if (m_gameOverTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverTitleText))
    {
        auto& titleComp = m_registry.GetComponent<TextComponent>(m_gameOverTitleText);
        titleComp.Text = "VICTORY !";
        titleComp.Tint = Color{255, 215, 0, 255};
    }

    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_replayBtn))
    {
        m_registry.GetComponent<TextComponent>(m_replayBtn).Text = "CONTINUE [SPACE]";
    }

    if (m_gameOverScoreText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverScoreText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverScoreText).Text =
            "VICTORY SCORE: " + std::to_string(finalScore) + "  |  ALL 5 STAGES CLEARED!";
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = "CHAMPION! ENTER YOUR PSEUDO:";
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = "";
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    if (m_victoryStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_victoryStatsText))
    {
        m_registry.GetComponent<TextComponent>(m_victoryStatsText).Text =
            "FINAL SCORE: " + std::to_string(finalScore);
    }

    RefreshLeaderboardUI();
}

void GameScene::OnVictoryUpdate()
{
}

void GameScene::OnVictoryExit()
{
    if (mp_context)
    {
        mp_context->Audio.StopMusic();
        m_playlist.PlayNext(*mp_context);
    }
    m_levelManager.ResetToFirstLevel();
    LoadLevel(0, /*preserveStats=*/true);
}

bool GameScene::IsTypingName()
{
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        return m_registry.GetComponent<TextInputComponent>(m_nameInputEntity).IsFocused;
    }
    return false;
}

void GameScene::RefreshLeaderboardUI()
{
    const auto& entries = m_leaderboard.GetEntries();
    for (size_t i = 0; i < m_leaderboardRowTexts.size(); ++i)
    {
        Entity rowEntity = m_leaderboardRowTexts[i];
        if (!m_registry.IsAlive(rowEntity) || !m_registry.HasComponent<TextComponent>(rowEntity))
            continue;

        auto& textComp = m_registry.GetComponent<TextComponent>(rowEntity);
        if (i < entries.size() && entries[i].Name != "---")
        {
            std::string rankStr = "#" + std::to_string(i + 1);
            std::string nameStr = entries[i].Name;
            while (nameStr.length() < 12) nameStr += " ";
            textComp.Text = rankStr + "   " + nameStr + "   " + std::to_string(entries[i].Score) + " PTS";
        }
        else
        {
            textComp.Text = "#" + std::to_string(i + 1) + "   ---            0 PTS";
        }
    }
}

void GameScene::SubmitHighScore(const std::string& name)
{
    std::string safeName = name.empty() ? "PLAYER" : name;
    int rank = m_leaderboard.AddOrUpdateEntry(m_enteredPlayerName, safeName, m_scoreManager.GetScore(), m_levelManager.GetCurrentLevelNumber());
    m_enteredPlayerName = safeName;
    m_hasSubmittedScore = true;

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.IsFocused = false;
        input.Text = safeName;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVED";
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        std::string rankMsg = (rank > 0 && rank <= 5) ? ("RANK #" + std::to_string(rank) + " - CONGRATULATIONS!") : "PSEUDO SAVED!";
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = rankMsg;
    }

    if (mp_context && m_scoreRecordedSfxId != 0)
    {
        mp_context->Audio.PlaySfx(m_scoreRecordedSfxId, 100.0f);
    }

    RefreshLeaderboardUI();
}

void GameScene::FullReset()
{
    if (mp_context)
    {
        mp_context->Audio.StopMusic();
        m_playlist.PlayNext(*mp_context);
    }
    m_levelManager.ResetToFirstLevel();
    LoadLevel(0, /*preserveStats=*/false);
}

void GameScene::HandleInput(const float dt, const GameContext& context)
{
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    constexpr float speed = 700.0f;

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

    if (context.Input.IsKeyDown(KeyCode::Q) || context.Input.IsKeyDown(KeyCode::A) || context.Input.IsKeyDown(KeyCode::Left)) paddleTransform.Position.X -= speed * dt;
    if (context.Input.IsKeyDown(KeyCode::D) || context.Input.IsKeyDown(KeyCode::Right)) paddleTransform.Position.X += speed * dt;

    if (context.Input.IsKeyPress(KeyCode::Tab) || context.Input.IsKeyPress(KeyCode::T))
    {
        TogglePowerUpTester();
    }

    if (context.Input.IsKeyPress(KeyCode::Numpad1) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num1))) ApplyPowerUp(PowerUpType::MultiBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad2) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num2))) ApplyPowerUp(PowerUpType::ExpandPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad3) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num3))) ApplyPowerUp(PowerUpType::ShrinkPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad4) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num4))) ApplyPowerUp(PowerUpType::LaserPaddle);
    if (context.Input.IsKeyPress(KeyCode::Numpad5) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num5))) ApplyPowerUp(PowerUpType::TempoBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad6) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num6))) ApplyPowerUp(PowerUpType::ExtraLife);
    if (context.Input.IsKeyPress(KeyCode::Numpad7) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num7))) ApplyPowerUp(PowerUpType::BigBall);
    if (context.Input.IsKeyPress(KeyCode::Numpad8) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::Num8))) ApplyPowerUp(PowerUpType::FireBall);
    if ((m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::P)))
    {
        SpawnPowerUp(Vector2f{paddleTransform.Position.X, -300.0f});
    }
    if (context.Input.IsKeyPress(KeyCode::Numpad9) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::O)))
    {
        SpawnAllPowerUps();
    }
    if ((m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::R)))
    {
        ResetBallAndPaddle(true);
    }
    if (context.Input.IsKeyPress(KeyCode::Numpad0) || (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::B)))
    {
        RespawnBricks();
    }
    if ((m_powerUpTesterActive || context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked)) && context.Input.IsKeyPress(KeyCode::N))
    {
        AdvanceToNextLevel();
    }
    if (m_powerUpTesterActive && context.Input.IsKeyPress(KeyCode::U))
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

    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);
    const float halfPaddleWidth = paddleCollider.GetEffectiveSize(paddleTransform.Scale).X * 0.5f;
    const float limitX = 800.0f - halfPaddleWidth;
    paddleTransform.Position.X = std::clamp(paddleTransform.Position.X, -limitX, limitX);

    if (m_registry.HasComponent<PaddleComponent>(m_paddle))
    {
        auto& paddleComp = m_registry.GetComponent<PaddleComponent>(m_paddle);
        if (paddleComp.HasLaser)
        {
            paddleComp.LaserCooldown -= dt;
            if (context.Input.IsKeyPress(KeyCode::Space) || paddleComp.LaserCooldown <= 0.0f)
            {
                FireLasers();
                paddleComp.LaserCooldown = PowerUpManager::Get().Laser().GetEffectiveCooldown();
            }
        }
    }

    if (m_ballState == BallState::Attached)
    {
        if (m_registry.HasComponent<Transform2D>(m_ball))
        {
            auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
            ballTransform.Position = Vector2f{paddleTransform.Position.X, paddleTransform.Position.Y - 40.0f};

            if (context.Input.IsKeyPress(KeyCode::Space))
            {
                m_ballState = BallState::Active;
                if (m_registry.HasComponent<RigidBody>(m_ball))
                {
                    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
                    float launchSpeed = (m_tempoBallDuration > 0.0f) ? 850.0f : 640.0f;
                    Vector2f launchDir = Vector2f{500.0f, -400.0f}.Normalized();
                    ballRb.Velocity = launchDir * launchSpeed;
                    ballRb.IsKinematic = false;
                }
            }
        }
    }
}

void GameScene::ResetBallAndPaddle(bool smooth)
{
    // Clean up extra balls
    for (Entity b : m_balls)
    {
        if (b != m_ball && m_registry.IsAlive(b))
        {
            m_registry.DestroyEntityDeferred(b);
        }
    }
    m_balls.clear();

    if (!m_registry.IsAlive(m_ball))
    {
        m_ball = CreateBall(Vector2f{0.0f, 0.0f}, Vector2f{0.0f, 0.0f});
    }
    else
    {
        m_balls.push_back(m_ball);
    }

    // Clean up lasers
    for (Entity laser : m_lasers)
    {
        if (m_registry.IsAlive(laser))
        {
            m_registry.DestroyEntityDeferred(laser);
        }
    }
    m_lasers.clear();

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
            if (m_lives - 1 >= 0 && m_lives - 1 < m_heartEntities.size())
            {
                Entity actualHeart = m_heartEntities[m_lives - 1];

                if (m_registry.HasComponent<RectTransform>(actualHeart) && 
                    m_registry.HasComponent<RectTransform>(m_explodingHeart) && 
                    m_registry.HasComponent<SpriteComponent>(m_explodingHeart) && 
                    m_registry.HasComponent<TweenComponent>(m_explodingHeart))
                {
                    auto& actualTransform = m_registry.GetComponent<RectTransform>(actualHeart);
                    auto& t = m_registry.GetComponent<RectTransform>(m_explodingHeart);
                    auto& s = m_registry.GetComponent<SpriteComponent>(m_explodingHeart);
                    auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_explodingHeart);
                    
                    t.Position = actualTransform.Position;
                    t.AnchorPoint = actualTransform.AnchorPoint;
                    t.Parent = actualTransform.Parent;
                    t.Size = actualTransform.Size;
                    s.Tint = Colors::White;
                    
                    TweenConfig<float> config;
                    config.Start = 0.0f;
                    config.End = 1.0f;
                    config.Duration = 0.5f;
                    config.Ease = EasingFunctions::EasingType::EaseOutQuad;
                    
                    Entity heartEntity = m_explodingHeart;
                    Vector2f baseSize = t.Size;
                    
                    config.Setter = [this, heartEntity, baseSize](float val) {
                        if (m_registry.HasComponent<RectTransform>(heartEntity) && m_registry.HasComponent<SpriteComponent>(heartEntity)) {
                            auto& transform = m_registry.GetComponent<RectTransform>(heartEntity);
                            auto& sprite = m_registry.GetComponent<SpriteComponent>(heartEntity);
                            
                            transform.Size = Vector2f{baseSize.X + baseSize.X * 1.5f * val, baseSize.Y + baseSize.Y * 1.5f * val};
                            sprite.Tint.a = static_cast<uint8_t>(255.0f * (1.0f - val));
                        }
                    };
                    
                    tweenComp.AddTween(config);
                    actualTransform.IsActive = false;
                }
            }

            m_lives--;
        }

        if (m_lives > 0 || infiniteLives)
        {
            ResetBallAndPaddle(true);
        }
    };

    m_registry.GetComponent<TweenComponent>(m_ball).AddTween(scaleTween);
}

void GameScene::HandleBrickCollision(Entity entity, Entity ballEntity)
{
    if (m_registry.IsPendingDestroy(entity)) return;
    if (!m_registry.HasComponent<BrickComponent>(entity)) return;

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

    auto& brick = m_registry.GetComponent<BrickComponent>(entity);
    if (m_bigBallDuration > 0.0f || isFireBall || brick.HitPoints <= 1)
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
        
        if (m_registry.HasComponent<SpriteComponent>(entity))
        {
            auto& sprite = m_registry.GetComponent<SpriteComponent>(entity);
            if (brick.MaxHitPoints > 1)
            {
                sprite.Shader.OverlayTextureId = m_brickCrackTexId;
                sprite.Shader.ShaderId = m_crackShaderId;
                float ratio = static_cast<float>(brick.MaxHitPoints - brick.HitPoints) / static_cast<float>(brick.MaxHitPoints - 1);
                if (ratio > 1.0f) ratio = 1.0f;
                if (ratio < 0.0f) ratio = 0.0f;
                sprite.Shader.ShaderValue = ratio;
            }
            
            if (m_registry.HasComponent<Transform2D>(entity))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(entity);
                SpawnExplosionParticles(transform.Position, sprite.Tint, 5);
            }
        }
    }

    mp_context->Events.Publish(BrickHitEvent(entity, isDestroyed, brick.IsSpecial, m_scoreManager.GetComboMultiplier()));

    if (m_scoreManager.GetComboMultiplier() > 1 && m_registry.HasComponent<Transform2D>(entity))
    {
        const auto& transform = m_registry.GetComponent<Transform2D>(entity);
        m_textFeedback->SpawnComboText(m_uiCanvas, transform.Position, m_scoreManager.GetComboMultiplier(), m_registry.GetComponent<Camera2D>(m_camera));
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

    mp_context->Events.Publish(PaddleHitEvent(m_paddle, ballEntity));
}

void GameScene::HandleBallBottomCollision(Entity ballEntity)
{
    if (m_ballState != BallState::Active) return;

    std::erase_if(m_balls, [this](Entity b) { return !m_registry.IsAlive(b); });
    std::erase(m_balls, ballEntity);

    if (!m_balls.empty())
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
            m_ball = m_balls.front();
        }

        m_registry.DestroyEntityDeferred(ballEntity);
    }
    else
    {
        m_balls.clear();
        HandleDeath();
    }
}

Entity GameScene::CreateBall(const Vector2f& position, const Vector2f& velocity)
{
    Entity ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(ball, Transform2D{position});
    m_registry.AddComponent<CircleCollider>(ball, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false});
    m_registry.AddComponent<RigidBody>(ball, RigidBody{velocity, 1.0f, 1.0f, false});
    m_registry.AddComponent<SpriteComponent>(ball, SpriteComponent{m_ballTexId});
    m_registry.AddComponent<TweenComponent>(ball, TweenComponent{});
    m_registry.AddComponent<BallComponent>(ball, BallComponent{});

    if (m_bigBallDuration > 0.0f)
    {
        m_registry.GetComponent<BallComponent>(ball).IsBig = true;
        const float bigScale = PowerUpManager::Get().Big().GetEffectiveScale();
        m_registry.GetComponent<Transform2D>(ball).Scale = Vector2f{bigScale, bigScale};
    }
    if (m_tempoBallDuration > 0.0f)
    {
        m_registry.GetComponent<SpriteComponent>(ball).Tint = Color{210, 160, 255, 255};
    }

    m_balls.push_back(ball);
    return ball;
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
        m_lasers.push_back(laser);
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
            if (m_balls.size() >= maxBalls) break;

            const int ballsToSpawn = cfg.GetBallsToSpawn();
            const float rad = cfg.SplitAngleDeg * 3.14159265f / 180.0f;
            const float cosA = std::cos(rad);
            const float sinA = std::sin(rad);

            std::vector<Entity> currentBalls = m_balls;
            for (Entity existingBall : currentBalls)
            {
                if (m_balls.size() >= maxBalls) break;
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
                if (ballsToSpawn > 1 && m_balls.size() < maxBalls)
                {
                    CreateBall(trans.Position, vel2);
                }
                for (int extra = 2; extra < ballsToSpawn && m_balls.size() < maxBalls; ++extra)
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
                if (m_registry.HasComponent<RigidBody>(m_ball))
                {
                    auto& rb = m_registry.GetComponent<RigidBody>(m_ball);
                    rb.Velocity = Vector2f{500.0f, -400.0f};
                    rb.IsKinematic = false;
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
            const float fastSpeed = cfg.GetEffectiveFastSpeed();
            for (Entity b : m_balls)
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
                    if (m_lives < static_cast<int>(m_heartEntities.size()))
                    {
                        Entity heart = m_heartEntities[m_lives];
                        if (m_registry.HasComponent<RectTransform>(heart))
                        {
                            m_registry.GetComponent<RectTransform>(heart).IsActive = true;
                        }
                    }
                    if (m_lives < cfg.MaxLivesCap)
                    {
                        m_lives++;
                    }
                }
            }
            break;
        }
        case PowerUpType::BigBall:
        {
            const auto& cfg = PowerUpManager::Get().Big();
            m_bigBallDuration = cfg.GetEffectiveDuration();
            const float bigScale = cfg.GetEffectiveScale();
            for (Entity b : m_balls)
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
            for (Entity b : m_balls)
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
            for (Entity b : m_balls)
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

            for (Entity b : m_balls)
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
            for (Entity b : m_balls)
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

    // 5. Fire Ball Timers & Dynamic Effects
    const auto& fireCfg = PowerUpManager::Get().FireBall();
    for (Entity b : m_balls)
    {
        if (!m_registry.IsAlive(b)) continue;
        if (!m_registry.HasComponent<BallComponent>(b) || !m_registry.HasComponent<Transform2D>(b)) continue;

        auto& bc = m_registry.GetComponent<BallComponent>(b);
        if (!bc.IsFireBall) continue;

        auto& bTrans = m_registry.GetComponent<Transform2D>(b);
        Vector2f vel{0.0f, 0.0f};
        if (m_registry.HasComponent<RigidBody>(b))
        {
            vel = m_registry.GetComponent<RigidBody>(b).Velocity;
        }

        // Emit dynamic trail particles
        bc.TrailTimer += dt;
        const float trailInterval = bc.IsFuseActive ? 0.015f : 0.03f;
        while (bc.TrailTimer >= trailInterval)
        {
            bc.TrailTimer -= trailInterval;
            SpawnFireTrailParticle(bTrans.Position, vel, bc.IsFuseActive);
        }

        // If the fuse has started (first brick was pierced)
        if (bc.IsFuseActive)
        {
            bc.FuseTimer -= dt;

            // Visual warning: fast pulsating intensity and tint
            if (m_registry.HasComponent<SpriteComponent>(b))
            {
                auto& spr = m_registry.GetComponent<SpriteComponent>(b);
                float blink = std::sin((fireCfg.FuseDuration - bc.FuseTimer) * 22.0f);
                if (blink > 0.0f)
                {
                    spr.Tint = Color{255, 240, 80, 255};
                }
                else
                {
                    spr.Tint = Color{255, 50, 10, 255};
                }
            }

            if (bc.FuseTimer <= 0.0f)
            {
                ExplodeFireBall(b, bTrans.Position);
            }
        }
    }

    // Update active effects indicator in PowerUpTester panel if alive
    if (m_powerUpStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_powerUpStatusText))
    {
        std::string status;
        if (m_balls.size() > 1) status += "Balls: " + std::to_string(m_balls.size()) + "/" + std::to_string(PowerUpManager::Get().MultiBall().MaxTotalBalls) + "  ";
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
        for (Entity b : m_balls)
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
        m_registry.GetComponent<TextComponent>(m_powerUpStatusText).Text = status;
    }
}

void GameScene::CreateUILayout(GameContext& context)
{
    m_uiCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_uiCanvas, CanvasComponent{.IsEnabled = true});

    // Score Text
    m_scoreTextEntity = UIFactory::CreateText(m_registry, m_uiCanvas, TextDescriptor{
        .Text = "Score : 0",
        .Position = {330.0f, 130.0f},
        .FontId = m_fontId,
        .FontSize = 48.0f,
        .AnchorPoint = Anchor::TopLeft,
        .TextCenter = false
    });

    // Hearts UI
    m_heartEntities.clear();
    for (int i = 0; i < 3; ++i)
    {
        Entity heart = m_registry.CreateEntity();
        m_registry.AddComponent<RectTransform>(heart, RectTransform{
            .Position = {-380.0f - i * 100.0f, 130.0f},
            .Size = {80.0f, 80.0f},
            .AnchorPoint = Anchor::TopRight,
            .Parent = m_uiCanvas
        });
        m_registry.AddComponent<SpriteComponent>(heart, SpriteComponent{m_heartTexId, Colors::White});
        m_heartEntities.push_back(heart);
    }

    m_explodingHeart = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(m_explodingHeart, RectTransform{
        .Position = {0.0f, 0.0f},
        .Size = {40.0f, 40.0f},
        .AnchorPoint = Anchor::TopRight,
        .Parent = m_uiCanvas
    });
    m_registry.AddComponent<SpriteComponent>(m_explodingHeart, SpriteComponent{m_heartTexId, Colors::Transparent});
    m_registry.AddComponent<TweenComponent>(m_explodingHeart, TweenComponent{});
}

void GameScene::CreatePauseMenu(const GameContext& context)
{
    m_pauseCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_pauseCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(m_registry, m_pauseCanvas, TextDescriptor{
        .Text = "PAUSE",
        .Position = {0, -200.0f},
        .FontId = m_fontId,
        .FontSize = 80.0f
    });

    // Continue Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "CONTINUE",
        .OnClick = [this]() {
            if (mp_state_machine) mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
        },
        .Position = {0, -50.0f},
        .FontId = m_fontId
    });

    // Options Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "OPTIONS",
        .OnClick = [this]() {
            auto& pauseCanvas = m_registry.GetComponent<CanvasComponent>(m_pauseCanvas);
            pauseCanvas.IsEnabled = false;
            auto& settingsCanvas = m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas);
            settingsCanvas.IsEnabled = true;
        },
        .Position = {0, 50.0f},
        .FontId = m_fontId
    });

    // Quit Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "QUIT",
        .OnClick = [this]() {
            if (mp_context) mp_context->Scenes.LoadScene<MenuScene>();
        },
        .Position = {0, 150.0f},
        .FontId = m_fontId
    });

    // Dark overlay background
    UIFactory::CreatePanel(m_registry, m_pauseCanvas, PanelDescriptor{
        .Position = {0, 0},
        .Size = {context.Render.GetLogicalViewSize().X * 2.0f, context.Render.GetLogicalViewSize().Y * 2.0f},
        .Tint = Color{0, 0, 0, 200},
        .AnchorPoint = Anchor::Center
    });
}

void GameScene::UpdateVolumeBars(const std::vector<Entity>& bars, float volume)
{
    for (size_t i = 0; i < bars.size(); ++i)
    {
        if (m_registry.HasComponent<PanelComponent>(bars[i]))
        {
            auto& panel = m_registry.GetComponent<PanelComponent>(bars[i]);
            if ((i * 10.0f) < volume)
                panel.Tint.a = 255;
            else
                panel.Tint.a = 50;
        }
    }
}

void GameScene::CreateSettingsLayout(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X - 300;
    float viewY = context.Render.GetLogicalViewSize().Y - 350;

    float leftPanelWidth = viewX * 0.25f;
    float leftPanelX = -viewX * 0.375f + 200.f;

    m_settingsLayoutCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_settingsLayoutCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(m_registry, m_settingsLayoutCanvas, TextDescriptor{
        .Text = "SETTINGS",
        .Position = {leftPanelX, -viewY * 0.3f},
        .FontId = m_fontId,
        .FontSize = 50.0f
    });

    // 4 Tab Buttons
    float startY = -viewY * 0.15f;
    float stepY = viewY * 0.08f;

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "AUDIO",
        .OnClick = [this]() { OpenSettingsTab(m_audioCanvas); },
        .Position = {leftPanelX, startY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "RENDER",
        .OnClick = [this]() { OpenSettingsTab(m_renderCanvas); },
        .Position = {leftPanelX, startY + stepY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "INPUTS",
        .OnClick = [this]() { OpenSettingsTab(m_inputsCanvas); },
        .Position = {leftPanelX, startY + stepY * 2.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "GAMERULES",
        .OnClick = [this]() { OpenSettingsTab(m_gamerulesCanvas); },
        .Position = {leftPanelX, startY + stepY * 3.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    m_cheatsTabBtn = UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "CHEATS",
        .OnClick = [this]() { OpenSettingsTab(m_cheatsCanvas); },
        .Position = {leftPanelX, startY + stepY * 4.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    // Back Button
    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "RETOUR",
        .OnClick = [this]() {
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
            if (m_activeTabCanvas != NULL_ENTITY) {
                m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
                m_activeTabCanvas = NULL_ENTITY;
            }
            m_registry.GetComponent<CanvasComponent>(m_pauseCanvas).IsEnabled = true;
        },
        .Position = {leftPanelX, viewY * 0.4f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .DefaultColor = Colors::Crimson,
        .HoverColor = Colors::LightCoral,
        .PressedColor = Colors::DarkRed,
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(m_registry, m_settingsLayoutCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX + 300, viewY + 350},
        .Tint = Color{0, 0, 0, 200}
    });

    // Left Tabs Panel
    UIFactory::CreatePanel(m_registry, m_settingsLayoutCanvas, PanelDescriptor{
        .Position = {leftPanelX, 0.0f},
        .Size = {leftPanelWidth, viewY - 50},
        .Tint = Colors::Transparent
    });
}

void GameScene::CreateAudioTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_audioCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_audioCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_audioCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_audioCanvas, TextDescriptor{
        .Text = "AUDIO SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateVolumeControl(m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "SFX",
        .OnMinus = [&]() {
            float vol = std::max(0.0f, context.Audio.GetSfxVolume() - 10.0f);
            context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, context.Audio.GetSfxVolume() + 10.0f);
            context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .BarsOut = &m_sfxVolumeBars,
        .Position = {rightPanelX * 0.5f, -viewY * 0.1f + 20.f},
        .FontId = m_fontId
    });

    UIFactory::CreateVolumeControl(m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "MUSIC",
        .OnMinus = [&]() {
            float vol = std::max(0.0f, context.Audio.GetMusicVolume() - 10.0f);
            context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, context.Audio.GetMusicVolume() + 10.0f);
            context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .BarsOut = &m_musicVolumeBars,
        .Position = {rightPanelX * 0.5f, viewY * 0.1f - 50.f},
        .FontId = m_fontId
    });

    UpdateVolumeBars(m_sfxVolumeBars, context.Audio.GetSfxVolume());
    UpdateVolumeBars(m_musicVolumeBars, context.Audio.GetMusicVolume());

    UIFactory::CreatePanel(m_registry, m_audioCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void GameScene::CreateRenderTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_renderCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_renderCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_renderCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_renderCanvas, TextDescriptor{
        .Text = "RENDER SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    bool isFullscreen = context.Render.GetWindow().GetConfig().Mode == WindowMode::Fullscreen;
    std::string fsText = isFullscreen ? "FULL-SCREEN: ON" : "FULL-SCREEN: OFF";
    
    m_fsBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = fsText,
        .OnClick = [&context, this]() {
            WindowConfig config = context.Render.GetWindow().GetConfig();
            if (config.Mode == WindowMode::Fullscreen) {
                config.Mode = WindowMode::Windowed;
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: OFF";
                }
            } else {
                config.Mode = WindowMode::Fullscreen;
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: ON";
                }
            }
            context.Render.GetWindow().ApplyConfig(config);
        },
        .Position = {rightPanelX - 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    std::vector<Resolution> uniqueModes = Window::GetSupportedResolutions();
    std::vector<std::string> options;
    
    for (const auto& res : uniqueModes) {
        options.push_back(std::to_string(res.Width) + "x" + std::to_string(res.Height));
    }

    std::string currentRes = std::to_string(context.Render.GetWindow().GetConfig().Width) + "x" + std::to_string(context.Render.GetWindow().GetConfig().Height);

    UIFactory::CreateDropdown(m_registry, m_renderCanvas, DropdownDescriptor{
        .DefaultText = currentRes,
        .Options = options,
        .OnSelect = [&context, uniqueModes](int index, const std::string& text) {
            WindowConfig config = context.Render.GetWindow().GetConfig();
            config.Width = uniqueModes[index].Width;
            config.Height = uniqueModes[index].Height;
            context.Render.GetWindow().ApplyConfig(config);
        },
        .Position = {rightPanelX + 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId,
    });
    
    bool isShader = context.Rules.GetRule(Rule::Graphics::EnableShader);
    std::string shaderText = isShader ? "SHADER: ON" : "SHADER: OFF";
    m_shaderBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = shaderText,
        .OnClick = [&context, this]() {
            bool state = context.Rules.GetRule(Rule::Graphics::EnableShader);
            context.Rules.SetRule(Rule::Graphics::EnableShader, !state);
            if (m_registry.HasComponent<TextComponent>(m_shaderBtn)) {
                m_registry.GetComponent<TextComponent>(m_shaderBtn).Text = !state ? "SHADER: ON" : "SHADER: OFF";
            }
        },
        .Position = {rightPanelX - 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });
    
    bool isParticles = context.Rules.GetRule(Rule::Graphics::EnableParticles);
    std::string particlesText = isParticles ? "PARTICLES: ON" : "PARTICLES: OFF";
    m_particlesBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = particlesText,
        .OnClick = [&context, this]() {
            bool state = context.Rules.GetRule(Rule::Graphics::EnableParticles);
            context.Rules.SetRule(Rule::Graphics::EnableParticles, !state);
            if (m_registry.HasComponent<TextComponent>(m_particlesBtn)) {
                m_registry.GetComponent<TextComponent>(m_particlesBtn).Text = !state ? "PARTICLES: ON" : "PARTICLES: OFF";
            }
        },
        .Position = {rightPanelX + 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(m_registry, m_renderCanvas, PanelDescriptor{
       .Position = {rightPanelX, 0.0f},
       .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void GameScene::CreateInputsTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_inputsCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_inputsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_inputsCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "INPUTS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "Left: Q",
        .Position = {rightPanelX, -viewY * 0.1f},
        .FontId = m_fontId
    });
    
    UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "Right: D",
        .Position = {rightPanelX, viewY * 0.1f - 200.f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(m_registry, m_inputsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void GameScene::CreateGamerulesTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_gamerulesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_gamerulesCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_gamerulesCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_gamerulesCanvas, TextDescriptor{
        .Text = "GAMERULES",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateButton(m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = "Difficulty",
        .Position = {rightPanelX, -viewY * 0.1f},
        .FontId = m_fontId
    });
    
    UIFactory::CreatePanel(m_registry, m_gamerulesCanvas, PanelDescriptor{
      .Position = {rightPanelX, 0.0f},
      .Size = {rightPanelWidth, viewY},
      .Tint = Colors::Transparent
    });
}

void GameScene::CreateCheatsTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_cheatsCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_cheatsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_cheatsCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_cheatsCanvas, TextDescriptor{
        .Text = "CHEATS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateTextInput(m_registry, m_cheatsCanvas, TextInputDescriptor{
        .Placeholder = "Enter Cheat Code...",
        .OnSubmit = [&context](const std::string& code) {
            context.Events.Publish(CheatSubmitEvent(code));
        },
        .Position = {rightPanelX, 0.0f},
        .Size = {600.0f, 60.0f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreatePanel(m_registry, m_cheatsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void GameScene::OpenSettingsTab(Entity targetCanvas)
{
    if (m_activeTabCanvas == targetCanvas) {
        m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = false;
        m_activeTabCanvas = NULL_ENTITY;
        return;
    }

    if (m_activeTabCanvas != NULL_ENTITY) {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    m_activeTabCanvas = targetCanvas;
    m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = true;

    if (m_registry.HasComponent<TweenComponent>(targetCanvas))
    {
        auto& tweenComp = m_registry.GetComponent<TweenComponent>(targetCanvas);
        tweenComp.Clear();
        
        m_registry.View<RectTransform>([&](Entity e, RectTransform& rect) {
            if (rect.Parent == targetCanvas) {
                Vector2f originalPos = rect.Position;
                Vector2f startPos = {originalPos.X, originalPos.Y - 50.0f};
                
                tweenComp.AddTween(TweenConfig<Vector2f>{
                    .Start = startPos,
                    .End = originalPos,
                    .Duration = 0.25f,
                    .Setter = [this, e](Vector2f pos) {
                        if (m_registry.HasComponent<RectTransform>(e)) {
                            m_registry.GetComponent<RectTransform>(e).Position = pos;
                        }
                    },
                    .Ease = EasingFunctions::EasingType::EaseOutQuad
                });
            }
        });
    }
}

void GameScene::SetPowerUpTesterActive(bool active)
{
    m_powerUpTesterActive = active;
    if (m_powerUpTesterCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_powerUpTesterCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_powerUpTesterCanvas).IsEnabled = active;
    }
}

void GameScene::TogglePowerUpTester()
{
    SetPowerUpTesterActive(!m_powerUpTesterActive);
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

void GameScene::CreatePowerUpTesterUI(const GameContext& context)
{
    (void)context;
    m_powerUpTesterCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_powerUpTesterCanvas, CanvasComponent{.IsEnabled = m_powerUpTesterActive});

    // Dark semi-transparent panel on right side of the screen
    UIFactory::CreatePanel(m_registry, m_powerUpTesterCanvas, PanelDescriptor{
        .Position = {-155.0f, 0.0f},
        .Size = {290.0f, 790.0f},
        .Tint = Color{15, 20, 30, 220},
        .AnchorPoint = Anchor::MiddleRight
    });

    // Title
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "POWER-UP TESTER",
        .Position = {-155.0f, -330.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::Yellow,
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Helper hint text
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "[TAB] to toggle",
        .Position = {-155.0f, -295.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{180, 180, 180, 255},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Active power-up indicators
    m_powerUpStatusText = UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "",
        .Position = {-155.0f, -265.0f},
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .Tint = Color{255, 230, 100, 255},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    struct PowerUpBtnDef {
        const char* text;
        PowerUpType type;
        Color normalColor;
        Color hoverColor;
    };

    PowerUpBtnDef buttons[] = {
        {"1. Multi-Ball", PowerUpType::MultiBall, Color{0, 120, 160, 255}, Color{0, 180, 220, 255}},
        {"2. Expand Paddle", PowerUpType::ExpandPaddle, Color{30, 130, 50, 255}, Color{50, 180, 80, 255}},
        {"3. Shrink Paddle", PowerUpType::ShrinkPaddle, Color{150, 40, 40, 255}, Color{200, 60, 60, 255}},
        {"4. Laser Paddle", PowerUpType::LaserPaddle, Color{160, 120, 20, 255}, Color{220, 170, 30, 255}},
        {"5. Tempo Ball", PowerUpType::TempoBall, Color{110, 40, 160, 255}, Color{150, 60, 210, 255}},
        {"6. Extra Life", PowerUpType::ExtraLife, Color{160, 50, 110, 255}, Color{210, 70, 150, 255}},
        {"7. Big Ball", PowerUpType::BigBall, Color{180, 80, 15, 255}, Color{230, 110, 25, 255}},
        {"8. Fire Ball", PowerUpType::FireBall, Color{190, 40, 10, 255}, Color{240, 80, 15, 255}}
    };

    float startY = -240.0f;
    float stepY = 35.0f;

    for (size_t i = 0; i < 8; ++i)
    {
        PowerUpType pType = buttons[i].type;
        UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
            .Text = buttons[i].text,
            .OnClick = [this, pType]() { ApplyPowerUp(pType); },
            .Position = {-155.0f, startY + i * stepY},
            .Size = {260.0f, 32.0f},
            .TextOffset = {0.0f, -8.0f},
            .DefaultColor = buttons[i].normalColor,
            .HoverColor = buttons[i].hoverColor,
            .PressedColor = Color{20, 20, 20, 255},
            .TextColor = Colors::White,
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .AnchorPoint = Anchor::MiddleRight
        });
    }

    float actionY = startY + 8 * stepY + 6.0f;
    float actionStepY = 35.0f;

    // Drop capsule button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop Capsule [P]",
        .OnClick = [this]() {
            if (m_registry.HasComponent<Transform2D>(m_paddle)) {
                SpawnPowerUp(Vector2f{m_registry.GetComponent<Transform2D>(m_paddle).Position.X, -300.0f});
            }
        },
        .Position = {-155.0f, actionY},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Drop all 8 capsules button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop All 8 [O]",
        .OnClick = [this]() { SpawnAllPowerUps(); },
        .Position = {-155.0f, actionY + actionStepY},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Reset paddle / ball button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Reset Ball [R]",
        .OnClick = [this]() { ResetBallAndPaddle(true); },
        .Position = {-155.0f, actionY + actionStepY * 2},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Respawn bricks button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Refill Bricks [B]",
        .OnClick = [this]() { RespawnBricks(); },
        .Position = {-155.0f, actionY + actionStepY * 3},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Level +1 / Cycle button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Level +1 / Cycle [U]",
        .OnClick = [this]() {
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
        },
        .Position = {-155.0f, actionY + actionStepY * 4},
        .Size = {260.0f, 34.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{110, 70, 25, 255},
        .HoverColor = Color{150, 100, 35, 255},
        .PressedColor = Color{40, 25, 10, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Next Level button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Next Level [N]",
        .OnClick = [this]() { AdvanceToNextLevel(); },
        .Position = {-155.0f, actionY + actionStepY * 5},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{50, 80, 120, 255},
        .HoverColor = Color{70, 110, 160, 255},
        .PressedColor = Color{30, 50, 80, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });
}

void GameScene::CreateGameOverMenu(const GameContext& context)
{
    m_gameOverCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_gameOverCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Inputs, Texts (created first -> rendered last on top) ---

    // Replay Button
    m_replayBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "REPLAY [SPACE]",
        .OnClick = [this]() {
            if (mp_state_machine)
            {
                mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
            }
            else
            {
                FullReset();
            }
        },
        .Position = {-180.0f, 310.0f},
        .Size = {300.0f, 60.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{65, 65, 90, 255},
        .HoverColor = Color{95, 95, 130, 255},
        .PressedColor = Color{35, 35, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    // Main Menu Button
    UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            if (mp_context)
            {
                mp_context->Audio.StopMusic();
                mp_context->Scenes.LoadScene<MenuScene>();
            }
        },
        .Position = {180.0f, 310.0f},
        .Size = {300.0f, 60.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{95, 45, 45, 255},
        .HoverColor = Color{135, 65, 65, 255},
        .PressedColor = Color{60, 25, 25, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    // Submit button
    m_submitNameBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "SAVE",
        .OnClick = [this]() {
            if (m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
            {
                auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
                SubmitHighScore(input.Text);
            }
        },
        .Position = {170.0f, 195.0f},
        .Size = {140.0f, 54.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{40, 130, 75, 255},
        .HoverColor = Color{60, 175, 105, 255},
        .PressedColor = Color{25, 90, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Name text input field
    m_nameInputEntity = UIFactory::CreateTextInput(m_registry, m_gameOverCanvas, TextInputDescriptor{
        .Placeholder = "YOUR NAME",
        .OnSubmit = [this](const std::string& name) {
            SubmitHighScore(name);
        },
        .Position = {-100.0f, 195.0f},
        .Size = {360.0f, 54.0f},
        .DefaultColor = Color{35, 30, 45, 255},
        .FocusedColor = Color{75, 65, 95, 255},
        .TextColor = Colors::Yellow,
        .FontId = m_fontId,
        .FontSize = 30.0f,
        .AnchorPoint = Anchor::Center,
        .MaxLength = 10
    });

    // Status label
    m_gameOverStatusText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "ENTER YOUR NAME:",
        .Position = {0.0f, 135.0f},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    // 5 Rows of Leaderboard
    m_leaderboardRowTexts.clear();
    float rowStartY = -110.0f;
    float rowSpacing = 42.0f;
    for (size_t i = 0; i < 5; ++i)
    {
        Color rankColor = (i == 0) ? Color{255, 215, 0, 255}
                        : ((i == 1) ? Color{220, 225, 240, 255}
                        : ((i == 2) ? Color{215, 140, 70, 255} : Colors::White));

        Entity rowEntity = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
            .Text = "#" + std::to_string(i + 1) + "   ---            0 PTS",
            .Position = {0.0f, rowStartY + i * rowSpacing},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = rankColor,
            .AnchorPoint = Anchor::Center
        });
        m_leaderboardRowTexts.push_back(rowEntity);
    }

    // Leaderboard Header
    UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "- HALL OF FAME  (TOP 5 ARCADE LEGENDS) -",
        .Position = {0.0f, -155.0f},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .Tint = Color{255, 215, 0, 255},
        .AnchorPoint = Anchor::Center
    });

    // Final score text
    m_gameOverScoreText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "FINAL SCORE: 0  |  STAGE REACHED: 1",
        .Position = {0.0f, -230.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Color{255, 230, 120, 255},
        .AnchorPoint = Anchor::Center
    });

    // Title: GAME OVER
    m_gameOverTitleText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "GAME OVER",
        .Position = {0.0f, -295.0f},
        .FontId = m_fontId,
        .FontSize = 72.0f,
        .Tint = Color{240, 50, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner plates ---

    // Leaderboard card inner background plate
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, -40.0f},
        .Size = {1020.0f, 290.0f},
        .Tint = Color{25, 20, 38, 230},
        .AnchorPoint = Anchor::Center
    });

    // Header accent line
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, -345.0f},
        .Size = {1060.0f, 8.0f},
        .Tint = Color{240, 50, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay (created last -> rendered first) ---

    // Dark semi-transparent card backdrop
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1120.0f, 820.0f},
        .Tint = Color{16, 12, 24, 252},
        .AnchorPoint = Anchor::Center
    });

    // Outer neon glow border
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1128.0f, 828.0f},
        .Tint = Color{220, 50, 50, 180},
        .AnchorPoint = Anchor::Center
    });

    // Fullscreen dimmed backdrop
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 215},
        .AnchorPoint = Anchor::Center
    });
}

void GameScene::CreateLevelClearMenu(const GameContext& context)
{
    m_levelClearCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_levelClearCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Texts (created first -> rendered last on top) ---

    // Continue button
    UIFactory::CreateButton(m_registry, m_levelClearCanvas, ButtonDescriptor{
        .Text = "CONTINUE [SPACE]",
        .OnClick = [this]() {
            m_levelTransitionTimer = 0.0f;
        },
        .Position = {0.0f, 195.0f},
        .Size = {340.0f, 64.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{45, 125, 70, 255},
        .HoverColor = Color{65, 170, 95, 255},
        .PressedColor = Color{30, 85, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    // Countdown / next stage status
    m_levelClearTimerText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "NEXT STAGE IN 10s (OR PRESS SPACE)",
        .Position = {0.0f, 85.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    // Stats
    m_levelClearStatsText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "Accumulated Score: 0  |  Lives: 3",
        .Position = {0.0f, -30.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Colors::White,
        .AnchorPoint = Anchor::Center
    });

    // Subtitle & congratulations
    UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "- EXCELLENT WORK -",
        .Position = {0.0f, -135.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{255, 220, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // Title: STAGE CLEARED!
    m_levelClearTitleText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "STAGE CLEARED!",
        .Position = {0.0f, -200.0f},
        .FontId = m_fontId,
        .FontSize = 72.0f,
        .Tint = Color{70, 240, 110, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner Stats panel & accent line ---

    // Inner Stats panel
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, -30.0f},
        .Size = {1000.0f, 130.0f},
        .Tint = Color{20, 48, 35, 230},
        .AnchorPoint = Anchor::Center
    });

    // Top accent line
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, -260.0f},
        .Size = {1060.0f, 8.0f},
        .Tint = Color{50, 220, 90, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay (created last -> rendered first) ---

    // Backdrop panel
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1120.0f, 640.0f},
        .Tint = Color{12, 28, 20, 252},
        .AnchorPoint = Anchor::Center
    });

    // Glowing green outer border
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1128.0f, 648.0f},
        .Tint = Color{50, 205, 80, 180},
        .AnchorPoint = Anchor::Center
    });

    // Fullscreen dimmed backdrop
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 200},
        .AnchorPoint = Anchor::Center
    });
}

void GameScene::CreateVictoryMenu(const GameContext& context)
{
    m_victoryCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_victoryCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Texts (created first -> rendered last on top) ---

    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "PLAY AGAIN [SPACE]",
        .OnClick = [this]() {
            if (mp_state_machine)
            {
                mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
            }
            else
            {
                FullReset();
            }
        },
        .Position = {-170.0f, 175.0f},
        .Size = {300.0f, 60.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{50, 110, 160, 255},
        .HoverColor = Color{75, 145, 205, 255},
        .PressedColor = Color{35, 75, 110, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            if (mp_context)
            {
                mp_context->Audio.StopMusic();
                mp_context->Scenes.LoadScene<MenuScene>();
            }
        },
        .Position = {170.0f, 175.0f},
        .Size = {300.0f, 60.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{80, 80, 80, 255},
        .HoverColor = Color{115, 115, 115, 255},
        .PressedColor = Color{45, 45, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    m_victoryStatsText = UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "FINAL SCORE: 0",
        .Position = {0.0f, -30.0f},
        .FontId = m_fontId,
        .FontSize = 42.0f,
        .Tint = Colors::White,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "ALL STAGES COMPLETED - GAME CLEARED !",
        .Position = {0.0f, -140.0f},
        .FontId = m_fontId,
        .FontSize = 34.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "VICTORY !",
        .Position = {0.0f, -210.0f},
        .FontId = m_fontId,
        .FontSize = 80.0f,
        .Tint = Color{255, 225, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner Stats panel & accent line ---

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, -30.0f},
        .Size = {1000.0f, 130.0f},
        .Tint = Color{35, 30, 55, 230},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, -280.0f},
        .Size = {1080.0f, 8.0f},
        .Tint = Color{255, 215, 0, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay (created last -> rendered first) ---

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1140.0f, 680.0f},
        .Tint = Color{22, 18, 34, 252},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1148.0f, 688.0f},
        .Tint = Color{255, 215, 0, 180},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 215},
        .AnchorPoint = Anchor::Center
    });
}



