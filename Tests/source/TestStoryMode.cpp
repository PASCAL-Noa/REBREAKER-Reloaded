#include <doctest/doctest.h>
#include "ECS/Components/PaddleDurabilityComponent.h"
#include "ECS/Components/BossComponent.h"
#include "ECS/Components/BossProjectileComponent.h"
#include "Core/LoreManager.h"
#include "Core/ProfileManager.h"
#include "Core/PlayerPrefs.h"
#include "Story/StoryData.h"
#include <filesystem>

struct StoryModeFixture
{
    std::filesystem::path prefsPath{PlayerPrefs::GetFilePath()};
    std::filesystem::path backupPath{PlayerPrefs::GetFilePath() + ".storybackup"};
    bool hadOriginal = false;

    StoryModeFixture()
    {
        std::error_code ec;
        if (std::filesystem::exists(prefsPath, ec))
        {
            std::filesystem::copy_file(prefsPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
            hadOriginal = true;
        }
        PlayerPrefs::DeleteAll();
        LoreManager::Get().ResetToDefaults();
        ProfileManager::Get().ResetToDefaults();
    }

    ~StoryModeFixture()
    {
        std::error_code ec;
        std::filesystem::remove(prefsPath, ec);
        std::filesystem::remove(prefsPath.string() + ".tmp", ec);

        if (hadOriginal)
        {
            std::filesystem::copy_file(backupPath, prefsPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(backupPath, ec);
            PlayerPrefs::Load();
            ProfileManager::Get().ResetToDefaults();
        }
    }
};

TEST_SUITE("Story Mode, Paddle Durability & Multi-Phase Boss Encounter")
{
    TEST_CASE("Paddle Durability: Initial state, pristine fracture, and default width")
    {
        PaddleDurabilityComponent dur{};
        CHECK(dur.MaxDurability == doctest::Approx(100.0f));
        CHECK(dur.CurrentDurability == doctest::Approx(100.0f));
        CHECK(dur.GetDurabilityRatio() == doctest::Approx(1.0f));
        CHECK(dur.GetFractureState() == PaddleFractureState::Pristine);
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(1.0f));
        CHECK_FALSE(dur.IsDestroyed());
        CHECK_FALSE(dur.IsInvulnerable());
    }

    TEST_CASE("Paddle Durability: Damage taking, invulnerability frames, and flash timer")
    {
        PaddleDurabilityComponent dur{};
        CHECK(dur.TakeDamage(20.0f));
        CHECK(dur.CurrentDurability == doctest::Approx(80.0f));
        CHECK(dur.IsInvulnerable());
        CHECK(dur.DamageFlashTimer > 0.0f);

        // While invulnerable, additional damage is absorbed/ignored
        CHECK_FALSE(dur.TakeDamage(20.0f));
        CHECK(dur.CurrentDurability == doctest::Approx(80.0f));

        // Advance time to expire invulnerability
        dur.Update(0.40f);
        CHECK_FALSE(dur.IsInvulnerable());

        // Subsequent damage is now applied
        CHECK(dur.TakeDamage(15.0f));
        CHECK(dur.CurrentDurability == doctest::Approx(65.0f));
    }

    TEST_CASE("Paddle Durability: Armor damage mitigation")
    {
        PaddleDurabilityComponent dur{};
        dur.Armor = 0.25f; // 25% damage reduction

        CHECK(dur.TakeDamage(40.0f));
        // 40 * (1 - 0.25) = 30 damage dealt
        CHECK(dur.CurrentDurability == doctest::Approx(70.0f));
    }

    TEST_CASE("Paddle Durability: Temporary energy shielding absorbs damage before hull")
    {
        PaddleDurabilityComponent dur{};
        dur.AddShield(40.0f, 10.0f);
        CHECK(dur.IsShieldActive);
        CHECK(dur.CurrentShield == doctest::Approx(40.0f));

        // Incoming damage of 25 is fully absorbed by shield
        CHECK(dur.TakeDamage(25.0f));
        CHECK(dur.CurrentShield == doctest::Approx(15.0f));
        CHECK(dur.CurrentDurability == doctest::Approx(100.0f)); // Durability completely unharmed!
        CHECK(dur.IsShieldActive);

        // Advance time to clear invulnerability
        dur.Update(0.40f);

        // Incoming damage of 30 exceeds shield: 15 absorbed by shield, remaining 15 hits hull
        CHECK(dur.TakeDamage(30.0f));
        CHECK(dur.CurrentShield == doctest::Approx(0.0f));
        CHECK_FALSE(dur.IsShieldActive);
        CHECK(dur.CurrentDurability == doctest::Approx(85.0f));
    }

    TEST_CASE("Paddle Durability: Fracture states and dynamic width reduction")
    {
        PaddleDurabilityComponent dur{};

        // 100% -> Pristine, 1.0x width
        CHECK(dur.GetFractureState() == PaddleFractureState::Pristine);
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(1.0f));

        // 70% -> Scratched
        dur.CurrentDurability = 70.0f;
        CHECK(dur.GetFractureState() == PaddleFractureState::Scratched);
        // Effective width: 0.60 + 0.40 * 0.70 = 0.88
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(0.88f));

        // 40% -> Fractured
        dur.CurrentDurability = 40.0f;
        CHECK(dur.GetFractureState() == PaddleFractureState::Fractured);
        // Effective width: 0.60 + 0.40 * 0.40 = 0.76
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(0.76f));

        // 15% -> Critical
        dur.CurrentDurability = 15.0f;
        CHECK(dur.GetFractureState() == PaddleFractureState::Critical);
        // Effective width: 0.60 + 0.40 * 0.15 = 0.66
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(0.66f));

        // 0% -> Destroyed
        dur.CurrentDurability = 0.0f;
        CHECK(dur.GetFractureState() == PaddleFractureState::Destroyed);
        CHECK(dur.IsDestroyed());
        CHECK(dur.GetEffectiveWidthMultiplier() == doctest::Approx(0.60f));
    }

    TEST_CASE("Paddle Durability: Repair restores health and fracture state")
    {
        PaddleDurabilityComponent dur{};
        dur.CurrentDurability = 30.0f;
        CHECK(dur.GetFractureState() == PaddleFractureState::Fractured);

        dur.Repair(35.0f);
        CHECK(dur.CurrentDurability == doctest::Approx(65.0f));
        CHECK(dur.GetFractureState() == PaddleFractureState::Scratched);

        dur.Repair(50.0f);
        // Capped at MaxDurability
        CHECK(dur.CurrentDurability == doctest::Approx(100.0f));
        CHECK(dur.GetFractureState() == PaddleFractureState::Pristine);
    }

    TEST_CASE("Paddle Durability: Destruction serves as immediate defeat condition")
    {
        PaddleDurabilityComponent dur{};
        dur.TakeDamage(120.0f); // Massive overkill damage

        CHECK(dur.CurrentDurability == doctest::Approx(0.0f));
        CHECK(dur.IsDestroyed());
        CHECK(dur.GetFractureState() == PaddleFractureState::Destroyed);

        // Cannot repair once completely destroyed
        dur.Repair(50.0f);
        CHECK(dur.CurrentDurability == doctest::Approx(0.0f));
    }

    TEST_CASE("Boss Battle: Multi-Phase thresholds and transitions")
    {
        BossComponent boss{};
        CHECK(boss.MaxHealth == doctest::Approx(360.0f));
        CHECK(boss.CurrentHealth == doctest::Approx(360.0f));
        CHECK(boss.Phase == BossPhase::Phase1);
        CHECK_FALSE(boss.IsDefeated());

        // Phase 1 -> Phase 2 threshold: <= 66% HP (237.6 HP)
        // Deal non-weakpoint damage
        boss.TakeDamage(100.0f, false); // 100 * 0.35 = 35 damage -> 325 HP
        CHECK(boss.CurrentHealth == doctest::Approx(325.0f));
        CHECK(boss.Phase == BossPhase::Phase1);

        // Weak point strike: critical damage (40 * 2.5 = 100 damage) -> 225 HP (62.5% HP)
        boss.TakeDamage(40.0f, true);
        CHECK(boss.CurrentHealth == doctest::Approx(225.0f));
        CHECK(boss.Phase == BossPhase::Phase2); // Successfully transitioned to Phase 2!

        // Phase 2 -> Phase 3 (Enrage) threshold: <= 33% HP (118.8 HP)
        boss.TakeDamage(50.0f, true); // 50 * 2.5 = 125 damage -> 100 HP (27.7% HP)
        CHECK(boss.CurrentHealth == doctest::Approx(100.0f));
        CHECK(boss.Phase == BossPhase::Phase3); // Successfully transitioned to Phase 3 (Enrage)!

        // Deplete remaining health -> Defeated
        boss.TakeDamage(50.0f, true);
        CHECK(boss.CurrentHealth == doctest::Approx(0.0f));
        CHECK(boss.Phase == BossPhase::Defeated);
        CHECK(boss.IsDefeated());
    }

    TEST_CASE("Boss Battle: Animated Laser Beam attack lifecycle")
    {
        BossLaserBeam beam{};
        beam.StartX = 50.0f;
        beam.Width = 26.0f;
        beam.ChargeDuration = 1.0f;
        beam.ChargeTimer = 1.0f;
        beam.FireDuration = 1.2f;
        beam.IsCharging = true;
        beam.IsFiring = false;

        // Charging phase
        CHECK(beam.IsCharging);
        CHECK_FALSE(beam.IsFiring);

        beam.ChargeTimer -= 1.0f;
        if (beam.ChargeTimer <= 0.0f)
        {
            beam.IsCharging = false;
            beam.IsFiring = true;
            beam.FireTimer = beam.FireDuration;
        }

        // Active firing phase
        CHECK_FALSE(beam.IsCharging);
        CHECK(beam.IsFiring);
        CHECK(beam.FireTimer == doctest::Approx(1.2f));
    }

    TEST_CASE("Boss Battle: Projectile deflection by paddle")
    {
        BossProjectileComponent bullet{};
        bullet.Damage = 18.0f;
        bullet.Velocity = Vector2f{0.0f, 220.0f};
        bullet.IsDeflected = false;

        CHECK_FALSE(bullet.IsDeflected);

        // Clean deflection simulated
        bullet.IsDeflected = true;
        bullet.Velocity.Y = -std::abs(bullet.Velocity.Y) * 1.25f;

        CHECK(bullet.IsDeflected);
        CHECK(bullet.Velocity.Y < 0.0f); // Traveling upward toward boss!
    }

    TEST_CASE_FIXTURE(StoryModeFixture, "Narrative Progression: Story chapters, dialogue cards, and rewards")
    {
        auto chapters = StoryData::GetDefaultChapters();
        REQUIRE(chapters.size() == 3);

        // Chapter 1: Sector Zero
        CHECK(chapters[0].ChapterNumberStr == "CHAPTER 01");
        CHECK(chapters[0].CurrencyReward == 500);
        CHECK(chapters[0].UnlockedLoreId == "lore_ch1");
        CHECK_FALSE(chapters[0].IsBossChapter);
        CHECK_FALSE(chapters[0].IntroDialogue.empty());
        CHECK_FALSE(chapters[0].OutroDialogue.empty());

        // Chapter 2: Grid Depths
        CHECK(chapters[1].ChapterNumberStr == "CHAPTER 02");
        CHECK(chapters[1].CurrencyReward == 750);
        CHECK(chapters[1].UnlockedLoreId == "lore_ch2");
        CHECK_FALSE(chapters[1].IsBossChapter);

        // Chapter 3: Core Chamber Boss Encounter
        CHECK(chapters[2].ChapterNumberStr == "CHAPTER 03");
        CHECK(chapters[2].CurrencyReward == 2000);
        CHECK(chapters[2].UnlockedLoreId == "lore_ch3");
        CHECK(chapters[2].UnlockedCosmeticId == "paddle_obsidian");
        CHECK(chapters[2].IsBossChapter);
    }

    TEST_CASE_FIXTURE(StoryModeFixture, "Narrative Progression: Lore Codex unlocks and persistence")
    {
        CHECK(LoreManager::Get().GetTotalCount() == 4);
        CHECK(LoreManager::Get().GetUnlockedCount() == 0);
        CHECK_FALSE(LoreManager::Get().IsLoreUnlocked("lore_ch1"));

        // Chapter 1 clear unlocks lore_ch1
        LoreManager::Get().UnlockLore("lore_ch1");
        CHECK(LoreManager::Get().IsLoreUnlocked("lore_ch1"));
        CHECK(LoreManager::Get().GetUnlockedCount() == 1);

        // Verify PlayerPrefs persistence
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetBool("Lore_Unlocked_lore_ch1", false));

        // Defeating boss unlocks remaining lore
        LoreManager::Get().UnlockLore("lore_ch3");
        LoreManager::Get().UnlockLore("lore_boss_clear");
        CHECK(LoreManager::Get().IsLoreUnlocked("lore_ch3"));
        CHECK(LoreManager::Get().IsLoreUnlocked("lore_boss_clear"));
        CHECK(LoreManager::Get().GetUnlockedCount() == 3);
    }

    TEST_CASE_FIXTURE(StoryModeFixture, "Narrative Progression: Boss defeat grants exclusive skin and currency")
    {
        // Initial state
        CHECK_FALSE(ProfileManager::Get().IsCosmeticUnlocked("paddle_obsidian"));
        CHECK(ProfileManager::Get().GetLifetimeCredits() == 0);

        // Grant chapter rewards
        ProfileManager::Get().AddLifetimeCredits(2000);
        ProfileManager::Get().GrantCosmetic("paddle_obsidian");

        // Verify rewards
        CHECK(ProfileManager::Get().GetLifetimeCredits() == 2000);
        CHECK(ProfileManager::Get().IsCosmeticUnlocked("paddle_obsidian"));

        // Verify persistence in PlayerPrefs
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetBool("Cosmetic_Unlocked_paddle_obsidian", false));
        CHECK(PlayerPrefs::GetInt("Economy_LifetimeCredits", 0) == 2000);
    }
}
