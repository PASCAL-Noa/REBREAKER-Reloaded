#include "Scenes/TetrisScene.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Components/BrickComponent.h"
#include "ECS/Components/Camera2D.h"
#include "Events/GameplayEvents.h"
#include "Core/GameContext.h"
#include "AudioMixer.h"
#include "StateMachine/Transition.h"
#include "StateMachine/Condition.h"
#include "Conditions/GameConditions.h"
#include "Actions/GameOverAction.h"
#include "Actions/VictoryAction.h"
#include <algorithm>
#include <cmath>

TetrisScene::TetrisScene()
    : m_linesCleared(0),
      m_targetLines(10),
      m_stageWon(false),
      m_dropTimer(0.0f),
      m_dropInterval(0.85f),
      m_lockDelayTimer(0.0f),
      m_dangerGraceTimer(0.0f),
      m_dangerActive(false),
      m_dangerAlarmTimer(0.0f),
      m_lineClearBannerTimer(0.0f),
      m_animTimer(0.0f),
      m_matrixOrigin(Vector2f{0.0f, -120.0f}),
      m_cellSize(Vector2f{44.0f, 24.0f})
{
}

void TetrisScene::OnInit(GameContext& context)
{
    // 1. Base GameScene initialization
    GameScene::OnInit(context);

    // 2. Clear any default campaign level bricks
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
    m_brickCount = 0;

    // 3. Reset lives
    m_lives = 3;
    context.Events.Publish(LivesChangedEvent(m_lives));

    // 4. Load Theme Music and Retro SFX
    m_moveSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_move.wav");
    m_rotateSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_rotate.wav");
    m_dropSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_drop.wav");
    m_lineClearSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_line_clear.wav");
    m_tetrisSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_tetris.wav");
    m_dangerSfxId = context.Resources.LoadResource("Resources/audio/sfx/tetris_danger.wav");

    context.Audio.PlayMusic("Resources/audio/music/tetris_8bit_theme.wav", 65.0f, true);

    // 5. Initialize Tetris Matrix and spawn initial playfield bricks
    m_matrix.Reset();
    m_linesCleared = 0;
    m_targetLines = 10;
    m_stageWon = false;
    m_dropTimer = 0.0f;
    m_dropInterval = 0.85f;
    m_lockDelayTimer = 0.0f;
    m_dangerGraceTimer = 0.0f;
    m_dangerActive = false;

    SpawnInitialPlayfield();
    m_matrix.SpawnNextPiece();

    // 6. Custom State Machine strictly isolated for Tetris Mode:
    // Victory only triggers when lines cleared reaches target (m_linesCleared >= m_targetLines)!
    // NEVER when m_brickCount <= 0!
    class TetrisVictoryCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            auto* pTetris = dynamic_cast<TetrisScene*>(pOwner);
            return pTetris && pTetris->IsStageWon();
        }
    };

    class TetrisPauseToCountdownCondition : public Condition<GameScene>
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

    class TetrisPauseToPlayCondition : public Condition<GameScene>
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

    class TetrisCountdownCompleteCondition : public Condition<GameScene>
    {
    public:
        bool OnTest(GameScene* pOwner) override
        {
            return pOwner && pOwner->IsCountdownComplete();
        }
    };

    class TetrisPlayToPauseCondition : public Condition<GameScene>
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
    countdownState->AddTransition(new Transition<GameScene>(new TetrisCountdownCompleteCondition(), static_cast<int>(SceneState::Playing)));
    countdownState->AddTransition(new Transition<GameScene>(new TetrisPlayToPauseCondition(), static_cast<int>(SceneState::Paused)));

    State<GameScene>* playingState = mp_state_machine->CreateState(static_cast<int>(SceneState::Playing));
    playingState->AddTransition(new Transition<GameScene>(new TetrisPlayToPauseCondition(), static_cast<int>(SceneState::Paused)));
    playingState->AddTransition(new Transition<GameScene>(new LivesCondition<GameScene>(), static_cast<int>(SceneState::GameOver)));
    playingState->AddTransition(new Transition<GameScene>(new TetrisVictoryCondition(), static_cast<int>(SceneState::Victory)));

    State<GameScene>* pauseState = mp_state_machine->CreateState(static_cast<int>(SceneState::Paused));
    pauseState->AddTransition(new Transition<GameScene>(new TetrisPauseToCountdownCondition(), static_cast<int>(SceneState::Countdown)));
    pauseState->AddTransition(new Transition<GameScene>(new TetrisPauseToPlayCondition(), static_cast<int>(SceneState::Playing)));

    State<GameScene>* gameOverState = mp_state_machine->CreateState(static_cast<int>(SceneState::GameOver));
    gameOverState->AddAction(new GameOverAction<GameScene>());
    gameOverState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    State<GameScene>* victoryState = mp_state_machine->CreateState(static_cast<int>(SceneState::Victory));
    victoryState->AddAction(new VictoryAction<GameScene>());
    victoryState->AddTransition(new Transition<GameScene>(new GameOverReplayCondition<GameScene>(), static_cast<int>(SceneState::Countdown)));

    StartCountdown();
}

void TetrisScene::SpawnInitialPlayfield()
{
    // Seed an authentic starter brick pattern in bottom rows
    // Row 19 (near bottom floor): fill 8 cells, leaving col 4 and 9 as targets to complete!
    const int starterCols19[] = {0, 1, 2, 3, 5, 6, 7, 8};
    for (int col : starterCols19)
    {
        TetrominoType t = (col < 4) ? TetrominoType::I : (col < 8 ? TetrominoType::T : TetrominoType::O);
        TetrisCell cell{};
        cell.Occupied = true;
        cell.Type = t;
        cell.CellColor = TetrisMatrix::GetPieceColor(t);
        m_matrix.SetCell(col, 19, cell);

        Vector2f worldPos = TetrisMatrix::GridToWorld(col, 19, m_matrixOrigin, m_cellSize);
        Entity brick = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(brick, Transform2D{worldPos});
        m_registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f}, Vector2f{0.0f, 0.0f}, false, false});
        m_registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
        m_registry.AddComponent<TweenComponent>(brick, TweenComponent{});

        BrickComponent comp;
        comp.Type = BrickType::Light;
        comp.HitPoints = 1;
        comp.MaxHitPoints = 1;
        comp.ScoreValue = 120;
        m_registry.AddComponent<BrickComponent>(brick, comp);
        m_registry.AddComponent<SpriteComponent>(brick, SpriteComponent{m_brickTexId, cell.CellColor});

        cell.BrickEntity = brick;
        m_matrix.SetCell(col, 19, cell);
        m_brickCount++;
    }

    // Row 18: fill 6 cells (columns 0, 1, 2, 6, 7, 8)
    const int starterCols18[] = {0, 1, 2, 6, 7, 8};
    for (int col : starterCols18)
    {
        TetrominoType t = (col < 3) ? TetrominoType::S : TetrominoType::L;
        TetrisCell cell{};
        cell.Occupied = true;
        cell.Type = t;
        cell.CellColor = TetrisMatrix::GetPieceColor(t);
        m_matrix.SetCell(col, 18, cell);

        Vector2f worldPos = TetrisMatrix::GridToWorld(col, 18, m_matrixOrigin, m_cellSize);
        Entity brick = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(brick, Transform2D{worldPos});
        m_registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f}, Vector2f{0.0f, 0.0f}, false, false});
        m_registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
        m_registry.AddComponent<TweenComponent>(brick, TweenComponent{});

        BrickComponent comp;
        comp.Type = BrickType::Light;
        comp.HitPoints = 1;
        comp.MaxHitPoints = 1;
        comp.ScoreValue = 120;
        m_registry.AddComponent<BrickComponent>(brick, comp);
        m_registry.AddComponent<SpriteComponent>(brick, SpriteComponent{m_brickTexId, cell.CellColor});

        cell.BrickEntity = brick;
        m_matrix.SetCell(col, 18, cell);
        m_brickCount++;
    }
}

void TetrisScene::OnDestroy(GameContext& context)
{
    GameScene::OnDestroy(context);
}

void TetrisScene::OnGameOverEnter()
{
    GameScene::OnGameOverEnter();
}

void TetrisScene::OnGameOverExit()
{
    GameScene::OnGameOverExit();
    // Replay sequence: reset matrix and playfield
    m_matrix.Reset();
    m_linesCleared = 0;
    m_stageWon = false;
    m_overdriveTimer = 0.0f;
    m_manualOverrideTimer = 0.0f;
    m_magneticLocked = false;
    m_dangerGraceTimer = 0.0f;
    m_dangerActive = false;
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();
    m_brickCount = 0;
    SpawnInitialPlayfield();
    m_matrix.SpawnNextPiece();
}

void TetrisScene::OnVictoryEnter()
{
    GameScene::OnVictoryEnter();
    m_stageWon = true;
}

void TetrisScene::OnUpdate(float dt, GameContext& context)
{
    GameScene::OnUpdate(dt, context);

    if (IsPaused() || m_stageWon || IsCountdownActive())
    {
        return;
    }

    m_animTimer += dt;
    if (m_lineClearBannerTimer > 0.0f)
    {
        m_lineClearBannerTimer -= dt;
    }
    if (m_overdriveTimer > 0.0f)
    {
        m_overdriveTimer -= dt;
    }
    if (m_trickshotFlashTimer > 0.0f)
    {
        m_trickshotFlashTimer -= dt;
    }

    UpdatePaddleTracking(dt, context);
    HandlePlayerInput(context);
    CheckBallTetrominoCollision(context);
    StepMatrixLogic(dt, context);
}

void TetrisScene::UpdatePaddleTracking(float dt, GameContext& context)
{
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        float paddleX = m_registry.GetComponent<Transform2D>(m_paddle).Position.X;
        const float totalW = static_cast<float>(TetrisMatrix::WIDTH) * m_cellSize.X;
        const float matrixLeft = m_matrixOrigin.X - (totalW * 0.5f);
        int targetCol = static_cast<int>(std::floor((paddleX - matrixLeft) / m_cellSize.X));
        targetCol = std::clamp(targetCol, 0, TetrisMatrix::WIDTH - 1);
        m_activeGuideCol = targetCol;

        // Holding Shift or Gamepad LB locks the magnetic beam in place
        m_magneticLocked = context.Input.IsKeyDown(KeyCode::LShift) ||
                           context.Input.IsKeyDown(KeyCode::RShift) ||
                           context.Input.IsJoystickButtonDown(0, 4);

        if (m_manualOverrideTimer > 0.0f)
        {
            m_manualOverrideTimer -= dt;
        }
        else if (!m_magneticLocked && m_magneticTracking)
        {
            m_columnTrackTimer += dt;
            if (m_columnTrackTimer >= 0.04f)
            {
                m_columnTrackTimer = 0.0f;
                m_matrix.MoveTowardsColumn(targetCol);
            }
        }
    }
}

void TetrisScene::HandlePlayerInput(GameContext& context)
{
    if (IsPaused() || m_stageWon || IsCountdownActive()) return;

    // 1. ROTATE CLOCKWISE: Right Click / W / Up Arrow / Gamepad Y (3) / Gamepad B (1)
    const bool rotateCw = context.Input.IsMouseButtonPress(MouseButton::Right) ||
                          context.Input.IsKeyPress(KeyCode::Up) ||
                          context.Input.IsKeyPress(KeyCode::W) ||
                          context.Input.IsJoystickButtonPress(0, 3) ||
                          context.Input.IsJoystickButtonPress(0, 1);
    if (rotateCw)
    {
        if (m_matrix.RotateClockwise() && m_rotateSfxId != 0)
        {
            context.Audio.PlaySfx(m_rotateSfxId, 55.0f);
        }
    }

    // 2. ROTATE COUNTER-CLOCKWISE: Q / Gamepad X (2)
    if (context.Input.IsKeyPress(KeyCode::Q) || context.Input.IsJoystickButtonPress(0, 2))
    {
        if (m_matrix.RotateCounterClockwise() && m_rotateSfxId != 0)
        {
            context.Audio.PlaySfx(m_rotateSfxId, 55.0f);
        }
    }

    // 3. HARD DROP / SLAM: Space (when ball in play) / S / Down / Middle Mouse / Gamepad A (0)
    const bool hardDrop = (m_ballState == BallState::Active && context.Input.IsKeyPress(KeyCode::Space)) ||
                          context.Input.IsKeyPress(KeyCode::S) ||
                          context.Input.IsKeyPress(KeyCode::Down) ||
                          context.Input.IsMouseButtonPress(MouseButton::Middle) ||
                          context.Input.IsJoystickButtonPress(0, 0);
    if (hardDrop)
    {
        if (m_matrix.HasActivePiece())
        {
            auto settled = m_matrix.HardDrop();
            if (!settled.empty())
            {
                if (m_dropSfxId != 0) context.Audio.PlaySfx(m_dropSfxId, 65.0f);
                SyncSettledBricks(settled);
                ProcessLineClears(context);
                m_matrix.SpawnNextPiece();
                m_dropTimer = 0.0f;
                m_lockDelayTimer = 0.0f;
            }
        }
    }

    // 4. SOFT DROP: Down or S speeds up gravity
    if (context.Input.IsKeyDown(KeyCode::Down) || context.Input.IsKeyDown(KeyCode::S))
    {
        m_dropInterval = 0.08f;
    }
    else
    {
        m_dropInterval = std::max(0.40f, 1.05f - static_cast<float>(m_linesCleared) * 0.05f);
    }

    // 5. MANUAL NUDGE: Left / Right arrows
    if (context.Input.IsKeyPress(KeyCode::Left))
    {
        m_manualOverrideTimer = 1.0f;
        if (m_matrix.MoveLeft() && m_moveSfxId != 0)
        {
            context.Audio.PlaySfx(m_moveSfxId, 45.0f);
        }
    }
    else if (context.Input.IsKeyPress(KeyCode::Right))
    {
        m_manualOverrideTimer = 1.0f;
        if (m_matrix.MoveRight() && m_moveSfxId != 0)
        {
            context.Audio.PlaySfx(m_moveSfxId, 45.0f);
        }
    }
}

void TetrisScene::CheckBallTetrominoCollision(GameContext& context)
{
    if (m_ballState != BallState::Active || !m_matrix.HasActivePiece()) return;

    std::vector<Entity> activeBalls;
    if (mp_ballSystem)
    {
        activeBalls = mp_ballSystem->GetBalls();
    }
    else if (m_registry.IsAlive(m_ball))
    {
        activeBalls.push_back(m_ball);
    }

    const auto activeBlocks = m_matrix.GetActivePieceBlocks();
    if (activeBlocks.empty()) return;

    for (Entity ballEntity : activeBalls)
    {
        if (!m_registry.IsAlive(ballEntity) ||
            !m_registry.HasComponent<Transform2D>(ballEntity) ||
            !m_registry.HasComponent<RigidBody>(ballEntity))
        {
            continue;
        }

        auto& ballTrans = m_registry.GetComponent<Transform2D>(ballEntity);
        auto& ballRb = m_registry.GetComponent<RigidBody>(ballEntity);
        const float ballRadius = 12.0f;

        for (const auto& [gx, gy] : activeBlocks)
        {
            Vector2f blockPos = TetrisMatrix::GridToWorld(gx, gy, m_matrixOrigin, m_cellSize);
            const float halfW = m_cellSize.X * 0.5f;
            const float halfH = m_cellSize.Y * 0.5f;

            float closestX = std::clamp(ballTrans.Position.X, blockPos.X - halfW, blockPos.X + halfW);
            float closestY = std::clamp(ballTrans.Position.Y, blockPos.Y - halfH, blockPos.Y + halfH);

            float dx = ballTrans.Position.X - closestX;
            float dy = ballTrans.Position.Y - closestY;
            float distSq = dx * dx + dy * dy;

            if (distSq < (ballRadius * ballRadius))
            {
                // Reflect velocity
                if (std::abs(dx) > std::abs(dy))
                {
                    ballRb.Velocity.X = (dx >= 0.0f) ? std::abs(ballRb.Velocity.X) : -std::abs(ballRb.Velocity.X);
                }
                else
                {
                    ballRb.Velocity.Y = (dy >= 0.0f) ? std::abs(ballRb.Velocity.Y) : -std::abs(ballRb.Velocity.Y);
                }

                // Trickshot rotation!
                if (m_matrix.RotateClockwise() && m_rotateSfxId != 0)
                {
                    context.Audio.PlaySfx(m_rotateSfxId, 75.0f);
                }

                SpawnExplosionParticles(Vector2f{closestX, closestY}, Colors::Cyan, 8);
                m_scoreManager.AddScore(250);
                m_lastClearBanner = "TRICKSHOT ROTATION! +250";
                m_lastClearColor = Colors::Cyan;
                m_lineClearBannerTimer = 1.2f;
                m_trickshotFlashTimer = 0.15f;
                TriggerPostProcessFlash(0.15f, 0.12f, Colors::Cyan, 0.25f);
                return;
            }
        }
    }
}

void TetrisScene::StepMatrixLogic(float dt, GameContext& context)
{
    if (!m_matrix.HasActivePiece())
    {
        m_matrix.SpawnNextPiece();
    }

    m_dropTimer += dt;
    if (m_dropTimer >= m_dropInterval)
    {
        m_dropTimer = 0.0f;
        if (!m_matrix.StepDown())
        {
            m_lockDelayTimer += dt + m_dropInterval;
        }
        else
        {
            m_lockDelayTimer = 0.0f;
        }
    }
    else
    {
        if (!m_matrix.CanPlace(m_matrix.GetCurrentType(), m_matrix.GetCurrentX(), m_matrix.GetCurrentY() + 1, m_matrix.GetCurrentRotation()))
        {
            m_lockDelayTimer += dt;
        }
        else
        {
            m_lockDelayTimer = 0.0f;
        }
    }

    if (m_lockDelayTimer >= kLockDelayDuration)
    {
        auto settled = m_matrix.SettlePiece();
        if (!settled.empty())
        {
            if (m_dropSfxId != 0) context.Audio.PlaySfx(m_dropSfxId, 55.0f);
            SyncSettledBricks(settled);
            ProcessLineClears(context);
            m_matrix.SpawnNextPiece();
            m_lockDelayTimer = 0.0f;
            m_dropTimer = 0.0f;
        }
    }

    // Danger Zone Check: Blocks reaching near the paddle or top-out
    bool bottomDanger = m_matrix.IsDangerLineReached(TetrisMatrix::BOTTOM_DANGER_ROW);
    bool topDanger = m_matrix.IsTopDangerActive();
    m_dangerActive = (bottomDanger || topDanger);

    if (m_dangerActive)
    {
        m_dangerAlarmTimer += dt;
        if (m_dangerAlarmTimer >= 1.2f)
        {
            m_dangerAlarmTimer = 0.0f;
            if (m_dangerSfxId != 0)
            {
                context.Audio.PlaySfx(m_dangerSfxId, 60.0f);
            }
        }

        m_dangerGraceTimer += dt;
        if (m_dangerGraceTimer >= 7.0f)
        {
            OnGameOverEnter();
        }
    }
    else
    {
        m_dangerGraceTimer = 0.0f;
        m_dangerAlarmTimer = 0.0f;
    }
}

void TetrisScene::SyncSettledBricks(const std::vector<std::pair<int, int>>& settled)
{
    for (const auto& [x, y] : settled)
    {
        Vector2f worldPos = TetrisMatrix::GridToWorld(x, y, m_matrixOrigin, m_cellSize);
        Entity brick = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(brick, Transform2D{worldPos});
        m_registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f}, Vector2f{0.0f, 0.0f}, false, false});
        m_registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
        m_registry.AddComponent<TweenComponent>(brick, TweenComponent{});

        BrickComponent comp;
        comp.Type = BrickType::Light;
        comp.HitPoints = 1;
        comp.MaxHitPoints = 1;
        comp.ScoreValue = 120;
        m_registry.AddComponent<BrickComponent>(brick, comp);

        Color pieceCol = TetrisMatrix::GetPieceColor(m_matrix.GetCell(x, y).Type);
        m_registry.AddComponent<SpriteComponent>(brick, SpriteComponent{m_brickTexId, pieceCol});

        TetrisCell cell = m_matrix.GetCell(x, y);
        cell.BrickEntity = brick;
        cell.CellColor = pieceCol;
        m_matrix.SetCell(x, y, cell);
        m_brickCount++;
    }
}

void TetrisScene::ProcessLineClears(GameContext& context)
{
    auto clearedRows = m_matrix.CheckAndClearLines();
    if (clearedRows.empty()) return;

    int lineCount = static_cast<int>(clearedRows.size());
    m_linesCleared += lineCount;

    // Bonus Scoring Cascade
    int bonusScore = 0;
    if (lineCount == 1)
    {
        bonusScore = 1000;
        m_lastClearBanner = "SINGLE CLEAR! +1,000";
        m_lastClearColor = Colors::Cyan;
    }
    else if (lineCount == 2)
    {
        bonusScore = 3000;
        m_lastClearBanner = "DOUBLE CLEAR!! +3,000";
        m_lastClearColor = Colors::Green;
    }
    else if (lineCount == 3)
    {
        bonusScore = 6000;
        m_lastClearBanner = "TRIPLE CLEAR!!! +6,000";
        m_lastClearColor = Colors::Orange;
    }
    else
    {
        bonusScore = 12000;
        m_lastClearBanner = "TETRIS CLEARED!!!! +12,000";
        m_lastClearColor = Colors::Gold;
    }

    m_scoreManager.AddScore(bonusScore);
    m_lineClearBannerTimer = 2.2f;
    AddUltimateCharge(static_cast<float>(lineCount) * 15.0f);

    // Overdrive Supercharge on Line Clears!
    m_overdriveTimer = 5.0f + static_cast<float>(lineCount) * 1.5f;

    // Audio SFX
    if (lineCount >= 4 && m_tetrisSfxId != 0)
    {
        context.Audio.PlaySfx(m_tetrisSfxId, 85.0f);
    }
    else if (m_lineClearSfxId != 0)
    {
        context.Audio.PlaySfx(m_lineClearSfxId, 75.0f);
    }

    // Visual Punch Flash
    TriggerPostProcessFlash(0.25f * static_cast<float>(lineCount), 0.20f, Colors::White, 0.35f * static_cast<float>(lineCount));

    // Particle explosions at cleared rows
    for (int row : clearedRows)
    {
        for (int col = 0; col < TetrisMatrix::WIDTH; ++col)
        {
            Vector2f worldPos = TetrisMatrix::GridToWorld(col, row, m_matrixOrigin, m_cellSize);
            SpawnExplosionParticles(worldPos, Colors::Gold, 6);
        }
    }

    // Synchronize all remaining matrix bricks with their new grid positions
    for (int y = 0; y < TetrisMatrix::HEIGHT; ++y)
    {
        for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
        {
            const auto& cell = m_matrix.GetCell(x, y);
            if (cell.Occupied && cell.BrickEntity != NULL_ENTITY && m_registry.IsAlive(cell.BrickEntity))
            {
                Vector2f targetPos = TetrisMatrix::GridToWorld(x, y, m_matrixOrigin, m_cellSize);
                if (m_registry.HasComponent<Transform2D>(cell.BrickEntity))
                {
                    m_registry.GetComponent<Transform2D>(cell.BrickEntity).Position = targetPos;
                }
            }
        }
    }

    // Victory Check: Stage won when target lines are cleared!
    if (m_linesCleared >= m_targetLines && !m_stageWon)
    {
        m_stageWon = true;
        OnVictoryEnter();
    }
}

void TetrisScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    if (m_overdriveTimer > 0.0f)
    {
        damage += 2;
        m_scoreManager.AddScore(150);
    }

    GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, damage);

    // If brick was destroyed by ball or powerup, remove it from matrix to create a gap!
    if (m_registry.IsPendingDestroy(entity) || !m_registry.IsAlive(entity))
    {
        for (int y = 0; y < TetrisMatrix::HEIGHT; ++y)
        {
            for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
            {
                if (m_matrix.GetCell(x, y).BrickEntity == entity)
                {
                    m_matrix.RemoveCell(x, y);
                    return;
                }
            }
        }
    }
}

void TetrisScene::OnRender(GameContext& context)
{
    // 1. Base GameScene renders paddle, ball, settled ECS bricks
    GameScene::OnRender(context);

    // 2. Set World Camera to render matrix rails, danger line, ghost and falling piece
    const auto& cameraComp = m_registry.GetComponent<Camera2D>(m_camera);
    context.Render.SetCamera(cameraComp);

    RenderTetrisPlayfield(context);

    context.Render.ResetCamera();
}

void TetrisScene::RenderTetrisPlayfield(GameContext& context)
{
    const float totalW = static_cast<float>(TetrisMatrix::WIDTH) * m_cellSize.X;
    const float totalH = static_cast<float>(TetrisMatrix::HEIGHT) * m_cellSize.Y;

    // 1. Matrix Neon Backdrop & Grid Cells
    context.Render.DrawRectangle(totalW, totalH, Transform2D{m_matrixOrigin}, Color{10, 16, 28, 160});
    context.Render.DrawRectangleOutline(totalW, totalH, Transform2D{m_matrixOrigin}, Color{35, 75, 130, 200}, 2.0f);

    // Internal Grid lines
    const float startX = m_matrixOrigin.X - (totalW * 0.5f);
    const float startY = m_matrixOrigin.Y - (totalH * 0.5f);

    for (int c = 1; c < TetrisMatrix::WIDTH; ++c)
    {
        float gx = startX + static_cast<float>(c) * m_cellSize.X;
        context.Render.DrawRectangle(1.0f, totalH, Transform2D{Vector2f{gx, m_matrixOrigin.Y}}, Color{35, 60, 95, 45});
    }

    for (int r = 1; r < TetrisMatrix::HEIGHT; ++r)
    {
        float gy = startY + static_cast<float>(r) * m_cellSize.Y;
        context.Render.DrawRectangle(totalW, 1.0f, Transform2D{Vector2f{m_matrixOrigin.X, gy}}, Color{35, 60, 95, 45});
    }

    // 2. Vertical Magnetic Projection Beam aligned with paddle
    float beamX = startX + (static_cast<float>(m_activeGuideCol) + 0.5f) * m_cellSize.X;
    uint8_t beamAlpha = m_magneticLocked ?
        static_cast<uint8_t>(40.0f + 20.0f * std::sin(m_animTimer * 10.0f)) :
        static_cast<uint8_t>(25.0f + 15.0f * std::sin(m_animTimer * 5.0f));
    Color beamColor = m_magneticLocked ? Color{255, 180, 40, beamAlpha} : Color{0, 240, 255, beamAlpha};
    Color beamBorder = m_magneticLocked ? Color{255, 200, 80, 90} : Color{0, 240, 255, 80};

    context.Render.DrawRectangle(m_cellSize.X - 2.0f, totalH, Transform2D{Vector2f{beamX, m_matrixOrigin.Y}}, beamColor);
    context.Render.DrawRectangleOutline(m_cellSize.X - 2.0f, totalH, Transform2D{Vector2f{beamX, m_matrixOrigin.Y}}, beamBorder, 1.0f);

    // 3. Danger Laser Line across the arena near paddle threshold
    Vector2f dangerPos = TetrisMatrix::GridToWorld(0, TetrisMatrix::BOTTOM_DANGER_ROW, m_matrixOrigin, m_cellSize);
    float dangerY = dangerPos.Y - (m_cellSize.Y * 0.5f);

    uint8_t laserAlpha = m_dangerActive ? static_cast<uint8_t>(180.0f + 70.0f * std::sin(m_animTimer * 12.0f)) : 90;
    Color dangerLaserColor = m_dangerActive ? Color{255, 30, 60, laserAlpha} : Color{255, 140, 40, laserAlpha};

    context.Render.DrawRectangle(totalW + 60.0f, 3.0f, Transform2D{Vector2f{m_matrixOrigin.X, dangerY}}, dangerLaserColor);

    // 4. Ghost Piece (Landing Preview)
    if (m_matrix.HasActivePiece())
    {
        auto ghostBlocks = m_matrix.GetGhostPieceBlocks();
        Color pieceCol = TetrisMatrix::GetPieceColor(m_matrix.GetCurrentType());
        Color ghostCol{pieceCol.r, pieceCol.g, pieceCol.b, 75};

        for (const auto& [gx, gy] : ghostBlocks)
        {
            Vector2f worldPos = TetrisMatrix::GridToWorld(gx, gy, m_matrixOrigin, m_cellSize);
            context.Render.DrawRectangleOutline(m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f, Transform2D{worldPos}, ghostCol, 1.5f);
        }

        // 5. Active Falling Tetromino
        auto activeBlocks = m_matrix.GetActivePieceBlocks();
        Color drawPieceCol = (m_trickshotFlashTimer > 0.0f) ? Colors::White : pieceCol;
        for (const auto& [ax, ay] : activeBlocks)
        {
            Vector2f worldPos = TetrisMatrix::GridToWorld(ax, ay, m_matrixOrigin, m_cellSize);
            context.Render.DrawRectangle(m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f, Transform2D{worldPos}, drawPieceCol);
            context.Render.DrawRectangleOutline(m_cellSize.X - 2.0f, m_cellSize.Y - 2.0f, Transform2D{worldPos}, Colors::White, 1.5f);
        }
    }
}

void TetrisScene::RenderSceneSpecificHUD(GameContext& context)
{
    RenderTetrisHUD(context);
}

void TetrisScene::RenderTetrisHUD(GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;

    // Top-Center HUD: Stage Title & Lines Cleared
    context.Render.DrawTextCentered("TETRIS MATRIX GIMMICK STAGE", m_fontId, 22.0f, Transform2D{Vector2f{screenCenterX, 38.0f}}, Colors::Gold);

    std::string linesText = "LINES CLEARED: " + std::to_string(m_linesCleared) + " / " + std::to_string(m_targetLines);
    context.Render.DrawTextCentered(linesText, m_fontId, 18.0f, Transform2D{Vector2f{screenCenterX, 68.0f}}, Colors::Cyan);

    // Progress Bar
    const float barW = 280.0f;
    const float barH = 10.0f;
    const float barY = 90.0f;
    float fillRatio = std::clamp(static_cast<float>(m_linesCleared) / static_cast<float>(m_targetLines), 0.0f, 1.0f);

    context.Render.DrawRectangle(barW + 4.0f, barH + 4.0f, Transform2D{Vector2f{screenCenterX, barY}}, Color{20, 30, 48, 200});
    context.Render.DrawRectangleOutline(barW + 4.0f, barH + 4.0f, Transform2D{Vector2f{screenCenterX, barY}}, Colors::Cyan, 1.0f);
    if (fillRatio > 0.0f)
    {
        float filledW = barW * fillRatio;
        float filledX = screenCenterX - (barW * 0.5f) + (filledW * 0.5f);
        context.Render.DrawRectangle(filledW, barH, Transform2D{Vector2f{filledX, barY}}, Colors::Gold);
    }

    // Overdrive Supercharge Indicator
    if (m_overdriveTimer > 0.0f)
    {
        std::string odText = "★ NEON OVERDRIVE x2 (" + std::to_string(static_cast<int>(std::ceil(m_overdriveTimer))) + "s) ★";
        Color odCol = (std::sin(m_animTimer * 16.0f) > 0.0f) ? Colors::Gold : Color{0, 255, 240, 255};
        context.Render.DrawTextCentered(odText, m_fontId, 20.0f, Transform2D{Vector2f{screenCenterX, 115.0f}}, odCol);
    }

    // Right Side: NEXT PIECE Preview Box
    const float previewBoxX = screenCenterX + 340.0f;
    const float previewBoxY = 160.0f;
    context.Render.DrawRectangle(130.0f, 120.0f, Transform2D{Vector2f{previewBoxX, previewBoxY}}, Color{12, 18, 30, 220});
    context.Render.DrawRectangleOutline(130.0f, 120.0f, Transform2D{Vector2f{previewBoxX, previewBoxY}}, Color{60, 110, 180, 220}, 2.0f);
    context.Render.DrawTextCentered("NEXT", m_fontId, 16.0f, Transform2D{Vector2f{previewBoxX, previewBoxY - 42.0f}}, Color{180, 210, 255, 230});

    TetrominoType nextType = m_matrix.GetNextPieceType();
    if (nextType != TetrominoType::None)
    {
        auto offsets = TetrisMatrix::GetShapeOffsets(nextType, 0);
        Color col = TetrisMatrix::GetPieceColor(nextType);
        const float miniSize = 18.0f;
        const float originMiniX = previewBoxX - miniSize * 1.5f;
        const float originMiniY = previewBoxY - miniSize * 0.5f;

        for (const auto& [dx, dy] : offsets)
        {
            Vector2f miniPos{originMiniX + static_cast<float>(dx) * miniSize, originMiniY + static_cast<float>(dy) * miniSize};
            context.Render.DrawRectangle(miniSize - 2.0f, miniSize - 2.0f, Transform2D{miniPos}, col);
        }
    }

    // Line Clear Cascade Announcement
    if (m_lineClearBannerTimer > 0.0f)
    {
        float scale = 1.0f + 0.15f * std::sin((2.2f - m_lineClearBannerTimer) * 8.0f);
        context.Render.DrawTextCentered(m_lastClearBanner, m_fontId, 32.0f * scale, Transform2D{Vector2f{screenCenterX, 220.0f}}, m_lastClearColor);
    }

    // Danger Alert Banner
    if (m_dangerActive)
    {
        int remainingSec = std::max(0, static_cast<int>(std::ceil(7.0f - m_dangerGraceTimer)));
        std::string dangerText = "WARNING: MATRIX OVERFLOW! EVACUATE ROW IN " + std::to_string(remainingSec) + "s!";
        Color alertCol = (std::sin(m_animTimer * 14.0f) > 0.0f) ? Color{255, 30, 60, 255} : Colors::Gold;
        context.Render.DrawTextCentered(dangerText, m_fontId, 20.0f, Transform2D{Vector2f{screenCenterX, 145.0f}}, alertCol);
    }

    // Bottom Control Guide Banner
    const float bottomPanelY = viewSize.Y - 24.0f;
    context.Render.DrawRectangle(viewSize.X - 100.0f, 32.0f, Transform2D{Vector2f{screenCenterX, bottomPanelY}}, Color{10, 15, 26, 220});
    context.Render.DrawRectangleOutline(viewSize.X - 100.0f, 32.0f, Transform2D{Vector2f{screenCenterX, bottomPanelY}}, Color{35, 75, 130, 200}, 1.2f);

    std::string tips = "[SOURIS / A-D] GUIDER | [CLIC DROIT / W] PIVOTER | [ESPACE / S] SLAM DROP | [SHIFT] VERROUILLER | BALLE = RICOCHET & TRICKSHOT";
    context.Render.DrawTextCentered(tips, m_fontId, 13.5f, Transform2D{Vector2f{screenCenterX, bottomPanelY}}, Color{180, 225, 255, 240});
}
