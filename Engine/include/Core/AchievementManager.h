#pragma once

#include "Core/ProfileManager.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Events/EventDispatcher.h"
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

enum class AchievementCategory : uint8_t
{
    Combat = 0,
    Endless = 1,
    Campaign = 2,
    PowerUps = 3,
    Progression = 4,
    Secret = 5,
    Count = 6
};

struct AchievementDefinition
{
    std::string         Id;
    std::string         Title;
    std::string         Description;
    std::string         ProgressUnit;
    AchievementCategory Category = AchievementCategory::Combat;
    int                 Target = 1;
    bool                IsSecret = false;
    int                 RewardCredits = 0;
    std::string         RewardCosmeticId;
};

struct AchievementState
{
    int         Progress = 0;
    bool        Unlocked = false;
    std::string UnlockedTimestamp;
};

struct AchievementToast
{
    std::string         Id;
    std::string         Title;
    std::string         Description;
    AchievementCategory Category = AchievementCategory::Combat;
    bool                IsSecret = false;
    int                 RewardCredits = 0;
    std::string         RewardCosmeticId;
    std::string         RewardCosmeticName;
};

class AchievementManager
{
public:
    static AchievementManager& Get();

    [[nodiscard]] const std::vector<AchievementDefinition>& GetDefinitions() const noexcept { return m_definitions; }
    [[nodiscard]] const AchievementDefinition*              GetDefinitionById(const std::string& id) const;
    [[nodiscard]] AchievementState                          GetState(const std::string& id) const;
    [[nodiscard]] bool                                      IsUnlocked(const std::string& id) const;

    [[nodiscard]] int   GetUnlockedCount() const;
    [[nodiscard]] int   GetTotalCount() const noexcept { return static_cast<int>(m_definitions.size()); }
    [[nodiscard]] int   GetUnlockedCountByCategory(AchievementCategory category) const;
    [[nodiscard]] int   GetTotalCountByCategory(AchievementCategory category) const;
    [[nodiscard]] float GetCompletionPercentage() const;
    [[nodiscard]] int   GetTotalRewardCreditsEarned() const;

    [[nodiscard]] static const char* GetCategoryName(AchievementCategory category) noexcept;
    [[nodiscard]] static int         GetCorePowerUpCount() noexcept { return 13; }
    [[nodiscard]] static int         GetCorePowerUpBitIndex(PowerUpType type) noexcept;

    [[nodiscard]] uint32_t GetCollectedPowerUpMask() const noexcept { return m_collectedPowerUpMask; }
    [[nodiscard]] uint32_t GetUsedUltimatesMask() const noexcept { return m_usedUltimatesMask; }
    [[nodiscard]] int      GetTotalBricksDestroyed() const noexcept { return m_totalBricksDestroyed; }
    [[nodiscard]] int      GetArenaEventsSurvived() const noexcept { return m_arenaEventsSurvived; }
    [[nodiscard]] int      GetBestSingleLaunchBricks() const noexcept { return m_bestSingleLaunchBricks; }
    [[nodiscard]] int      GetCurrentLaunchBricks() const noexcept { return m_bricksThisLaunch; }
    [[nodiscard]] int      GetMaxComboReached() const noexcept { return m_maxComboReached; }

    void               SetTrackingEnabled(bool enabled) noexcept { m_trackingEnabled = enabled; }
    [[nodiscard]] bool IsTrackingEnabled() const noexcept { return m_trackingEnabled; }

    void BindEvents(EventDispatcher& dispatcher);
    void UnbindEvents();

    void ResetSessionCounters() noexcept;
    void NotifyBallLaunched();
    void NotifyBrickHit(bool destroyed, uint32_t combo);
    void NotifyPaddleHit();
    void NotifyBallLost();
    void NotifyPowerUpTriggered(PowerUpType type);
    void NotifyUltimateActivated(UltimateAbilityType type);
    void NotifyArenaEventSurvived();
    void NotifyPaddleDamaged();
    void NotifyBossEncounterStarted();
    void NotifyBossDefeated();
    void NotifyEndlessProgress(float survivalTime, int wavesCleared);
    void NotifyEndlessRunCompleted(float survivalTime, int wavesCleared, uint32_t score);
    void NotifyStageCleared(int levelNumber, int livesRemaining);
    void NotifyWorkshopUpgraded();
    void NotifyKonamiCodeActivated();
    void EvaluateProfileMilestones();

    [[nodiscard]] bool HasPendingToast() const noexcept { return !m_pendingToasts.empty(); }
    bool               PopNextToast(AchievementToast& outToast);
    void               ClearPendingToasts();

    [[nodiscard]] std::string SerializeToJson() const;
    void                      DeserializeFromJson(const std::string& jsonStr);
    void                      ResetToDefaults();

private:
    AchievementManager();
    void InitDefinitions();
    void UpdateProgressMax(const std::string& id, int value);
    void UnlockInternal(const std::string& id);
    static std::string GetCurrentTimestampIso();
    static int         CountSetBits(uint32_t mask) noexcept;

    std::vector<AchievementDefinition>              m_definitions;
    std::unordered_map<std::string, AchievementState> m_states;
    std::deque<AchievementToast>                    m_pendingToasts;

    bool     m_trackingEnabled = true;
    int      m_bricksThisLaunch = 0;
    int      m_bestSingleLaunchBricks = 0;
    int      m_totalBricksDestroyed = 0;
    int      m_maxComboReached = 0;
    int      m_arenaEventsSurvived = 0;
    uint32_t m_collectedPowerUpMask = 0;
    uint32_t m_usedUltimatesMask = 0;
    bool     m_bossEncounterActive = false;
    bool     m_bossPaddleDamaged = false;

    ScopedSubscription m_brickHitSub;
    ScopedSubscription m_paddleHitSub;
    ScopedSubscription m_ballDeathSub;
    ScopedSubscription m_powerUpSub;
    ScopedSubscription m_arenaEventEndedSub;
};
