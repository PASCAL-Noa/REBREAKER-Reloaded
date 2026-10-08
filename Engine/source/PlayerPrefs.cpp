#include "Core/PlayerPrefs.h"
#include <fstream>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

json PlayerPrefs::s_Data = json::object();
const std::string PlayerPrefs::s_FilePath = "playerprefs.json";

void PlayerPrefs::SetInt(const std::string& key, int value)
{
    s_Data[key] = value;
}

void PlayerPrefs::SetFloat(const std::string& key, float value)
{
    s_Data[key] = value;
}

void PlayerPrefs::SetBool(const std::string& key, bool value)
{
    s_Data[key] = value;
}

void PlayerPrefs::SetString(const std::string& key, const std::string& value)
{
    s_Data[key] = value;
}

int PlayerPrefs::GetInt(const std::string& key, int defaultValue)
{
    if (s_Data.contains(key) && s_Data[key].is_number_integer())
    {
        return s_Data[key].get<int>();
    }
    return defaultValue;
}

float PlayerPrefs::GetFloat(const std::string& key, float defaultValue)
{
    if (s_Data.contains(key) && s_Data[key].is_number())
    {
        return s_Data[key].get<float>();
    }
    return defaultValue;
}

bool PlayerPrefs::GetBool(const std::string& key, bool defaultValue)
{
    if (s_Data.contains(key) && s_Data[key].is_boolean())
    {
        return s_Data[key].get<bool>();
    }
    return defaultValue;
}

std::string PlayerPrefs::GetString(const std::string& key, const std::string& defaultValue)
{
    if (s_Data.contains(key) && s_Data[key].is_string())
    {
        return s_Data[key].get<std::string>();
    }
    return defaultValue;
}

void PlayerPrefs::Save()
{
    std::string tmpFilePath = s_FilePath + ".tmp";
    std::ofstream file(tmpFilePath);
    if (file.is_open())
    {
        file << s_Data.dump(4);
        file.close();

#if defined(_WIN32)
        std::filesystem::path srcPath(tmpFilePath);
        std::filesystem::path dstPath(s_FilePath);

        // MoveFileExW with MOVEFILE_REPLACE_EXISTING performs an atomic replacement on NTFS
        if (!MoveFileExW(srcPath.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            // Fallback to std::filesystem::copy_file with overwrite_existing
            std::error_code ec;
            std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(srcPath, ec);
            if (ec)
            {
                std::cerr << "Failed to atomically save PlayerPrefs: " << ec.message() << std::endl;
            }
        }
#else
        std::error_code ec;
        std::filesystem::copy_file(tmpFilePath, s_FilePath, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpFilePath, ec);
        if (ec)
        {
            std::cerr << "Failed to atomically save PlayerPrefs: " << ec.message() << std::endl;
        }
#endif
    }
    else
    {
        std::cerr << "Failed to open temporary file for PlayerPrefs: " << tmpFilePath << std::endl;
    }
}

void PlayerPrefs::Load()
{
    std::string tmpFilePath = s_FilePath + ".tmp";
    std::error_code ec;
    if (!std::filesystem::exists(s_FilePath, ec) && std::filesystem::exists(tmpFilePath, ec))
    {
        std::filesystem::copy_file(tmpFilePath, s_FilePath, std::filesystem::copy_options::overwrite_existing, ec);
    }

    std::ifstream file(s_FilePath);
    if (file.is_open())
    {
        try
        {
            file >> s_Data;
        }
        catch (const json::parse_error& e)
        {
            std::cerr << "Failed to parse PlayerPrefs: " << e.what() << std::endl;
            s_Data = json::object();
        }
        file.close();
    }
    else
    {
        s_Data = json::object();
    }
}

void PlayerPrefs::DeleteAll()
{
    s_Data = json::object();
    Save();
}
