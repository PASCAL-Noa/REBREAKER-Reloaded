#include "Core/AchievementManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/json.hpp"
#include "Events/ArenaEvents.h"
#include "Events/GameplayEvents.h"
#include "PowerUps/PowerUpConfig.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace
{
    int CountWorkshopUpgradesPurchased()
    {
        int total = 0;
        const PowerUpType types[] = {
            PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::ShrinkPaddle,
            PowerUpType::Storm, PowerUpType::TempoBall, PowerUpType::ExtraLife,
            PowerUpType::BigBall, PowerUpType::FireBall, PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall,
            PowerUpType::ItemRegen
        };
        for (PowerUpType t : types)
        {
            total += std::max(0, PowerUpManager::Get().GetPersistedLevel(t) - 1);
        }
        return total;
    }
}

AchievementManager& AchievementManager::Get()
{
    static AchievementManager instance;
    return instance;
}

AchievementManager::AchievementManager()
{
    InitDefinitions();
}

void AchievementManager::InitDefinitions()
{
    m_definitions = {
        {
            "single_launch_50",
            "CHAIN REACTION",
            "Hit 50 bricks in a single ball launch.",
            "BRICKS",
            AchievementCategory::Combat,
            50,
            false,
            350,
            ""
        },
        {
            "combo_25",
            "RHYTHM BREAKER",
            "Reach a 25x brick combo multiplier.",
            "COMBO",
            AchievementCategory::Combat,
            25,
            false,
            150,
            ""
        },
        {
            "combo_50",
            "OVERDRIVE FLOW",
            "Reach a 50x brick combo multiplier.",
            "COMBO",
            AchievementCategory::Combat,
            50,
            false,
            300,
            ""
        },
        {
            "brick_demolisher",
            "WALL BREAKER",
            "Destroy 1,000 bricks across all modes.",
            "BRICKS",
            AchievementCategory::Combat,
            1000,
            false,
            250,
            ""
        },
        {
            "arena_survivor",
            "STORM RIDER",
            "Survive 10 dynamic Arena Events.",
            "EVENTS",
            AchievementCategory::Combat,
            10,
            false,
            200,
            ""
        },
        {
            "endless_10_min",
            "UNSTOPPABLE",
            "Survive 10 minutes in Endless Mode.",
            "SEC",
            AchievementCategory::Endless,
            600,
            false,
            500,
            ""
        },
        {
            "endless_wave_5",
            "ENDLESS INITIATE",
            "Reach Wave 5 in Endless Mode.",
            "WAVES",
            AchievementCategory::Endless,
            5,
            false,
            150,
            "color_pri_obsidian"
        },
        {
            "endless_wave_8",
            "HIGH VOLTAGE",
            "Reach Wave 8 in Endless Mode.",
            "WAVES",
            AchievementCategory::Endless,
            8,
            false,
            250,
            "trail_void_lightning"
        },
        {
            "endless_wave_10",
            "PRISM ASCENDANT",
            "Reach Wave 10 in Endless Mode.",
            "WAVES",
            AchievementCategory::Endless,
            10,
            false,
            400,
            "paddle_prism_wing"
        },
        {
            "endless_veteran",
            "LOOP VETERAN",
            "Complete 5 runs in Endless Mode.",
            "RUNS",
            AchievementCategory::Endless,
            5,
            false,
            200,
            "death_matrix_glitch"
        },
        {
            "stage_1_clear",
            "FIRST BREACH",
            "Clear Stage 1 in Arcade or Campaign.",
            "STAGE",
            AchievementCategory::Campaign,
            1,
            false,
            100,
            "color_sec_teal"
        },
        {
            "stage_3_clear",
            "SECTOR CLEARED",
            "Clear Stage 3 in Arcade or Campaign.",
            "STAGES",
            AchievementCategory::Campaign,
            3,
            false,
            200,
            "color_sec_crimson"
        },
        {
            "boss_defeat_omega",
            "OMEGA SLAYER",
            "Defeat OMEGA-01 in Campaign Chapter 3.",
            "BOSS",
            AchievementCategory::Campaign,
            1,
            false,
            300,
            "paddle_obsidian"
        },
        {
            "boss_flawless",
            "UNTOUCHABLE",
            "Clear a boss without taking paddle damage.",
            "BOSS",
            AchievementCategory::Campaign,
            1,
            false,
            600,
            ""
        },
        {
            "all_powerups",
            "FULL ARSENAL",
            "Trigger all 13 core power-ups.",
            "TYPES",
            AchievementCategory::PowerUps,
            13,
            false,
            450,
            ""
        },
        {
            "ultimate_master",
            "ULTIMATE ARSENAL",
            "Unleash all 3 Ultimate abilities at least once.",
            "ULTS",
            AchievementCategory::PowerUps,
            3,
            false,
            250,
            ""
        },
        {
            "workshop_tinkerer",
            "ROBO ENGINEER",
            "Purchase 5 Workshop power-up upgrades.",
            "UPGRADES",
            AchievementCategory::PowerUps,
            5,
            false,
            150,
            "ball_steampunk_gear"
        },
        {
            "score_25k",
            "SCORE HUNTER",
            "Reach 25,000 lifetime points.",
            "PTS",
            AchievementCategory::Progression,
            25000,
            false,
            250,
            "ball_void_eye"
        },
        {
            "score_50k",
            "SOLAR LEGEND",
            "Reach 50,000 lifetime points.",
            "PTS",
            AchievementCategory::Progression,
            50000,
            false,
            500,
            "color_pri_overdrive"
        },
        {
            "credits_1k",
            "CREDITS MAGNATE",
            "Earn 1,000 lifetime credits.",
            "CR",
            AchievementCategory::Progression,
            1000,
            false,
            150,
            "color_sec_aurora"
        },
        {
            "secret_konami",
            "OLD SCHOOL CODE",
            "Enter the legendary secret code.",
            "CODE",
            AchievementCategory::Secret,
            1,
            true,
            250,
            ""
        },
        {
            "secret_last_stand",
            "LAST STAND",
            "Clear a stage with only 1 life remaining.",
            "CLEAR",
            AchievementCategory::Secret,
            1,
            true,
            300,
            ""
        }
    };

    m_states.clear();
    for (const auto& def : m_definitions)
    {
        m_states[def.Id] = AchievementState{};
    }
}

const AchievementDefinition* AchievementManager::GetDefinitionById(const std::string& id) const
{
    for (const auto& def : m_definitions)
    {
        if (def.Id == id)
        {
            return &def;
        }
    }
    return nullptr;
}

AchievementState AchievementManager::GetState(const std::string& id) const
{
    auto it = m_states.find(id);
    if (it != m_states.end())
    {
        return it->second;
    }
    return AchievementState{};
}

bool AchievementManager::IsUnlocked(const std::string& id) const
{
    auto it = m_states.find(id);
    return it != m_states.end() && it->second.Unlocked;
}

int AchievementManager::GetUnlockedCount() const
{
    int count = 0;
    for (const auto& def : m_definitions)
    {
        if (IsUnlocked(def.Id)) ++count;
    }
    return count;
}

int AchievementManager::GetUnlockedCountByCategory(AchievementCategory category) const
{
    int count = 0;
    for (const auto& def : m_definitions)
    {
        if (def.Category == category && IsUnlocked(def.Id)) ++count;
    }
    return count;
}

int AchievementManager::GetTotalCountByCategory(AchievementCategory category) const
{
    int count = 0;
    for (const auto& def : m_definitions)
    {
        if (def.Category == category) ++count;
    }
    return count;
}

float AchievementManager::GetCompletionPercentage() const
{
    if (m_definitions.empty()) return 0.0f;
    return (static_cast<float>(GetUnlockedCount()) / static_cast<float>(m_definitions.size())) * 100.0f;
}

int AchievementManager::GetTotalRewardCreditsEarned() const
{
    int total = 0;
    for (const auto& def : m_definitions)
    {
        if (IsUnlocked(def.Id))
        {
            total += def.RewardCredits;
        }
    }
    return total;
}

const char* AchievementManager::GetCategoryName(AchievementCategory category) noexcept
{
    switch (category)
    {
        case AchievementCategory::Combat:      return "COMBAT";
        case AchievementCategory::Endless:     return "ENDLESS";
        case AchievementCategory::Campaign:    return "CAMPAIGN";
        case AchievementCategory::PowerUps:    return "POWER-UPS";
        case AchievementCategory::Progression: return "PROGRESS";
        case AchievementCategory::Secret:      return "SECRET";
        default:                               return "ALL";
    }
}

int AchievementManager::GetCorePowerUpBitIndex(PowerUpType type) noexcept
{
    switch (type)
    {
        case PowerUpType::MultiBall:     return 0;
        case PowerUpType::ExpandPaddle:  return 1;
        case PowerUpType::ShrinkPaddle:  return 2;
        case PowerUpType::Storm:         return 3;
        case PowerUpType::TempoBall:     return 4;
        case PowerUpType::ExtraLife:     return 5;
        case PowerUpType::BigBall:       return 6;
        case PowerUpType::FireBall:      return 7;
        case PowerUpType::StickyPaddle:  return 8;
        case PowerUpType::BottomBarrier: return 9;
        case PowerUpType::MagnetPaddle:  return 10;
        case PowerUpType::GhostBall:     return 11;
        case PowerUpType::ItemRegen:     return 12;
        default:                         return -1;
    }
}

int AchievementManager::CountSetBits(uint32_t mask) noexcept
{
    int count = 0;
    while (mask != 0)
    {
        mask &= (mask - 1);
        ++count;
    }
    return count;
}

std::string AchievementManager::GetCurrentTimestampIso()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M");
    return oss.str();
}

void AchievementManager::BindEvents(EventDispatcher& dispatcher)
{
    m_brickHitSub = dispatcher.SubscribeScoped<BrickHitEvent>([this](const BrickHitEvent& e) {
        NotifyBrickHit(e.IsDestroyed, e.Combo);
    });
    m_paddleHitSub = dispatcher.SubscribeScoped<PaddleHitEvent>([this](const PaddleHitEvent&) {
        NotifyPaddleHit();
    });
    m_ballDeathSub = dispatcher.SubscribeScoped<BallDeathEvent>([this](const BallDeathEvent&) {
        NotifyBallLost();
    });
    m_powerUpSub = dispatcher.SubscribeScoped<PowerUpEvent>([this](const PowerUpEvent& e) {
        if (e.PowerUpEntity != NULL_ENTITY)
        {
            NotifyPowerUpTriggered(e.Type);
        }
    });
    m_arenaEventEndedSub = dispatcher.SubscribeScoped<ArenaEventEndedEvent>([this](const ArenaEventEndedEvent&) {
        NotifyArenaEventSurvived();
    });
}

void AchievementManager::UnbindEvents()
{
    m_brickHitSub = {};
    m_paddleHitSub = {};
    m_ballDeathSub = {};
    m_powerUpSub = {};
    m_arenaEventEndedSub = {};
}

void AchievementManager::ResetSessionCounters() noexcept
{
    m_bricksThisLaunch = 0;
    m_bossEncounterActive = false;
    m_bossPaddleDamaged = false;
}

void AchievementManager::NotifyBallLaunched()
{
    if (!m_trackingEnabled) return;
    m_bricksThisLaunch = 0;
}

void AchievementManager::NotifyBrickHit(bool destroyed, uint32_t combo)
{
    if (!m_trackingEnabled) return;

    ++m_bricksThisLaunch;
    if (m_bricksThisLaunch > m_bestSingleLaunchBricks)
    {
        m_bestSingleLaunchBricks = m_bricksThisLaunch;
    }
    UpdateProgressMax("single_launch_50", m_bestSingleLaunchBricks);

    if (destroyed)
    {
        ++m_totalBricksDestroyed;
        UpdateProgressMax("brick_demolisher", m_totalBricksDestroyed);
    }

    const int comboInt = static_cast<int>(combo);
    if (comboInt > m_maxComboReached)
    {
        m_maxComboReached = comboInt;
    }
    UpdateProgressMax("combo_25", m_maxComboReached);
    UpdateProgressMax("combo_50", m_maxComboReached);
}

void AchievementManager::NotifyPaddleHit()
{
    if (!m_trackingEnabled) return;
    m_bricksThisLaunch = 0;
}

void AchievementManager::NotifyBallLost()
{
    if (!m_trackingEnabled) return;
    m_bricksThisLaunch = 0;
}

void AchievementManager::NotifyPowerUpTriggered(PowerUpType type)
{
    if (!m_trackingEnabled) return;
    const int bit = GetCorePowerUpBitIndex(type);
    if (bit < 0) return;

    m_collectedPowerUpMask |= (1u << static_cast<uint32_t>(bit));
    UpdateProgressMax("all_powerups", CountSetBits(m_collectedPowerUpMask));
}

void AchievementManager::NotifyUltimateActivated(UltimateAbilityType type)
{
    if (!m_trackingEnabled) return;
    const int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(UltimateAbilityType::Count)) return;

    m_usedUltimatesMask |= (1u << static_cast<uint32_t>(idx));
    UpdateProgressMax("ultimate_master", CountSetBits(m_usedUltimatesMask));
}

void AchievementManager::NotifyArenaEventSurvived()
{
    if (!m_trackingEnabled) return;
    ++m_arenaEventsSurvived;
    UpdateProgressMax("arena_survivor", m_arenaEventsSurvived);
}

void AchievementManager::NotifyPaddleDamaged()
{
    if (!m_trackingEnabled) return;
    if (m_bossEncounterActive)
    {
        m_bossPaddleDamaged = true;
    }
}

void AchievementManager::NotifyBossEncounterStarted()
{
    if (!m_trackingEnabled) return;
    m_bossEncounterActive = true;
    m_bossPaddleDamaged = false;
}

void AchievementManager::NotifyBossDefeated()
{
    if (!m_trackingEnabled) return;
    UpdateProgressMax("boss_defeat_omega", 1);
    if (m_bossEncounterActive && !m_bossPaddleDamaged)
    {
        UpdateProgressMax("boss_flawless", 1);
    }
    m_bossEncounterActive = false;
}

void AchievementManager::NotifyEndlessProgress(float survivalTime, int wavesCleared)
{
    if (!m_trackingEnabled) return;
    const int seconds = std::max(0, static_cast<int>(survivalTime));
    UpdateProgressMax("endless_10_min", seconds);
    UpdateProgressMax("endless_wave_5", std::max(0, wavesCleared));
    UpdateProgressMax("endless_wave_8", std::max(0, wavesCleared));
    UpdateProgressMax("endless_wave_10", std::max(0, wavesCleared));
}

void AchievementManager::NotifyEndlessRunCompleted(float survivalTime, int wavesCleared, uint32_t /*score*/)
{
    if (!m_trackingEnabled) return;
    NotifyEndlessProgress(survivalTime, wavesCleared);
    EvaluateProfileMilestones();
}

void AchievementManager::NotifyStageCleared(int levelNumber, int livesRemaining)
{
    if (!m_trackingEnabled) return;
    const int highestCleared = std::max(levelNumber, PlayerPrefs::GetInt("HighestClearedLevel", 0));
    UpdateProgressMax("stage_1_clear", highestCleared);
    UpdateProgressMax("stage_3_clear", highestCleared);

    if (livesRemaining == 1)
    {
        UpdateProgressMax("secret_last_stand", 1);
    }

    EvaluateProfileMilestones();
}

void AchievementManager::NotifyWorkshopUpgraded()
{
    UpdateProgressMax("workshop_tinkerer", CountWorkshopUpgradesPurchased());
}

void AchievementManager::NotifyKonamiCodeActivated()
{
    UpdateProgressMax("secret_konami", 1);
}

void AchievementManager::EvaluateProfileMilestones()
{
    const auto& pm = ProfileManager::Get();
    const int highestCleared = std::max(0, PlayerPrefs::GetInt("HighestClearedLevel", 0));
    const int storyChapter = std::max(0, PlayerPrefs::GetInt("Story_CompletedChapter", 0));

    UpdateProgressMax("stage_1_clear", highestCleared);
    UpdateProgressMax("stage_3_clear", highestCleared);
    if (storyChapter >= 3 || pm.IsCosmeticUnlocked("paddle_obsidian"))
    {
        UpdateProgressMax("boss_defeat_omega", 1);
    }

    UpdateProgressMax("endless_10_min", static_cast<int>(pm.GetEndlessBestTime()));
    UpdateProgressMax("endless_wave_5", pm.GetEndlessBestWaves());
    UpdateProgressMax("endless_wave_8", pm.GetEndlessBestWaves());
    UpdateProgressMax("endless_wave_10", pm.GetEndlessBestWaves());
    UpdateProgressMax("endless_veteran", pm.GetEndlessTotalRuns());

    UpdateProgressMax("workshop_tinkerer", CountWorkshopUpgradesPurchased());
    UpdateProgressMax("score_25k", static_cast<int>(pm.GetLifetimeScore()));
    UpdateProgressMax("score_50k", static_cast<int>(pm.GetLifetimeScore()));
    UpdateProgressMax("credits_1k", static_cast<int>(pm.GetLifetimeCredits()));
}

void AchievementManager::UpdateProgressMax(const std::string& id, int value)
{
    const AchievementDefinition* def = GetDefinitionById(id);
    if (!def) return;

    auto& state = m_states[id];
    const int clamped = std::clamp(value, 0, def->Target);
    if (clamped > state.Progress)
    {
        state.Progress = clamped;
    }

    if (!state.Unlocked && state.Progress >= def->Target)
    {
        UnlockInternal(id);
    }
}

void AchievementManager::UnlockInternal(const std::string& id)
{
    const AchievementDefinition* def = GetDefinitionById(id);
    if (!def) return;

    auto& state = m_states[id];
    if (state.Unlocked) return;

    state.Progress = def->Target;
    state.Unlocked = true;
    state.UnlockedTimestamp = GetCurrentTimestampIso();

    auto& pm = ProfileManager::Get();
    std::string cosmeticName;
    if (!def->RewardCosmeticId.empty())
    {
        if (const CosmeticItem* item = pm.GetCosmeticById(def->RewardCosmeticId))
        {
            cosmeticName = item->Name;
        }
        if (!pm.IsCosmeticUnlocked(def->RewardCosmeticId))
        {
            pm.GrantCosmetic(def->RewardCosmeticId);
        }
    }

    if (def->RewardCredits > 0)
    {
        PowerUpManager::Get().AddCoins(def->RewardCredits);
        const int curLifetime = std::max(0, PlayerPrefs::GetInt("Economy_LifetimeCredits", 0));
        PlayerPrefs::SetInt("Economy_LifetimeCredits", curLifetime + def->RewardCredits);
    }

    AchievementToast toast;
    toast.Id = def->Id;
    toast.Title = def->Title;
    toast.Description = def->Description;
    toast.Category = def->Category;
    toast.IsSecret = def->IsSecret;
    toast.RewardCredits = def->RewardCredits;
    toast.RewardCosmeticId = def->RewardCosmeticId;
    toast.RewardCosmeticName = cosmeticName;
    m_pendingToasts.push_back(std::move(toast));

    PlayerPrefs::Save();
    pm.SaveProfile();
}

bool AchievementManager::PopNextToast(AchievementToast& outToast)
{
    if (m_pendingToasts.empty()) return false;
    outToast = std::move(m_pendingToasts.front());
    m_pendingToasts.pop_front();
    return true;
}

void AchievementManager::ClearPendingToasts()
{
    m_pendingToasts.clear();
}

std::string AchievementManager::SerializeToJson() const
{
    json root = json::object();
    root["bestSingleLaunchBricks"] = m_bestSingleLaunchBricks;
    root["totalBricksDestroyed"] = m_totalBricksDestroyed;
    root["maxComboReached"] = m_maxComboReached;
    root["arenaEventsSurvived"] = m_arenaEventsSurvived;
    root["collectedPowerUpMask"] = m_collectedPowerUpMask;
    root["usedUltimatesMask"] = m_usedUltimatesMask;

    json items = json::object();
    for (const auto& def : m_definitions)
    {
        const AchievementState st = GetState(def.Id);
        items[def.Id] = {
            {"progress", st.Progress},
            {"unlocked", st.Unlocked},
            {"unlockedAt", st.UnlockedTimestamp}
        };
    }
    root["items"] = items;
    return root.dump();
}

void AchievementManager::DeserializeFromJson(const std::string& jsonStr)
{
    for (const auto& def : m_definitions)
    {
        m_states[def.Id] = AchievementState{};
    }
    m_bestSingleLaunchBricks = 0;
    m_totalBricksDestroyed = 0;
    m_maxComboReached = 0;
    m_arenaEventsSurvived = 0;
    m_collectedPowerUpMask = 0;
    m_usedUltimatesMask = 0;

    if (jsonStr.empty()) return;

    try
    {
        json root = json::parse(jsonStr);
        if (!root.is_object()) return;

        if (root.contains("bestSingleLaunchBricks") && root["bestSingleLaunchBricks"].is_number_integer())
            m_bestSingleLaunchBricks = std::max(0, root["bestSingleLaunchBricks"].get<int>());
        if (root.contains("totalBricksDestroyed") && root["totalBricksDestroyed"].is_number_integer())
            m_totalBricksDestroyed = std::max(0, root["totalBricksDestroyed"].get<int>());
        if (root.contains("maxComboReached") && root["maxComboReached"].is_number_integer())
            m_maxComboReached = std::max(0, root["maxComboReached"].get<int>());
        if (root.contains("arenaEventsSurvived") && root["arenaEventsSurvived"].is_number_integer())
            m_arenaEventsSurvived = std::max(0, root["arenaEventsSurvived"].get<int>());
        if (root.contains("collectedPowerUpMask") && root["collectedPowerUpMask"].is_number_unsigned())
            m_collectedPowerUpMask = root["collectedPowerUpMask"].get<uint32_t>();
        else if (root.contains("collectedPowerUpMask") && root["collectedPowerUpMask"].is_number_integer())
            m_collectedPowerUpMask = static_cast<uint32_t>(std::max(0, root["collectedPowerUpMask"].get<int>()));
        if (root.contains("usedUltimatesMask") && root["usedUltimatesMask"].is_number_unsigned())
            m_usedUltimatesMask = root["usedUltimatesMask"].get<uint32_t>();
        else if (root.contains("usedUltimatesMask") && root["usedUltimatesMask"].is_number_integer())
            m_usedUltimatesMask = static_cast<uint32_t>(std::max(0, root["usedUltimatesMask"].get<int>()));

        if (root.contains("items") && root["items"].is_object())
        {
            const auto& items = root["items"];
            for (const auto& def : m_definitions)
            {
                if (!items.contains(def.Id) || !items[def.Id].is_object()) continue;
                const auto& entry = items[def.Id];
                AchievementState st{};
                if (entry.contains("progress") && entry["progress"].is_number_integer())
                {
                    st.Progress = std::clamp(entry["progress"].get<int>(), 0, def.Target);
                }
                if (entry.contains("unlocked") && entry["unlocked"].is_boolean())
                {
                    st.Unlocked = entry["unlocked"].get<bool>();
                }
                if (entry.contains("unlockedAt") && entry["unlockedAt"].is_string())
                {
                    st.UnlockedTimestamp = entry["unlockedAt"].get<std::string>();
                }
                if (st.Unlocked)
                {
                    st.Progress = def.Target;
                }
                m_states[def.Id] = st;
            }
        }
    }
    catch (...)
    {
    }
}

void AchievementManager::ResetToDefaults()
{
    for (const auto& def : m_definitions)
    {
        m_states[def.Id] = AchievementState{};
    }
    m_pendingToasts.clear();
    m_bricksThisLaunch = 0;
    m_bestSingleLaunchBricks = 0;
    m_totalBricksDestroyed = 0;
    m_maxComboReached = 0;
    m_arenaEventsSurvived = 0;
    m_collectedPowerUpMask = 0;
    m_usedUltimatesMask = 0;
    m_bossEncounterActive = false;
    m_bossPaddleDamaged = false;
}
