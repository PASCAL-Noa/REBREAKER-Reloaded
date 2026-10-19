#include <doctest/doctest.h>
#include "Core/PlayerPrefs.h"
#include <filesystem>
#include <fstream>

struct PlayerPrefsTestFixture
{
    std::filesystem::path prefsPath{"playerprefs.json"};
    std::filesystem::path backupPath{"playerprefs.json.testbackup"};
    bool hadOriginal = false;

    PlayerPrefsTestFixture()
    {
        std::error_code ec;
        if (std::filesystem::exists(prefsPath, ec))
        {
            std::filesystem::copy_file(prefsPath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
            hadOriginal = true;
        }
        PlayerPrefs::DeleteAll();
    }

    ~PlayerPrefsTestFixture()
    {
        std::error_code ec;
        std::filesystem::remove(prefsPath, ec);
        std::filesystem::remove("playerprefs.json.tmp", ec);

        if (hadOriginal)
        {
            std::filesystem::copy_file(backupPath, prefsPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(backupPath, ec);
            PlayerPrefs::Load();
        }
    }
};

TEST_SUITE("PlayerPrefs Atomic Persistence Tests")
{
    TEST_CASE_FIXTURE(PlayerPrefsTestFixture, "Set and Get in-memory values")
    {
        PlayerPrefs::SetInt("test_int", 42);
        PlayerPrefs::SetFloat("test_float", 3.14159f);
        PlayerPrefs::SetBool("test_bool", true);
        PlayerPrefs::SetString("test_string", "rebreaker");

        CHECK(PlayerPrefs::GetInt("test_int") == 42);
        CHECK(PlayerPrefs::GetFloat("test_float") == doctest::Approx(3.14159f));
        CHECK(PlayerPrefs::GetBool("test_bool") == true);
        CHECK(PlayerPrefs::GetString("test_string") == "rebreaker");
    }

    TEST_CASE_FIXTURE(PlayerPrefsTestFixture, "Default fallback values for missing keys or wrong types")
    {
        CHECK(PlayerPrefs::GetInt("non_existent_key", 100) == 100);
        CHECK(PlayerPrefs::GetFloat("non_existent_key", 2.5f) == doctest::Approx(2.5f));
        CHECK(PlayerPrefs::GetBool("non_existent_key", true) == true);
        CHECK(PlayerPrefs::GetString("non_existent_key", "default_val") == "default_val");

        // Type mismatch fallback
        PlayerPrefs::SetString("int_as_string", "not_a_number");
        CHECK(PlayerPrefs::GetInt("int_as_string", -1) == -1);

        PlayerPrefs::SetInt("number_key", 999);
        CHECK(PlayerPrefs::GetString("number_key", "fallback") == "fallback");
    }

    TEST_CASE_FIXTURE(PlayerPrefsTestFixture, "Atomic persistence cycle (Save and Load)")
    {
        PlayerPrefs::SetInt("high_score", 99999);
        PlayerPrefs::SetFloat("sfx_volume", 0.75f);
        PlayerPrefs::SetBool("fullscreen", true);
        PlayerPrefs::SetString("player_name", "Antigravity");

        PlayerPrefs::Save();

        // Verify file was written to disk
        std::error_code ec;
        CHECK(std::filesystem::exists("playerprefs.json", ec));
        CHECK(!std::filesystem::exists("playerprefs.json.tmp", ec));

        // Overwrite in-memory values
        PlayerPrefs::SetInt("high_score", 0);
        PlayerPrefs::SetFloat("sfx_volume", 0.0f);
        PlayerPrefs::SetBool("fullscreen", false);
        PlayerPrefs::SetString("player_name", "Unknown");

        // Reload from disk
        PlayerPrefs::Load();

        CHECK(PlayerPrefs::GetInt("high_score") == 99999);
        CHECK(PlayerPrefs::GetFloat("sfx_volume") == doctest::Approx(0.75f));
        CHECK(PlayerPrefs::GetBool("fullscreen") == true);
        CHECK(PlayerPrefs::GetString("player_name") == "Antigravity");
    }

    TEST_CASE_FIXTURE(PlayerPrefsTestFixture, "DeleteAll clears memory and persisted file")
    {
        PlayerPrefs::SetInt("temp_data", 1234);
        PlayerPrefs::Save();
        CHECK(PlayerPrefs::GetInt("temp_data") == 1234);

        PlayerPrefs::DeleteAll();

        CHECK(PlayerPrefs::GetInt("temp_data", -1) == -1);

        // Reload to ensure persisted file was also emptied
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetInt("temp_data", -1) == -1);
    }

    TEST_CASE_FIXTURE(PlayerPrefsTestFixture, "Load recovery from .tmp file if main file is missing")
    {
        PlayerPrefs::SetInt("recovered_key", 777);
        PlayerPrefs::Save();

        // Simulate crash: rename playerprefs.json to playerprefs.json.tmp
        std::error_code ec;
        std::filesystem::rename("playerprefs.json", "playerprefs.json.tmp", ec);
        REQUIRE(!ec);

        // Clear memory
        PlayerPrefs::SetInt("recovered_key", 0);

        // Load should detect .tmp and recover it
        PlayerPrefs::Load();
        CHECK(PlayerPrefs::GetInt("recovered_key") == 777);
    }
}
