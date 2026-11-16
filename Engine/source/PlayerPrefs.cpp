#include "Core/PlayerPrefs.h"
#include "Core/json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <mutex>

using json = nlohmann::json;

static std::recursive_mutex& GetPrefsMutex()
{
    static std::recursive_mutex s_prefsMutex;
    return s_prefsMutex;
}

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

static json s_Data = json::object();
const std::string PlayerPrefs::s_FilePath = "playerprefs.json";
std::string PlayerPrefs::s_CustomFilePath = "";

void PlayerPrefs::SetCustomFilePath(const std::string& path)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    s_CustomFilePath = path;
}

std::string PlayerPrefs::GetFilePath()
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (!s_CustomFilePath.empty())
    {
        return s_CustomFilePath;
    }
    return s_FilePath;
}

static std::filesystem::path GetExecutableDir()
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len > 0)
    {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

static bool AtomicSaveToFile(const std::filesystem::path& targetPath, const json& data)
{
    try
    {
        std::filesystem::path tmpPath = targetPath;
        tmpPath += ".tmp";

        std::ofstream file(tmpPath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open temporary file for PlayerPrefs: " << tmpPath.string() << std::endl;
            return false;
        }

        file << data.dump(4);
        file.close();

#if defined(_WIN32)
        if (!MoveFileExW(tmpPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::error_code ec;
            std::filesystem::copy_file(tmpPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
            if (ec)
            {
                std::cerr << "Failed to atomically save PlayerPrefs: " << ec.message() << std::endl;
                return false;
            }
        }
#else
        std::error_code ec;
        std::filesystem::copy_file(tmpPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath, ec);
        if (ec)
        {
            std::cerr << "Failed to atomically save PlayerPrefs: " << ec.message() << std::endl;
            return false;
        }
#endif
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception while saving PlayerPrefs: " << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cerr << "Unknown exception while saving PlayerPrefs." << std::endl;
        return false;
    }
}

void PlayerPrefs::SetInt(const std::string& key, int value)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    s_Data[key] = value;
}

void PlayerPrefs::SetFloat(const std::string& key, float value)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (std::isnan(value) || std::isinf(value))
    {
        value = 0.0f;
    }
    s_Data[key] = value;
}

void PlayerPrefs::SetBool(const std::string& key, bool value)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    s_Data[key] = value;
}

void PlayerPrefs::SetString(const std::string& key, const std::string& value)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    s_Data[key] = value;
}

int PlayerPrefs::GetInt(const std::string& key, int defaultValue)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (s_Data.contains(key) && s_Data[key].is_number_integer())
    {
        return s_Data[key].get<int>();
    }
    return defaultValue;
}

float PlayerPrefs::GetFloat(const std::string& key, float defaultValue)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (s_Data.contains(key) && s_Data[key].is_number())
    {
        return s_Data[key].get<float>();
    }
    return defaultValue;
}

bool PlayerPrefs::GetBool(const std::string& key, bool defaultValue)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (s_Data.contains(key) && s_Data[key].is_boolean())
    {
        return s_Data[key].get<bool>();
    }
    return defaultValue;
}

std::string PlayerPrefs::GetString(const std::string& key, const std::string& defaultValue)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (s_Data.contains(key) && s_Data[key].is_string())
    {
        return s_Data[key].get<std::string>();
    }
    return defaultValue;
}

void PlayerPrefs::Save()
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (!s_CustomFilePath.empty())
    {
        AtomicSaveToFile(std::filesystem::path(s_CustomFilePath), s_Data);
        return;
    }

    std::filesystem::path cwdPath = std::filesystem::current_path() / s_FilePath;
    AtomicSaveToFile(cwdPath, s_Data);

    std::filesystem::path exeDir = GetExecutableDir();
    std::filesystem::path exePath = exeDir / s_FilePath;
    std::error_code ec;
    if (!std::filesystem::equivalent(cwdPath.parent_path(), exeDir, ec))
    {
        AtomicSaveToFile(exePath, s_Data);
    }
}

void PlayerPrefs::Load()
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    if (!s_CustomFilePath.empty())
    {
        std::string tmpFilePath = s_CustomFilePath + ".tmp";
        std::error_code ec;
        if (!std::filesystem::exists(s_CustomFilePath, ec) && std::filesystem::exists(tmpFilePath, ec))
        {
            std::filesystem::copy_file(tmpFilePath, s_CustomFilePath, std::filesystem::copy_options::overwrite_existing, ec);
        }

        std::ifstream file(s_CustomFilePath);
        if (file.is_open())
        {
            try
            {
                file >> s_Data;
                if (!s_Data.is_object())
                {
                    s_Data = json::object();
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to parse PlayerPrefs (" << s_CustomFilePath << "): " << e.what() << std::endl;
                s_Data = json::object();
            }
            file.close();
        }
        else
        {
            s_Data = json::object();
        }
        return;
    }

    std::filesystem::path cwdPath = std::filesystem::current_path() / s_FilePath;
    std::filesystem::path exeDir = GetExecutableDir();
    std::filesystem::path exePath = exeDir / s_FilePath;

    auto tryLoadJson = [](const std::filesystem::path& path) -> json {
        std::error_code ec;
        std::filesystem::path tmpPath = path;
        tmpPath += ".tmp";
        if (!std::filesystem::exists(path, ec) && std::filesystem::exists(tmpPath, ec))
        {
            std::filesystem::copy_file(tmpPath, path, std::filesystem::copy_options::overwrite_existing, ec);
        }

        std::ifstream f(path);
        if (f.is_open())
        {
            try
            {
                json j;
                f >> j;
                if (j.is_object())
                {
                    f.close();
                    return j;
                }
            }
            catch (...) {}
            f.close();
        }
        return json::object();
    };

    json cwdData = tryLoadJson(cwdPath);
    json exeData = json::object();
    std::error_code ec;
    bool differentDirs = !std::filesystem::equivalent(cwdPath.parent_path(), exeDir, ec);
    if (differentDirs)
    {
        exeData = tryLoadJson(exePath);
    }

    auto getScoreOrCoins = [](const json& j) -> int64_t {
        if (!j.is_object()) return 0;
        int64_t score = 0;
        if (j.contains("Economy_Coins") && j["Economy_Coins"].is_number_integer())
            score += j["Economy_Coins"].get<int64_t>();
        if (j.contains("HighScore") && j["HighScore"].is_number_integer())
            score += j["HighScore"].get<int64_t>();
        if (j.contains("Endless_HighScore") && j["Endless_HighScore"].is_number_integer())
            score += j["Endless_HighScore"].get<int64_t>();
        return score;
    };

    int64_t cwdScore = getScoreOrCoins(cwdData);
    int64_t exeScore = getScoreOrCoins(exeData);

    if (exeScore > cwdScore)
    {
        s_Data = std::move(exeData);
        AtomicSaveToFile(cwdPath, s_Data);
    }
    else if (!cwdData.empty())
    {
        s_Data = std::move(cwdData);
        if (differentDirs && exeScore < cwdScore)
        {
            AtomicSaveToFile(exePath, s_Data);
        }
    }
    else if (!exeData.empty())
    {
        s_Data = std::move(exeData);
        AtomicSaveToFile(cwdPath, s_Data);
    }
    else
    {
        s_Data = json::object();
    }
}

void PlayerPrefs::DeleteAll()
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    s_Data = json::object();
    Save();
}

bool PlayerPrefs::HasKey(const std::string& key)
{
    std::lock_guard<std::recursive_mutex> lock(GetPrefsMutex());
    return s_Data.contains(key);
}
