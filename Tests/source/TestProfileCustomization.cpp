#include <doctest/doctest.h>
#include "Core/ProfileManager.h"
#include "Core/PlayerPrefs.h"
#include "PowerUps/PowerUpConfig.h"
#include "Graphics/CosmeticRenderer.h"
#include "UI/ProfileGarageState.h"
#include "UI/ProfileGarageLayout.h"
#include <filesystem>

struct ProfileCustomizationFixture
{
    ProfileCustomizationFixture()
    {
        PlayerPrefs::DeleteAll();
        std::error_code ec;
        std::filesystem::remove(ProfileManager::GetProfileFilePath(), ec);
        std::filesystem::remove(ProfileManager::GetProfileFilePath() + ".tmp", ec);
        ProfileManager::Get().ResetToDefaults();
        PowerUpManager::Get().ResetToDefaults();
    }

    ~ProfileCustomizationFixture()
    {
        PlayerPrefs::DeleteAll();
        ProfileManager::Get().ResetToDefaults();
        PowerUpManager::Get().ResetToDefaults();
        std::error_code ec;
        std::filesystem::remove(ProfileManager::GetProfileFilePath(), ec);
        std::filesystem::remove(ProfileManager::GetProfileFilePath() + ".tmp", ec);
    }
};

TEST_SUITE("Profile Customization Garage & Persistence")
{
    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "Catalog populates all 6 cosmetic categories and required archetypes")
    {
        auto& pm = ProfileManager::Get();

        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::PrimaryColor).size() >= 12);
        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::SecondaryColor).size() >= 12);
        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::PaddleSkin).size() >= 5);
        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::BallSkin).size() >= 5);
        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::BallTrail).size() >= 5);
        CHECK(pm.GetCosmeticsBySlot(CosmeticSlot::DeathEffect).size() >= 5);

        // Original classic default skins are preserved & unlocked by default
        CHECK(pm.IsCosmeticUnlocked("paddle_classic_default"));
        CHECK(pm.IsCosmeticUnlocked("ball_classic_default"));
        CHECK(pm.IsCosmeticUnlocked("trail_classic_white"));
        CHECK(pm.IsCosmeticUnlocked("death_classic_default"));
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::PaddleSkin) == "paddle_classic_default");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::BallSkin) == "ball_classic_default");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::BallTrail) == "trail_classic_white");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::DeathEffect) == "death_classic_default");
        CHECK(pm.GetEquippedPaddleSkinStyle() == PaddleSkinStyle::ClassicSprite);
        CHECK(pm.GetEquippedBallSkinStyle() == BallSkinStyle::ClassicSprite);
        CHECK(pm.GetEquippedTrailStyle() == TrailStyle::ClassicWhite);
        CHECK(pm.GetEquippedDeathEffectStyle() == DeathEffectStyle::ClassicMinimal);

        // Required paddle archetypes: Classic Default + Cyber, Neon, Retro Wood, Golden, Steampunk
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_classic_default") == PaddleSkinStyle::ClassicSprite);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_neon_cyan") == PaddleSkinStyle::Cyber);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_neon_pulse") == PaddleSkinStyle::Neon);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_retro_wood") == PaddleSkinStyle::RetroWood);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_gold_plating") == PaddleSkinStyle::Golden);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_steampunk") == PaddleSkinStyle::Steampunk);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_obsidian") == PaddleSkinStyle::VoidObsidian);
        CHECK(ProfileManager::ResolvePaddleSkinStyle("paddle_prism_wing") == PaddleSkinStyle::PrismStriker);

        // Required particle trail styles: Classic White + Ribbon, Stardust, Flame, Matrix Binary, Rainbow
        CHECK(ProfileManager::ResolveTrailStyle("trail_classic_white") == TrailStyle::ClassicWhite);
        CHECK(ProfileManager::ResolveTrailStyle("trail_plasma_blue") == TrailStyle::Ribbon);
        CHECK(ProfileManager::ResolveTrailStyle("trail_stardust") == TrailStyle::Stardust);
        CHECK(ProfileManager::ResolveTrailStyle("trail_inferno_sparks") == TrailStyle::Flame);
        CHECK(ProfileManager::ResolveTrailStyle("trail_matrix_binary") == TrailStyle::MatrixBinary);
        CHECK(ProfileManager::ResolveTrailStyle("trail_rainbow_warp") == TrailStyle::Rainbow);
        CHECK(ProfileManager::ResolveTrailStyle("trail_void_lightning") == TrailStyle::ArcPulse);
    }

    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "Shop purchase unlocks cosmetic, deducts credits, and rejects insufficient funds or achievement items")
    {
        auto& pm = ProfileManager::Get();
        auto& pum = PowerUpManager::Get();

        // Locked shop item cannot be bought with 0 credits
        CHECK_FALSE(pm.IsCosmeticUnlocked("paddle_steampunk"));
        CHECK_FALSE(pm.UnlockCosmetic("paddle_steampunk"));

        // Achievement-locked item cannot be bought with credits even with plenty of coins
        pum.AddCoins(5000);
        CHECK_FALSE(pm.IsCosmeticUnlocked("paddle_prism_wing"));
        CHECK_FALSE(pm.UnlockCosmetic("paddle_prism_wing"));
        CHECK(pum.GetCoins() == 5000);

        // Purchase Steampunk Brass paddle (650 CR) & Matrix Binary trail (550 CR)
        const CosmeticItem* steampunk = pm.GetCosmeticById("paddle_steampunk");
        REQUIRE(steampunk != nullptr);
        CHECK(steampunk->UnlockType == CosmeticUnlockType::Shop);
        CHECK(steampunk->Price > 0);

        const int coinsBefore = pum.GetCoins();
        CHECK(pm.UnlockCosmetic("paddle_steampunk"));
        CHECK(pm.IsCosmeticUnlocked("paddle_steampunk"));
        CHECK(pum.GetCoins() == coinsBefore - steampunk->Price);

        pm.EquipCosmetic(CosmeticSlot::PaddleSkin, "paddle_steampunk");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::PaddleSkin) == "paddle_steampunk");
        CHECK(pm.GetEquippedPaddleSkinStyle() == PaddleSkinStyle::Steampunk);
    }

    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "Achievement milestones track live progress and auto-unlock cosmetics")
    {
        auto& pm = ProfileManager::Get();

        // Initially locked with 0 progress
        CHECK_FALSE(pm.IsCosmeticUnlocked("paddle_prism_wing"));
        const auto progBefore = pm.GetCosmeticMilestoneProgress("paddle_prism_wing");
        CHECK(progBefore.Current == 0);
        CHECK(progBefore.Target == 10);
        CHECK(progBefore.GetRatio() == doctest::Approx(0.0f));

        // Simulate Endless Mode run reaching wave 10 and high score (auto-evaluates milestones)
        pm.RecordEndlessRun(320.0f, 10, 35000);
        pm.AddLifetimeScore(35000);
        CHECK(pm.IsCosmeticUnlocked("paddle_prism_wing"));
        CHECK(pm.IsCosmeticUnlocked("trail_void_lightning"));
        CHECK(pm.IsCosmeticUnlocked("death_nova_implosion"));

        // Verify explicit EvaluateAchievementUnlocks unlocks campaign stage milestones
        CHECK_FALSE(pm.IsCosmeticUnlocked("color_pri_ice"));
        CHECK_FALSE(pm.IsCosmeticUnlocked("color_sec_crimson"));
        PlayerPrefs::SetInt("HighestClearedLevel", 3);
        const int newlyUnlocked = pm.EvaluateAchievementUnlocks();
        CHECK(newlyUnlocked >= 2);
        CHECK(pm.IsCosmeticUnlocked("color_pri_ice"));
        CHECK(pm.IsCosmeticUnlocked("color_sec_crimson"));

        const auto progAfter = pm.GetCosmeticMilestoneProgress("paddle_prism_wing");
        CHECK(progAfter.IsAchieved());
        CHECK(progAfter.GetRatio() == doctest::Approx(1.0f));
    }

    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "Selected customizations persist atomically in profile.json across reload")
    {
        auto& pm = ProfileManager::Get();
        auto& pum = PowerUpManager::Get();
        pum.AddCoins(3000);

        pm.SetUsername("GD_PILOT");
        REQUIRE(pm.UnlockCosmetic("paddle_retro_wood"));
        REQUIRE(pm.UnlockCosmetic("trail_matrix_binary"));
        REQUIRE(pm.UnlockCosmetic("ball_sawblade"));
        REQUIRE(pm.UnlockCosmetic("death_pixel_shatter"));
        pm.GrantCosmetic("death_matrix_glitch");

        pm.EquipCosmetic(CosmeticSlot::PrimaryColor, "color_pri_lime");
        pm.EquipCosmetic(CosmeticSlot::SecondaryColor, "color_sec_gold");
        pm.EquipCosmetic(CosmeticSlot::PaddleSkin, "paddle_retro_wood");
        pm.EquipCosmetic(CosmeticSlot::BallSkin, "ball_sawblade");
        pm.EquipCosmetic(CosmeticSlot::BallTrail, "trail_matrix_binary");
        pm.EquipCosmetic(CosmeticSlot::DeathEffect, "death_matrix_glitch");
        pm.SaveProfile();

        REQUIRE(std::filesystem::exists(ProfileManager::GetProfileFilePath()));

        // Wipe PlayerPrefs in memory to prove reload comes from profile.json
        PlayerPrefs::DeleteAll();
        pm.LoadProfile();

        CHECK(pm.GetUsername() == "GD_PILOT");
        CHECK(pm.IsCosmeticUnlocked("paddle_retro_wood"));
        CHECK(pm.IsCosmeticUnlocked("trail_matrix_binary"));
        CHECK(pm.IsCosmeticUnlocked("ball_sawblade"));
        CHECK(pm.IsCosmeticUnlocked("death_pixel_shatter"));
        CHECK(pm.IsCosmeticUnlocked("death_matrix_glitch"));
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::PrimaryColor) == "color_pri_lime");
        CHECK(pm.GetEquippedCosmetic(CosmeticSlot::SecondaryColor) == "color_sec_gold");
        CHECK(pm.GetEquippedPaddleSkinStyle() == PaddleSkinStyle::RetroWood);
        CHECK(pm.GetEquippedBallSkinStyle() == BallSkinStyle::NeonSawblade);
        CHECK(pm.GetEquippedTrailStyle() == TrailStyle::MatrixBinary);
        CHECK(pm.GetEquippedDeathEffectStyle() == DeathEffectStyle::MatrixGlitch);
    }

    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "CosmeticRenderer trail configs and explosion pool lifecycle work cleanly")
    {
        const Color pri{0, 230, 255, 255};
        const Color sec{255, 220, 60, 255};

        const TrailConfig classicWhite = CosmeticRenderer::BuildTrailConfig(TrailStyle::ClassicWhite, pri, sec);
        CHECK(classicWhite.CosmeticId == TrailCosmeticId::DefaultNeon);
        CHECK(classicWhite.Shape == TrailShape::Ribbon);

        const TrailConfig ribbon = CosmeticRenderer::BuildTrailConfig(TrailStyle::Ribbon, pri, sec);
        CHECK(ribbon.Shape == TrailShape::Ribbon);

        const TrailConfig stardust = CosmeticRenderer::BuildTrailConfig(TrailStyle::Stardust, pri, sec);
        CHECK(stardust.Shape == TrailShape::Stardust);

        const TrailConfig flame = CosmeticRenderer::BuildTrailConfig(TrailStyle::Flame, pri, sec);
        CHECK(flame.Shape == TrailShape::Flame);

        const TrailConfig matrix = CosmeticRenderer::BuildTrailConfig(TrailStyle::MatrixBinary, pri, sec);
        CHECK(matrix.Shape == TrailShape::MatrixBinary);

        const TrailConfig rainbow = CosmeticRenderer::BuildTrailConfig(TrailStyle::Rainbow, pri, sec);
        CHECK(rainbow.CosmeticId == TrailCosmeticId::Rainbow);

        CosmeticExplosionPool pool;
        CHECK(pool.GetActiveCount() == 0);
        pool.Spawn(Vector2f{0.0f, 0.0f}, DeathEffectStyle::ClassicMinimal, pri, sec, 1.0f);
        pool.Spawn(Vector2f{100.0f, 50.0f}, DeathEffectStyle::MatrixGlitch, pri, sec, 1.0f);
        pool.Spawn(Vector2f{-50.0f, 20.0f}, DeathEffectStyle::ShockwaveRing, pri, sec, 1.2f);
        CHECK(pool.GetActiveCount() == 3);

        pool.Update(0.15f);
        CHECK(pool.GetActiveCount() == 3);

        // Advance well past max duration -> bursts expire cleanly
        for (int i = 0; i < 30; ++i)
        {
            pool.Update(0.05f);
        }
        CHECK(pool.GetActiveCount() == 0);
    }

    TEST_CASE_FIXTURE(ProfileCustomizationFixture, "ProfileGarageState uses 2x4 grid and paginates within active category without switching tabs")
    {
        ProfileGarageState state;
        state.Initialize();

        CHECK(ProfileGarageState::kTabCount == 4);
        CHECK(ProfileGarageState::kSideButtonCount == 3);
        CHECK(ProfileGarageState::kGridCols == 4);
        CHECK(ProfileGarageState::kGridRows == 2);
        CHECK(ProfileGarageState::kItemsPerPage == 8);
        CHECK(state.GetActiveTab() == 0);
        CHECK(state.GetActiveSlot() == CosmeticSlot::PaddleSkin);
        CHECK(state.GetCurrentPage() == 0);
        CHECK(state.GetPageItemCount() <= ProfileGarageState::kItemsPerPage);
        CHECK(state.GetPageItemCount() > 0);

        const int initialTab = state.GetActiveTab();
        state.NextPage();
        CHECK(state.GetActiveTab() == initialTab);
        CHECK(state.GetActiveSlot() == CosmeticSlot::PaddleSkin);

        state.PrevPage();
        CHECK(state.GetActiveTab() == initialTab);
        CHECK(state.GetCurrentPage() == 0);

        CHECK(state.SwitchTab(1));
        CHECK(state.GetActiveSlot() == CosmeticSlot::BallSkin);
        CHECK(state.GetCurrentPage() == 0);

        state.SetSelectedIndex(2);
        CHECK_FALSE(state.GetInspectedItem()->Unlocked);
        state.NotifySelectedLockedItem();
        CHECK_FALSE(state.GetToastText().empty());
        CHECK(state.ActivateSelectedItem() == ProfileActionResult::InsufficientFunds);

        PowerUpManager::Get().AddCoins(1000);
        CHECK(state.ActivateSelectedItem() == ProfileActionResult::UnlockedAndEquipped);
        CHECK(state.GetInspectedItem()->Unlocked);
    }

    TEST_CASE("ProfileGarageLayout centers 3 action buttons above 4 tabs and flanks inline stats panels without overlap")
    {
        const Vector2f view{1920.0f, 1080.0f};
        const UIRect back = ProfileGarageLayout::GetBackButtonRect(view);
        const UIRect nick = ProfileGarageLayout::GetNicknameRect(view);
        const UIRect statsLeft = ProfileGarageLayout::GetProfileStatsLeftRect(view);
        const UIRect statsRight = ProfileGarageLayout::GetProfileStatsRightRect(view);
        const UIRect showcase = ProfileGarageLayout::GetShowcaseBounds(view);
        const UIRect action0 = ProfileGarageLayout::GetSideButtonRect(0, view);
        const UIRect action1 = ProfileGarageLayout::GetSideButtonRect(1, view);
        const UIRect action2 = ProfileGarageLayout::GetSideButtonRect(2, view);
        const UIRect tab0 = ProfileGarageLayout::GetTabRect(0, view);
        const UIRect tab3 = ProfileGarageLayout::GetTabRect(3, view);
        const UIRect drawer = ProfileGarageLayout::GetDrawerRect(view);
        const UIRect pills = ProfileGarageLayout::GetPageIndicatorRect(0, 2, view);
        const UIRect leftArrow = ProfileGarageLayout::GetPageArrowRect(false, view);
        const UIRect rightArrow = ProfileGarageLayout::GetPageArrowRect(true, view);
        const UIRect colorModal = ProfileGarageLayout::GetColorModalRect(view);

        CHECK(action0.Center.Y == doctest::Approx(action1.Center.Y));
        CHECK(action1.Center.Y == doctest::Approx(action2.Center.Y));
        CHECK(action1.Center.X == doctest::Approx(0.0f));
        CHECK(action0.Center.X == doctest::Approx(-action2.Center.X));
        CHECK(action0.Right() < action1.Left());
        CHECK(action1.Right() < action2.Left());

        CHECK(tab0.Center.Y == doctest::Approx(tab3.Center.Y));
        CHECK(tab0.Center.X == doctest::Approx(-tab3.Center.X));

        CHECK(nick.Center.Y > back.Center.Y);
        CHECK(nick.Bottom() < showcase.Top());
        CHECK(showcase.Bottom() < action0.Top());
        CHECK(action0.Bottom() < tab0.Top());
        CHECK(tab0.Bottom() < drawer.Top());
        CHECK(drawer.Bottom() < pills.Top());

        CHECK(std::abs((nick.Top() + pills.Bottom()) * 0.5f) <= 15.0f);
        CHECK(colorModal.Center.X == doctest::Approx(0.0f));
        CHECK(colorModal.Center.Y == doctest::Approx(0.0f));

        CHECK(leftArrow.Center.Y == doctest::Approx(drawer.Center.Y));
        CHECK(rightArrow.Center.Y == doctest::Approx(drawer.Center.Y));
        CHECK(leftArrow.Size.X == doctest::Approx(leftArrow.Size.Y));

        CHECK(statsLeft.Center.X == doctest::Approx(-statsRight.Center.X));
        CHECK(statsLeft.Center.Y == doctest::Approx(statsRight.Center.Y));
        CHECK(statsLeft.Top() > back.Bottom());
        CHECK(statsLeft.Bottom() < drawer.Top());
        CHECK(statsLeft.Right() < tab0.Left());
        CHECK(statsLeft.Right() < nick.Left());
        CHECK(statsLeft.Right() < showcase.Left());
        CHECK(statsRight.Left() > tab3.Right());
        CHECK(statsRight.Left() > nick.Right());
        CHECK(statsRight.Left() > showcase.Right());

        const Vector2f view720{1280.0f, 720.0f};
        const UIRect back720 = ProfileGarageLayout::GetBackButtonRect(view720);
        const UIRect drawer720 = ProfileGarageLayout::GetDrawerRect(view720);
        const UIRect tab0720 = ProfileGarageLayout::GetTabRect(0, view720);
        const UIRect left720 = ProfileGarageLayout::GetProfileStatsLeftRect(view720);
        const UIRect right720 = ProfileGarageLayout::GetProfileStatsRightRect(view720);
        CHECK(left720.Top() > back720.Bottom());
        CHECK(left720.Bottom() < drawer720.Top());
        CHECK(left720.Right() < tab0720.Left());
        CHECK(left720.Left() > -view720.X * 0.5f);
        CHECK(right720.Right() < view720.X * 0.5f);
    }
}
