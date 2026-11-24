#pragma once
#include "Scenes/GameScene.h"
#include "Story/StoryData.h"
#include "ECS/Components/PaddleDurabilityComponent.h"
#include "ECS/Components/BossComponent.h"
#include "ECS/Components/BossProjectileComponent.h"
#include <vector>
#include <string>

class StoryScene : public GameScene
{
public:
    StoryScene();
    ~StoryScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    // Victory & Defeat condition overrides
    [[nodiscard]] bool IsGameOver() const override;
    [[nodiscard]] bool IsLevelCleared() const override;
    [[nodiscard]] int  GetCurrentLevel() const override { return m_currentChapterIndex + 1; }
    [[nodiscard]] int  GetLevelCount() const override { return static_cast<int>(m_chapters.size()); }
    [[nodiscard]] bool HasNextLevel() const override;
    void AdvanceToNextLevel() override;
    void StartLevelTransition() override;
    void FullReset() override;
    void OnGameOverEnter() override;
    void OnGameOverExit() override;
    void OnVictoryEnter() override;
    void OnVictoryExit() override;

    // Paddle modifiers overrides
    [[nodiscard]] float GetBasePaddleScaleX() const override;

    // Story & Chapter queries
    [[nodiscard]] int   GetCurrentChapter() const noexcept { return m_currentChapterIndex; }
    [[nodiscard]] const StoryChapter& GetCurrentStoryChapter() const { return m_chapters[m_currentChapterIndex]; }
    [[nodiscard]] bool  IsDialogueActive() const noexcept { return m_isDialogueActive; }
    [[nodiscard]] bool  IsBossEncounter() const noexcept { return m_chapters[m_currentChapterIndex].IsBossChapter; }
    [[nodiscard]] bool  IsPaddleDestroyed() const noexcept { return m_isPaddleDestroyed; }

    // Dialogue Navigation
    void AdvanceDialogue();
    void SkipDialogue();

    // Boss & Durability Testing API
    void LoadChapter(int chapterIndex);
    Entity SpawnBoss(const Vector2f& position);
    void SpawnBossBullet(const Vector2f& pos, const Vector2f& vel, float damage = 18.0f);
    void TriggerBossLaser(float startX, float chargeDuration = 1.0f, float fireDuration = 1.2f);
    [[nodiscard]] Entity GetBossEntity() const noexcept { return m_bossEntity; }
    [[nodiscard]] const std::vector<Entity>& GetBossBullets() const noexcept { return m_bossBullets; }

protected:
    void RenderSceneSpecificHUD(GameContext& context) override;
    void HandleBrickCollision(Entity entity, Entity ballEntity = NULL_ENTITY, bool isLaser = false, bool isExplosion = false, int damage = 1) override;

private:
    void InitChapters();
    void SetupChapterPlayfield(int chapterIndex);
    void StartChapterDialogue(bool isIntro);
    void CompleteChapter();
    void GrantChapterRewards();

    // Boss Combat Loop
    void UpdateBoss(float dt, GameContext& context);
    void UpdateBossAttacks(float dt, GameContext& context);
    void UpdateBossProjectiles(float dt, GameContext& context);
    void CheckBossCollisions(GameContext& context);
    void TriggerBossDefeat(GameContext& context);
    void SpawnBossShieldBricks();
    void SpawnPowerUpCapsule(const Vector2f& position, PowerUpType type);

    // Turret Bricks & Downward Triangle Projectiles
    struct TriangleProjectile
    {
        Vector2f Position{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 210.0f};
        float    Size = 16.0f;
    };

    struct TurretBrick
    {
        Entity EntityId = NULL_ENTITY;
        float  ShootTimer = 2.0f;
        float  Cooldown = 2.5f;
    };

    void UpdateTurretBricks(float dt, GameContext& context);
    void UpdateTriangleProjectiles(float dt, GameContext& context);
    void RenderTurretBricks(GameContext& context);
    void RenderTriangleProjectiles(GameContext& context);
    void TriggerPaddleExplosion(GameContext& context);

    // Paddle Durability & Hazards Loop
    void UpdatePaddleDurability(float dt, GameContext& context);
    void CheckHazardPaddleCollisions(GameContext& context);
    void TriggerPaddleDestruction(GameContext& context);

    // Rendering Sub-passes
    void RenderDialogueModal(GameContext& context);
    void RenderPaddleDurabilityOverlay(GameContext& context);
    void RenderBossHUD(GameContext& context);
    void RenderBossLasers(GameContext& context);
    void RenderBossWeakPoint(GameContext& context);

    // Chapter & Dialogue state
    int                        m_currentChapterIndex = 0;
    std::vector<StoryChapter>  m_chapters;
    bool                       m_isDialogueActive = false;
    bool                       m_isIntroDialogue = true;
    size_t                     m_dialogueCardIndex = 0;
    float                      m_dialogueCharTimer = 0.0f;
    int                        m_dialogueVisibleChars = 0;

    // Turret and Triangle Projectiles State
    std::vector<TurretBrick>        m_turretBricks;
    std::vector<TriangleProjectile> m_triangleProjectiles;
    float                           m_paddleDamageFlashTimer = 0.0f;

    // Paddle Durability state
    bool  m_isPaddleDestroyed = false;
    float m_paddleDestroyedTimer = 0.0f;
    float m_sparkEmissionTimer = 0.0f;

    // Boss state
    Entity              m_bossEntity = NULL_ENTITY;
    std::vector<Entity> m_bossBullets;
    std::vector<Entity> m_bossShieldBricks;
    float               m_bossLaserSweepTimer = 0.0f;
    float               m_bossDefeatCinematicTimer = 0.0f;
    int                 m_bossExplosionIndex = 0;
    bool                m_chapterRewardsGranted = false;
    bool                m_bossDefeated = false;

    // Textures & Audio
    uint32_t m_bossChassisTexId = 0;
    uint32_t m_bossCoreTexId = 0;
    uint32_t m_bossLaserSfxId = 0;
    uint32_t m_bossWarningSfxId = 0;
    uint32_t m_bossHitSfxId = 0;
    uint32_t m_bossDefeatSfxId = 0;
    uint32_t m_dialogueBlipSfxId = 0;
    uint32_t m_paddleCrackSfxId = 0;
    uint32_t m_shieldAbsorbSfxId = 0;
    uint32_t m_repairSfxId = 0;
};
