#pragma once

#include "Scenes/GameScene.h"
#include "Editor/LevelData.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>

enum class EditorTool : uint8_t
{
    Paint = 0,
    Eraser = 1, // Gomme
    Select = 2  // Selection / Inspector
};

enum class EditorTab : uint8_t
{
    Inspector = 0,
    Procedural = 1,
    ThemeLighting = 2,
    EventQueue = 3,
    Gamerules = 4
};

class EditorScene : public GameScene
{
public:
    EditorScene();
    explicit EditorScene(std::string editLevelId, bool returnToRobostock = false);
    ~EditorScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    // Safety and lifecycle hooks
    [[nodiscard]] bool IsGameOver() const override;
    [[nodiscard]] bool IsLevelCleared() const override;
    void OnGameOverEnter() override;
    void OnGameOverExit() override;
    void OnVictoryEnter() override;
    void OnVictoryExit() override;
    void FullReset() override;
    void OnReplayRequested() override;
    void LoadLevel(int levelIndex, bool preserveStats = false) override;
    void StartLevelTransition() override;
    void CompleteLevelTransition() override;
    void RestorePostUltimateMusic() override;
    [[nodiscard]] bool HasNextLevel() const override { return false; }
    [[nodiscard]] uint32_t GetPostProcessShader() const override { return m_isPlaytesting ? m_shaderId : 0; }

    // Playtest Mode Controls
    void StartPlaytesting(GameContext& context);
    void StopPlaytesting(GameContext& context);
    [[nodiscard]] bool IsPlaytesting() const noexcept { return m_isPlaytesting; }

    // Editor Actions
    void PushUndo();
    void Undo();
    void SetTool(EditorTool tool);
    void SetSelectedTile(EditorTileType tile);
    void SaveLevel(GameContext& context);
    void SaveLevelAs(GameContext& context);
    void GenerateProceduralLevel();

    // Event Queue Drag & Drop / Trash
    void ClearEventQueue();
    void DeleteEvent(size_t index);

protected:
    void RenderSceneSpecificHUD(GameContext& context) override;
    void HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false, int damage = 1) override;

private:
    struct EditorGridLayout
    {
        float CanvasMinX = 0.0f;
        float CanvasMaxX = 0.0f;
        float CanvasMinY = 0.0f;
        float CanvasMaxY = 0.0f;
        float PageBarHeight = 64.0f;
        float GridMinX = 0.0f;
        float GridMinY = 0.0f;
        float BrickW = 80.0f;
        float BrickH = 34.0f;
        float PadX = 6.0f;
        float PadY = 8.0f;
        float StepX = 86.0f;
        float StepY = 42.0f;

        [[nodiscard]] Vector2f GetCellCenter(int col, int row) const
        {
            return Vector2f{
                GridMinX + BrickW * 0.5f + static_cast<float>(col) * StepX,
                GridMinY + BrickH * 0.5f + static_cast<float>(row) * StepY
            };
        }
    };

    [[nodiscard]] EditorGridLayout ComputeGridLayout(const Vector2f& viewSize) const;
    [[nodiscard]] std::optional<size_t> FindNextPlayablePage(size_t startIndex) const;
    void RebuildPlayfieldFromGrid(GameContext& context);
    void RebuildPlayfieldFromPage(size_t pageIdx, GameContext& context, bool withDescent = true);
    void ApplyEditorThemeToPlaytest();
    void ExitEditor(GameContext& context);

    void HandleEditorInput(GameContext& context);
    void RenderEditorUI(GameContext& context);
    void RenderToolbar(GameContext& context);
    void RenderPalette(GameContext& context);
    void RenderTabsAndPanels(GameContext& context);
    void RenderInspectorTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse);
    bool HandleInspectorTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick);
    void RenderEventQueueTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse);
    bool HandleEventQueueTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick);
    void RenderPageBar(GameContext& context, const EditorGridLayout& layout, const Vector2f& worldMouse);
    bool HandlePageBarInput(const EditorGridLayout& layout, const Vector2f& worldMouse, bool leftClick, GameContext& context);
    void RenderEventTrashBin(GameContext& context, const Vector2f& trashPos, const Vector2f& trashSize);
    void RenderGrid(GameContext& context);
    void RenderDefeatModal(GameContext& context);
    void HandleDefeatModalInput(GameContext& context);
    void OpenSaveAsModal();
    void ConfirmSaveAsModal(GameContext& context);
    void RenderSaveAsModal(GameContext& context);
    void HandleSaveAsModalInput(GameContext& context);
    void RenderPlaytestStopButton(GameContext& context);

    [[nodiscard]] EditorCellData GetInspectorTargetData() const;
    void ApplyInspectorTargetData(const EditorCellData& updated);

    // Vector Icon Drawing Helpers
    void DrawIconPlay(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconStop(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconUndo(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconSelect(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconEraser(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconFloppy(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconFloppyPlus(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconTrash(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawIconExit(GameContext& context, const Vector2f& pos, float size, Color color);
    void DrawChevron(GameContext& context, const Vector2f& pos, float size, bool pointsRight, Color color);
    [[nodiscard]] Color GetBrickColor(EditorTileType tile) const;

    // Turret Bricks & Downward Projectiles in Playtest (Campaign-fidelity)
    struct TriangleProjectile
    {
        Vector2f Position{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 210.0f};
        float    Size = 16.0f;
    };

    struct TurretBrick
    {
        Entity EntityId = NULL_ENTITY;
        float  ShootTimer = 3.5f;
        float  Cooldown = 3.2f;
        float  ProjectileSpeed = 210.0f;
        int    BaseHP = 3;
    };

    void UpdateTurrets(float dt, GameContext& context);
    void RenderTurrets(GameContext& context);
    void TriggerPaddleExplosion(GameContext& context);
    void RenderPaddleDurabilityOverlay(GameContext& context);

    // Editor State
    LevelData       m_levelData;
    EditorTool      m_tool = EditorTool::Paint;
    EditorTileType  m_selectedTile = EditorTileType::Light;
    EditorTab       m_activeTab = EditorTab::Inspector;
    bool            m_isPlaytesting = false;
    bool            m_showDefeatModal = false;
    bool            m_leftPanelOpen = true;
    bool            m_rightPanelOpen = true;
    int             m_hoveredToolbarBtn = -1;

    // Robostock & Save As Modal state
    std::string     m_editingLevelId;
    bool            m_returnToRobostock = false;
    bool            m_showSaveAsModal = false;
    std::string     m_saveAsNameBuffer;
    std::string     m_saveAsPreviewId;

    // Selected cell for Inspector
    bool            m_hasSelectedCell = false;
    int             m_selectedCol = 0;
    int             m_selectedRow = 0;

    // Multi-page & EventQueue Playtest tracking
    size_t          m_playtestPageIndex = 0;
    float           m_pageBannerTimer = 0.0f;
    float           m_playtestElapsed = 0.0f;
    size_t          m_nextQueuedEventIdx = 0;

    // Undo stack (stores full multi-page + event queue snapshots)
    std::vector<LevelData::Snapshot> m_undoStack;

    // Procedural Generator Settings
    int   m_procBrickCount = 44;
    float m_procDifficulty = 0.50f;
    int   m_procSymmetry = 1; // 0: None, 1: Horiz, 2: Vert, 3: Dual

    // Drag and Drop & Scroll for Event Queue
    int      m_draggedEventIdx = -1;
    Vector2f m_dragMousePos{0.0f, 0.0f};
    bool     m_isDraggingEvent = false;
    bool     m_trashCanHovered = false;
    int      m_eventQueueScroll = 0;

    // Turrets, Projectiles & Paddle Damage Feedback
    std::vector<TurretBrick>        m_turretBricks;
    std::vector<TriangleProjectile> m_turretBullets;
    float                           m_paddleDamageFlashTimer = 0.0f;
    bool                            m_isPaddleDestroyed = false;
    float                           m_paddleDestroyedTimer = 0.0f;
    float                           m_sparkEmissionTimer = 0.0f;

    // Notifications and Status
    std::string m_statusMessage = "REBREAKER MAKER READY";
    float       m_statusTimer = 2.5f;
    Color       m_statusColor{100, 255, 160, 255};
    std::string m_currentSavePath = "Resources/levels/custom_level.txt";
    int         m_saveAsIndex = 1;

    // Audio & Visual Assets
    uint32_t m_fontId = 0;
    uint32_t m_clickSfxId = 0;
    uint32_t m_switchSfxId = 0;
    uint32_t m_shootSfxId = 0;
    uint32_t m_hitSfxId = 0;
    uint32_t m_bossLaserSfxId = 0;
    uint32_t m_paddleCrackSfxId = 0;
    float    m_pulseTimer = 0.0f;
};
