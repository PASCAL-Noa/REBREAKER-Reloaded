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
#include "StateMachine/Transition.h"
#include "Conditions/KeyPressCondition.h"
#include "Conditions/GameConditions.h"
#include "Actions/ResetGameAction.h"
#include "AudioMixer.h"
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

    context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", true, RuleAccess::Public);
    context.Rules.RegisterRule(Rule::Graphics::EnableParticles, "Toggle Particles", true, RuleAccess::Public);
    context.Rules.RegisterRule(Rule::Debug::ShowCollider, "Toggle Hitbox", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::Invincible, "God mod", false, RuleAccess::Private);
    context.Rules.RegisterRule(Rule::Gameplay::InfiniteLives, "Infinite lives", false, RuleAccess::Private);

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
    m_fireTexId = context.Resources.LoadResource("Resources/sprite/fire.png");
    m_heartTexId = context.Resources.LoadResource("Resources/sprite/heart.png");

    m_systemManager.AddSystem<PhysicsSystem>(m_registry, context.Events);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.AddSystem<AnimatorSystem>(m_registry);
    m_systemManager.AddSystem<ParticleSystem>(m_registry, context.Render, 20000);
    m_systemManager.AddSystem<GameFeelSystem>(m_registry, context);
    m_systemManager.AddSystem<TweenSystem>(m_registry);

    m_registry.AddComponent<TweenComponent>(m_camera, TweenComponent{});
    auto& camTween = m_registry.GetComponent<TweenComponent>(m_camera);
    
    TweenEffects::CameraBreathing(camTween, camera, 0.74f, 0.76f, 6.0f);
    
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

    context.Events.Subscribe<CollisionEvent>([&context, this](const CollisionEvent& e)
    {
        if (m_ballState != BallState::Active) return;

        if (e.EntityA != m_ball && e.EntityB != m_ball) return;

        bool isBrick = m_registry.HasComponent<BrickComponent>(e.EntityA) ||
                       m_registry.HasComponent<BrickComponent>(e.EntityB);
        bool isPaddle = (e.EntityA == m_paddle || e.EntityB == m_paddle);
        bool isBottomWall = (e.EntityA == m_bottomWall || e.EntityB == m_bottomWall);

        HandleBrickCollision(e.EntityA);
        HandleBrickCollision(e.EntityB);

        if (isPaddle)
        {
            HandlePaddleCollision();
        }
        else if (isBottomWall)
        {
            int currentState = mp_state_machine ? mp_state_machine->GetCurrentState() : static_cast<int>(SceneState::Playing);
            if (!context.Rules.GetRule(Rule::Gameplay::Invincible) && currentState == static_cast<int>(SceneState::Playing))
            {
                HandleDeath();
            }
        }
        else if (!isBrick && !isPaddle && !isBottomWall)
        {
            context.Audio.PlaySfx(m_bounceSfxId, 50.0f);
        }

        if (isPaddle || (!isBrick && !isPaddle && !isBottomWall))
        {
            if (m_registry.HasComponent<Transform2D>(m_ball) && m_registry.HasComponent<SpriteComponent>(m_ball))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(m_ball);
                const auto& sprite = m_registry.GetComponent<SpriteComponent>(m_ball);
                SpawnExplosionParticles(transform.Position, sprite.Tint, 5);
            }
        }
    });

    CreateWall(0.0f, -550.0f, 2000.0f, 200.0f);
    CreateWall(-900.0f, 0.0f, 200.0f, 1200.0f);
    CreateWall(900.0f, 0.0f, 200.0f, 1200.0f);
    m_bottomWall = CreateWall(0.0f, 550.0f, 2000.0f, 200.0f);

    m_ball = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_ball, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<CircleCollider>(m_ball, CircleCollider{20.0f, Vector2f{0.0f, 0.0f}, false});
    m_registry.AddComponent<RigidBody>(m_ball, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_ball, SpriteComponent{m_ballTexId});
    m_registry.AddComponent<TweenComponent>(m_ball, TweenComponent{});

    m_paddle = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_paddle, Transform2D{Vector2f{0.0f, 300.0f}});
    m_registry.AddComponent<BoxCollider>(m_paddle, BoxCollider{Vector2f{120.0f, 20.0f}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(m_paddle, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    m_registry.AddComponent<SpriteComponent>(m_paddle, SpriteComponent{m_paddleTexId});
    m_registry.AddComponent<TweenComponent>(m_paddle, TweenComponent{});

    CreateUILayout(context);
    m_textFeedback = std::make_unique<TextFeedback>(m_registry, *mp_context, m_fontId, m_fireTexId);
    m_textFeedback->OnInit(m_uiCanvas);

    m_systemManager.OnInit();

    mp_levelGenerator = new FileLevelGenerator("Resources/levels/level01.txt");
    m_brickCount = mp_levelGenerator->Generate(m_registry, context, m_brickTexId);
    ResetBallAndPaddle();

    m_playlist.AddTrack(context, "Resources/audio/music/Game-1.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-2.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-3.ogg");
    m_playlist.AddTrack(context, "Resources/audio/music/Game-4.ogg");
    m_playlist.PlayNext(context);

    mp_state_machine = new StateMachine<GameScene>(this, 4);

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new VictoryCondition<GameScene>(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Escape), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new ResetGameAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Space), static_cast<int>(SceneState::Playing)));

    State<GameScene>* victoryState = mp_state_machine->CreateState(static_cast<int>(SceneState::Victory));
    victoryState->AddAction(new ResetGameAction<GameScene>());
    victoryState->AddTransition(new Transition<GameScene>(new KeyPressCondition<GameScene>(KeyCode::Space), static_cast<int>(SceneState::Playing)));

    mp_state_machine->SetState(static_cast<int>(SceneState::Playing));

    CreatePauseMenu(context);
    CreateAudioTab(context);
    CreateRenderTab(context);
    CreateInputsTab(context);
    CreateGamerulesTab(context);
    CreateSettingsLayout(context);

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
        
        if (currentState != static_cast<int>(SceneState::Paused) && m_settingsLayoutCanvas != NULL_ENTITY)
        {
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
            if (m_activeTabCanvas != NULL_ENTITY) {
                m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
                m_activeTabCanvas = NULL_ENTITY;
            }
        }
    }

    if (m_cheatTimer > 0.0f) {
        m_cheatTimer -= dt;
        if (m_cheatTimer <= 0.0f && m_cheatTextEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatTextEntity)) {
            m_registry.GetComponent<RectTransform>(m_cheatTextEntity).IsActive = false;
        }
    }

    if (currentState == static_cast<int>(SceneState::Playing))
    {
        HandleInput(dt, context);
        
        if (m_ballState == BallState::Dying)
        {
            if (m_registry.HasComponent<Transform2D>(m_ball))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(m_ball);
                SpawnBleedParticles(transform.Position);
            }
        }
        
        mp_levelGenerator->Update(dt, m_registry, context);
        m_scoreManager.Update(dt);

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
        m_playlist.Update(context);
        m_systemManager.OnUpdate(dt);
    }

    UISystem::OnUpdate(dt, m_registry, context);
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
        else if (state == static_cast<int>(SceneState::Victory)) gameStateStr = "VICTOIRE - ESPACE POUR REJOUER";
    }

    std::string debugStr = showDebug ? "ON" : "OFF";
    std::string shaderStr = context.Rules.GetRule(Rule::Graphics::EnableShader) ? "ON" : "OFF";
    std::string invStr = context.Rules.GetRule(Rule::Gameplay::Invincible) ? "ON" : "OFF";
    std::string infLivesStr = context.Rules.GetRule(Rule::Gameplay::InfiniteLives) ? "ON" : "OFF";

    std::string stats = "Record : " + std::to_string(m_scoreManager.GetHighScore());

    stats += "\nDebug (G) : " + debugStr + " | Shader (F) : " + shaderStr;
    stats += "\nGod mod (I) : " + invStr + " | Infinite lives (L) : " + infLivesStr;
    stats += "\nState : " + gameStateStr;

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
    
    if (chance(rng) > 0.5f)
    {
        std::uniform_real_distribution<float> posOffsetX(-10.0f, 10.0f);
        std::uniform_real_distribution<float> posOffsetY(-10.0f, 10.0f);
        std::uniform_real_distribution<float> velDist(-150.0f, 150.0f);
        
        Entity p = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(p, Transform2D{Vector2f{position.X + posOffsetX(rng), position.Y + posOffsetY(rng)}});
        
        ParticleComponent particle;
        particle.Velocity = Vector2f{velDist(rng), velDist(rng)};
        particle.Life = 0.6f;
        particle.MaxLife = 0.6f;
        particle.Size = 10.0f;
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
    std::uniform_real_distribution<float> lifeDist(0.6f, 1.f);
    std::uniform_real_distribution<float> sizeDist(3.0f, 8.0f);
    std::uniform_real_distribution<float> posOffsetX(-15.0f, 15.0f);
    std::uniform_real_distribution<float> posOffsetY(-10.0f, 10.0f);

    for (int i = 0; i < count; ++i)
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

void GameScene::OnDestroy(GameContext& context)
{
    delete mp_state_machine;
    mp_state_machine = nullptr;

    delete mp_levelGenerator;
    mp_levelGenerator = nullptr;

    context.Events.Clear();
    DefaultScene::OnDestroy(context);
}

void GameScene::FullReset()
{
    m_scoreManager.Reset();
    m_lives = 3;
    m_brickCount = 0;

    for (Entity heart : m_heartEntities)
    {
        if (m_registry.HasComponent<RectTransform>(heart))
        {
            m_registry.GetComponent<RectTransform>(heart).IsActive = true;
        }
    }

    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntity(e);
    });

    m_brickCount = mp_levelGenerator->Generate(m_registry, *mp_context, m_brickTexId);
    ResetBallAndPaddle();
}

void GameScene::HandleInput(const float dt, const GameContext& context)
{
    CheckKonamiCode(context);

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

    if (context.Input.IsKeyDown(KeyCode::Q)) paddleTransform.Position.X -= speed * dt;
    if (context.Input.IsKeyDown(KeyCode::D)) paddleTransform.Position.X += speed * dt;

    constexpr float limitX = 740.0f;

    if (paddleTransform.Position.X < -limitX)
    {
        paddleTransform.Position.X = -limitX;
    }
    else if (paddleTransform.Position.X > limitX)
    {
        paddleTransform.Position.X = limitX;
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
                    ballRb.Velocity = Vector2f{500.0f, -400.0f};
                    ballRb.IsKinematic = false;
                }
            }
        }
    }
}

void GameScene::ResetBallAndPaddle()
{
    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
    auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);

    paddleTransform.Position = Vector2f{0.0f, 300.0f};

    ballTransform.Position = Vector2f{paddleTransform.Position.X, paddleTransform.Position.Y - 40.0f};
    ballRb.Velocity = Vector2f{0.0f, 0.0f};
    ballRb.IsKinematic = true;

    m_ballState = BallState::Spawning;
    ballTransform.Scale = Vector2f{0.0f, 0.0f};

    if (!m_registry.HasComponent<TweenComponent>(m_ball)) {
        m_registry.AddComponent<TweenComponent>(m_ball, TweenComponent{});
    }

    TweenEffects::BallIn(m_registry.GetComponent<TweenComponent>(m_ball), ballTransform, [this]() {
        m_ballState = BallState::Attached;
    }, 0.5f);
}

void GameScene::HandleDeath()
{
    m_scoreManager.BreakCombo();
    mp_context->Events.Publish(BallDeathEvent(m_ball));

    m_ballState = BallState::Dying;
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
            ResetBallAndPaddle();
        }
    };

    m_registry.GetComponent<TweenComponent>(m_ball).AddTween(scaleTween);
}

void GameScene::HandleBrickCollision(Entity entity)
{
    if (!m_registry.HasComponent<BrickComponent>(entity)) return;

    auto& brick = m_registry.GetComponent<BrickComponent>(entity);
    brick.HitPoints--;

    bool isDestroyed = (brick.HitPoints <= 0);

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
        }
        
        m_registry.DestroyEntity(entity);
    }
}

void GameScene::HandlePaddleCollision()
{
    m_scoreManager.BreakCombo();

    auto& ballRb = m_registry.GetComponent<RigidBody>(m_ball);
    const auto& ballTransform = m_registry.GetComponent<Transform2D>(m_ball);
    const auto& paddleTransform = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& paddleCollider = m_registry.GetComponent<BoxCollider>(m_paddle);

    const float paddleHalfWidth = paddleCollider.Size.X * 0.5f;
    const float offset = ballTransform.Position.X - paddleTransform.Position.X;

    float hitFactor = offset / paddleHalfWidth;
    if (hitFactor < -1.0f) hitFactor = -1.0f;
    if (hitFactor > 1.0f) hitFactor = 1.0f;

    const float speed = std::sqrt(ballRb.Velocity.X * ballRb.Velocity.X + ballRb.Velocity.Y * ballRb.Velocity.Y);
    constexpr float maxAngle = 60.0f * 3.14159265f / 180.0f;
    const float bounceAngle = hitFactor * maxAngle;

    ballRb.Velocity.X = speed * std::sin(bounceAngle);
    ballRb.Velocity.Y = -speed * std::cos(bounceAngle);

    mp_context->Events.Publish(PaddleHitEvent(m_paddle, m_ball));
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

    // Cheat Text (Hidden by default)
    m_cheatTextEntity = UIFactory::CreateText(m_registry, m_settingsLayoutCanvas, TextDescriptor{
        .Text = "CHEAT UNLOCKED!",
        .Position = {0.0f, viewY * 0.45f},
        .FontId = m_fontId,
        .FontSize = 40.0f,
        .Tint = Colors::Yellow
    });

    m_registry.GetComponent<RectTransform>(m_cheatTextEntity).IsActive = false;

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
            context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, context.Audio.GetSfxVolume() + 10.0f);
            context.Audio.SetSfxVolume(vol);
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
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, context.Audio.GetMusicVolume() + 10.0f);
            context.Audio.SetMusicVolume(vol);
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

void GameScene::CheckKonamiCode(const GameContext& context)
{
    if (m_konamiSequence.empty()) return;

    if (context.Input.IsKeyPress(m_konamiSequence[m_konamiIndex]))
    {
        m_konamiIndex++;
        if (m_konamiIndex >= m_konamiSequence.size())
        {
            m_konamiIndex = 0;
            if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            
            if (m_cheatTextEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatTextEntity))
            {
                m_registry.GetComponent<RectTransform>(m_cheatTextEntity).IsActive = true;
                m_cheatTimer = 5.0f;
            }
        }
    }
    else
    {
        for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i)
        {
            if (context.Input.IsKeyPress(static_cast<KeyCode>(i)))
            {
                m_konamiIndex = 0;
                if (context.Input.IsKeyPress(m_konamiSequence[0])) {
                    m_konamiIndex = 1;
                }
                break;
            }
        }
    }
}

