#pragma once

#include "Scenes/GameScene.h"
#include "Tetris/TetrisMatrix.h"
#include <vector>
#include <utility>
#include <string>

class TetrisScene : public GameScene
{
public:
    TetrisScene();
    ~TetrisScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    [[nodiscard]] bool HasNextLevel() const override { return false; }
    void OnGameOverEnter() override;
    void OnGameOverExit() override;
    void OnVictoryEnter() override;

    [[nodiscard]] int GetLinesCleared() const noexcept { return m_linesCleared; }
    [[nodiscard]] int GetTargetLines() const noexcept { return m_targetLines; }
    [[nodiscard]] bool IsStageWon() const noexcept { return m_stageWon; }
    [[nodiscard]] bool IsOverdriveActive() const noexcept { return m_overdriveTimer > 0.0f; }
    [[nodiscard]] bool IsMagneticLocked() const noexcept { return m_magneticLocked; }
    [[nodiscard]] int GetActiveGuideCol() const noexcept { return m_activeGuideCol; }
    [[nodiscard]] const TetrisMatrix& GetMatrix() const noexcept { return m_matrix; }

protected:
    void RenderSceneSpecificHUD(GameContext& context) override;
    void HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false, int damage = 1) override;

private:
    void HandlePlayerInput(GameContext& context);
    void UpdatePaddleTracking(float dt, GameContext& context);
    void CheckBallTetrominoCollision(GameContext& context);
    void StepMatrixLogic(float dt, GameContext& context);
    void SyncSettledBricks(const std::vector<std::pair<int, int>>& settled);
    void ProcessLineClears(GameContext& context);
    void SpawnInitialPlayfield();
    void RenderTetrisPlayfield(GameContext& context);
    void RenderTetrisHUD(GameContext& context) const;

    TetrisMatrix m_matrix;

    // Progression & Fusion Mechanics
    int   m_linesCleared = 0;
    int   m_targetLines = 10;
    bool  m_stageWon = false;
    bool  m_magneticTracking = true;
    bool  m_magneticLocked = false;
    float m_manualOverrideTimer = 0.0f;
    float m_columnTrackTimer = 0.0f;
    float m_overdriveTimer = 0.0f;
    int   m_activeGuideCol = 4;
    float m_trickshotFlashTimer = 0.0f;

    // Timers & Active Falling Piece
    float m_dropTimer = 0.0f;
    float m_dropInterval = 0.85f;
    float m_lockDelayTimer = 0.0f;
    static constexpr float kLockDelayDuration = 0.45f;

    // Danger & Visual Feedback
    float m_dangerGraceTimer = 0.0f;
    bool  m_dangerActive = false;
    float m_dangerAlarmTimer = 0.0f;
    float m_lineClearBannerTimer = 0.0f;
    std::string m_lastClearBanner;
    Color m_lastClearColor = Colors::Gold;
    float m_animTimer = 0.0f;

    // Audio SFX IDs
    uint32_t m_moveSfxId = 0;
    uint32_t m_rotateSfxId = 0;
    uint32_t m_dropSfxId = 0;
    uint32_t m_lineClearSfxId = 0;
    uint32_t m_tetrisSfxId = 0;
    uint32_t m_dangerSfxId = 0;

    // Layout
    Vector2f m_matrixOrigin{0.0f, -120.0f};
    Vector2f m_cellSize{44.0f, 24.0f};
};
