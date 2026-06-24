#pragma once

#include <string>
#include <unordered_map>
#include "json.hpp"

using json = nlohmann::json;

class PlayerPrefs
{
public:
    static void     SetInt(const std::string& key, int value);
    static void     SetFloat(const std::string& key, float value);
    static void     SetBool(const std::string& key, bool value);
    static void     SetString(const std::string& key, const std::string& value);

    static int  GetInt(const std::string& key, int defaultValue = 0);
    static float    GetFloat(const std::string& key, float defaultValue = 0.0f);
    static bool     GetBool(const std::string& key, bool defaultValue = false);
    static std::string  GetString(const std::string& key, const std::string& defaultValue = "");

    static void     Save();
    static void     Load();
    static void     DeleteAll();

private:
    static json     s_Data;
    static const std::string    s_FilePath;
};
