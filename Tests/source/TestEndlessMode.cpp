#include <doctest/doctest.h>
#include "Core/ProfileManager.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/PlayerPrefs.h"
#include "Endless/RogueliteDraft.h"
#include "Generators/ProceduralEndlessGenerator.h"
#include "Factories/BrickFactory.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <set>

struct EndlessModeTestFixture
{
    std::filesystem::path prefsPath{PlayerPrefs::GetFilePath()};
    std::filesystem::path backupPath{PlayerPrefs::GetFilePath() + ".endlessbackup"};
    bool hadOriginal = false;

    EndlessModeTestFixture()
    {
        std::error_code ec;
        if (std::filesystem::exists(prefsPath, ec))
        {
            std::filesystem::copy_file(prefsPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
            hadOriginal = true;
        }
        PlayerPrefs::DeleteAll();
        ProfileManager::Get().ResetToDefaults();
        PowerUpManager::Get().ResetToDefaults();
    }

    ~EndlessModeTestFixture()
    {
        std::error_code ec;
        std::filesystem::remove(prefsPath, ec);
        std::filesystem::remove(prefsPath.string() + ".tmp", ec);

        if (hadOriginal)
        {
            std::filesystem::copy_file(backupPath, prefsPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(backupPath, ec);
            PlayerPrefs::Load();
            PowerUpManager::Get().LoadFromPrefs();
        }
    }
};

TEST_SUITE("Endless Mode Progression & Roguelite Mechanics Tests")
{
    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless statistics initial defaults")
    {
        CHECK(ProfileManager::Get().GetEndlessHighScore() == 0);
        CHECK(ProfileManager::Get().GetEndlessBestTime() == doctest::Approx(0.0f));
        CHECK(ProfileManager::Get().GetEndlessBestTimeFormatted() == "00:00");
        CHECK(ProfileManager::Get().GetEndlessBestWaves() == 0);
        CHECK(ProfileManager::Get().GetEndlessTotalRuns() == 0);
        CHECK(ProfileManager::Get().GetEndlessTotalWaves() == 0);
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless time formatting MM:SS")
    {
        ProfileManager::Get().RecordEndlessRun(0.0f, 0, 0);
        CHECK(ProfileManager::Get().GetEndlessBestTimeFormatted() == "00:00");

        ProfileManager::Get().RecordEndlessRun(59.0f, 1, 100);
        CHECK(ProfileManager::Get().GetEndlessBestTimeFormatted() == "00:59");

        ProfileManager::Get().RecordEndlessRun(75.0f, 2, 200);
        CHECK(ProfileManager::Get().GetEndlessBestTimeFormatted() == "01:15");

        ProfileManager::Get().RecordEndlessRun(605.0f, 10, 5000);
        CHECK(ProfileManager::Get().GetEndlessBestTimeFormatted() == "10:05");
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless run recording updates records, totals, and awards credits")
    {
        PowerUpManager::Get().SpendCoins(PowerUpManager::Get().GetCoins());

        // Run 1: 120s survival, 3 waves, 15000 score
        ProfileManager::Get().RecordEndlessRun(120.0f, 3, 15000);

        CHECK(ProfileManager::Get().GetEndlessHighScore() == 15000);
        CHECK(ProfileManager::Get().GetEndlessBestTime() == doctest::Approx(120.0f));
        CHECK(ProfileManager::Get().GetEndlessBestWaves() == 3);
        CHECK(ProfileManager::Get().GetEndlessTotalRuns() == 1);
        CHECK(ProfileManager::Get().GetEndlessTotalWaves() == 3);

        // Credits earned = 15000 / 50 = 300 CR
        CHECK(PowerUpManager::Get().GetCoins() == 300);

        // Run 2: Shorter survival & lower score, but higher waves
        ProfileManager::Get().RecordEndlessRun(90.0f, 5, 8000);

        // High score & best time should remain unchanged (records maintained)
        CHECK(ProfileManager::Get().GetEndlessHighScore() == 15000);
        CHECK(ProfileManager::Get().GetEndlessBestTime() == doctest::Approx(120.0f));
        // Best waves should update to 5
        CHECK(ProfileManager::Get().GetEndlessBestWaves() == 5);
        // Totals incremented
        CHECK(ProfileManager::Get().GetEndlessTotalRuns() == 2);
        CHECK(ProfileManager::Get().GetEndlessTotalWaves() == 8);

        // Additional credits: 8000 / 50 = 160 CR -> Total 460 CR
        CHECK(PowerUpManager::Get().GetCoins() == 460);
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless perks tier limits and costs")
    {
        CHECK(ProfileManager::Get().GetEndlessPerkMaxLevel(EndlessPerkType::InitialRerolls) == 3);
        CHECK(ProfileManager::Get().GetEndlessPerkMaxLevel(EndlessPerkType::ShieldRecovery) == 3);
        CHECK(ProfileManager::Get().GetEndlessPerkMaxLevel(EndlessPerkType::ScoreMultiplier) == 5);
        CHECK(ProfileManager::Get().GetEndlessPerkMaxLevel(EndlessPerkType::StartingLives) == 2);

        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls) == 0);
        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ShieldRecovery) == 0);
        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::ScoreMultiplier) == 0);
        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::StartingLives) == 0);

        // Base costs at level 0
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::InitialRerolls) == 200);
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::ShieldRecovery) == 300);
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::ScoreMultiplier) == 150);
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::StartingLives) == 400);
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless perks upgrade transaction and affordability")
    {
        PowerUpManager::Get().SpendCoins(PowerUpManager::Get().GetCoins());
        PowerUpManager::Get().AddCoins(350);

        // Initial Rerolls costs 200 CR at level 0
        CHECK(ProfileManager::Get().CanUpgradeEndlessPerk(EndlessPerkType::InitialRerolls) == true);
        bool upgraded = ProfileManager::Get().UpgradeEndlessPerk(EndlessPerkType::InitialRerolls);
        CHECK(upgraded == true);
        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls) == 1);
        CHECK(PowerUpManager::Get().GetCoins() == 150);

        // Level 1 cost: 200 * (1 + 1) = 400 CR. Balance is 150 CR -> insufficient
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::InitialRerolls) == 400);
        CHECK(ProfileManager::Get().CanUpgradeEndlessPerk(EndlessPerkType::InitialRerolls) == false);
        CHECK(ProfileManager::Get().UpgradeEndlessPerk(EndlessPerkType::InitialRerolls) == false);
        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls) == 1);
        CHECK(PowerUpManager::Get().GetCoins() == 150);

        // Add funds and upgrade to max level
        PowerUpManager::Get().AddCoins(50000);
        while (ProfileManager::Get().CanUpgradeEndlessPerk(EndlessPerkType::InitialRerolls))
        {
            ProfileManager::Get().UpgradeEndlessPerk(EndlessPerkType::InitialRerolls);
        }

        CHECK(ProfileManager::Get().GetEndlessPerkLevel(EndlessPerkType::InitialRerolls) == 3);
        CHECK(ProfileManager::Get().CanUpgradeEndlessPerk(EndlessPerkType::InitialRerolls) == false);
        CHECK(ProfileManager::Get().GetEndlessPerkUpgradeCost(EndlessPerkType::InitialRerolls) == 0);
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless perks metadata and names")
    {
        CHECK(ProfileManager::GetEndlessPerkName(EndlessPerkType::InitialRerolls) == "DRAFT REROLLS");
        CHECK(ProfileManager::GetEndlessPerkName(EndlessPerkType::ShieldRecovery) == "SHIELD RECOVERY");
        CHECK(ProfileManager::GetEndlessPerkName(EndlessPerkType::ScoreMultiplier) == "SCORE OVERDRIVE");
        CHECK(ProfileManager::GetEndlessPerkName(EndlessPerkType::StartingLives) == "REINFORCED CHASSIS");

        CHECK_FALSE(ProfileManager::GetEndlessPerkDescription(EndlessPerkType::InitialRerolls).empty());
        CHECK_FALSE(ProfileManager::GetEndlessPerkTierInfo(EndlessPerkType::InitialRerolls, 2).empty());
    }

    TEST_CASE("Endless Procedural wave creep interval scaling math")
    {
        // Wave interval scaling: 4.5f * (0.95f ^ (wave - 1)), clamped to minimum 1.8f
        auto getCreepInterval = [](int wave) -> float {
            float interval = 4.5f * std::pow(0.95f, static_cast<float>(std::max(0, wave - 1)));
            return std::max(1.8f, interval);
        };

        CHECK(getCreepInterval(1) == doctest::Approx(4.5f));
        CHECK(getCreepInterval(2) < 4.5f);
        CHECK(getCreepInterval(10) < getCreepInterval(2));
        CHECK(getCreepInterval(100) == doctest::Approx(1.8f));
    }

    TEST_CASE("Endless Procedural dynamic event hazard interval scaling math")
    {
        // Event interval: 40.0f * (0.93f ^ (wave - 1)), clamped to minimum 15.0f
        auto getEventInterval = [](int wave) -> float {
            float interval = 40.0f * std::pow(0.93f, static_cast<float>(std::max(0, wave - 1)));
            return std::max(15.0f, interval);
        };

        CHECK(getEventInterval(1) == doctest::Approx(40.0f));
        CHECK(getEventInterval(5) < 40.0f);
        CHECK(getEventInterval(50) == doctest::Approx(15.0f));
    }

    TEST_CASE("Roguelite Perk Rarity categorization")
    {
        // Common perks (4)
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::TitanPaddle) == PerkRarity::Common);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::HyperDrive) == PerkRarity::Common);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::ScoreFrenzy) == PerkRarity::Common);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::ChronoStasis) == PerkRarity::Common);

        // Uncommon perks (5)
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::KineticCore) == PerkRarity::Uncommon);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::SplitStream) == PerkRarity::Uncommon);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::MagneticTether) == PerkRarity::Uncommon);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::JackpotCredits) == PerkRarity::Uncommon);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::ShieldSiphon) == PerkRarity::Uncommon);

        // Rare perks (4)
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::ReinforcedHull) == PerkRarity::Rare);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::ThermalRounds) == PerkRarity::Rare);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::EmergencyShield) == PerkRarity::Rare);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::OverdriveCapacitor) == PerkRarity::Rare);

        // Epic perks (4)
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::HeavyImpact) == PerkRarity::Epic);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::LightningRod) == PerkRarity::Epic);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::PlasmaField) == PerkRarity::Epic);
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::VortexPulse) == PerkRarity::Epic);

        // Legendary perks (1)
        CHECK(RogueliteDraftManager::GetPerkRarity(RoguelitePerkType::InstantOvercharge) == PerkRarity::Legendary);
    }

    TEST_CASE("Roguelite Card metadata and rarity styling")
    {
        for (int i = 0; i < static_cast<int>(RoguelitePerkType::Count); ++i)
        {
            auto perk = static_cast<RoguelitePerkType>(i);
            auto card = RogueliteDraftManager::GetCardInfo(perk);

            CHECK(card.Type == perk);
            CHECK(card.Rarity == RogueliteDraftManager::GetPerkRarity(perk));
            CHECK_FALSE(card.Title.empty());
            CHECK_FALSE(card.Category.empty());
            CHECK_FALSE(card.Description.empty());
            CHECK_FALSE(card.RarityName.empty());
            CHECK(card.CardColor.a == 255);
            CHECK(card.RarityColor.a == 255);
        }
    }

    TEST_CASE("Roguelite Draft: PeriodicSupply vs WaveClear rarity probability distributions")
    {
        // 1. Periodic Supply (Every 1 minute): Standard / lower rarity
        // Common: 50%, Uncommon: 35%, Rare: 12%, Epic: 3%, Legendary: 0%
        int periodicLegendaryCount = 0;
        int periodicCommonOrUncommon = 0;
        const int sampleCount = 60;

        for (int i = 0; i < sampleCount; ++i)
        {
            auto draft = RogueliteDraftManager::GenerateDraft(3, DraftType::PeriodicSupply, 1000 + i * 17);
            REQUIRE(draft.size() == 3);

            // Verify no duplicates
            std::set<RoguelitePerkType> uniqueTypes;
            for (const auto& card : draft)
            {
                uniqueTypes.insert(card.Type);
                if (card.Rarity == PerkRarity::Legendary) ++periodicLegendaryCount;
                if (card.Rarity == PerkRarity::Common || card.Rarity == PerkRarity::Uncommon) ++periodicCommonOrUncommon;
            }
            CHECK(uniqueTypes.size() == 3);
        }

        // Legendary perks must NEVER appear in PeriodicSupply drafts
        CHECK(periodicLegendaryCount == 0);
        // Common & Uncommon must dominate PeriodicSupply (> 70% of total 180 cards)
        CHECK(periodicCommonOrUncommon >= 120);

        // 2. Wave Clear: High rarity elite draft
        // Common: 0%, Uncommon: 20%, Rare: 45%, Epic: 25%, Legendary: 10%
        int waveClearCommonCount = 0;
        int waveClearHighRarityCount = 0;

        for (int i = 0; i < sampleCount; ++i)
        {
            auto draft = RogueliteDraftManager::GenerateDraft(3, DraftType::WaveClear, 5000 + i * 23);
            REQUIRE(draft.size() == 3);

            // Verify no duplicates
            std::set<RoguelitePerkType> uniqueTypes;
            for (const auto& card : draft)
            {
                uniqueTypes.insert(card.Type);
                if (card.Rarity == PerkRarity::Common) ++waveClearCommonCount;
                if (card.Rarity == PerkRarity::Rare || card.Rarity == PerkRarity::Epic || card.Rarity == PerkRarity::Legendary)
                {
                    ++waveClearHighRarityCount;
                }
            }
            CHECK(uniqueTypes.size() == 3);
        }

        // Common perks must NEVER appear in WaveClear drafts
        CHECK(waveClearCommonCount == 0);
        // Rare, Epic, Legendary must dominate WaveClear (> 65% of total 180 cards)
        CHECK(waveClearHighRarityCount >= 115);
    }

    TEST_CASE("Roguelite Draft Determinism with Seed")
    {
        auto draftA = RogueliteDraftManager::GenerateDraft(3, DraftType::WaveClear, 99999);
        auto draftB = RogueliteDraftManager::GenerateDraft(3, DraftType::WaveClear, 99999);

        REQUIRE(draftA.size() == 3);
        REQUIRE(draftB.size() == 3);

        for (size_t i = 0; i < 3; ++i)
        {
            CHECK(draftA[i].Type == draftB[i].Type);
            CHECK(draftA[i].Rarity == draftB[i].Rarity);
            CHECK(draftA[i].Title == draftB[i].Title);
        }
    }

    TEST_CASE("Dynamic Arena Event - EMP Ultimate Lockdown (45s duration & blocked activation)")
    {
        Registry registry;
        DynamicEventSystem eventSystem(registry, nullptr);

        CHECK(std::strcmp(GetArenaEventName(ArenaEventType::UltimateLockdown), "EMP ULTIMATE LOCKDOWN") == 0);
        CHECK_FALSE(eventSystem.IsUltimateBlocked());

        // Activate Ultimate Lockdown immediately without warning (skipWarning = true)
        eventSystem.TriggerEvent(ArenaEventType::UltimateLockdown, true);

        CHECK(eventSystem.GetState() == ArenaEventState::Active);
        CHECK(eventSystem.GetCurrentEventType() == ArenaEventType::UltimateLockdown);
        CHECK(eventSystem.IsUltimateBlocked());
        CHECK(eventSystem.GetTimeRemaining() == doctest::Approx(45.0f));

        // Advance 20s
        eventSystem.UpdateLifecycle(20.0f);
        CHECK(eventSystem.IsUltimateBlocked());
        CHECK(eventSystem.GetTimeRemaining() == doctest::Approx(25.0f));

        // Advance remaining 25.1s -> transitions to Cooldown
        eventSystem.UpdateLifecycle(25.1f);
        CHECK(eventSystem.GetState() == ArenaEventState::Cooldown);
        CHECK_FALSE(eventSystem.IsUltimateBlocked());
    }

    TEST_CASE("Procedural Endless Generator: Wave Duration Mathematical Scaling")
    {
        // Wave 1 = 60s (1 min)
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(1) == doctest::Approx(60.0f));

        // Wave 100 = 480s (8 min)
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(100) == doctest::Approx(480.0f));

        // Wave 50 smooth interpolation: 60 + (49/99)*420 = 267.878f
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(50) == doctest::Approx(267.878f));

        // Wave 130 = 600s (10 min)
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(130) == doctest::Approx(600.0f));

        // Beyond 100 capped at 600s (10 min)
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(150) == doctest::Approx(600.0f));
        CHECK(ProceduralEndlessGenerator::GetWaveTargetDuration(500) == doctest::Approx(600.0f));
    }

    TEST_CASE("Procedural Endless Generator: Wave Quotas, Escalating Difficulty & Hazard Variety")
    {
        Registry registry;
        ProceduralEndlessGenerator gen;

        // Wave 1
        gen.GenerateWave(1, registry, 0);
        CHECK(gen.GetCurrentWave() == 1);
        CHECK(gen.GetWaveTimer() == doctest::Approx(0.0f));
        CHECK(gen.GetCurrentWaveTargetDuration() == doctest::Approx(60.0f));
        CHECK_FALSE(gen.IsWaveTimeElapsed());
        CHECK_FALSE(gen.IsWaveTargetReached());
        // Wave 1 has 0 hazards (warmup)
        CHECK(gen.GetWaveHazards().empty());

        // Simulating wave timer progression
        gen.SetWaveTimer(30.0f);
        CHECK(gen.GetWaveTimeProgress() == doctest::Approx(0.5f));
        CHECK_FALSE(gen.IsWaveTimeElapsed());
        CHECK_FALSE(gen.IsWaveTargetReached());

        // Simulating climax threshold (>= 85%)
        gen.SetWaveTimer(55.0f);
        CHECK(gen.IsWaveClimaxActive());
        CHECK_FALSE(gen.IsWaveTimeElapsed());

        // Completing wave duration (60s)
        gen.SetWaveTimer(60.0f);
        CHECK(gen.IsWaveTimeElapsed());
        CHECK(gen.IsWaveTargetReached());

        // Wave 2
        gen.AdvanceWave(registry, 0);
        CHECK(gen.GetCurrentWave() == 2);
        CHECK(gen.GetWaveTimer() == doctest::Approx(0.0f));
        CHECK(gen.GetCurrentWaveTargetDuration() > 60.0f);
        CHECK_FALSE(gen.IsWaveTargetReached());
        // Wave 2 spawns 1 hazard with ScreenGlitch (popular triple vision chromatic glitch)
        REQUIRE(gen.GetWaveHazards().size() == 1);
        CHECK(gen.GetWaveHazards()[0].Effect == HazardEffectType::ScreenGlitch);

        // Wave 3
        gen.AdvanceWave(registry, 0);
        CHECK(gen.GetCurrentWave() == 3);
        CHECK(gen.GetWaveTargetBricks() > 0);
        REQUIRE(gen.GetWaveHazards().size() == 2);
        CHECK(gen.GetWaveHazards()[0].Effect != gen.GetWaveHazards()[1].Effect);

        // Downward Creep Interval & ChronoStasis perk multiplier
        float initialInterval = gen.GetCreepInterval();
        gen.MultiplyCreepInterval(1.30f); // ChronoStasis +30% interval
        CHECK(gen.GetCreepInterval() > initialInterval);
        CHECK(gen.GetCreepInterval() == doctest::Approx(initialInterval * 1.30f));

        // Wave Armor Tier Scaling
        gen.SetCurrentWave(1);
        CHECK(gen.GetArmorTier() == 0);
        gen.SetCurrentWave(3);
        CHECK(gen.GetArmorTier() == 1);
        gen.SetCurrentWave(5);
        CHECK(gen.GetArmorTier() == 2);
        gen.SetCurrentWave(7);
        CHECK(gen.GetArmorTier() == 3);
        gen.SetCurrentWave(15);
        CHECK(gen.GetArmorTier() == 3); // Capped at tier 3

        // Brick Durability Scaling by Armor Tier
        Registry testReg;
        Entity b0 = BrickFactory::Create(testReg, 0.0f, 0.0f, BrickType::Light, false, 0, 0);
        CHECK(testReg.GetComponent<BrickComponent>(b0).HitPoints == 1);
        CHECK(testReg.GetComponent<BrickComponent>(b0).ArmorTier == 0);

        Entity b1 = BrickFactory::Create(testReg, 0.0f, 0.0f, BrickType::Light, false, 0, 1);
        CHECK(testReg.GetComponent<BrickComponent>(b1).HitPoints == 2);
        CHECK(testReg.GetComponent<BrickComponent>(b1).ArmorTier == 1);

        Entity b2 = BrickFactory::Create(testReg, 0.0f, 0.0f, BrickType::Hard, false, 0, 2);
        CHECK(testReg.GetComponent<BrickComponent>(b2).HitPoints == 7);
        CHECK(testReg.GetComponent<BrickComponent>(b2).ArmorTier == 2);

        Entity b3 = BrickFactory::Create(testReg, 0.0f, 0.0f, BrickType::Hard, false, 0, 3);
        CHECK(testReg.GetComponent<BrickComponent>(b3).HitPoints == 9);
        CHECK(testReg.GetComponent<BrickComponent>(b3).ArmorTier == 3);

        // Hazard Effect Type UltimateDrain verification
        CHECK(static_cast<int>(HazardEffectType::UltimateDrain) == 5);
    }

    TEST_CASE("Procedural Endless Generator: Fluid replenishment & Paddle Safety Clearance")
    {
        alignas(GameContext) char dummyBuffer[sizeof(GameContext) * 2];
        GameContext& dummyContext = *reinterpret_cast<GameContext*>(dummyBuffer);

        Registry registry;
        ProceduralEndlessGenerator gen(4242u);

        gen.GenerateWave(1, registry, 0);
        int initialBricks = gen.GetBreakableBricksRemaining();
        CHECK(initialBricks > 0);

        // Paddle is at Y = 300.0f
        constexpr float kPaddleY = 300.0f;
        CHECK(gen.GetLowestBrickY() <= ProceduralEndlessGenerator::kMaxAllowedBrickY);
        CHECK(kPaddleY - gen.GetLowestBrickY() > 200.0f);

        // Test Case A: Destroy all bricks to 0 -> Update must immediately replenish rows at top
        registry.View<BrickComponent>([&](Entity e, BrickComponent&) {
            registry.DestroyEntityDeferred(e);
        });
        registry.ProcessDeferredCommands();

        // Update with 0 remaining bricks
        gen.Update(0.016f, registry, dummyContext);
        CHECK(gen.GetBreakableBricksRemaining() > 0);
        CHECK(gen.GetLowestBrickY() <= ProceduralEndlessGenerator::kMaxAllowedBrickY);
        CHECK(kPaddleY - gen.GetLowestBrickY() > 200.0f);

        // Test Case B: Strict safety ceiling on downward creep
        // Force a brick close to the safety boundary (Y = 80.0f)
        Entity boundaryBrick = BrickFactory::Create(registry, 0.0f, 80.0f, BrickType::Light, false, 0, 0);
        (void)boundaryBrick;

        // Run Update multiple times simulating creep timer expiration
        for (int i = 0; i < 10; ++i)
        {
            gen.Update(1.0f, registry, dummyContext);
        }

        // Even after repeated updates, bricks must NEVER cross kMaxAllowedBrickY (85.0f)
        registry.View<Transform2D, BrickComponent>([&](Entity, const Transform2D& t, BrickComponent&) {
            CHECK(t.Position.Y <= ProceduralEndlessGenerator::kMaxAllowedBrickY);
            CHECK(kPaddleY - t.Position.Y >= 215.0f);
        });

        // Test Case C: Emergency Burn eliminates any brick beyond 120.0f
        Entity rogueBrick = BrickFactory::Create(registry, 0.0f, 150.0f, BrickType::Light, false, 0, 0);
        CHECK(registry.IsAlive(rogueBrick));
        bool burned = gen.CheckEmergencyBurn(registry, ProceduralEndlessGenerator::kEmergencyVaporizeY);
        CHECK(burned);
        CHECK_FALSE(registry.IsAlive(rogueBrick));
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "Endless procedural seed system generates unique wave variations")
    {
        ProceduralEndlessGenerator gen1(12345u);
        ProceduralEndlessGenerator gen2(67890u);

        CHECK(gen1.GetSeed() == 12345u);
        CHECK(gen2.GetSeed() == 67890u);

        // Random seed generation produces non-zero seeds
        ProceduralEndlessGenerator genRandom;
        genRandom.GenerateRandomSeed();
        CHECK(genRandom.GetSeed() != 0);

        // Reset with seed 0 generates a fresh non-zero seed
        genRandom.Reset(0);
        uint32_t seedA = genRandom.GetSeed();
        CHECK(seedA != 0);

        // Setting explicit seed works
        genRandom.SetSeed(99999u);
        CHECK(genRandom.GetSeed() == 99999u);

        // Two different seeds generate different waves
        Registry reg1;
        Registry reg2;
        int count1 = gen1.GenerateWave(1, reg1, 0);
        int count2 = gen2.GenerateWave(1, reg2, 0);
        CHECK(count1 > 0);
        CHECK(count2 > 0);
    }

    TEST_CASE_FIXTURE(EndlessModeTestFixture, "PowerUpManager transient level does not overwrite persistent saved Storm level")
    {
        // 1. Player purchases Storm upgrade to level 4
        PowerUpManager::Get().SetLevel(PowerUpType::Storm, 4);
        CHECK(PowerUpManager::Get().GetLevel(PowerUpType::Storm) == 4);
        CHECK(PlayerPrefs::GetInt("PowerUp_Storm_Level", 1) == 4);

        // 2. Preview mode tests level 1 or 2 using SetLevelTransient
        PowerUpManager::Get().SetLevelTransient(PowerUpType::Storm, 1);
        CHECK(PowerUpManager::Get().GetLevel(PowerUpType::Storm) == 1);

        // 3. Persistent prefs in PlayerPrefs must remain untouched at level 4!
        CHECK(PlayerPrefs::GetInt("PowerUp_Storm_Level", 1) == 4);

        // 4. Reloading from prefs restores real level 4
        PowerUpManager::Get().LoadFromPrefs();
        CHECK(PowerUpManager::Get().GetLevel(PowerUpType::Storm) == 4);
    }
}
