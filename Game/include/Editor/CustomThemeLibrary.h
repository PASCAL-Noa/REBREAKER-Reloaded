#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <optional>
#include "Editor/LevelData.h"

class CustomThemeLibrary
{
public:
    explicit CustomThemeLibrary(std::string baseDir = "Resources/levels/custom")
        : m_baseDir(std::move(baseDir))
    {
        Load();
    }

    static std::string SanitizeName(const std::string& raw, const std::string& fallback = "CUSTOM THEME")
    {
        std::string out;
        out.reserve(raw.size());
        for (char ch : raw)
        {
            if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t')
            {
                out.push_back(' ');
            }
            else if (static_cast<unsigned char>(ch) >= 32)
            {
                out.push_back(ch);
            }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back() == ' ') out.pop_back();
        if (out.size() > 20) out.resize(20);
        return out.empty() ? fallback : out;
    }

    [[nodiscard]] std::string PeekNextThemeId() const
    {
        return FormatId("THM-", m_nextCounter, 3);
    }

    void Load()
    {
        m_themes.clear();
        m_nextCounter = 1;

        std::error_code ec;
        std::filesystem::create_directories(m_baseDir, ec);

        std::ifstream in(GetFilePath());
        if (!in.is_open()) return;

        std::string line;
        while (std::getline(in, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            const auto parts = SplitPipe(line);
            if (parts.empty()) continue;

            if (parts[0] == "COUNTER" && parts.size() >= 2)
            {
                try
                {
                    m_nextCounter = std::max(1, std::stoi(parts[1]));
                }
                catch (...) {}
            }
            else if (parts[0] == "THEME" && parts.size() >= 15)
            {
                try
                {
                    EditorThemeData thm;
                    thm.ThemeId = parts[1];
                    thm.Name = SanitizeName(parts[2], thm.ThemeId);
                    thm.Preset = static_cast<EditorThemePreset>(
                        std::clamp(std::stoi(parts[3]), 0, static_cast<int>(EditorThemePreset::Count) - 1));
                    thm.BgColor = ParseRgb(parts[4], Color{10, 14, 26, 255});
                    thm.BgSecondaryColor = ParseRgb(parts[5], Color{24, 38, 72, 255});
                    thm.AmbientColor = ParseRgb(parts[6], Color{55, 95, 165, 255});
                    thm.AmbientIntensity = std::stof(parts[7]);
                    thm.PaddleConeColor = ParseRgb(parts[8], Color{80, 200, 255, 255});
                    thm.PaddleConeAngle = std::stof(parts[9]);
                    thm.PaddleLightIntensity = std::stof(parts[10]);
                    thm.PaddleLightRange = std::stof(parts[11]);
                    thm.VignetteIntensity = std::stof(parts[12]);
                    thm.VignetteColor = ParseRgb(parts[13], Color{0, 0, 0, 255});
                    thm.MusicTrack = parts[14];
                    thm.MusicPlaylist = {thm.MusicTrack};
                    if (parts.size() >= 16)
                    {
                        thm.Lighting = static_cast<LightingMode>(std::clamp(std::stoi(parts[15]), 0, 2));
                    }
                    if (parts.size() >= 17 && !parts[16].empty())
                    {
                        std::vector<std::string> pl;
                        std::istringstream pss(parts[16]);
                        std::string item;
                        while (std::getline(pss, item, ','))
                        {
                            if (!item.empty()) pl.push_back(item);
                        }
                        if (!pl.empty()) thm.MusicPlaylist = std::move(pl);
                    }
                    thm.ClampValues();
                    m_themes.push_back(std::move(thm));
                }
                catch (...) {}
            }
        }
        EnsureCounterAboveExisting();
    }

    bool Save() const
    {
        std::error_code ec;
        std::filesystem::create_directories(m_baseDir, ec);

        std::ofstream out(GetFilePath());
        if (!out.is_open()) return false;

        out << "#REBREAKER_THEMES_V1\n";
        out << "COUNTER|" << m_nextCounter << "\n";
        for (const auto& thm : m_themes)
        {
            std::string plJoined;
            for (size_t i = 0; i < thm.MusicPlaylist.size(); ++i)
            {
                if (i > 0) plJoined.push_back(',');
                plJoined += thm.MusicPlaylist[i];
            }
            out << "THEME|" << thm.ThemeId << "|" << SanitizeName(thm.Name, thm.ThemeId) << "|"
                << static_cast<int>(thm.Preset) << "|"
                << FormatRgb(thm.BgColor) << "|"
                << FormatRgb(thm.BgSecondaryColor) << "|"
                << FormatRgb(thm.AmbientColor) << "|"
                << thm.AmbientIntensity << "|"
                << FormatRgb(thm.PaddleConeColor) << "|"
                << thm.PaddleConeAngle << "|"
                << thm.PaddleLightIntensity << "|"
                << thm.PaddleLightRange << "|"
                << thm.VignetteIntensity << "|"
                << FormatRgb(thm.VignetteColor) << "|"
                << thm.MusicTrack << "|"
                << static_cast<int>(thm.Lighting) << "|"
                << plJoined << "\n";
        }
        return true;
    }

    std::string CreateTheme(const std::string& rawName, const EditorThemeData& source)
    {
        EnsureCounterAboveExisting();
        EditorThemeData copy = source;
        copy.ThemeId = FormatId("THM-", m_nextCounter++, 3);
        copy.Name = SanitizeName(rawName, "THEME " + std::to_string(m_themes.size() + 1));
        copy.ClampValues();
        m_themes.push_back(copy);
        Save();
        return copy.ThemeId;
    }

    bool UpdateTheme(const std::string& id, const EditorThemeData& updated)
    {
        auto it = FindIter(id);
        if (it == m_themes.end()) return false;

        EditorThemeData copy = updated;
        copy.ThemeId = it->ThemeId;
        copy.Name = SanitizeName(updated.Name.empty() ? it->Name : updated.Name, it->Name);
        copy.ClampValues();
        *it = copy;
        return Save();
    }

    bool RenameTheme(const std::string& id, const std::string& newName)
    {
        auto it = FindIter(id);
        if (it == m_themes.end()) return false;

        it->Name = SanitizeName(newName, it->Name);
        return Save();
    }

    bool DeleteTheme(const std::string& id)
    {
        auto it = FindIter(id);
        if (it == m_themes.end()) return false;

        m_themes.erase(it);
        return Save();
    }

    [[nodiscard]] const std::vector<EditorThemeData>& GetThemes() const noexcept { return m_themes; }

    [[nodiscard]] const EditorThemeData* FindTheme(const std::string& id) const
    {
        for (const auto& t : m_themes)
        {
            if (t.ThemeId == id) return &t;
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<size_t> FindThemeIndex(const std::string& id) const
    {
        for (size_t i = 0; i < m_themes.size(); ++i)
        {
            if (m_themes[i].ThemeId == id) return i;
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] std::string GetFilePath() const
    {
        return m_baseDir + "/themes.cfg";
    }

    std::vector<EditorThemeData>::iterator FindIter(const std::string& id)
    {
        return std::find_if(m_themes.begin(), m_themes.end(), [&](const EditorThemeData& t) {
            return t.ThemeId == id;
        });
    }

    void EnsureCounterAboveExisting()
    {
        for (const auto& t : m_themes)
        {
            if (t.ThemeId.rfind("THM-", 0) == 0)
            {
                try
                {
                    int n = std::stoi(t.ThemeId.substr(4));
                    m_nextCounter = std::max(m_nextCounter, n + 1);
                }
                catch (...) {}
            }
        }
    }

    static std::string FormatId(const std::string& prefix, int value, int width)
    {
        std::ostringstream oss;
        oss << prefix << std::setw(width) << std::setfill('0') << value;
        return oss.str();
    }

    static std::string FormatRgb(const Color& c)
    {
        return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b);
    }

    static Color ParseRgb(const std::string& str, Color fallback)
    {
        std::istringstream ss(str);
        std::string part;
        int vals[3] = {fallback.r, fallback.g, fallback.b};
        int i = 0;
        while (std::getline(ss, part, ',') && i < 3)
        {
            try
            {
                vals[i] = std::clamp(std::stoi(part), 0, 255);
            }
            catch (...) {}
            ++i;
        }
        return Color{static_cast<uint8_t>(vals[0]), static_cast<uint8_t>(vals[1]), static_cast<uint8_t>(vals[2]), 255};
    }

    static std::vector<std::string> SplitPipe(const std::string& line)
    {
        std::vector<std::string> out;
        std::istringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '|'))
        {
            out.push_back(item);
        }
        return out;
    }

    std::string m_baseDir;
    std::vector<EditorThemeData> m_themes;
    int m_nextCounter = 1;
};
