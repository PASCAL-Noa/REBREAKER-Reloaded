#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include "Core/PlayerPrefs.h"
#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv)
{
#if defined(__linux__)
    setenv("ALSOFT_DRIVERS", "null", 0);
#endif

    // Strictly isolate tests from live player save files
    PlayerPrefs::SetCustomFilePath("test_playerprefs.json");

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int result = context.run();

    std::error_code ec;
    std::filesystem::remove("test_playerprefs.json", ec);
    std::filesystem::remove("test_playerprefs.json.tmp", ec);
    std::filesystem::remove("test_playerprefs.json.testbackup", ec);
    std::filesystem::remove("test_playerprefs.json.econbackup", ec);
    std::filesystem::remove("test_playerprefs.json.endlessbackup", ec);

    return result;
}

