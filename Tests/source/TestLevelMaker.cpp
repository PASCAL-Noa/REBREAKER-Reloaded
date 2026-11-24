#include <doctest/doctest.h>
#include "Editor/LevelData.h"
#include "Editor/CustomLevelLibrary.h"
#include "Factories/BrickFactory.h"
#include "ECS/Components/SpriteComponent.h"

TEST_SUITE("LevelMaker")
{
    TEST_CASE("LevelData - Grid Dimensions and Cell Positioning")
    {
        LevelData data;
        CHECK(LevelData::GRID_COLS == 12);
        CHECK(LevelData::GRID_ROWS == 10);
        CHECK(data.GetBrickCount() == 0);

        // Center calculation
        Vector2f c00 = data.GetCellCenter(0, 0);
        CHECK(c00.X == doctest::Approx(-605.0f));
        CHECK(c00.Y == doctest::Approx(-350.0f));

        Vector2f c11_0 = data.GetCellCenter(11, 0);
        CHECK(c11_0.X == doctest::Approx(605.0f)); // Symmetrical across X=0
    }

    TEST_CASE("LevelData - Tile Placement and Clearing")
    {
        LevelData data;
        CHECK(data.GetTile(0, 0) == EditorTileType::Empty);

        data.SetTile(0, 0, EditorTileType::Light);
        data.SetTile(5, 3, EditorTileType::Turret);
        data.SetTile(11, 9, EditorTileType::Bomb);

        CHECK(data.GetTile(0, 0) == EditorTileType::Light);
        CHECK(data.GetTile(5, 3) == EditorTileType::Turret);
        CHECK(data.GetTile(11, 9) == EditorTileType::Bomb);
        CHECK(data.GetBrickCount() == 3);

        // Out-of-bounds safety
        CHECK(data.GetTile(-1, 0) == EditorTileType::Empty);
        CHECK(data.GetTile(12, 0) == EditorTileType::Empty);
        data.SetTile(-1, 5, EditorTileType::Hard);
        CHECK(data.GetBrickCount() == 3);

        data.ClearGrid();
        CHECK(data.GetBrickCount() == 0);
        CHECK(data.GetTile(0, 0) == EditorTileType::Empty);
    }

    TEST_CASE("LevelData - Procedural Generator & Symmetry Modes")
    {
        LevelData data;

        // 1. None symmetry
        data.ProceduralGenerate(20, 0.3f, 0, 42);
        CHECK(data.GetBrickCount() >= 15);

        // 2. Horizontal symmetry (left mirrors right)
        data.ProceduralGenerate(30, 0.5f, 1, 999);
        int horizSymmetricPairs = 0;
        for (int r = 0; r < LevelData::GRID_ROWS; ++r)
        {
            for (int c = 0; c < LevelData::GRID_COLS / 2; ++c)
            {
                if (data.GetTile(c, r) != EditorTileType::Empty)
                {
                    CHECK(data.GetTile(c, r) == data.GetTile(LevelData::GRID_COLS - 1 - c, r));
                    horizSymmetricPairs++;
                }
            }
        }
        CHECK(horizSymmetricPairs > 0);

        // 3. Vertical symmetry (top mirrors bottom)
        data.ProceduralGenerate(24, 0.7f, 2, 888);
        int vertSymmetricPairs = 0;
        for (int r = 0; r < LevelData::GRID_ROWS / 2; ++r)
        {
            for (int c = 0; c < LevelData::GRID_COLS; ++c)
            {
                if (data.GetTile(c, r) != EditorTileType::Empty)
                {
                    CHECK(data.GetTile(c, r) == data.GetTile(c, LevelData::GRID_ROWS - 1 - r));
                    vertSymmetricPairs++;
                }
            }
        }
        CHECK(vertSymmetricPairs > 0);

        // 4. Dual symmetry
        data.ProceduralGenerate(36, 0.9f, 3, 777);
        CHECK(data.GetBrickCount() > 0);
        for (int r = 0; r < LevelData::GRID_ROWS / 2; ++r)
        {
            for (int c = 0; c < LevelData::GRID_COLS / 2; ++c)
            {
                EditorTileType t = data.GetTile(c, r);
                if (t != EditorTileType::Empty)
                {
                    CHECK(data.GetTile(LevelData::GRID_COLS - 1 - c, r) == t);
                    CHECK(data.GetTile(c, LevelData::GRID_ROWS - 1 - r) == t);
                    CHECK(data.GetTile(LevelData::GRID_COLS - 1 - c, LevelData::GRID_ROWS - 1 - r) == t);
                }
            }
        }
    }

    TEST_CASE("LevelData - Event Queue & Trash Bin Logic")
    {
        LevelData data;
        CHECK(data.GetEventQueue().empty());

        data.AddEvent(EditorEventType::HazardSurge);
        data.AddEvent(EditorEventType::LaserSweep);
        data.AddEvent(EditorEventType::DarknessEclipse);
        CHECK(data.GetEventQueue().size() == 3);
        CHECK(data.GetEventQueue()[0].Type == EditorEventType::HazardSurge);
        CHECK(data.GetEventQueue()[1].Type == EditorEventType::LaserSweep);
        CHECK(data.GetEventQueue()[2].Type == EditorEventType::DarknessEclipse);

        // Adjust trigger delay & clamp
        const float initialDelay = data.GetEventQueue()[0].TriggerDelay;
        data.AdjustEventDelay(0, 6.0f);
        CHECK(data.GetEventQueue()[0].TriggerDelay == doctest::Approx(initialDelay + 6.0f));
        data.AdjustEventDelay(0, -999.0f);
        CHECK(data.GetEventQueue()[0].TriggerDelay == doctest::Approx(3.0f));

        // Reorder events via drag-and-drop
        data.ReorderEvent(0, 2);
        CHECK(data.GetEventQueue()[2].Type == EditorEventType::HazardSurge);

        // Drag-and-drop to Corbeille: delete individual event
        data.RemoveEvent(1);
        CHECK(data.GetEventQueue().size() == 2);
        CHECK(data.GetEventQueue()[0].Type == EditorEventType::LaserSweep);
        CHECK(data.GetEventQueue()[1].Type == EditorEventType::HazardSurge);

        // Click Corbeille: clear entire queue
        data.ClearEventQueue();
        CHECK(data.GetEventQueue().empty());
    }

    TEST_CASE("LevelData - Event Filters")
    {
        LevelData data;
        data.AddEvent(EditorEventType::SpeedBoost);      // type 5
        data.AddEvent(EditorEventType::HazardSurge);     // type 0
        data.AddEvent(EditorEventType::TurretOvercharge);// type 3

        // Clockwise (ascending Type)
        data.ApplyEventFilter(EventSortFilter::Clockwise);
        CHECK(data.GetEventQueue()[0].Type == EditorEventType::HazardSurge);
        CHECK(data.GetEventQueue()[1].Type == EditorEventType::TurretOvercharge);
        CHECK(data.GetEventQueue()[2].Type == EditorEventType::SpeedBoost);

        // Counter-Clockwise (descending Type)
        data.ApplyEventFilter(EventSortFilter::CounterClockwise);
        CHECK(data.GetEventQueue()[0].Type == EditorEventType::SpeedBoost);
        CHECK(data.GetEventQueue()[1].Type == EditorEventType::TurretOvercharge);
        CHECK(data.GetEventQueue()[2].Type == EditorEventType::HazardSurge);
    }

    TEST_CASE("LevelData - Theme and Lighting Presets")
    {
        LevelData data;
        CHECK(data.GetTheme().Preset == EditorThemePreset::DeepSpace);
        CHECK(data.GetTheme().Name == "DEEP SPACE");

        data.NextThemePreset();
        CHECK(data.GetTheme().Preset == EditorThemePreset::CyberNeon);
        CHECK(data.GetTheme().Name == "CYBER NEON");

        data.NextThemePreset();
        CHECK(data.GetTheme().Preset == EditorThemePreset::SolarFlare);

        data.PrevThemePreset();
        CHECK(data.GetTheme().Preset == EditorThemePreset::CyberNeon);
    }

    TEST_CASE("LevelData - ASCII Export and Import Roundtrip")
    {
        LevelData data;
        data.SetTile(0, 0, EditorTileType::Light);
        data.SetTile(1, 0, EditorTileType::Medium);
        data.SetTile(2, 0, EditorTileType::Hard);
        data.SetTile(3, 0, EditorTileType::Special);
        data.SetTile(4, 0, EditorTileType::Metal);
        data.SetTile(5, 0, EditorTileType::Bomb);
        data.SetTile(6, 0, EditorTileType::Turret);

        std::string ascii = data.ExportToAscii();
        CHECK(!ascii.empty());
        CHECK(ascii.find("123SMB") != std::string::npos);

        LevelData imported;
        imported.ImportFromAscii(ascii);
        CHECK(imported.GetTile(0, 0) == EditorTileType::Light);
        CHECK(imported.GetTile(1, 0) == EditorTileType::Medium);
        CHECK(imported.GetTile(2, 0) == EditorTileType::Hard);
        CHECK(imported.GetTile(3, 0) == EditorTileType::Special);
        CHECK(imported.GetTile(4, 0) == EditorTileType::Metal);
        CHECK(imported.GetTile(5, 0) == EditorTileType::Bomb);
        CHECK(imported.GetTile(6, 0) == EditorTileType::Turret);
    }

    TEST_CASE("LevelData - Multi-Page Grid Management, Reorder, Duplicate & Snapshot Undo")
    {
        LevelData data;
        CHECK(data.GetPageCount() == 1);
        CHECK(data.GetActivePageIndex() == 0);
        CHECK_FALSE(data.RemoveActivePage()); // Cannot remove the sole page

        data.SetTile(0, 0, EditorTileType::Light);
        data.SetTile(1, 0, EditorTileType::Metal);
        CHECK(data.GetBreakableBrickCount(0) == 1); // Metal is not counted as breakable

        // Add blank page 2
        size_t p2 = data.AddPage(false);
        CHECK(p2 == 1);
        CHECK(data.GetPageCount() == 2);
        CHECK(data.GetActivePageIndex() == 1);
        CHECK(data.GetBrickCount() == 0);

        data.SetTile(2, 2, EditorTileType::Hard);
        CHECK(data.GetTotalBrickCount() == 3);

        // Duplicate page 2 -> page 3
        size_t p3 = data.AddPage(true);
        CHECK(p3 == 2);
        CHECK(data.GetPageCount() == 3);
        CHECK(data.GetTile(2, 2) == EditorTileType::Hard);

        // Snapshot before reorder
        auto snap = data.CreateSnapshot();

        // Move active page left (index 2 -> 1)
        data.SetTile(3, 3, EditorTileType::Turret);
        CHECK(data.MoveActivePageLeft());
        CHECK(data.GetActivePageIndex() == 1);
        CHECK(data.GetTile(3, 3) == EditorTileType::Turret);

        // Move right back to 2
        CHECK(data.MoveActivePageRight());
        CHECK(data.GetActivePageIndex() == 2);
        CHECK_FALSE(data.MoveActivePageRight()); // Already at last

        // Remove page
        CHECK(data.RemoveActivePage());
        CHECK(data.GetPageCount() == 2);

        // Restore snapshot
        data.RestoreSnapshot(snap);
        CHECK(data.GetPageCount() == 3);
        CHECK(data.GetActivePageIndex() == 2);
        CHECK(data.GetTile(3, 3) == EditorTileType::Empty);
    }

    TEST_CASE("LevelData - Palette Presets & V2 Multi-Page Metadata Roundtrip")
    {
        LevelData data;

        // Modify palette presets
        EditorCellData lightPreset = data.GetPalettePreset(EditorTileType::Light);
        lightPreset.HitPoints = 8;
        data.SetPalettePreset(EditorTileType::Light, lightPreset);

        EditorCellData specialPreset = data.GetPalettePreset(EditorTileType::Special);
        specialPreset.RandomPowerUp = false;
        specialPreset.SpecificPowerUp = PowerUpType::FireBall;
        data.SetPalettePreset(EditorTileType::Special, specialPreset);

        EditorCellData turretPreset = data.GetPalettePreset(EditorTileType::Turret);
        turretPreset.HitPoints = 7;
        turretPreset.TurretCooldown = 1.6f;
        turretPreset.TurretInitialDelay = 2.0f;
        turretPreset.TurretProjectileSpeed = 330.0f;
        data.SetPalettePreset(EditorTileType::Turret, turretPreset);

        // Paint using updated presets on Page 0
        data.SetTile(0, 0, EditorTileType::Light);
        data.SetTile(1, 0, EditorTileType::Special);

        // Page 1 gets customized turret + metal
        data.AddPage(false);
        data.SetTile(4, 1, EditorTileType::Turret);
        data.SetTile(5, 1, EditorTileType::Metal);

        std::string v2 = data.ExportLevelV2();
        CHECK(v2.find("#REBREAKER_LEVEL_V2") != std::string::npos);

        LevelData imported;
        imported.ImportLevelV2(v2);
        CHECK(imported.GetPageCount() == 2);

        const EditorCellData impLight = imported.GetCell(0, 0, 0);
        CHECK(impLight.Type == EditorTileType::Light);
        CHECK(impLight.HitPoints == 8);

        const EditorCellData impSpecial = imported.GetCell(1, 0, 0);
        CHECK(impSpecial.Type == EditorTileType::Special);
        CHECK_FALSE(impSpecial.RandomPowerUp);
        CHECK(impSpecial.SpecificPowerUp == PowerUpType::FireBall);

        const EditorCellData impTurret = imported.GetCell(4, 1, 1);
        CHECK(impTurret.Type == EditorTileType::Turret);
        CHECK(impTurret.HitPoints == 7);
        CHECK(impTurret.TurretCooldown == doctest::Approx(1.6f));
        CHECK(impTurret.TurretInitialDelay == doctest::Approx(2.0f));
        CHECK(impTurret.TurretProjectileSpeed == doctest::Approx(330.0f));
    }

    TEST_CASE("BrickFactory - Dynamic 1..9 HP Colors and Custom HP Creation")
    {
        // All 1..9 HP levels should yield distinct colors matching LevelData::GetColorForHitPoints
        for (int hp = 1; hp <= 9; ++hp)
        {
            Color fc = BrickFactory::GetColorForHitPoints(hp);
            Color lc = LevelData::GetColorForHitPoints(hp);
            CHECK(fc.r == lc.r);
            CHECK(fc.g == lc.g);
            CHECK(fc.b == lc.b);
        }
        CHECK(BrickFactory::GetColorForHitPoints(1) != BrickFactory::GetColorForHitPoints(9));
        CHECK(BrickFactory::GetColorForHitPoints(4) != BrickFactory::GetColorForHitPoints(7));

        Registry reg;
        Entity e9 = BrickFactory::Create(reg, 0.0f, -100.0f, BrickType::Light, false, 0, 0, 9);
        CHECK(reg.HasComponent<BrickComponent>(e9));
        CHECK(reg.GetComponent<BrickComponent>(e9).HitPoints == 9);
        CHECK(reg.GetComponent<BrickComponent>(e9).MaxHitPoints == 9);
        Color expected9 = BrickFactory::GetColorForHitPoints(9);
        CHECK(reg.GetComponent<SpriteComponent>(e9).Tint == expected9);

        Entity eMetal = BrickFactory::Create(reg, 100.0f, -100.0f, BrickType::Metal, false, 0, 0, 3);
        CHECK(reg.GetComponent<BrickComponent>(eMetal).HitPoints == 999999);
    }

    TEST_CASE("LevelData - V2 ID, Name, and Custom Event Delay Roundtrip")
    {
        LevelData data;
        data.SetLevelId("RBK-0042");
        data.SetLevelName("CITADELLE NEON");
        data.AddEvent(EditorEventType::DarknessEclipse);
        data.AdjustEventDelay(0, 9.0f);
        const float expectedDelay = data.GetEventQueue()[0].TriggerDelay;

        const std::string v2 = data.ExportLevelV2();
        LevelData loaded;
        loaded.ImportLevelV2(v2);

        CHECK(loaded.GetLevelId() == "RBK-0042");
        CHECK(loaded.GetLevelName() == "CITADELLE NEON");
        REQUIRE(loaded.GetEventQueue().size() == 1);
        CHECK(loaded.GetEventQueue()[0].Type == EditorEventType::DarknessEclipse);
        CHECK(loaded.GetEventQueue()[0].TriggerDelay == doctest::Approx(expectedDelay));
    }

    TEST_CASE("CustomLevelLibrary - Robostock Vault CRUD, Reorder & 10-Slot Special Playlists")
    {
        const std::string tempDir = "temp_robostock_unit_test";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);

        {
            CustomLevelLibrary lib(tempDir, /*autoImportLegacy=*/false);
            CHECK(lib.GetLevels().empty());
            CHECK(lib.GetPlaylists().empty());

            LevelData lvl1;
            lvl1.SetTile(0, 0, EditorTileType::Light);
            lvl1.SetTile(1, 0, EditorTileType::Turret);
            const std::string id1 = lib.SaveNewLevel("STAGE ALPHA", lvl1);
            CHECK(id1 == "RBK-0001");

            LevelData lvl2;
            lvl2.SetTile(3, 2, EditorTileType::Hard);
            const std::string id2 = lib.SaveNewLevel("STAGE BETA", lvl2);
            CHECK(id2 == "RBK-0002");

            REQUIRE(lib.GetLevels().size() == 2);
            CHECK(lib.GetLevels()[0].PreviewBricks.size() == 2);
            CHECK(lib.GetLevels()[1].PreviewBricks.size() == 1);

            // Reorder levels via drag & drop
            CHECK(lib.ReorderLevel(0, 1));
            CHECK(lib.GetLevels()[0].Id == "RBK-0002");
            CHECK(lib.GetLevels()[1].Id == "RBK-0001");

            // Rename level
            CHECK(lib.RenameLevel(id1, "ALPHA REMIX"));
            CHECK(lib.FindLevel(id1)->Name == "ALPHA REMIX");

            // Special Playlist with strict 10-slot cap
            const std::string plId = lib.CreatePlaylist("MARATHON");
            CHECK(plId == "LST-01");
            for (size_t s = 0; s < CustomPlaylist::MAX_SLOTS; ++s)
            {
                CHECK(lib.AddLevelToPlaylist(plId, (s % 2 == 0) ? id1 : id2));
            }
            REQUIRE(lib.FindPlaylist(plId)->LevelIds.size() == 10);
            // 11th append must be rejected
            CHECK_FALSE(lib.AddLevelToPlaylist(plId, id1));
            CHECK(lib.FindPlaylist(plId)->LevelIds.size() == 10);

            // Reorder & remove playlist slot
            CHECK(lib.ReorderPlaylistSlot(plId, 0, 1));
            CHECK(lib.FindPlaylist(plId)->LevelIds[0] == id2);
            CHECK(lib.RemoveSlotFromPlaylist(plId, 0));
            CHECK(lib.FindPlaylist(plId)->LevelIds.size() == 9);

            // Deleting a level prunes it from playlists automatically
            CHECK(lib.DeleteLevel(id2));
            CHECK(lib.GetLevels().size() == 1);
            for (const auto& slotLvl : lib.FindPlaylist(plId)->LevelIds)
            {
                CHECK(slotLvl == id1);
            }
        }

        // Reload persistence check
        {
            CustomLevelLibrary reloaded(tempDir, /*autoImportLegacy=*/false);
            REQUIRE(reloaded.GetLevels().size() == 1);
            CHECK(reloaded.GetLevels()[0].Id == "RBK-0001");
            CHECK(reloaded.GetLevels()[0].Name == "ALPHA REMIX");
            REQUIRE(reloaded.GetPlaylists().size() == 1);
            CHECK(reloaded.GetPlaylists()[0].Name == "MARATHON");
            CHECK(reloaded.GetPlaylists()[0].LevelIds.size() == 5);
        }

        std::filesystem::remove_all(tempDir, ec);
    }
}

