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
#include <random>
#include <cmath>

#include "ECS/Components/TweenComponent.h"
#include "Factories/BrickFactory.h"
#include "Generators/FileLevelGenerator.h"
#include "Events/GameplayEvents.h"
#include "ECS/Systems/GameFeelSystem.h"
#include "ECS/Systems/TweenSystem.h"
#include "ECS/Systems/AnimatorSystem.h"
#include "ECS/Components/AnimatorComponent.h"
#include "TweenEffects/TweenEffects.h"

void GameScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    mp_context = &context;

    context.Rules.RegisterRule(Rule::Graphics::EnableShader, "Toggle Shaders", false, RuleAccess::Public);
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

    m_explodingHeart = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_explodingHeart, Transform2D{Vector2f{0.0f, 0.0f}, 0.0f, Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<SpriteComponent>(m_explodingHeart, SpriteComponent{m_heartTexId, Colors::Transparent});
    m_registry.AddComponent<TweenComponent>(m_explodingHeart, TweenComponent{});

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

    m_textFeedback = std::make_unique<TextFeedback>(m_registry, *mp_context, m_fontId, m_fireTexId);
    m_textFeedback->OnInit();

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
}

void GameScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;

    if (mp_state_machine)
    {
        mp_state_machine->Update();
    }

    int currentState = mp_state_machine ? mp_state_machine->GetCurrentState() : static_cast<int>(SceneState::Playing);

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

        m_textFeedback->OnUpdate(dt, m_scoreManager.GetComboMultiplier(), m_scoreManager.GetScore());
    }

    if (currentState != static_cast<int>(SceneState::Paused))
    {
        m_playlist.Update(context);
        m_systemManager.OnUpdate(dt);
    }
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

    bool showDebug = context.Rules.GetRule(Rule::Debug::ShowCollider);
    if (showDebug)
    {
        m_registry.View<Transform2D, BoxCollider>([&](Entity, const Transform2D& t, const BoxCollider& b) {
            Color color = b.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, color, -2.0f);
        });

        m_registry.View<Transform2D, CircleCollider>([&](Entity, const Transform2D& t, const CircleCollider& c) {
            Color color = c.IsColliding ? Colors::Red : Colors::Green;
            context.Render.DrawCircleOutline(c.Radius, t, color, -2.0f);
        });
    }

    DrawScoreAndFlame(context);
    DrawHearts(context);

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
}

uint32_t GameScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
}

Entity GameScene::CreateWall(float x, float y, float w, float h)
{
    Entity wall = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(wall, Transform2D{Vector2f{x, y}});
    m_registry.AddComponent<BoxCollider>(wall, BoxCollider{Vector2f{w, h}, Vector2f{0.0f, 0.0f}, false, false});
    m_registry.AddComponent<RigidBody>(wall, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});

    return wall;
}

void GameScene::SpawnBleedParticles(const Vector2f& position)
{
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

void GameScene::SpawnExplosionParticles(const Vector2f& position, const Color& color, int count)
{
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

    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntity(e);
    });

    m_brickCount = mp_levelGenerator->Generate(m_registry, *mp_context, m_brickTexId);
    ResetBallAndPaddle();
}

void GameScene::HandleInput(float dt, const GameContext& context)
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
            if (m_registry.HasComponent<Transform2D>(m_explodingHeart) && m_registry.HasComponent<SpriteComponent>(m_explodingHeart) && m_registry.HasComponent<TweenComponent>(m_explodingHeart))
            {
                auto& t = m_registry.GetComponent<Transform2D>(m_explodingHeart);
                auto& s = m_registry.GetComponent<SpriteComponent>(m_explodingHeart);
                auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_explodingHeart);
                
                t.Position = Vector2f{780.0f - (m_lives - 1) * 70.0f, -486.0f};
                t.Scale = Vector2f{3.0f, 3.0f};
                s.Tint = Colors::White;
                
                TweenConfig<float> config;
                config.Start = 0.0f;
                config.End = 1.0f;
                config.Duration = 0.5f;
                config.Ease = EasingFunctions::EasingType::EaseOutQuad;
                
                Entity heartEntity = m_explodingHeart;
                config.Setter = [this, heartEntity](float val) {
                    if (m_registry.HasComponent<Transform2D>(heartEntity) && m_registry.HasComponent<SpriteComponent>(heartEntity)) {
                        auto& transform = m_registry.GetComponent<Transform2D>(heartEntity);
                        auto& sprite = m_registry.GetComponent<SpriteComponent>(heartEntity);
                        
                        transform.Scale = Vector2f{3.0f + 4.0f * val, 3.0f + 4.0f * val};
                        sprite.Tint.a = static_cast<uint8_t>(255.0f * (1.0f - val));
                    }
                };
                
                tweenComp.AddTween(config);
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
        m_textFeedback->SpawnComboText(transform.Position, m_scoreManager.GetComboMultiplier());
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

void GameScene::DrawScoreAndFlame(GameContext& context) const
{
    m_textFeedback->OnRender();

    std::string scoreStr = "Score : " + std::to_string(m_scoreManager.GetScore());
    
    Vector2f viewSize = context.Render.GetLogicalViewSize();
    Vector2f scorePos{-(viewSize.X / 2.0f) + 500.0f, -(viewSize.Y / 2.0f) + 290.0f};
    
    context.Render.DrawText(scoreStr, m_fontId, 48.0f, Transform2D{scorePos}, Colors::White);

    Vector2f scoreSize = context.Render.GetTextSize(scoreStr, m_fontId, 48.0f);
    m_textFeedback->SetFlamePosition(Vector2f{scorePos.X + scoreSize.X + 30.0f, scorePos.Y + scoreSize.Y + 24.0f});
}

void GameScene::DrawHearts(GameContext& context) const
{
    if (m_heartTexId)
    {
        Vector2f viewSize = context.Render.GetLogicalViewSize();
        for (int i = 0; i < m_lives; ++i)
        {
            SpriteComponent heartSprite{m_heartTexId};
            heartSprite.Tint.a = static_cast<uint8_t>(m_heartsAlpha);
            
            Transform2D heartTransform;
            heartTransform.Position = Vector2f{(viewSize.X / 2.0f) - 500.0f - (m_lives - 1 - i) * 70.0f, -(viewSize.Y / 2.0f) + 310.0f};
            heartTransform.Scale = Vector2f{3.f, 3.f};
            context.Render.DrawSprite(heartSprite, heartTransform);
        }
    }
}
