#pragma once

#include "Scenes/GameScene.h"
#include "Editor/LevelData.h"
#include "Editor/CustomLevelLibrary.h"
#include <string>
#include <vector>
#include <optional>

class CustomLevelPlayScene : public GameScene
{
public:
    // Single level run (does NOT chain; returns to RobostockScene on completion)
    explicit CustomLevelPlayScene(std::string singleLevelId);

    // Special Playlist run (chains up to 10 levels in order; returns to RobostockScene on completion)
    CustomLevelPlayScene(std::vector<std::string> playlistLevelIds, std::string playlistName);

    ~CustomLevelPlayScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

    [[nodiscard]] bool IsGameOver() const override;
    [[nodiscard]] bool IsLevelCleared() const override;
    [[nodiscard]] bool HasNextLevel() const override;
    [[nodiscard]] int  GetCurrentLevel() const override { return static_cast<int>(m_currentSlot); }
    [[nodiscard]] int  GetLevelCount() const override { return static_cast<int>(m_levelIds.size()); }

    void LoadLevel(int levelIndex, bool preserveStats = false) override;
    void AdvanceToNextLevel() override;
    void OnVictoryEnter() override;
    void FullReset() override;
    void OnReplayRequested() override;
    void RestorePostUltimateMusic() override;

protected:
    void RenderSceneSpecificHUD(GameContext& context) override;
    void OnPaddleShattered(GameContext& context) override;
    void OnLifeLost(GameContext& context) override;

private:
    struct TriangleProjectile
    {
        Vector2f Position{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 210.0f};
        float    Size = 16.0f;
        float    Damage = 34.0f;
    };

    struct TurretBrick
    {
        Entity EntityId = NULL_ENTITY;
        float  ShootTimer = 3.5f;
        float  Cooldown = 3.2f;
        float  ProjectileSpeed = 210.0f;
        float  Damage = 34.0f;
        bool   Aimed = false;
        int    BaseHP = 3;
    };

    [[nodiscard]] std::optional<size_t> FindNextPlayablePage(size_t startIndex) const;
    void LoadSlotLevel(size_t slotIndex, GameContext& context, bool preserveStats);
    void RebuildPlayfieldFromPage(size_t pageIdx, GameContext& context, bool withDescent);
    void ApplyCustomTheme(GameContext& context);
    void ResetTurretsAndProjectiles();
    void UpdateTurrets(float dt, GameContext& context);
    void RenderTurrets(GameContext& context) const;
    void ReturnToRobostock(GameContext& context);

    CustomLevelLibrary              m_library;
    std::vector<std::string>        m_levelIds;
    std::string                     m_playlistName;
    bool                            m_isPlaylist = false;
    size_t                          m_currentSlot = 0;

    LevelData                       m_levelData;
    size_t                          m_activePageIndex = 0;
    float                           m_pageBannerTimer = 0.0f;

    // Scheduled EventQueue & Initial Event runtime
    float                           m_levelElapsed = 0.0f;
    size_t                          m_nextQueuedEventIdx = 0;
    bool                            m_initialEventTriggered = false;

    // Campaign-fidelity turrets
    std::vector<TurretBrick>        m_turretBricks;
    std::vector<TriangleProjectile> m_turretBullets;

    // Auto-return timer on Victory so completed runs return cleanly to Vault
    bool                            m_victoryReturnActive = false;
    float                           m_victoryReturnTimer = 0.0f;

    uint32_t                        m_bossLaserSfxId = 0;
};
