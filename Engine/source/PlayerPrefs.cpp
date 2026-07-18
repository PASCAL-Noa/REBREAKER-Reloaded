#include "Core/PlayerPrefs.h"
#include <fstream>
#include <iostream>

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

#include <filesystem>

void PlayerPrefs::Save()
{
    std::string tmpFilePath = s_FilePath + ".tmp";
    std::ofstream file(tmpFilePath);
    if (file.is_open())
    {
        file << s_Data.dump(4);
        file.close();

        std::error_code ec;
        std::filesystem::rename(tmpFilePath, s_FilePath, ec);
        if (ec)
        {
            std::cerr << "Failed to atomically save PlayerPrefs: " << ec.message() << std::endl;
        }
    }
    else
    {
        std::cerr << "Failed to open temporary file for PlayerPrefs: " << tmpFilePath << std::endl;
    }
}

void PlayerPrefs::Load()
{
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
