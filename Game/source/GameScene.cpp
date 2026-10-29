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

namespace
{
    Color DiscoRainbowColor(float hue, uint8_t alpha = 255)
    {
        float h = std::fmod(hue, 1.0f);
        if (h < 0.0f) h += 1.0f;
        float r = std::clamp((std::abs(h * 6.0f - 3.0f) - 1.0f), 0.0f, 1.0f);
        float g = std::clamp((2.0f - std::abs(h * 6.0f - 2.0f)), 0.0f, 1.0f);
        float b = std::clamp((2.0f - std::abs(h * 6.0f - 4.0f)), 0.0f, 1.0f);
        return Color{
            static_cast<uint8_t>(r * 255.0f),
            static_cast<uint8_t>(g * 255.0f),
            static_cast<uint8_t>(b * 255.0f),
            alpha
        };
    }

    void DrawMusicNote(GameContext& context, const Vector2f& pos, float scale, float rotAngle, const Color& color, bool doubleNote)
    {
        float r = 5.0f * scale;
        float stemH = 20.0f * scale;
        float lineThick = std::max(1.5f, 2.0f * scale);

        auto rotateVec = [rotAngle](const Vector2f& v) -> Vector2f {
            float cosA = std::cos(rotAngle);
            float sinA = std::sin(rotAngle);
            return Vector2f{v.X * cosA - v.Y * sinA, v.X * sinA + v.Y * cosA};
        };

        if (!doubleNote)
        {
            // Single note (eighth note ♪)
            Vector2f headCenter = pos;
            Transform2D headTrans{headCenter, rotAngle - 0.35f, Vector2f{1.35f * scale, 0.95f * scale}};
            context.Render.DrawCircle(r, headTrans, color);

            Vector2f stemBottom = headCenter + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
            Vector2f stemTop = headCenter + rotateVec(Vector2f{r * 0.85f * scale, -stemH});
            context.Render.DrawLine(stemBottom, stemTop, color, lineThick);

            Vector2f flagMid = stemTop + rotateVec(Vector2f{6.0f * scale, 5.0f * scale});
            Vector2f flagTip = stemTop + rotateVec(Vector2f{4.0f * scale, 12.0f * scale});
            context.Render.DrawLine(stemTop, flagMid, color, lineThick);
            context.Render.DrawLine(flagMid, flagTip, color, lineThick * 0.85f);
        }
        else
        {
            // Double note (beamed pair ♫)
            float noteDist = 16.0f * scale;
            Vector2f head1 = pos + rotateVec(Vector2f{-noteDist * 0.5f, 2.0f * scale});
            Vector2f head2 = pos + rotateVec(Vector2f{noteDist * 0.5f, -2.0f * scale});

            Transform2D t1{head1, rotAngle - 0.35f, Vector2f{1.3f * scale, 0.9f * scale}};
            Transform2D t2{head2, rotAngle - 0.35f, Vector2f{1.3f * scale, 0.9f * scale}};
            context.Render.DrawCircle(r, t1, color);
            context.Render.DrawCircle(r, t2, color);

            Vector2f stem1Bottom = head1 + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
            Vector2f stem1Top = head1 + rotateVec(Vector2f{r * 0.85f * scale, -stemH});
            Vector2f stem2Bottom = head2 + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
            Vector2f stem2Top = head2 + rotateVec(Vector2f{r * 0.85f * scale, -stemH});

            context.Render.DrawLine(stem1Bottom, stem1Top, color, lineThick);
            context.Render.DrawLine(stem2Bottom, stem2Top, color, lineThick);

            // Cross beam
            context.Render.DrawLine(stem1Top, stem2Top, color, lineThick * 1.8f);
        }
    }
}
#include "Generators/FileLevelGenerator.h"
#include "Scenes/MenuScene.h"
#include "TweenEffects/TweenEffects.h"

void GameScene::PlaySfx(uint32_t id, float volume, float pitch, float pan) const
{
    if (m_audioEnabled && mp_context)
    {
        mp_context->Audio.PlaySfx(id, volume, pitch, pan);
    }
}

void GameScene::SetAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
    if (mp_gameFeelSystem)
    {
        mp_gameFeelSystem->SetAudioEnabled(enabled);
    }
}

void GameScene::RestorePostUltimateMusic()
{
    if (!mp_context || !m_musicEnabled) return;
    m_playlist.PlayNext(*mp_context, 1.2f);
}

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
    m_powerUpRainShaderId = context.Resources.LoadResource("Resources/shaders/lucky.frag");
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
    m_ultimateReadySfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_buff.wav");
    m_plasmaLaunchSfxId = context.Resources.LoadResource("Resources/audio/sfx/plasma_launch.wav");
    m_plasmaExplodeSfxId = context.Resources.LoadResource("Resources/audio/sfx/plasma_explode.wav");
    m_discoShockwaveSfxId = context.Resources.LoadResource("Resources/audio/sfx/disco_shockwave.wav");
    m_discoVoiceoverSfxId = context.Resources.LoadResource("Resources/audio/sfx/disco_voiceover.wav");
    m_discoHitSfxIds[0] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_bass.wav");
    m_discoHitSfxIds[1] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_brass.wav");
    m_discoHitSfxIds[2] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_guitar.wav");
    m_discoHitSfxIds[3] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_synth.wav");
    m_discoHitSfxIds[4] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_cowbell.wav");
    m_discoHitSfxIds[5] = context.Resources.LoadResource("Resources/audio/sfx/disco_hit_strings.wav");
    m_itemRegenRestoreSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_buff.wav");
    m_plasmaShaderId = context.Resources.LoadResource("Resources/shaders/plasma.frag");
    m_discoShaderId = context.Resources.LoadResource("Resources/shaders/disco_ball.frag");

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
    mp_gameFeelSystem = m_systemManager.AddSystem<GameFeelSystem>(m_registry, context);
    if (mp_gameFeelSystem)
    {
        mp_gameFeelSystem->SetAudioEnabled(m_audioEnabled);
    }
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
            if (m_audioEnabled)
            {
                context.Audio.PlaySfx(m_bounceSfxId, 50.0f, 1.0f, bouncePan);
            }
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
        UpdateRegeneratingItemBricks(dt);
        UpdateUltimateAbilities(dt);

        bool canSpaceTrigger = m_stuckBalls.empty();
        if ((canSpaceTrigger && context.Input.IsKeyPress(KeyCode::Space)) ||
            context.Input.IsJoystickButtonPress(0, 2) ||
            context.Input.IsJoystickButtonPress(0, 5))
        {
            TryTriggerUltimate();
        }

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

                    if (m_bounceSfxId != 0 && m_audioEnabled)
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

    // Smoothly calculate disco blackout / club darkness factor (lights turned off)
    float discoDarkFactor = 0.0f;
    if (m_discoTimer > 0.0f)
    {
        float entry = std::clamp((m_discoDuration - m_discoTimer) / 0.45f, 0.0f, 1.0f);
        float exit = std::clamp(m_discoTimer / 0.75f, 0.0f, 1.0f);
        discoDarkFactor = std::min(entry, exit);
    }

    Color effectiveBgColor = m_bgColor;
    if (discoDarkFactor > 0.0f)
    {
        effectiveBgColor.r = static_cast<uint8_t>(m_bgColor.r * (1.0f - discoDarkFactor * 0.85f));
        effectiveBgColor.g = static_cast<uint8_t>(m_bgColor.g * (1.0f - discoDarkFactor * 0.85f));
        effectiveBgColor.b = static_cast<uint8_t>(m_bgColor.b * (1.0f - discoDarkFactor * 0.85f));
    }

    // Render ambient background layer (behind gameplay entities)
    m_ambientBackground.OnRender(context.Render, effectiveBgColor, cameraComp.Position, paddlePos);

    // Wandering yellow musical note particles in the background
    RenderDiscoBackgroundMusicNotes(context);

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

    // Disco Fever illuminated dancefloor tile frames around bricks
    if (m_discoTimer > 0.0f)
    {
        float pulse = 0.5f + 0.5f * std::sin(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 20.0f : 12.0f));
        m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity, const Transform2D& t, const BoxCollider& b, const BrickComponent& brick)
        {
            if (brick.IsRegenerating) return;
            float colIdx = std::floor((t.Position.X + 800.0f) / 70.0f);
            float rowIdx = std::floor((t.Position.Y + 450.0f) / 30.0f);
            float discoHue = std::fmod(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 2.5f : 1.2f) + (colIdx * 0.08f) + (rowIdx * 0.12f), 1.0f);
            Color discoOutline = DiscoRainbowColor(discoHue + 0.5f, static_cast<uint8_t>(180 + 75 * pulse));

            // Disco dancefloor illuminated tile frame
            context.Render.DrawRectangleOutline(b.Size.X, b.Size.Y, t, discoOutline, -2.0f);
            // Inner neon facet reflection
            context.Render.DrawRectangleOutline(b.Size.X - 8.0f, b.Size.Y - 6.0f, t, Color{255, 255, 255, static_cast<uint8_t>(120 + 80 * pulse)}, -1.0f);
        });
    }

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
    const bool isDiscoDarkActive = (discoDarkFactor > 0.0f);
    if (m_currentTheme.HasLighting() || isDiscoDarkActive)
    {
        Color baseAmbient = m_currentTheme.AmbientLight;
        Color clubDark{10, 8, 18, 255}; // Extinguished room lights -> Authentic Disco Club!
        Color ambient{
            static_cast<uint8_t>(baseAmbient.r * (1.0f - discoDarkFactor) + clubDark.r * discoDarkFactor),
            static_cast<uint8_t>(baseAmbient.g * (1.0f - discoDarkFactor) + clubDark.g * discoDarkFactor),
            static_cast<uint8_t>(baseAmbient.b * (1.0f - discoDarkFactor) + clubDark.b * discoDarkFactor),
            255
        };
        context.Render.BeginLightingPass(ambient);

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

        // 6. Active Plasma Orbs illumination in dark levels (capped at 12 to preserve GPU fillrate)
        size_t plasmaLightCount = 0;
        for (const auto& orb : m_plasmaOrbs)
        {
            if (++plasmaLightCount > 12) break;
            float lightRadius = 160.0f * orb.Scale;
            float pulse = 1.0f + 0.2f * std::sin(orb.AnimTime * 15.0f);
            Color orbCol = (orb.Generation > 1) ? Color{160, 60, 255, 255} : Color{90, 210, 255, 255};
            context.Render.DrawPointLight(orb.Position, lightRadius * 1.3f, orbCol, 1.6f * pulse, BlendMode::Add);
        }

        // 7. Disco Fever dynamic illumination in club darkness
        if (m_discoTimer > 0.0f)
        {
            // A. Sweeping ceiling spotlights illuminating the dancefloor
            const Vector2f spotOrigins[4] = {
                {-650.0f, -440.0f},
                {-220.0f, -440.0f},
                {220.0f, -440.0f},
                {650.0f, -440.0f}
            };
            const Color spotColors[4] = {
                Color{255, 40, 140, 255},  // Neon Magenta
                Color{40, 220, 255, 255},  // Electric Cyan
                Color{255, 220, 40, 255},  // Golden Sun
                Color{170, 50, 255, 255}   // Ultraviolet
            };
            for (int sp = 0; sp < 4; ++sp)
            {
                float sweep = std::sin(m_gameplayTime * 2.2f + static_cast<float>(sp) * 1.57f);
                Vector2f targetFloor{
                    spotOrigins[sp].X * 0.4f + sweep * 440.0f,
                    380.0f
                };
                Vector2f toFloor = targetFloor - spotOrigins[sp];
                float spotAngleDeg = std::atan2(toFloor.Y, toFloor.X) * 180.0f / 3.14159265f;
                float spotDist = toFloor.Length() + 60.0f;
                context.Render.DrawConeLight(spotOrigins[sp], spotDist, spotAngleDeg, 38.0f, spotColors[sp], 2.4f * discoDarkFactor, BlendMode::Add);
                context.Render.DrawPointLight(targetFloor, 140.0f, spotColors[sp], 1.9f * discoDarkFactor, BlendMode::Add);
            }

            // B. Glowing Disco Bricks illumination in the darkened club
            m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity, const Transform2D& t, const BoxCollider&, const BrickComponent& brick)
            {
                if (brick.IsRegenerating) return;
                float colIdx = std::floor((t.Position.X + 800.0f) / 70.0f);
                float rowIdx = std::floor((t.Position.Y + 450.0f) / 30.0f);
                float discoHue = std::fmod(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 2.5f : 1.2f) + (colIdx * 0.08f) + (rowIdx * 0.12f), 1.0f);
                Color discoCol = DiscoRainbowColor(discoHue, 255);
                context.Render.DrawPointLight(t.Position, 72.0f, discoCol, 1.25f * discoDarkFactor, BlendMode::Add);
            });

            // C. Dancing Rainbow Big Balls & Trails
            for (const auto& db : m_discoBigBalls)
            {
                context.Render.DrawPointLight(db.Position, 220.0f, db.BaseColor, 1.8f, BlendMode::Add);
                for (size_t ti = 0; ti < db.Trail.size(); ti += 2)
                {
                    float tRatio = db.Trail[ti].Lifetime / db.Trail[ti].MaxLifetime;
                    context.Render.DrawPointLight(db.Trail[ti].Position, 90.0f * tRatio, db.BaseColor, 0.75f * tRatio, BlendMode::Add);
                }
            }

            // B. Boule a Facettes (Level 5 Mirror Ball)
            if (m_discoMirrorBall.IsActive)
            {
                float mLightR = m_discoMirrorBall.IsFrenzy ? 780.0f : 460.0f;
                float mLightInt = m_discoMirrorBall.IsFrenzy ? 3.6f : 2.6f;
                Color mLightCol = m_discoMirrorBall.IsFrenzy ? Color{255, 245, 210, 255} : Color{245, 250, 255, 255};
                context.Render.DrawPointLight(m_discoMirrorBall.Position, mLightR, mLightCol, mLightInt, BlendMode::Add);
            }

            // C. Wandering yellow musical notes illumination
            for (const auto& note : m_discoMusicNotes)
            {
                float nRatio = std::clamp(note.Lifetime / note.MaxLifetime, 0.0f, 1.0f);
                context.Render.DrawPointLight(note.Position, 75.0f * note.Scale, Color{255, 235, 60, 255}, 0.55f * nRatio, BlendMode::Add);
            }
        }

        context.Render.EndLightingPass(BlendMode::Multiply);
    }

    RenderRegeneratingBricks(context);
    RenderActiveUltimateVisuals(context);

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

        RenderUltimateMeterHUD(context);
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
    const float safeMaxOffsetFire = std::max(0.0f, radius * 0.25f);
    std::uniform_real_distribution<float> offsetDist(0.0f, safeMaxOffsetFire);
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

void GameScene::SpawnPlasmaExplosionParticles(const Vector2f& position, float radius, int count)
{
    if (mp_context && !mp_context->Rules.GetRule(Rule::Graphics::EnableParticles)) {
        return;
    }
    if (!m_particleSystem) return;

    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> speedDist(140.0f, 540.0f);
    std::uniform_real_distribution<float> lifeDist(0.35f, 0.75f);
    std::uniform_real_distribution<float> sizeDist(7.0f, 22.0f);
    const float safeMaxOffsetPlasma = std::max(0.0f, radius * 0.25f);
    std::uniform_real_distribution<float> offsetDist(0.0f, safeMaxOffsetPlasma);
    std::uniform_int_distribution<int> colorPick(0, 4);

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
        if (cp == 0)      c = Color{235, 250, 255, 255}; // Radiant white-cyan
        else if (cp == 1) c = Color{80, 220, 255, 255};  // Electric cyan
        else if (cp == 2) c = Color{40, 140, 255, 255};  // Deep plasma blue
        else if (cp == 3) c = Color{150, 70, 255, 240};  // Plasma violet-purple
        else              c = Color{90, 30, 220, 210};   // Deep indigo

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

    if (mp_context && m_audioEnabled)
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
    ClearActiveUltimates();
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
    ClearActiveUltimates();
    m_levelManager.SetLevel(levelIndex);
    ApplyLevelTheme(m_levelManager.GetCurrentLevelNumber());

    if (!preserveStats)
    {
        m_scoreManager.Reset();
        m_lives = PlayerPrefs::GetInt("Lives", 3);
        m_levelStartScore = 0;
        SetUltimateCharge(0.0f);
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
    ClearActiveUltimates();
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
    ClearActiveUltimates();
    if (mp_context)
    {
        if (m_musicEnabled) mp_context->Audio.FadeOutMusic(0.8f);
        if (m_audioEnabled && m_levelClearSfxId != 0)
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
    if (mp_context && m_musicEnabled)
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
    if (m_renderGameUI && mp_context)
    {
        mp_context->Events.Publish(GameStateChangedEvent(GameUIState::Countdown));
        if (m_audioEnabled && m_countdownReadySfxId != 0)
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
    ClearActiveUltimates();
    m_scoreManager.BreakCombo();

    if (mp_context)
    {
        // Smoothly fade out stage music and fade in game-over stinger
        if (m_musicEnabled) mp_context->Audio.FadeToMusic("Resources/audio/music/Game-death.ogg", 0.6f, 50.0f, false);
        if (m_audioEnabled && m_gameOverSfxId != 0)
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
    ClearActiveUltimates();
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

    if (mp_context && m_audioEnabled)
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
            if (nBrick.IsRegenerating) continue;

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

            if (nBrick.IsSpecial)
            {
                SpawnPowerUp(nPos);
                nBrick.IsRegenerating = true;
                nBrick.RegenCooldown = PowerUpManager::Get().ItemRegen().GetEffectiveCooldown();
                nBrick.RegenTimer = nBrick.RegenCooldown;
                if (m_registry.HasComponent<SpriteComponent>(neighbor))
                {
                    m_registry.GetComponent<SpriteComponent>(neighbor).Tint.a = 0;
                }
                if (m_registry.HasComponent<BoxCollider>(neighbor))
                {
                    m_registry.GetComponent<BoxCollider>(neighbor).IsTrigger = true;
                }
                mp_context->Events.Publish(BrickHitEvent(neighbor, true, true, m_scoreManager.GetComboMultiplier(), nPos));
                continue;
            }

            m_scoreManager.AddScore(nBrick.ScoreValue);
            m_brickCount--;

            mp_context->Events.Publish(BrickHitEvent(neighbor, true, nBrick.IsSpecial, m_scoreManager.GetComboMultiplier(), nPos));

            m_registry.DestroyEntityDeferred(neighbor);
        }
    }
}

void GameScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    if (m_registry.IsPendingDestroy(entity)) return;
    if (!m_registry.HasComponent<BrickComponent>(entity)) return;

    auto& brick = m_registry.GetComponent<BrickComponent>(entity);
    if (brick.IsRegenerating) return;

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
            if (mp_context && m_audioEnabled && m_bounceSfxId != 0)
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

            if (mp_context && m_audioEnabled && m_explosionSfxId != 0)
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

            if (mp_context && m_audioEnabled && m_bounceSfxId != 0)
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

    // Charge Ultimate Meter with controlled progression & combo bonus
    float chargeGain = isDestroyed ? 1.25f : 0.45f;
    chargeGain *= (1.0f + static_cast<float>(std::min(m_scoreManager.GetComboMultiplier(), 8u) - 1) * 0.15f);
    AddUltimateCharge(chargeGain);

    if (isDestroyed)
    {
        if (brick.IsSpecial)
        {
            if (m_registry.HasComponent<Transform2D>(entity) && m_registry.HasComponent<SpriteComponent>(entity))
            {
                const auto& transform = m_registry.GetComponent<Transform2D>(entity);
                auto& sprite = m_registry.GetComponent<SpriteComponent>(entity);

                SpawnExplosionParticles(transform.Position, sprite.Tint);
                SpawnPowerUp(transform.Position);

                brick.IsRegenerating = true;
                brick.RegenCooldown = PowerUpManager::Get().ItemRegen().GetEffectiveCooldown();
                brick.RegenTimer = brick.RegenCooldown;

                sprite.Tint.a = 0;
                if (m_registry.HasComponent<BoxCollider>(entity))
                {
                    m_registry.GetComponent<BoxCollider>(entity).IsTrigger = true;
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

            mp_context->Events.Publish(BrickHitEvent(entity, true, true, m_scoreManager.GetComboMultiplier(), contactPos));
            return;
        }

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

        if (m_bounceSfxId != 0 && mp_context && m_audioEnabled)
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

            if (m_bounceSfxId != 0 && mp_context && m_audioEnabled)
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

            if (m_bounceSfxId != 0 && mp_context && m_audioEnabled)
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
        if (mp_context && m_audioEnabled) mp_context->Audio.PlaySfx(m_despawnSfxId, 80.0f);

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
        if (mp_context && m_audioEnabled && m_conductionSfxId != 0)
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
    if (mp_context && m_audioEnabled)
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
    m_preDiscoBrickTints.clear();
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

void GameScene::AddUltimateCharge(float amount)
{
    if (m_ultimateReady) return;

    m_ultimateCharge = std::min(m_ultimateCharge + amount, m_ultimateMaxCharge);
    if (m_ultimateCharge >= m_ultimateMaxCharge)
    {
        m_ultimateCharge = m_ultimateMaxCharge;
        m_ultimateReady = true;
        if (mp_context && m_audioEnabled && m_ultimateReadySfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_ultimateReadySfxId, 95.0f);
        }
        TriggerPostProcessFlash(0.015f, 0.4f, Color{0, 220, 255, 255}, 0.2f, Color{255, 240, 150, 255});
    }
}

void GameScene::SetUltimateCharge(float charge)
{
    m_ultimateCharge = std::clamp(charge, 0.0f, m_ultimateMaxCharge);
    m_ultimateReady = (m_ultimateCharge >= m_ultimateMaxCharge);
}

void GameScene::TryTriggerUltimate()
{
    if (!m_ultimateReady) return;

    UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();
    int level = ProfileManager::Get().GetUltimateLevel(equipped);

    switch (equipped)
    {
        case UltimateAbilityType::PlasmaOrb:
            ExecutePlasmaOrb(level);
            break;
        case UltimateAbilityType::DiscoBall:
            ExecuteDiscoBall(level);
            break;
        case UltimateAbilityType::PowerUpRain:
            ExecutePowerUpRain(level);
            break;
        default:
            break;
    }

    if (mp_context && mp_context->Rules.GetRule(Rule::Gameplay::InfiniteUltimate))
    {
        m_ultimateCharge = m_ultimateMaxCharge;
        m_ultimateReady = true;
    }
    else
    {
        m_ultimateCharge = 0.0f;
        m_ultimateReady = false;
        m_ultimateReadyPulseTime = 0.0f;
    }
}

void GameScene::ClearActiveUltimates()
{
    m_plasmaOrbs.clear();
    m_soundWaves.clear();
    m_powerUpRainQueue.clear();

    if (m_plasmaActiveTimer > 0.0f || !m_prePlasmaMusicPath.empty())
    {
        m_plasmaActiveTimer = 0.0f;
        m_plasmaAnimTime = 0.0f;
        if (mp_context)
        {
            if (m_musicEnabled)
            {
                if (!m_prePlasmaMusicPath.empty() && m_prePlasmaMusicPath.find("plasma_ultimate") == std::string::npos)
                {
                    mp_context->Audio.PlayMusic(m_prePlasmaMusicPath, 80.0f, true);
                }
                else
                {
                    RestorePostUltimateMusic();
                }
            }
            else
            {
                std::string cur = mp_context->Audio.GetCurrentMusicPath();
                if (cur.find("plasma_ultimate") != std::string::npos)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                }
            }
            m_prePlasmaMusicPath.clear();
        }
    }

    if (m_discoTimer > 0.0f || !m_preDiscoMusicPath.empty() || !m_preDiscoBrickTints.empty())
    {
        m_discoTimer = 0.0f;
        RestoreDiscoBricks();
        m_discoBigBalls.clear();
        m_discoMirrorBall.IsActive = false;
        m_discoMirrorBall.IsFrenzy = false;
        m_discoMirrorBall.Scale = 1.35f;
        m_discoMirrorBall.TargetScale = 1.35f;

        if (mp_context)
        {
            if (m_musicEnabled)
            {
                if (!m_preDiscoMusicPath.empty() && m_preDiscoMusicPath.find("disco_ultimate") == std::string::npos)
                {
                    mp_context->Audio.PlayMusic(m_preDiscoMusicPath, 80.0f, true);
                }
                else
                {
                    RestorePostUltimateMusic();
                }
            }
            else
            {
                std::string cur = mp_context->Audio.GetCurrentMusicPath();
                if (cur.find("disco_ultimate") != std::string::npos)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                }
            }
            m_preDiscoMusicPath.clear();
        }
    }
    m_discoRainbowHue = 0.0f;
    m_discoGrandFinaleBannerTimer = 0.0f;
    m_discoExplosionAnims.clear();
    m_discoMusicNotes.clear();
    m_discoNoteSpawnTimer = 0.0f;

    if (m_powerUpRainActiveTimer > 0.0f || !m_prePowerUpRainMusicPath.empty())
    {
        m_powerUpRainActiveTimer = 0.0f;
        m_luckyQuestionMarks.clear();
        m_luckyQuestionMarkSpawnTimer = 0.0f;

        if (mp_context)
        {
            if (m_musicEnabled)
            {
                if (!m_prePowerUpRainMusicPath.empty() && m_prePowerUpRainMusicPath.find("lucky_drop_ultimate") == std::string::npos)
                {
                    mp_context->Audio.PlayMusic(m_prePowerUpRainMusicPath, 80.0f, true);
                }
                else
                {
                    RestorePostUltimateMusic();
                }
            }
            else
            {
                std::string cur = mp_context->Audio.GetCurrentMusicPath();
                if (cur.find("lucky_drop_ultimate") != std::string::npos)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                }
            }
            m_prePowerUpRainMusicPath.clear();
        }
    }
}

void GameScene::ExecutePlasmaOrb(int level)
{
    int safeLvl = std::clamp(level, 1, 5);

    Vector2f paddlePos{0.0f, 350.0f};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    int orbCount = 1;
    float scale = 1.0f;
    int gen = 1;
    int childCount = 2;
    int damage = 2;

    switch (safeLvl)
    {
        case 1:
            orbCount = 1;
            scale = 1.0f;
            gen = 1;
            childCount = 2;
            damage = 2;
            break;
        case 2:
            orbCount = 2;
            scale = 1.0f;
            gen = 1;
            childCount = 2;
            damage = 2;
            break;
        case 3:
            orbCount = 2;
            scale = 1.5f;
            gen = 2;
            childCount = 2; // 2 orbs split into 4 total
            damage = 3;
            break;
        case 4:
            orbCount = 3;
            scale = 1.5f;
            gen = 2;
            childCount = 2; // 3 orbs split into 4 total
            damage = 3;
            break;
        case 5:
            orbCount = 3;
            scale = 2.0f;
            gen = 3;
            childCount = 4; // 3 mega orbs split into 12, then 48, then 96 total
            damage = 4;
            break;
        default:
            break;
    }

    constexpr float speed = 640.0f;

    for (int i = 0; i < orbCount; ++i)
    {
        float angleDeg = -90.0f;
        if (orbCount == 2)
        {
            angleDeg = (i == 0) ? -102.0f : -78.0f;
        }
        else if (orbCount == 3)
        {
            angleDeg = (i == 0) ? -110.0f : ((i == 1) ? -90.0f : -70.0f);
        }

        float rad = angleDeg * 3.14159265f / 180.0f;
        Vector2f vel{std::cos(rad) * speed, std::sin(rad) * speed};

        PlasmaOrbInstance orb;
        orb.Position = Vector2f{paddlePos.X + static_cast<float>(i - (orbCount - 1) / 2) * 25.0f, paddlePos.Y - 35.0f};
        orb.Velocity = vel;
        orb.Scale = scale;
        orb.Radius = 18.0f * scale;
        orb.Lifetime = 1.35f;
        orb.Generation = gen;
        orb.ChildCount = childCount;
        orb.Damage = damage;
        orb.AnimTime = 0.0f;

        m_plasmaOrbs.push_back(orb);
    }

    m_plasmaAnimTime = 0.0f;
    m_plasmaActiveTimer = 5.0f + static_cast<float>(gen) * 1.5f;

    if (mp_context)
    {
        if (m_plasmaLaunchSfxId != 0 && m_audioEnabled)
        {
            mp_context->Audio.PlaySfx(m_plasmaLaunchSfxId, 100.0f);
        }

        std::string cur = mp_context->Audio.GetCurrentMusicPath();
        if (m_prePlasmaMusicPath.empty() || m_prePlasmaMusicPath.find("plasma_ultimate") != std::string::npos)
        {
            if (cur.find("plasma_ultimate") == std::string::npos && !cur.empty())
            {
                m_prePlasmaMusicPath = cur;
            }
        }
        if (m_musicEnabled)
        {
            mp_context->Audio.PlayMusic("Resources/audio/music/plasma_ultimate.wav", 100.0f, true);
        }
    }
    TriggerPostProcessFlash(0.04f, 0.65f, Color{150, 40, 255, 255}, 0.35f, Color{0, 220, 255, 255});
    TweenEffects::Shake(m_registry, m_camera, 0.28f, 11.0f);
}

void GameScene::TriggerSoundWave(float targetHeightRatio, int damage, float delay)
{
    Vector2f paddlePos{0.0f, 350.0f};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    float startY = paddlePos.Y - 20.0f;
    float totalPlayHeight = 820.0f;
    float targetY = startY - totalPlayHeight * targetHeightRatio;

    SoundWaveInstance wave;
    wave.StartY = startY;
    wave.CurrentY = startY;
    wave.TargetY = targetY;
    wave.Lifetime = 0.85f;
    wave.MaxLifetime = 0.85f;
    wave.Damage = damage;
    wave.ArcWidth = 1450.0f;
    wave.Delay = delay;

    m_soundWaves.push_back(wave);

    if (delay <= 0.0f)
    {
        if (mp_context && m_audioEnabled && m_discoShockwaveSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_discoShockwaveSfxId, 100.0f);
        }
        TweenEffects::Shake(m_registry, m_camera, 0.22f, 8.0f);
    }
}

void GameScene::ExecuteDiscoBall(int level)
{
    m_discoLevel = std::clamp(level, 1, 5);
    m_discoDuration = 9.0f;
    m_discoTimer = m_discoDuration;
    m_discoRainbowHue = 0.0f;
    m_discoGrandFinaleBannerTimer = 0.0f;

    // Snapshot original tints of all alive bricks before applying disco theme
    m_preDiscoBrickTints.clear();
    m_registry.View<SpriteComponent, BrickComponent>([&](Entity e, const SpriteComponent& sc, const BrickComponent&)
    {
        m_preDiscoBrickTints[e] = sc.Tint;
    });

    // Switch music to the groovy layered disco stem for this level
    if (mp_context)
    {
        std::string cur = mp_context->Audio.GetCurrentMusicPath();
        if (m_preDiscoMusicPath.empty() || m_preDiscoMusicPath.find("disco_ultimate") != std::string::npos)
        {
            if (cur.find("disco_ultimate") == std::string::npos && !cur.empty())
            {
                m_preDiscoMusicPath = cur;
            }
        }
        std::string musicPath = "Resources/audio/music/disco_ultimate_lvl" + std::to_string(m_discoLevel) + ".wav";
        if (m_musicEnabled)
        {
            mp_context->Audio.PlayMusic(musicPath, 100.0f, true);
        }
    }

    // Soundwaves from paddle (scaling reach from 30% to 70% height smoothly across 5 levels)
    float heightRatio = 0.30f + static_cast<float>(m_discoLevel - 1) * 0.10f; // 30%, 40%, 50%, 60%, 70%
    int waveDmg = (m_discoLevel >= 3) ? 2 : 1;

    TriggerSoundWave(heightRatio, waveDmg, 0.0f);
    if (m_discoLevel >= 5)
    {
        // Max level fires 2 shockwaves reaching up to 70% of the playfield height
        TriggerSoundWave(heightRatio, waveDmg, 0.20f);
    }

    // Rainbow Big Balls dancing in the playfield
    int ballCount = 2 + (m_discoLevel - 1); // 2 to 6
    const Color rainbowColors[6] = {
        Color{255, 45, 120, 255},  // Hot Pink / Red
        Color{255, 140, 30, 255},  // Electric Orange
        Color{255, 230, 40, 255},  // Bright Gold
        Color{50, 255, 100, 255},  // Neon Lime
        Color{40, 220, 255, 255},  // Cyan Spark
        Color{190, 70, 255, 255}   // Ultraviolet
    };

    // If already active, keep existing dancing/independent balls and only append any missing ones
    size_t startIdx = 0;
    if (m_discoBigBalls.size() >= static_cast<size_t>(ballCount))
    {
        startIdx = static_cast<size_t>(ballCount);
    }
    else
    {
        startIdx = m_discoBigBalls.size();
    }

    for (int i = static_cast<int>(startIdx); i < ballCount; ++i)
    {
        DiscoBigBall db;
        db.InstrumentIndex = i % 6;
        db.DanceCenter = Vector2f{
            -420.0f + static_cast<float>(i) * (840.0f / static_cast<float>(std::max(1, ballCount - 1))),
            -100.0f + static_cast<float>((i % 2) * 120 - 60)
        };
        db.DanceVelocity = Vector2f{
            65.0f + static_cast<float>((i * 29) % 35) * ((i % 2 == 0) ? 1.0f : -1.0f),
            45.0f + static_cast<float>((i * 19) % 30) * ((i % 3 == 0) ? 1.0f : -1.0f)
        };
        db.DancePhaseX = static_cast<float>(i) * 1.047f;
        db.DancePhaseY = static_cast<float>(i) * 1.57f;
        db.DanceFreqX = 1.8f + static_cast<float>(i % 3) * 0.35f;
        db.DanceFreqY = 2.4f + static_cast<float>((i + 1) % 3) * 0.45f;
        db.DanceAmpX = 135.0f + static_cast<float>((i % 2) * 35);
        db.DanceAmpY = 80.0f + static_cast<float>((i % 3) * 25);
        db.Position = db.DanceCenter + Vector2f{std::cos(db.DancePhaseX) * db.DanceAmpX, std::sin(db.DancePhaseY) * db.DanceAmpY};
        db.Velocity = Vector2f{0.0f, 0.0f};
        db.Radius = 20.0f;
        db.Scale = 1.35f;
        db.IsIndependent = false;
        db.SparkleTimer = 0.0f;
        db.BaseColor = rainbowColors[i % 6];
        db.Trail.clear();
        db.DamagedBricks.clear();

        m_discoBigBalls.push_back(db);
    }

    // Level 5: Boule à Facettes (Sleek mirror disco ball)
    if (m_discoLevel >= 5)
    {
        m_discoMirrorBall.IsActive = true;
        m_discoMirrorBall.Lifetime = 5.0f;
        m_discoMirrorBall.Position = Vector2f{0.0f, -140.0f};
        m_discoMirrorBall.Velocity = Vector2f{350.0f, -260.0f};
        m_discoMirrorBall.Radius = 26.0f;
        m_discoMirrorBall.Scale = 1.35f;
        m_discoMirrorBall.TargetScale = 1.35f;
        m_discoMirrorBall.IsFrenzy = false;
        m_discoMirrorBall.FrenzyPulseTime = 0.0f;
    }
    else
    {
        m_discoMirrorBall.IsActive = false;
        m_discoMirrorBall.IsFrenzy = false;
        m_discoMirrorBall.Scale = 1.35f;
        m_discoMirrorBall.TargetScale = 1.35f;
    }

    // Pre-populate wandering yellow music note particles across background
    while (m_discoMusicNotes.size() < 36)
    {
        SpawnDiscoMusicNote(true);
    }

    TriggerPostProcessFlash(0.04f, 0.70f, Color{255, 60, 200, 255}, 0.35f, Color{255, 230, 80, 255});
}

void GameScene::ExecutePowerUpRain(int level)
{
    int safeLvl = std::clamp(level, 1, 5);

    int minCount = 5;
    int maxCount = 12;
    int minTier = 1;
    int maxTier = 3;

    switch (safeLvl)
    {
        case 1: minCount = 5;  maxCount = 12; minTier = 1; maxTier = 3; break;
        case 2: minCount = 10; maxCount = 24; minTier = 1; maxTier = 3; break;
        case 3: minCount = 15; maxCount = 36; minTier = 1; maxTier = 4; break;
        case 4: minCount = 20; maxCount = 48; minTier = 2; maxTier = 5; break;
        case 5: minCount = 25; maxCount = 60; minTier = 2; maxTier = 5; break;
        default: break;
    }

    int range = std::max(1, maxCount - minCount + 1);
    int count = minCount + (rand() % range);

    m_powerUpRainQueue.clear();

    const PowerUpType availableTypes[] = {
        PowerUpType::MultiBall,
        PowerUpType::BigBall,
        PowerUpType::FireBall,
        PowerUpType::LaserPaddle,
        PowerUpType::StickyPaddle,
        PowerUpType::BottomBarrier,
        PowerUpType::MagnetPaddle,
        PowerUpType::GhostBall,
        PowerUpType::TempoBall,
        PowerUpType::ExtraLife
    };
    constexpr size_t numAvailableTypes = sizeof(availableTypes) / sizeof(availableTypes[0]);

    for (int i = 0; i < count; ++i)
    {
        PowerUpRainDrop drop;
        drop.DelayTimer = (static_cast<float>(i) / static_cast<float>(count)) * 2.8f + (static_cast<float>(rand() % 100) / 1000.0f);
        drop.SpawnX = -650.0f + static_cast<float>(rand() % 1300);
        drop.Type = availableTypes[rand() % numAvailableTypes];
        drop.Tier = minTier + (rand() % (std::max(1, maxTier - minTier + 1)));

        m_powerUpRainQueue.push_back(drop);
    }

    m_powerUpRainActiveTimer = 7.2f;
    m_powerUpRainAnimTime = 0.0f;
    m_luckyQuestionMarkSpawnTimer = 0.0f;

    if (mp_context)
    {
        if (m_audioEnabled && m_ultimateReadySfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_ultimateReadySfxId, 100.0f, 1.25f);
        }

        std::string cur = mp_context->Audio.GetCurrentMusicPath();
        if (m_prePowerUpRainMusicPath.empty() || m_prePowerUpRainMusicPath.find("lucky_drop_ultimate") != std::string::npos)
        {
            if (cur.find("lucky_drop_ultimate") == std::string::npos && !cur.empty())
            {
                m_prePowerUpRainMusicPath = cur;
            }
        }
        if (m_musicEnabled)
        {
            mp_context->Audio.PlayMusic("Resources/audio/music/lucky_drop_ultimate.wav", 100.0f, true);
        }
    }

    // Spawn an initial cyber burst of Lucky Question Mark particles
    for (int q = 0; q < 16; ++q)
    {
        Vector2f burstPos{-600.0f + static_cast<float>(rand() % 1200), -380.0f + static_cast<float>(rand() % 450)};
        SpawnLuckyQuestionMark(burstPos, true);
    }

    TriggerPostProcessFlash(0.045f, 0.75f, Color{0, 240, 255, 255}, 0.45f, Color{255, 20, 147, 255});
    TweenEffects::Shake(m_registry, m_camera, 0.25f, 7.0f);
}

void GameScene::UpdateUltimateAbilities(float dt)
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Gameplay::InfiniteUltimate))
    {
        m_ultimateCharge = m_ultimateMaxCharge;
        m_ultimateReady = true;
    }

    if (m_ultimateReady)
    {
        m_ultimateReadyPulseTime += dt;
    }

    // 1. Plasma Orbs update
    struct ActiveBrickCache
    {
        Entity entity;
        Vector2f pos;
    };
    static thread_local std::vector<ActiveBrickCache> s_activeBricks;
    s_activeBricks.clear();
    m_registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& b)
    {
        if (!b.IsRegenerating)
        {
            s_activeBricks.push_back({e, t.Position});
        }
    });

    constexpr size_t kMaxPlasmaOrbs = 120;
    std::vector<PlasmaOrbInstance> newChildren;
    for (auto it = m_plasmaOrbs.begin(); it != m_plasmaOrbs.end(); )
    {
        it->Position += it->Velocity * dt;
        it->AnimTime += dt;
        it->Lifetime -= dt;

        // Update trail history
        for (auto& pt : it->Trail)
        {
            pt.Lifetime -= dt;
        }
        std::erase_if(it->Trail, [](const PlasmaTrailPoint& pt) {
            return pt.Lifetime <= 0.0f;
        });

        PlasmaTrailPoint newPt;
        newPt.Position = it->Position;
        newPt.Lifetime = 0.22f;
        newPt.MaxLifetime = 0.22f;
        it->Trail.push_back(newPt);

        // Spawn trailing plasma spark particles (throttled when many orbs are active)
        if (m_particleSystem && mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableParticles))
        {
            static uint32_t s_trailFrameCounter = 0;
            s_trailFrameCounter++;
            if (m_plasmaOrbs.size() <= 6 || (s_trailFrameCounter % 2 == 0))
            {
                static std::mt19937 trailRng(1337);
                std::uniform_real_distribution<float> jitterDist(-6.0f, 6.0f);
                std::uniform_real_distribution<float> velJitter(-25.0f, 25.0f);
                Vector2f spawnPos{it->Position.X + jitterDist(trailRng), it->Position.Y + jitterDist(trailRng)};
                Vector2f pVel{-it->Velocity.X * 0.12f + velJitter(trailRng), -it->Velocity.Y * 0.12f + velJitter(trailRng)};
                Color pCol = (trailRng() % 2 == 0) ? Color{160, 60, 255, 220} : Color{80, 210, 255, 230};
                m_particleSystem->Spawn(spawnPos, pVel, 0.20f, 8.0f * it->Scale, pCol);
            }
        }

        // Damage bricks on contact using cached active bricks and squared distance (zero per-orb allocation)
        const float hitRadius = it->Radius + 22.0f;
        const float hitRadiusSq = hitRadius * hitRadius;
        for (const auto& brick : s_activeBricks)
        {
            if (!m_registry.IsAlive(brick.entity) || m_registry.IsPendingDestroy(brick.entity)) continue;
            const float dx = brick.pos.X - it->Position.X;
            const float dy = brick.pos.Y - it->Position.Y;
            if (dx * dx + dy * dy <= hitRadiusSq)
            {
                HandleBrickCollision(brick.entity, NULL_ENTITY, /*isLaser=*/false, /*isExplosion=*/true, it->Damage);
                SpawnExplosionParticles(it->Position, Color{80, 200, 255, 230}, 4);
            }
        }

        // Wall collisions (same rules as ball: playfield bounds are [-800, 800] in X, [-450, 450] in Y)
        const float minX = -800.0f + it->Radius;
        const float maxX = 800.0f - it->Radius;
        const float minY = -450.0f + it->Radius;
        const float maxY = 450.0f - it->Radius;

        bool bounced = false;
        if (it->Position.X < minX)
        {
            it->Position.X = minX;
            if (it->Velocity.X < 0.0f)
            {
                it->Velocity.X = std::abs(it->Velocity.X);
                bounced = true;
            }
        }
        else if (it->Position.X > maxX)
        {
            it->Position.X = maxX;
            if (it->Velocity.X > 0.0f)
            {
                it->Velocity.X = -std::abs(it->Velocity.X);
                bounced = true;
            }
        }

        if (it->Position.Y < minY)
        {
            it->Position.Y = minY;
            if (it->Velocity.Y < 0.0f)
            {
                it->Velocity.Y = std::abs(it->Velocity.Y);
                bounced = true;
            }
        }
        else if (it->Position.Y > maxY)
        {
            it->Position.Y = maxY;
            if (it->Velocity.Y > 0.0f)
            {
                it->Velocity.Y = -std::abs(it->Velocity.Y);
                bounced = true;
            }
        }

        // Paddle collision (bounces upward off paddle when falling)
        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
        {
            const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
            const float halfWidth = 65.0f * pTrans.Scale.X + it->Radius;
            if (it->Velocity.Y > 0.0f &&
                it->Position.Y >= pTrans.Position.Y - 26.0f &&
                it->Position.Y <= pTrans.Position.Y + 26.0f &&
                std::abs(it->Position.X - pTrans.Position.X) <= halfWidth)
            {
                it->Position.Y = pTrans.Position.Y - 26.0f;
                float hitOffset = (it->Position.X - pTrans.Position.X) / halfWidth;
                float speed = it->Velocity.Length();
                float bounceAngle = -1.5707963f + hitOffset * 0.95f;
                it->Velocity = Vector2f{std::cos(bounceAngle) * speed, std::sin(bounceAngle) * speed};
                bounced = true;
            }
        }

        if (bounced)
        {
            SpawnPlasmaExplosionParticles(it->Position, it->Radius * 1.5f, 8);
            if (mp_context && m_audioEnabled && m_bounceSfxId != 0)
            {
                float pan = std::clamp(it->Position.X / 800.0f, -1.0f, 1.0f);
                mp_context->Audio.PlaySfx(m_bounceSfxId, 45.0f, 1.35f, pan);
            }
        }

        bool shouldExpire = (it->Lifetime <= 0.0f);

        if (shouldExpire)
        {
            bool canDivide = (it->Generation > 0 && it->ChildCount > 0 &&
                             (m_plasmaOrbs.size() + newChildren.size() < kMaxPlasmaOrbs));

            if (canDivide)
            {
                // Divide into ChildCount smaller plasma orbs
                int nextGen = it->Generation - 1;
                int nextChildCount = (nextGen == 2) ? 4 : ((nextGen == 1) ? 2 : 0);
                float nextScale = it->Scale * 0.72f;
                float nextRadius = std::max(9.0f, it->Radius * 0.72f);
                int nextDmg = std::max(1, it->Damage - 1);

                float stepAngle = (2.0f * 3.14159265f) / static_cast<float>(it->ChildCount);
                float baseSpeed = 520.0f;

                Vector2f spawnPos = it->Position;
                spawnPos.X = std::clamp(spawnPos.X, minX + 10.0f, maxX - 10.0f);
                spawnPos.Y = std::clamp(spawnPos.Y, minY + 10.0f, maxY - 10.0f);

                for (int c = 0; c < it->ChildCount; ++c)
                {
                    float a = stepAngle * static_cast<float>(c) + 0.35f;
                    Vector2f cVel{std::cos(a) * baseSpeed, std::sin(a) * baseSpeed};

                    // Deflect inward if near boundaries
                    if (spawnPos.X <= minX + 30.0f && cVel.X < 0.0f) cVel.X = std::abs(cVel.X);
                    if (spawnPos.X >= maxX - 30.0f && cVel.X > 0.0f) cVel.X = -std::abs(cVel.X);
                    if (spawnPos.Y <= minY + 30.0f && cVel.Y < 0.0f) cVel.Y = std::abs(cVel.Y);
                    if (spawnPos.Y >= maxY - 30.0f && cVel.Y > 0.0f) cVel.Y = -std::abs(cVel.Y);

                    PlasmaOrbInstance child;
                    child.Position = spawnPos;
                    child.Velocity = cVel;
                    child.Scale = nextScale;
                    child.Radius = nextRadius;
                    child.Lifetime = 1.1f;
                    child.Generation = nextGen;
                    child.ChildCount = nextChildCount;
                    child.Damage = nextDmg;
                    child.AnimTime = 0.0f;

                    newChildren.push_back(child);
                }

                SpawnPlasmaExplosionParticles(spawnPos, 40.0f, 10);
                if (mp_context && m_audioEnabled && m_plasmaLaunchSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_plasmaLaunchSfxId, 80.0f, 1.35f);
                }
            }
            else
            {
                // Final detonation
                static float s_lastDetonationSfx = -1.0f;
                if (mp_context && m_audioEnabled && m_plasmaExplodeSfxId != 0)
                {
                    if (m_gameplayTime - s_lastDetonationSfx >= 0.05f)
                    {
                        s_lastDetonationSfx = m_gameplayTime;
                        mp_context->Audio.PlaySfx(m_plasmaExplodeSfxId, 85.0f);
                    }
                }
                SpawnPlasmaExplosionParticles(it->Position, 75.0f, 18);
                SpawnLightFlash(it->Position, 320.0f, Color{80, 180, 255, 255}, 1.9f, 0.35f);

                // Area damage on explosion using cached bricks and squared distance
                constexpr float explRadiusSq = 95.0f * 95.0f;
                for (const auto& brick : s_activeBricks)
                {
                    if (!m_registry.IsAlive(brick.entity) || m_registry.IsPendingDestroy(brick.entity)) continue;
                    const float dx = brick.pos.X - it->Position.X;
                    const float dy = brick.pos.Y - it->Position.Y;
                    if (dx * dx + dy * dy <= explRadiusSq)
                    {
                        HandleBrickCollision(brick.entity, NULL_ENTITY, false, true, it->Damage);
                    }
                }
            }

            it = m_plasmaOrbs.erase(it);
        }
        else
        {
            ++it;
        }
    }
    m_plasmaOrbs.insert(m_plasmaOrbs.end(), newChildren.begin(), newChildren.end());

    // Plasma Ultimate duration, shader animation & music loop update
    if (m_plasmaActiveTimer > 0.0f)
    {
        m_plasmaActiveTimer -= dt;
        m_plasmaAnimTime += dt;

        if (m_plasmaOrbs.empty())
        {
            // Allow 0.85s of residual electric storm dissipation once all orbs have detonated
            m_plasmaActiveTimer = std::min(m_plasmaActiveTimer, 0.85f);
        }

        // When plasma ultimate ends, smoothly restore stage music
        if (m_plasmaActiveTimer <= 0.0f)
        {
            m_plasmaActiveTimer = 0.0f;
            if (mp_context && m_musicEnabled)
            {
                if (!m_prePlasmaMusicPath.empty() && m_prePlasmaMusicPath.find("plasma_ultimate") == std::string::npos)
                {
                    mp_context->Audio.FadeToMusic(m_prePlasmaMusicPath, 1.2f);
                }
                else
                {
                    m_playlist.PlayNext(*mp_context, 1.2f);
                }
                m_prePlasmaMusicPath.clear();
            }
        }
    }

    // 2. Disco Soundwaves update
    for (auto it = m_soundWaves.begin(); it != m_soundWaves.end(); )
    {
        if (it->Delay > 0.0f)
        {
            it->Delay -= dt;
            if (it->Delay <= 0.0f)
            {
                it->Delay = 0.0f;
                if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
                {
                    const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
                    it->StartY = pTrans.Position.Y - 20.0f;
                    it->CurrentY = it->StartY;
                    float totalPlayHeight = 820.0f;
                    float heightRatio = 0.30f + static_cast<float>(m_discoLevel - 1) * 0.10f;
                    it->TargetY = it->StartY - totalPlayHeight * heightRatio;
                }
                if (mp_context && m_audioEnabled && m_discoShockwaveSfxId != 0)
                {
                    mp_context->Audio.PlaySfx(m_discoShockwaveSfxId, 100.0f);
                }
                TweenEffects::Shake(m_registry, m_camera, 0.22f, 8.0f);
            }
            else
            {
                ++it;
                continue;
            }
        }

        it->Lifetime -= dt;
        float progress = 1.0f - (it->Lifetime / it->MaxLifetime);
        it->CurrentY = it->StartY + (it->TargetY - it->StartY) * progress;

        // Repel downward balls
        if (mp_ballSystem)
        {
            for (Entity b : mp_ballSystem->GetBalls())
            {
                if (!m_registry.IsAlive(b) || !m_registry.HasComponent<Transform2D>(b) || !m_registry.HasComponent<RigidBody>(b))
                    continue;

                auto& bTrans = m_registry.GetComponent<Transform2D>(b);
                auto& bRb = m_registry.GetComponent<RigidBody>(b);

                if (std::abs(bTrans.Position.Y - it->CurrentY) <= 45.0f && bRb.Velocity.Y > 0.0f)
                {
                    bRb.Velocity.Y = -std::abs(bRb.Velocity.Y) * 1.25f;
                    SpawnExplosionParticles(bTrans.Position, Color{255, 230, 80, 255}, 8);
                }
            }
        }

        // Damage bricks reached by wave
        std::vector<Entity> waveHits;
        m_registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& b)
        {
            if (b.IsRegenerating) return;
            if (std::abs(t.Position.Y - it->CurrentY) <= 30.0f)
            {
                waveHits.push_back(e);
            }
        });
        for (Entity be : waveHits)
        {
            if (m_registry.IsAlive(be) && !m_registry.IsPendingDestroy(be))
            {
                HandleBrickCollision(be, NULL_ENTITY, false, true, it->Damage);
            }
        }

        if (it->Lifetime <= 0.0f)
        {
            it = m_soundWaves.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 3. Disco Fever active state update
    // 3. Disco Fever active state update
    if (m_discoGrandFinaleBannerTimer > 0.0f)
    {
        m_discoGrandFinaleBannerTimer -= dt;
        if (m_discoGrandFinaleBannerTimer < 0.0f) m_discoGrandFinaleBannerTimer = 0.0f;
    }

    // Update animated expanding explosions
    for (auto it = m_discoExplosionAnims.begin(); it != m_discoExplosionAnims.end(); )
    {
        it->Lifetime -= dt;
        if (it->Lifetime <= 0.0f)
        {
            it = m_discoExplosionAnims.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Update wandering yellow musical note particles in background
    if (m_discoTimer > 0.0f)
    {
        m_discoNoteSpawnTimer += dt;
        if (m_discoNoteSpawnTimer >= 0.16f && m_discoMusicNotes.size() < 42)
        {
            m_discoNoteSpawnTimer = 0.0f;
            SpawnDiscoMusicNote(false);
        }
    }

    for (auto it = m_discoMusicNotes.begin(); it != m_discoMusicNotes.end(); )
    {
        it->Lifetime -= (m_discoTimer > 0.0f) ? dt : (dt * 2.2f);
        if (it->Lifetime <= 0.0f)
        {
            it = m_discoMusicNotes.erase(it);
            continue;
        }

        it->WobblePhase += dt * it->WobbleFreq;
        it->Rotation += it->RotSpeed * dt;

        Vector2f wanderDrift{
            std::sin(it->WobblePhase) * 32.0f,
            std::cos(it->WobblePhase * 0.8f) * 18.0f
        };
        it->Position += (it->Velocity + wanderDrift) * dt;

        // Soft wrap around stage boundaries so notes wander continuously
        if (it->Position.X < -780.0f) it->Position.X = 780.0f;
        else if (it->Position.X > 780.0f) it->Position.X = -780.0f;
        if (it->Position.Y < -440.0f) it->Position.Y = 440.0f;
        else if (it->Position.Y > 440.0f) it->Position.Y = -440.0f;

        ++it;
    }

    if (m_discoTimer > 0.0f)
    {
        m_discoTimer -= dt;
        m_discoRainbowHue += dt * 2.8f;

        // Animate bricks with a groovy, dynamic disco dancefloor theme without altering characteristics
        m_registry.View<Transform2D, SpriteComponent, BrickComponent>([&](Entity e, const Transform2D& t, SpriteComponent& sc, const BrickComponent& brick)
        {
            if (brick.IsRegenerating) return;
            if (m_preDiscoBrickTints.find(e) == m_preDiscoBrickTints.end())
            {
                m_preDiscoBrickTints[e] = sc.Tint;
            }
            float colIdx = std::floor((t.Position.X + 800.0f) / 70.0f);
            float rowIdx = std::floor((t.Position.Y + 450.0f) / 30.0f);
            float discoHue = std::fmod(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 2.5f : 1.2f) + (colIdx * 0.08f) + (rowIdx * 0.12f), 1.0f);
            sc.Tint = DiscoRainbowColor(discoHue, 255);
        });

        // A. Update Dancing & Independent Big Balls
        for (auto& db : m_discoBigBalls)
        {
            // 1. Ball intersection check to grant independence
            if (!db.IsIndependent)
            {
                m_registry.View<BallComponent, Transform2D>([&](Entity, const BallComponent&, const Transform2D& bt)
                {
                    float dist = (bt.Position - db.Position).Length();
                    if (dist <= (db.Radius * db.Scale + 20.0f))
                    {
                        db.IsIndependent = true;
                        Vector2f launchDir = db.Position - bt.Position;
                        if (launchDir.Length() < 1.0f)
                        {
                            float a = (static_cast<float>(rand() % 360) / 180.0f) * 3.14159265f;
                            launchDir = Vector2f{std::cos(a), std::sin(a)};
                        }
                        else
                        {
                            launchDir = launchDir.Normalized();
                        }
                        float speed = 500.0f + static_cast<float>(rand() % 120);
                        db.Velocity = launchDir * speed;

                        SpawnExplosionParticles(db.Position, db.BaseColor, 20);
                        SpawnLightFlash(db.Position, 320.0f, db.BaseColor, 2.0f, 0.35f);

                        // Unique disco instrument SFX for this specific ball
                        uint32_t hitSfx = (db.InstrumentIndex >= 0 && db.InstrumentIndex < 6) ? m_discoHitSfxIds[db.InstrumentIndex] : 0;
                        if (hitSfx != 0 && mp_context && m_audioEnabled)
                        {
                            mp_context->Audio.PlaySfx(hitSfx, 100.0f);
                        }
                        else if (m_conductionSfxId != 0 && mp_context && m_audioEnabled)
                        {
                            mp_context->Audio.PlaySfx(m_conductionSfxId, 85.0f);
                        }
                    }
                });
            }

            // 2. Motion
            if (!db.IsIndependent)
            {
                db.DancePhaseX += dt * db.DanceFreqX;
                db.DancePhaseY += dt * db.DanceFreqY;
                db.DanceCenter += db.DanceVelocity * dt;

                // Soft bounce dance center within stage boundaries
                if (db.DanceCenter.X < -500.0f) { db.DanceCenter.X = -500.0f; db.DanceVelocity.X = std::abs(db.DanceVelocity.X); }
                else if (db.DanceCenter.X > 500.0f) { db.DanceCenter.X = 500.0f; db.DanceVelocity.X = -std::abs(db.DanceVelocity.X); }
                if (db.DanceCenter.Y < -300.0f) { db.DanceCenter.Y = -300.0f; db.DanceVelocity.Y = std::abs(db.DanceVelocity.Y); }
                else if (db.DanceCenter.Y > 200.0f) { db.DanceCenter.Y = 200.0f; db.DanceVelocity.Y = -std::abs(db.DanceVelocity.Y); }

                db.Position = db.DanceCenter + Vector2f{std::cos(db.DancePhaseX) * db.DanceAmpX, std::sin(db.DancePhaseY) * db.DanceAmpY};
            }
            else
            {
                db.Position += db.Velocity * dt;

                // Autonomous bounce physics on stage walls and paddle zone
                if (db.Position.X < -740.0f) { db.Position.X = -740.0f; db.Velocity.X = std::abs(db.Velocity.X); }
                else if (db.Position.X > 740.0f) { db.Position.X = 740.0f; db.Velocity.X = -std::abs(db.Velocity.X); }
                if (db.Position.Y < -420.0f) { db.Position.Y = -420.0f; db.Velocity.Y = std::abs(db.Velocity.Y); }
                else if (db.Position.Y > 380.0f) { db.Position.Y = 380.0f; db.Velocity.Y = -std::abs(db.Velocity.Y); }
            }

            // 3. Piercing through bricks (damages each brick once while traversing without bouncing)
            float colRadius = db.Radius * db.Scale + 8.0f;
            float colRadiusSq = colRadius * colRadius;
            std::vector<Entity> currentlyOverlapping;
            m_registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& b)
            {
                if (b.IsRegenerating) return;
                float halfW = 50.0f;
                float halfH = 15.0f;
                if (m_registry.HasComponent<BoxCollider>(e))
                {
                    const auto& col = m_registry.GetComponent<BoxCollider>(e);
                    halfW = col.Size.X * 0.5f;
                    halfH = col.Size.Y * 0.5f;
                }
                float dx = std::max(0.0f, std::abs(db.Position.X - t.Position.X) - halfW);
                float dy = std::max(0.0f, std::abs(db.Position.Y - t.Position.Y) - halfH);
                if (dx * dx + dy * dy <= colRadiusSq)
                {
                    currentlyOverlapping.push_back(e);
                }
            });

            for (Entity be : currentlyOverlapping)
            {
                auto itDamaged = std::find(db.DamagedBricks.begin(), db.DamagedBricks.end(), be);
                if (itDamaged == db.DamagedBricks.end())
                {
                    db.DamagedBricks.push_back(be);
                    if (m_registry.IsAlive(be) && !m_registry.IsPendingDestroy(be))
                    {
                        HandleBrickCollision(be, NULL_ENTITY, false, true, 1);
                        SpawnExplosionParticles(db.Position, db.BaseColor, 12);
                        SpawnLightFlash(db.Position, 240.0f, db.BaseColor, 1.4f, 0.22f);
                    }
                }
            }

            for (auto itD = db.DamagedBricks.begin(); itD != db.DamagedBricks.end(); )
            {
                auto itStill = std::find(currentlyOverlapping.begin(), currentlyOverlapping.end(), *itD);
                if (itStill == currentlyOverlapping.end())
                {
                    itD = db.DamagedBricks.erase(itD);
                }
                else
                {
                    ++itD;
                }
            }

            // 4. Ribbon Trail update
            db.Trail.push_back(DiscoTrailPoint{db.Position, 0.28f, 0.28f});
            for (auto itT = db.Trail.begin(); itT != db.Trail.end(); )
            {
                itT->Lifetime -= dt;
                if (itT->Lifetime <= 0.0f)
                {
                    itT = db.Trail.erase(itT);
                }
                else
                {
                    ++itT;
                }
            }

            // 5. Sparkle particles behind ball
            db.SparkleTimer += dt;
            if (db.SparkleTimer >= 0.045f)
            {
                db.SparkleTimer = 0.0f;
                if (m_particleSystem)
                {
                    Vector2f pVel{
                        static_cast<float>((rand() % 80) - 40),
                        static_cast<float>((rand() % 80) - 40)
                    };
                    m_particleSystem->Spawn(db.Position, pVel, 0.40f, 4.0f, db.BaseColor);
                }
            }
        }

        // Check if all big balls are unlocked -> Mirror Ball goes crazy and grows!
        bool allBigBallsUnlocked = !m_discoBigBalls.empty();
        for (const auto& db : m_discoBigBalls)
        {
            if (!db.IsIndependent)
            {
                allBigBallsUnlocked = false;
                break;
            }
        }

        if (allBigBallsUnlocked && !m_discoMirrorBall.IsFrenzy)
        {
            m_discoMirrorBall.IsFrenzy = true;
            m_discoMirrorBall.TargetScale = 2.40f; // Grossis !
            m_discoMirrorBall.Lifetime = std::max(m_discoMirrorBall.Lifetime, 5.0f);
            m_discoTimer = std::max(m_discoTimer, 5.5f);

            if (!m_discoMirrorBall.IsActive)
            {
                m_discoMirrorBall.IsActive = true;
                m_discoMirrorBall.Position = Vector2f{0.0f, -100.0f};
                m_discoMirrorBall.Velocity = Vector2f{480.0f, -320.0f};
                m_discoMirrorBall.Radius = 26.0f;
                m_discoMirrorBall.Scale = 1.35f;
            }

            // Speed boost to frenzy velocity
            float curSpeed = m_discoMirrorBall.Velocity.Length();
            if (curSpeed < 1.0f)
            {
                m_discoMirrorBall.Velocity = Vector2f{520.0f, -380.0f};
            }
            else
            {
                m_discoMirrorBall.Velocity = m_discoMirrorBall.Velocity.Normalized() * 750.0f;
            }

            TweenEffects::Shake(m_registry, m_camera, 0.40f, 14.0f);
            SpawnLightFlash(m_discoMirrorBall.Position, 850.0f, Color{255, 240, 100, 255}, 3.5f, 0.75f);
            SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 235, 80, 255}, 45);
            SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 60, 200, 255}, 35);
            TriggerPostProcessFlash(0.08f, 0.95f, Color{255, 255, 255, 255}, 0.6f, Color{255, 230, 40, 255});

            if (mp_context)
            {
                if (m_discoShockwaveSfxId != 0 && m_audioEnabled)
                {
                    mp_context->Audio.PlaySfx(m_discoShockwaveSfxId, 100.0f, 1.3f);
                }
                if (m_musicEnabled)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/disco_ultimate_crazy.wav", 100.0f, true);
                }
            }
        }

        // B. Boule à Facettes (Level 5 Mirror Ball)
        if (m_discoMirrorBall.IsActive)
        {
            if (m_discoMirrorBall.Scale < m_discoMirrorBall.TargetScale)
            {
                m_discoMirrorBall.Scale += (m_discoMirrorBall.TargetScale - m_discoMirrorBall.Scale) * std::min(1.0f, dt * 5.0f);
            }

            if (m_discoMirrorBall.IsFrenzy)
            {
                m_discoMirrorBall.FrenzyPulseTime += dt * 22.0f;
                Vector2f perp{-m_discoMirrorBall.Velocity.Y, m_discoMirrorBall.Velocity.X};
                if (perp.Length() > 0.1f)
                {
                    m_discoMirrorBall.Position += perp.Normalized() * (std::sin(m_discoMirrorBall.FrenzyPulseTime) * 75.0f * dt);
                }

                if (m_particleSystem && (rand() % 3 == 0))
                {
                    Vector2f pVel{
                        static_cast<float>((rand() % 160) - 80),
                        static_cast<float>((rand() % 160) - 80)
                    };
                    Color pCol = DiscoRainbowColor(m_gameplayTime * 3.0f, 255);
                    m_particleSystem->Spawn(m_discoMirrorBall.Position, pVel, 0.35f, 5.0f * m_discoMirrorBall.Scale, pCol);
                }
            }

            m_discoMirrorBall.Position += m_discoMirrorBall.Velocity * dt;
            m_discoMirrorBall.Lifetime -= dt;

            // Bounces
            if (m_discoMirrorBall.Position.X < -740.0f) { m_discoMirrorBall.Position.X = -740.0f; m_discoMirrorBall.Velocity.X = std::abs(m_discoMirrorBall.Velocity.X); }
            else if (m_discoMirrorBall.Position.X > 740.0f) { m_discoMirrorBall.Position.X = 740.0f; m_discoMirrorBall.Velocity.X = -std::abs(m_discoMirrorBall.Velocity.X); }
            if (m_discoMirrorBall.Position.Y < -420.0f) { m_discoMirrorBall.Position.Y = -420.0f; m_discoMirrorBall.Velocity.Y = std::abs(m_discoMirrorBall.Velocity.Y); }
            else if (m_discoMirrorBall.Position.Y > 380.0f) { m_discoMirrorBall.Position.Y = 380.0f; m_discoMirrorBall.Velocity.Y = -std::abs(m_discoMirrorBall.Velocity.Y); }

            // Destroys all bricks in its path
            float mirrorRadius = m_discoMirrorBall.Radius * m_discoMirrorBall.Scale + 8.0f;
            float mRadiusSq = mirrorRadius * mirrorRadius;
            std::vector<Entity> crushed;
            m_registry.View<Transform2D, BrickComponent>([&](Entity e, const Transform2D& t, const BrickComponent& b)
            {
                if (b.IsRegenerating) return;
                float halfW = 50.0f;
                float halfH = 15.0f;
                if (m_registry.HasComponent<BoxCollider>(e))
                {
                    const auto& col = m_registry.GetComponent<BoxCollider>(e);
                    halfW = col.Size.X * 0.5f;
                    halfH = col.Size.Y * 0.5f;
                }
                float dx = std::max(0.0f, std::abs(m_discoMirrorBall.Position.X - t.Position.X) - halfW);
                float dy = std::max(0.0f, std::abs(m_discoMirrorBall.Position.Y - t.Position.Y) - halfH);
                if (dx * dx + dy * dy <= mRadiusSq)
                {
                    crushed.push_back(e);
                }
            });
            for (Entity be : crushed)
            {
                if (m_registry.IsAlive(be) && !m_registry.IsPendingDestroy(be))
                {
                    HandleBrickCollision(be, NULL_ENTITY, false, true, 99);
                    SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 230, 255, 255}, 16);
                }
            }

            // Grand Climax Explosion of Mirror Ball
            if (m_discoMirrorBall.Lifetime <= 0.0f)
            {
                m_discoMirrorBall.IsActive = false;
                if (mp_context && m_audioEnabled)
                {
                    mp_context->Audio.TriggerDucking(0.20f, 1.6f);
                    if (m_discoVoiceoverSfxId != 0)
                    {
                        mp_context->Audio.PlaySfx(m_discoVoiceoverSfxId, 100.0f);
                    }
                }
                m_discoGrandFinaleBannerTimer = 2.4f;

                DiscoExplosionAnim mAnim;
                mAnim.Position = m_discoMirrorBall.Position;
                mAnim.BallColor = Color{255, 245, 255, 255};
                mAnim.Lifetime = 0.85f;
                mAnim.MaxLifetime = 0.85f;
                mAnim.Radius = m_discoMirrorBall.Radius * m_discoMirrorBall.Scale;
                mAnim.MaxRadius = 280.0f;
                m_discoExplosionAnims.push_back(mAnim);

                SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 255, 255, 255}, 60);
                SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 80, 220, 255}, 40);
                SpawnExplosionParticles(m_discoMirrorBall.Position, Color{80, 240, 255, 255}, 40);
                SpawnLightFlash(m_discoMirrorBall.Position, 750.0f, Color{255, 250, 255, 255}, 3.2f, 0.85f);
                TriggerPostProcessFlash(0.06f, 0.85f, Color{255, 255, 255, 255}, 0.5f, Color{255, 40, 200, 255});
            }
        }

        // C. End of Disco Fever: Synchronous Big Balls Explosion Animation!
        if (m_discoTimer <= 0.0f)
        {
            m_discoTimer = 0.0f;
            RestoreDiscoBricks();

            for (const auto& db : m_discoBigBalls)
            {
                DiscoExplosionAnim anim;
                anim.Position = db.Position;
                anim.BallColor = db.BaseColor;
                anim.Lifetime = 0.65f;
                anim.MaxLifetime = 0.65f;
                anim.Radius = db.Radius * db.Scale;
                anim.MaxRadius = 160.0f;
                m_discoExplosionAnims.push_back(anim);

                SpawnExplosionParticles(db.Position, db.BaseColor, 32);
                SpawnLightFlash(db.Position, 420.0f, db.BaseColor, 2.2f, 0.45f);
            }
            if (m_plasmaExplodeSfxId != 0 && mp_context && m_audioEnabled)
            {
                mp_context->Audio.PlaySfx(m_plasmaExplodeSfxId, 90.0f);
            }
            m_discoBigBalls.clear();

            if (m_discoMirrorBall.IsActive)
            {
                m_discoMirrorBall.IsActive = false;
                DiscoExplosionAnim mAnim;
                mAnim.Position = m_discoMirrorBall.Position;
                mAnim.BallColor = Color{255, 245, 255, 255};
                mAnim.Lifetime = 0.85f;
                mAnim.MaxLifetime = 0.85f;
                mAnim.Radius = m_discoMirrorBall.Radius * m_discoMirrorBall.Scale;
                mAnim.MaxRadius = 280.0f;
                m_discoExplosionAnims.push_back(mAnim);

                SpawnExplosionParticles(m_discoMirrorBall.Position, Color{255, 255, 255, 255}, 60);
                SpawnLightFlash(m_discoMirrorBall.Position, 750.0f, Color{255, 250, 255, 255}, 3.2f, 0.85f);
                if (mp_context && m_audioEnabled)
                {
                    mp_context->Audio.TriggerDucking(0.20f, 1.6f);
                    if (m_discoVoiceoverSfxId != 0)
                    {
                        mp_context->Audio.PlaySfx(m_discoVoiceoverSfxId, 100.0f);
                    }
                }
                m_discoGrandFinaleBannerTimer = 2.4f;
            }

            // Restore stage music
            if (mp_context)
            {
                if (m_musicEnabled)
                {
                    if (!m_preDiscoMusicPath.empty() && m_preDiscoMusicPath.find("disco_ultimate") == std::string::npos)
                    {
                        mp_context->Audio.FadeToMusic(m_preDiscoMusicPath, 1.2f);
                    }
                    else
                    {
                        RestorePostUltimateMusic();
                    }
                }
                else
                {
                    std::string cur = mp_context->Audio.GetCurrentMusicPath();
                    if (cur.find("disco_ultimate") != std::string::npos)
                    {
                        mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                    }
                }
                m_preDiscoMusicPath.clear();
            }
        }
    }

    // 4. Power-Up Rain drops queue update
    for (auto it = m_powerUpRainQueue.begin(); it != m_powerUpRainQueue.end(); )
    {
        it->DelayTimer -= dt;
        if (it->DelayTimer <= 0.0f)
        {
            Entity capsule = m_registry.CreateEntity();
            m_registry.AddComponent<Transform2D>(capsule, Transform2D{Vector2f{it->SpawnX, -420.0f}, 0.0f, Vector2f{0.5f, 0.7f}});
            m_registry.AddComponent<BoxCollider>(capsule, BoxCollider{Vector2f{50.0f, 21.0f}, Vector2f{0.0f, 0.0f}, false, true});
            m_registry.AddComponent<PowerUpComponent>(capsule, PowerUpComponent{it->Type, 210.0f, false});
            Color color = GetPowerUpColor(it->Type);
            m_registry.AddComponent<SpriteComponent>(capsule, SpriteComponent{m_brickTexId, color});

            SpawnExplosionParticles(Vector2f{it->SpawnX, -420.0f}, Color{255, 235, 120, 255}, 8);
            SpawnLuckyQuestionMark(Vector2f{it->SpawnX, -420.0f}, true);
            it = m_powerUpRainQueue.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 5. Lucky Drop active duration, music loop & question marks update
    if (m_powerUpRainActiveTimer > 0.0f)
    {
        m_powerUpRainActiveTimer -= dt;
        m_powerUpRainAnimTime += dt;

        // Periodic ambient floating question marks
        m_luckyQuestionMarkSpawnTimer += dt;
        if (m_luckyQuestionMarkSpawnTimer >= 0.16f)
        {
            m_luckyQuestionMarkSpawnTimer = 0.0f;
            Vector2f spawnPos{-720.0f + static_cast<float>(rand() % 1440), 380.0f + static_cast<float>(rand() % 70)};
            SpawnLuckyQuestionMark(spawnPos, false);
        }

        // When lucky drop duration ends, smoothly restore stage music
        if (m_powerUpRainActiveTimer <= 0.0f)
        {
            m_powerUpRainActiveTimer = 0.0f;
            if (mp_context && m_musicEnabled)
            {
                if (!m_prePowerUpRainMusicPath.empty() && m_prePowerUpRainMusicPath.find("lucky_drop_ultimate") == std::string::npos)
                {
                    mp_context->Audio.FadeToMusic(m_prePowerUpRainMusicPath, 1.2f);
                }
                else
                {
                    m_playlist.PlayNext(*mp_context, 1.2f);
                }
                m_prePowerUpRainMusicPath.clear();
            }
        }
    }

    // Update floating question mark particles
    for (auto it = m_luckyQuestionMarks.begin(); it != m_luckyQuestionMarks.end(); )
    {
        it->Lifetime -= dt;
        if (it->Lifetime <= 0.0f)
        {
            it = m_luckyQuestionMarks.erase(it);
        }
        else
        {
            it->Position += it->Velocity * dt;
            it->Rotation += it->RotSpeed * dt;
            it->WobblePhase += it->WobbleFreq * dt;
            it->Position.X += std::sin(it->WobblePhase) * (20.0f * dt);
            ++it;
        }
    }
}

void GameScene::UpdateRegeneratingItemBricks(float dt)
{
    m_registry.View<Transform2D, SpriteComponent, BrickComponent>([&](Entity e, const Transform2D& t, SpriteComponent& sprite, BrickComponent& brick)
    {
        if (!brick.IsRegenerating) return;

        brick.RegenTimer -= dt;
        if (brick.RegenTimer <= 0.0f)
        {
            brick.RegenTimer = 0.0f;
            brick.IsRegenerating = false;
            brick.HitPoints = brick.MaxHitPoints;

            sprite.Tint.a = 255;
            if (m_registry.HasComponent<BoxCollider>(e))
            {
                m_registry.GetComponent<BoxCollider>(e).IsTrigger = false;
            }

            SpawnExplosionParticles(t.Position, Color{255, 235, 100, 255}, 20);
            SpawnLightFlash(t.Position, 420.0f, Color{255, 240, 140, 255}, 1.9f, 0.45f);

            if (mp_context && m_audioEnabled && m_itemRegenRestoreSfxId != 0)
            {
                mp_context->Audio.PlaySfx(m_itemRegenRestoreSfxId, 85.0f);
            }
        }
    });
}

void GameScene::RenderRegeneratingBricks(GameContext& context)
{
    m_registry.View<Transform2D, BoxCollider, BrickComponent>([&](Entity, const Transform2D& t, const BoxCollider& box, const BrickComponent& brick)
    {
        if (!brick.IsRegenerating) return;

        float progress = 1.0f - (brick.RegenTimer / std::max(0.01f, brick.RegenCooldown));
        progress = std::clamp(progress, 0.0f, 1.0f);

        float pulse = (std::sin(m_gameplayTime * 4.0f) + 1.0f) * 0.5f;
        Color ghostColor{100, 200, 255, static_cast<uint8_t>(70 + 50 * pulse)};
        context.Render.DrawRectangleOutline(box.Size.X, box.Size.Y, t, ghostColor, -1.5f);

        float cw = 12.0f;
        float ch = 8.0f;
        float hx = box.Size.X * 0.5f;
        float hy = box.Size.Y * 0.5f;
        context.Render.DrawLine(t.Position + Vector2f{-hx, -hy}, t.Position + Vector2f{-hx + cw, -hy}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{-hx, -hy}, t.Position + Vector2f{-hx, -hy + ch}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{hx, -hy}, t.Position + Vector2f{hx - cw, -hy}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{hx, -hy}, t.Position + Vector2f{hx, -hy + ch}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{-hx, hy}, t.Position + Vector2f{-hx + cw, hy}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{-hx, hy}, t.Position + Vector2f{-hx, hy - ch}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{hx, hy}, t.Position + Vector2f{hx - cw, hy}, ghostColor, 2.0f);
        context.Render.DrawLine(t.Position + Vector2f{hx, hy}, t.Position + Vector2f{hx, hy - ch}, ghostColor, 2.0f);

        float barMaxW = box.Size.X - 16.0f;
        float barH = 5.0f;
        Vector2f barCenter{t.Position.X, t.Position.Y + box.Size.Y * 0.25f};
        context.Render.DrawRectangle(barMaxW, barH, Transform2D{barCenter}, Color{20, 35, 55, 180});

        float currentBarW = barMaxW * progress;
        if (currentBarW > 1.0f)
        {
            Vector2f fillCenter{t.Position.X - (barMaxW - currentBarW) * 0.5f, barCenter.Y};
            Color fillColor{0, 220, 255, static_cast<uint8_t>(180 + 75 * pulse)};
            context.Render.DrawRectangle(currentBarW, barH, Transform2D{fillCenter}, fillColor);
        }

        context.Render.DrawCircle(3.0f + 2.0f * progress, t, Color{0, 240, 255, static_cast<uint8_t>(150 + 100 * pulse)});
    });
}

void GameScene::RenderUltimateMeterHUD(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float centerX = viewSize.X * 0.5f;
    const float centerY = 65.0f;
    const float barWidth = 360.0f;
    const float barHeight = 22.0f;

    Transform2D bgTrans{Vector2f{centerX, centerY}};
    context.Render.DrawRectangle(barWidth + 8.0f, barHeight + 8.0f, bgTrans, Color{10, 15, 25, 210});

    Color borderColor{40, 75, 120, 220};
    if (m_ultimateReady)
    {
        float pulse = (std::sin(m_ultimateReadyPulseTime * 8.0f) + 1.0f) * 0.5f;
        borderColor = Color{255, static_cast<uint8_t>(200 + 55 * pulse), static_cast<uint8_t>(50 * pulse), 255};
    }
    context.Render.DrawRectangleOutline(barWidth + 8.0f, barHeight + 8.0f, bgTrans, borderColor, m_ultimateReady ? -2.5f : -1.5f);

    float fillRatio = std::clamp(m_ultimateCharge / m_ultimateMaxCharge, 0.0f, 1.0f);
    if (fillRatio > 0.0f)
    {
        float filledW = barWidth * fillRatio;
        float filledCenterX = centerX - (barWidth - filledW) * 0.5f;
        Color barColor = m_ultimateReady 
            ? Color{255, 225, 50, 255} 
            : Color{0, 210, 255, 230};
        context.Render.DrawRectangle(filledW, barHeight, Transform2D{Vector2f{filledCenterX, centerY}}, barColor);
    }

    for (int i = 1; i <= 3; ++i)
    {
        float tickX = centerX - barWidth * 0.5f + (barWidth * 0.25f * static_cast<float>(i));
        context.Render.DrawLine(Vector2f{tickX, centerY - barHeight * 0.5f}, Vector2f{tickX, centerY + barHeight * 0.5f}, Color{15, 25, 40, 180}, 2.0f);
    }

    std::string ultLabel;
    UltimateAbilityType ultType = ProfileManager::Get().GetEquippedUltimate();
    std::string ultName = ProfileManager::Get().GetUltimateName(ultType);
    if (m_ultimateReady)
    {
        ultLabel = "[ ULTIMATE READY : PRESS SPACE ]";
        float pulse = (std::sin(m_ultimateReadyPulseTime * 10.0f) + 1.0f) * 0.5f;
        Color textColor{255, static_cast<uint8_t>(220 + 35 * pulse), static_cast<uint8_t>(80 * pulse), 255};
        context.Render.DrawTextCentered(ultLabel, m_fontId, 28.0f, Transform2D{Vector2f{centerX, centerY + 28.0f}}, textColor);
    }
    else
    {
        ultLabel = ultName + " : " + std::to_string(static_cast<int>(fillRatio * 100.0f)) + "%";
        context.Render.DrawTextCentered(ultLabel, m_fontId, 22.0f, Transform2D{Vector2f{centerX, centerY + 26.0f}}, Color{180, 220, 255, 210});
    }
}

void GameScene::RenderActiveUltimateVisuals(GameContext& context)
{
    // 0. Fullscreen High-Voltage Plasma Storm Shader Overlay & HUD Banner
    if (m_plasmaActiveTimer > 0.0f)
    {
        if (m_plasmaShaderId != 0 && context.Rules.GetRule(Rule::Graphics::EnableShader))
        {
            Vector2f viewSize = context.Render.GetLogicalViewSize();
            Color plasmaColor{190, 90, 255, static_cast<uint8_t>(std::clamp(m_plasmaActiveTimer * 50.0f, 0.0f, 215.0f))};
            context.Render.DrawScannerEffect(viewSize.X, viewSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, plasmaColor, m_plasmaAnimTime, m_plasmaShaderId);
        }

        // Plasma High-Voltage Banner
        float bannerPulse = 0.5f + 0.5f * std::sin(m_gameplayTime * 16.0f);
        Color bannerGlow = (bannerPulse > 0.5f) ? Color{180, 80, 255, 245} : Color{0, 220, 255, 245};
        context.Render.DrawTextCentered("[ * HIGH-VOLTAGE PLASMA ANNIHILATION * ]", m_fontId, 28.0f, Transform2D{Vector2f{0.0f, -220.0f}}, bannerGlow);
    }

    // 1. Plasma Orbs Rendering
    for (const auto& orb : m_plasmaOrbs)
    {
        // Trailing plasma ribbon / nodes (optimized connection lines & sampled glow nodes)
        for (size_t ti = 0; ti < orb.Trail.size(); ++ti)
        {
            const auto& pt = orb.Trail[ti];
            float ratio = std::clamp(pt.Lifetime / pt.MaxLifetime, 0.0f, 1.0f);
            float trailR = (orb.Radius * 0.65f) * ratio;
            uint8_t trailAlpha = static_cast<uint8_t>(ratio * 160.0f);

            Transform2D tt{pt.Position};
            if (ti % 2 == 0)
            {
                context.Render.DrawCircle(trailR, tt, Color{40, 180, 255, trailAlpha});
                context.Render.DrawCircle(trailR * 0.4f, tt, Color{220, 245, 255, trailAlpha});
            }

            if (ti + 1 < orb.Trail.size())
            {
                const auto& nextPt = orb.Trail[ti + 1];
                context.Render.DrawLine(pt.Position, nextPt.Position, Color{110, 140, 255, trailAlpha}, 3.0f * ratio);
            }
        }

        Transform2D ot{orb.Position};
        float wPulse = std::sin(orb.AnimTime * 14.0f) * 3.0f;

        // Outer Violet / Indigo Plasma Aura
        Color auraColor{130, 40, 255, 175};
        context.Render.DrawCircleOutline(orb.Radius + 6.0f + wPulse, ot, auraColor, -4.0f);

        // Electric Blue Swirling Ring
        Color blueRing{0, 220, 255, 230};
        context.Render.DrawCircleOutline(orb.Radius + wPulse * 0.5f, ot, blueRing, -3.0f);

        // Core White-Cyan Energy Sphere
        Color coreColor{225, 245, 255, 255};
        context.Render.DrawCircle(std::max(4.0f, orb.Radius * 0.55f), ot, coreColor);
    }

    // 2. Disco Soundwaves & Dancefloor Show
    for (const auto& wave : m_soundWaves)
    {
        if (wave.Delay > 0.0f) continue;

        float ratio = std::clamp(wave.Lifetime / wave.MaxLifetime, 0.0f, 1.0f);
        uint8_t a = static_cast<uint8_t>(255.0f * ratio);

        // Concentric speaker acoustic wave arc
        Color waveGlow{0, 240, 255, static_cast<uint8_t>(a * 0.65f)};
        Color waveCore{255, 255, 255, a};

        Vector2f p1{-wave.ArcWidth * 0.5f, wave.CurrentY};
        Vector2f p2{wave.ArcWidth * 0.5f, wave.CurrentY};

        context.Render.DrawLine(p1, p2, waveGlow, 8.0f);
        context.Render.DrawLine(p1, p2, waveCore, 3.0f);
    }

    if (m_discoTimer > 0.0f)
    {
        // A. Sweeping Disco Spotlights from ceiling
        const Vector2f spotOrigins[4] = {
            {-650.0f, -440.0f},
            {-220.0f, -440.0f},
            {220.0f, -440.0f},
            {650.0f, -440.0f}
        };
        const Color spotColors[4] = {
            Color{255, 40, 140, 65},  // Neon Magenta
            Color{40, 220, 255, 65},  // Electric Cyan
            Color{255, 220, 40, 65},  // Golden Sun
            Color{170, 50, 255, 65}   // Ultraviolet
        };

        for (int sp = 0; sp < 4; ++sp)
        {
            float sweep = std::sin(m_gameplayTime * 2.2f + static_cast<float>(sp) * 1.57f);
            Vector2f targetFloor{
                spotOrigins[sp].X * 0.4f + sweep * 440.0f,
                380.0f
            };

            // Translucent spotlight cone beams
            Vector2f leftCone{targetFloor.X - 100.0f, targetFloor.Y};
            Vector2f rightCone{targetFloor.X + 100.0f, targetFloor.Y};

            context.Render.DrawLine(spotOrigins[sp], leftCone, spotColors[sp], 2.0f);
            context.Render.DrawLine(spotOrigins[sp], rightCone, spotColors[sp], 2.0f);
            context.Render.DrawLine(spotOrigins[sp], targetFloor, Color{255, 255, 255, 50}, 3.0f);

            // Glowing floor pool
            Transform2D fSpot{targetFloor, 0.0f, Vector2f{1.6f, 0.5f}};
            context.Render.DrawCircle(65.0f, fSpot, Color{spotColors[sp].r, spotColors[sp].g, spotColors[sp].b, 45});
            context.Render.DrawCircle(32.0f, fSpot, Color{255, 255, 255, 75});
        }

        // B. Pulsating Neon Strobe Bars at stage borders
        float strobeAlpha = (0.5f + 0.5f * std::sin(m_gameplayTime * 14.0f)) * 140.0f;
        Color strobeCol{255, 230, 80, static_cast<uint8_t>(strobeAlpha)};
        context.Render.DrawLine(Vector2f{-760.0f, -435.0f}, Vector2f{760.0f, -435.0f}, strobeCol, 2.0f);
        context.Render.DrawLine(Vector2f{-760.0f, 395.0f}, Vector2f{760.0f, 395.0f}, strobeCol, 2.0f);

        // C. Big Balls Glowing Ribbon Trails
        for (const auto& db : m_discoBigBalls)
        {
            for (size_t ti = 0; ti < db.Trail.size(); ++ti)
            {
                float tRatio = db.Trail[ti].Lifetime / db.Trail[ti].MaxLifetime;
                uint8_t a = static_cast<uint8_t>(170.0f * tRatio);
                Color tCol{db.BaseColor.r, db.BaseColor.g, db.BaseColor.b, a};
                float r = db.Radius * db.Scale * (0.35f + 0.65f * tRatio);
                context.Render.DrawCircle(r, Transform2D{db.Trail[ti].Position}, tCol);
            }
        }

        // D. Dancing Rainbow Big Balls
        for (const auto& db : m_discoBigBalls)
        {
            Transform2D bt{db.Position, 0.0f, Vector2f{db.Scale, db.Scale}};

            // Cycling vibrant halo ring
            float rPulse = std::sin(m_gameplayTime * 12.0f + db.DancePhaseX) * 3.5f;
            Color haloCol{db.BaseColor.r, db.BaseColor.g, db.BaseColor.b, 160};
            context.Render.DrawCircleOutline(db.Radius * db.Scale + 6.0f + rPulse, bt, haloCol, -2.5f);
            context.Render.DrawCircle(db.Radius * db.Scale, bt, db.BaseColor);
            context.Render.DrawCircle(db.Radius * db.Scale * 0.45f, bt, Colors::White);
        }

        // E. Disco Mirror Ball (Level 5 Boule à Facettes)
        if (m_discoMirrorBall.IsActive)
        {
            Vector2f mirrorPos = m_discoMirrorBall.Position;
            float mirrorR = m_discoMirrorBall.Radius * m_discoMirrorBall.Scale;

            int rayCount = m_discoMirrorBall.IsFrenzy ? 24 : 12;
            float rayLength = m_discoMirrorBall.IsFrenzy ? 800.0f : 450.0f;
            float raySpeed = m_discoMirrorBall.IsFrenzy ? 4.0f : 1.5f;

            // Rotating prismatic mirror facet reflection beams
            for (int ray = 0; ray < rayCount; ++ray)
            {
                float rAng = static_cast<float>(ray) * (6.2831853f / static_cast<float>(rayCount)) + m_gameplayTime * raySpeed;
                Vector2f rayEnd = mirrorPos + Vector2f{std::cos(rAng) * rayLength, std::sin(rAng) * (rayLength * 0.7f)};
                Color rayCol = DiscoRainbowColor(std::fmod(static_cast<float>(ray) / static_cast<float>(rayCount) + m_gameplayTime * 0.6f, 1.0f), m_discoMirrorBall.IsFrenzy ? 140 : 85);
                context.Render.DrawLine(mirrorPos, rayEnd, rayCol, m_discoMirrorBall.IsFrenzy ? 3.5f : 2.5f);
            }

            // Sphere base & white boundary outline
            context.Render.DrawCircle(mirrorR, Transform2D{mirrorPos}, Color{210, 220, 240, 240});
            context.Render.DrawCircleOutline(mirrorR, Transform2D{mirrorPos}, Colors::White, m_discoMirrorBall.IsFrenzy ? 3.0f : 2.0f);

            // Pulsing iridescent halo ring
            float pulse = (std::sin(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 26.0f : 14.0f)) + 1.0f) * 0.5f;
            Color facetBorder{255, static_cast<uint8_t>(210 + 45 * pulse), 255, static_cast<uint8_t>(180 + 75 * pulse)};
            context.Render.DrawCircleOutline(mirrorR + (m_discoMirrorBall.IsFrenzy ? 8.0f : 4.0f), Transform2D{mirrorPos}, facetBorder, -3.0f);

            // Mirror tile facet grid matching workshop preview
            for (int fx = -2; fx <= 2; ++fx)
            {
                for (int fy = -2; fy <= 2; ++fy)
                {
                    if (fx * fx + fy * fy <= 4)
                    {
                        Vector2f tilePos = mirrorPos + Vector2f{static_cast<float>(fx) * 8.0f * m_discoMirrorBall.Scale, static_cast<float>(fy) * 8.0f * m_discoMirrorBall.Scale};
                        float tileHue = std::fmod(m_gameplayTime * (m_discoMirrorBall.IsFrenzy ? 4.0f : 2.0f) + static_cast<float>(fx + fy) * 0.15f, 1.0f);
                        Color tileCol = DiscoRainbowColor(tileHue, 220);
                        context.Render.DrawRectangle(6.0f * m_discoMirrorBall.Scale, 6.0f * m_discoMirrorBall.Scale, Transform2D{tilePos}, tileCol);
                        context.Render.DrawRectangleOutline(6.0f * m_discoMirrorBall.Scale, 6.0f * m_discoMirrorBall.Scale, Transform2D{tilePos}, Colors::White, 1.0f);
                    }
                }
            }
        }
    }

    if (m_discoMirrorBall.IsActive && m_discoMirrorBall.IsFrenzy)
    {
        float frenzyPulse = 0.5f + 0.5f * std::sin(m_gameplayTime * 18.0f);
        Color fColor = (frenzyPulse > 0.5f) ? Color{255, 230, 40, 240} : Color{255, 50, 200, 240};
        context.Render.DrawTextCentered("* CRAZY MIRROR BALL UNLEASHED! *", m_fontId, 26.0f, Transform2D{Vector2f{0.0f, -220.0f}}, fColor);
    }

    // 3. Disco Explosions Visuals (Expanding shockwave rings, flash center, and star burst rays)
    for (const auto& anim : m_discoExplosionAnims)
    {
        float progress = 1.0f - std::clamp(anim.Lifetime / anim.MaxLifetime, 0.0f, 1.0f);
        float currentRadius = anim.Radius + (anim.MaxRadius - anim.Radius) * progress;
        uint8_t alpha = static_cast<uint8_t>((1.0f - progress) * 255.0f);

        Transform2D expTrans{anim.Position};

        // Outer expanding colored shockwave ring
        Color ringColor{anim.BallColor.r, anim.BallColor.g, anim.BallColor.b, static_cast<uint8_t>(alpha * 0.85f)};
        context.Render.DrawCircleOutline(currentRadius, expTrans, ringColor, -3.5f * (1.0f - progress * 0.5f));

        // Inner white-hot pulse ring
        Color innerRing{255, 255, 255, static_cast<uint8_t>(alpha * 0.9f)};
        context.Render.DrawCircleOutline(currentRadius * 0.65f, expTrans, innerRing, -2.0f);

        // Core flash (quick fade)
        if (progress < 0.45f)
        {
            float coreProgress = progress / 0.45f;
            uint8_t coreAlpha = static_cast<uint8_t>((1.0f - coreProgress) * 240.0f);
            Color coreCol{255, 255, 255, coreAlpha};
            context.Render.DrawCircle(anim.Radius * (1.0f - coreProgress * 0.5f), expTrans, coreCol);
        }

        // Radiating star burst rays
        constexpr int kRays = 8;
        for (int r = 0; r < kRays; ++r)
        {
            float angle = static_cast<float>(r) * (6.2831853f / static_cast<float>(kRays)) + progress * 0.8f;
            float rayLen = currentRadius * 1.25f;
            Vector2f rayEnd = anim.Position + Vector2f{std::cos(angle) * rayLen, std::sin(angle) * rayLen};
            Color rayCol{anim.BallColor.r, anim.BallColor.g, anim.BallColor.b, static_cast<uint8_t>(alpha * 0.7f)};
            context.Render.DrawLine(anim.Position, rayEnd, rayCol, 2.0f * (1.0f - progress));
        }
    }

    // 4. Grand Finale Banner ("DISCO !")
    if (m_discoGrandFinaleBannerTimer > 0.0f)
    {
        float flashPulse = 0.5f + 0.5f * std::sin(m_gameplayTime * 28.0f);
        Color bannerGlow = (flashPulse > 0.5f) ? Color{255, 40, 200, 255} : Color{255, 230, 40, 255};
        Color bannerCore = Colors::White;

        Vector2f bannerPos{0.0f, -80.0f + std::sin(m_gameplayTime * 8.0f) * 10.0f};

        context.Render.DrawTextCentered("*   D  I  S  C  O  !   *", m_fontId, 68.0f, Transform2D{Vector2f{bannerPos.X - 3.0f, bannerPos.Y - 3.0f}}, bannerGlow);
        context.Render.DrawTextCentered("*   D  I  S  C  O  !   *", m_fontId, 68.0f, Transform2D{Vector2f{bannerPos.X + 3.0f, bannerPos.Y + 3.0f}}, Color{0, 240, 255, 255});
        context.Render.DrawTextCentered("*   D  I  S  C  O  !   *", m_fontId, 68.0f, Transform2D{bannerPos}, bannerCore);

        context.Render.DrawTextCentered("[ GRAND CLIMAX GROOVE ]", m_fontId, 28.0f, Transform2D{Vector2f{bannerPos.X, bannerPos.Y + 65.0f}}, Colors::Gold);
    }

    // 5. Power-Up Rain / Lucky Fortune Visuals & Question Marks
    if (m_powerUpRainActiveTimer > 0.0f)
    {
        // Fullscreen Cyber Cascade Shader Overlay
        if (m_powerUpRainShaderId != 0 && context.Rules.GetRule(Rule::Graphics::EnableShader))
        {
            Vector2f viewSize = context.Render.GetLogicalViewSize();
            Color luckyColor{0, 240, 255, static_cast<uint8_t>(std::clamp(m_powerUpRainActiveTimer * 45.0f, 0.0f, 215.0f))};
            context.Render.DrawScannerEffect(viewSize.X, viewSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, luckyColor, m_powerUpRainAnimTime, m_powerUpRainShaderId);
        }

        // Cyber Matrix Overdrive Banner
        float bannerPulse = 0.5f + 0.5f * std::sin(m_gameplayTime * 14.0f);
        Color bannerGlow = (bannerPulse > 0.5f) ? Color{0, 245, 255, 245} : Color{255, 30, 160, 245};
        context.Render.DrawTextCentered("[ * CYBER CASCADE // MATRIX OVERDRIVE * ]", m_fontId, 28.0f, Transform2D{Vector2f{0.0f, -220.0f}}, bannerGlow);
    }

    // Always render active question mark particles
    RenderLuckyQuestionMarks(context);
}

void GameScene::SpawnDiscoMusicNote(bool randomSpread)
{
    DiscoMusicNoteParticle note;
    if (randomSpread)
    {
        note.Position = Vector2f{
            static_cast<float>((rand() % 1480) - 740),
            static_cast<float>((rand() % 800) - 400)
        };
    }
    else
    {
        int edge = rand() % 3;
        if (edge == 0) // Bottom
        {
            note.Position = Vector2f{static_cast<float>((rand() % 1480) - 740), 430.0f};
        }
        else if (edge == 1) // Left
        {
            note.Position = Vector2f{-760.0f, static_cast<float>((rand() % 800) - 400)};
        }
        else // Right
        {
            note.Position = Vector2f{760.0f, static_cast<float>((rand() % 800) - 400)};
        }
    }

    float vx = static_cast<float>((rand() % 90) - 45);
    float vy = -30.0f - static_cast<float>(rand() % 45); // Gentle upward musical breeze
    note.Velocity = Vector2f{vx, vy};

    note.Scale = 0.75f + static_cast<float>(rand() % 50) * 0.01f;
    note.Rotation = static_cast<float>((rand() % 628) - 314) * 0.01f;
    note.RotSpeed = static_cast<float>((rand() % 140) - 70) * 0.01f;
    note.WobblePhase = static_cast<float>(rand() % 628) * 0.01f;
    note.WobbleFreq = 1.6f + static_cast<float>(rand() % 20) * 0.1f;
    note.Lifetime = 4.5f + static_cast<float>(rand() % 30) * 0.1f;
    note.MaxLifetime = note.Lifetime;
    note.BaseAlpha = static_cast<uint8_t>(170 + (rand() % 70));
    note.IsDoubleNote = (rand() % 2 == 0);

    m_discoMusicNotes.push_back(note);
}

void GameScene::RenderDiscoBackgroundMusicNotes(GameContext& context)
{
    if (m_discoMusicNotes.empty()) return;

    for (const auto& note : m_discoMusicNotes)
    {
        float ratio = std::clamp(note.Lifetime / note.MaxLifetime, 0.0f, 1.0f);
        float alphaFactor = 1.0f;
        if (ratio > 0.85f)
        {
            alphaFactor = (1.0f - ratio) / 0.15f;
        }
        else if (ratio < 0.25f)
        {
            alphaFactor = ratio / 0.25f;
        }

        uint8_t a = static_cast<uint8_t>(static_cast<float>(note.BaseAlpha) * alphaFactor);
        if (a < 5) continue;

        // Warm golden yellow glow aura
        Color haloCol{255, 205, 30, static_cast<uint8_t>(a * 0.45f)};
        DrawMusicNote(context, note.Position, note.Scale * 1.35f, note.Rotation, haloCol, note.IsDoubleNote);

        // Vibrant yellow main body
        Color mainCol{255, 240, 50, a};
        DrawMusicNote(context, note.Position, note.Scale, note.Rotation, mainCol, note.IsDoubleNote);

        // Core light yellow highlight
        Color coreCol{255, 255, 180, static_cast<uint8_t>(a * 0.85f)};
        DrawMusicNote(context, note.Position, note.Scale * 0.75f, note.Rotation, coreCol, note.IsDoubleNote);
    }
}

void GameScene::SpawnLuckyQuestionMark(const Vector2f& pos, bool popUp)
{
    LuckyQuestionMarkParticle qm;
    qm.Position = pos;
    if (popUp)
    {
        float angle = static_cast<float>(rand() % 628) * 0.01f;
        float spd = 70.0f + static_cast<float>(rand() % 160);
        qm.Velocity = Vector2f{std::cos(angle) * spd, std::sin(angle) * spd - 75.0f};
    }
    else
    {
        qm.Velocity = Vector2f{static_cast<float>((rand() % 90) - 45), -45.0f - static_cast<float>(rand() % 70)};
    }
    qm.Scale = 0.85f + static_cast<float>(rand() % 55) * 0.01f;
    qm.Rotation = static_cast<float>((rand() % 80) - 40) * 0.01f;
    qm.RotSpeed = static_cast<float>((rand() % 140) - 70) * 0.01f;
    qm.WobblePhase = static_cast<float>(rand() % 628) * 0.01f;
    qm.WobbleFreq = 1.8f + static_cast<float>(rand() % 25) * 0.1f;
    qm.Lifetime = 3.2f + static_cast<float>(rand() % 25) * 0.1f;
    qm.MaxLifetime = qm.Lifetime;
    qm.BaseAlpha = static_cast<uint8_t>(190 + (rand() % 65));

    int pick = rand() % 3;
    if (pick == 0) qm.Tint = Color{0, 240, 255, 255};      // Electric Cyan
    else if (pick == 1) qm.Tint = Color{255, 20, 147, 255}; // Hot Neon Magenta
    else qm.Tint = Color{255, 200, 30, 255};                // Cyber Gold

    m_luckyQuestionMarks.push_back(qm);
}

void GameScene::RenderLuckyQuestionMarks(GameContext& context)
{
    if (m_luckyQuestionMarks.empty()) return;

    for (const auto& qm : m_luckyQuestionMarks)
    {
        float ratio = std::clamp(qm.Lifetime / qm.MaxLifetime, 0.0f, 1.0f);
        float alphaFactor = 1.0f;
        if (ratio > 0.80f)
        {
            alphaFactor = (1.0f - ratio) / 0.20f;
        }
        else if (ratio < 0.25f)
        {
            alphaFactor = ratio / 0.25f;
        }

        uint8_t a = static_cast<uint8_t>(static_cast<float>(qm.BaseAlpha) * alphaFactor);
        if (a < 5) continue;

        float fontSize = 36.0f * qm.Scale;
        Transform2D trans{qm.Position, qm.Rotation, Vector2f{qm.Scale, qm.Scale}};

        // Chromatic Glitch Aura Shadow (Magenta on Cyan, or Cyan on Magenta/Gold)
        Color glowCol = (qm.Tint.r > 200 && qm.Tint.g < 100)
            ? Color{0, 240, 255, static_cast<uint8_t>(a * 0.50f)}
            : Color{255, 20, 147, static_cast<uint8_t>(a * 0.50f)};
        Transform2D glowTrans{qm.Position + Vector2f{2.0f, 1.5f}, qm.Rotation, Vector2f{qm.Scale * 1.15f, qm.Scale * 1.15f}};
        context.Render.DrawTextCentered("?", m_fontId, fontSize * 1.15f, glowTrans, glowCol);

        // Cyberpunk neon main body
        Color mainCol{qm.Tint.r, qm.Tint.g, qm.Tint.b, a};
        context.Render.DrawTextCentered("?", m_fontId, fontSize, trans, mainCol);

        // Core bright white-cyan highlight
        Color coreCol{230, 255, 255, static_cast<uint8_t>(a * 0.90f)};
        Transform2D coreTrans{qm.Position - Vector2f{1.0f, 1.0f}, qm.Rotation, Vector2f{qm.Scale * 0.85f, qm.Scale * 0.85f}};
        context.Render.DrawTextCentered("?", m_fontId, fontSize * 0.85f, coreTrans, coreCol);
    }
}

void GameScene::RestoreDiscoBricks()
{
    for (const auto& [entity, originalTint] : m_preDiscoBrickTints)
    {
        if (m_registry.IsAlive(entity) && !m_registry.IsPendingDestroy(entity))
        {
            if (m_registry.HasComponent<SpriteComponent>(entity))
            {
                auto& sc = m_registry.GetComponent<SpriteComponent>(entity);
                Color restored = originalTint;
                if (m_registry.HasComponent<BrickComponent>(entity))
                {
                    const auto& brick = m_registry.GetComponent<BrickComponent>(entity);
                    if (brick.IsRegenerating)
                    {
                        restored.a = 0;
                    }
                }
                sc.Tint = restored;
            }
        }
    }
    m_preDiscoBrickTints.clear();
}
