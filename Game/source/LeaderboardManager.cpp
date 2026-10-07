#include "LeaderboardManager.h"
#include "Core/PlayerPrefs.h"
#include <algorithm>

LeaderboardManager::LeaderboardManager()
{
    Load();
}

void LeaderboardManager::ResetToDefaults()
{
    m_entries.clear();
    for (size_t i = 0; i < MAX_ENTRIES; ++i)
    {
        m_entries.push_back(LeaderboardEntry{
            .Name = "---",
            .Score = 0,
            .Level = 1
        });
    }
    Save();
}

void LeaderboardManager::Load()
{
    m_entries.clear();

    int version = PlayerPrefs::GetInt("Leaderboard_Schema_Version", 0);
    if (version < 2)
    {
        PlayerPrefs::SetInt("Leaderboard_Schema_Version", 2);
        ResetToDefaults();
        return;
    }

    bool hasAny = false;
    for (size_t i = 0; i < MAX_ENTRIES; ++i)
    {
        std::string prefix = "Leaderboard_" + std::to_string(i) + "_";
        std::string name = PlayerPrefs::GetString(prefix + "Name", "");
        int score = PlayerPrefs::GetInt(prefix + "Score", -1);
        int level = PlayerPrefs::GetInt(prefix + "Level", 1);

        if (!name.empty() && score >= 0)
        {
            m_entries.push_back(LeaderboardEntry{
                .Name = name,
                .Score = static_cast<uint32_t>(score),
                .Level = level
            });
            hasAny = true;
        }
    }

    if (!hasAny || m_entries.empty())
    {
        ResetToDefaults();
    }
    else
    {
        std::sort(m_entries.begin(), m_entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
            if (a.Name != "---" && b.Name == "---") return true;
            if (a.Name == "---" && b.Name != "---") return false;
            if (a.Score != b.Score) return a.Score > b.Score;
            return a.Level > b.Level;
        });
        while (m_entries.size() < MAX_ENTRIES)
        {
            m_entries.push_back(LeaderboardEntry{"---", 0, 1});
        }
        if (m_entries.size() > MAX_ENTRIES)
        {
            m_entries.resize(MAX_ENTRIES);
        }
    }
}

void LeaderboardManager::Save()
{
    for (size_t i = 0; i < MAX_ENTRIES; ++i)
    {
        std::string prefix = "Leaderboard_" + std::to_string(i) + "_";
        if (i < m_entries.size())
        {
            PlayerPrefs::SetString(prefix + "Name", m_entries[i].Name);
            PlayerPrefs::SetInt(prefix + "Score", static_cast<int>(m_entries[i].Score));
            PlayerPrefs::SetInt(prefix + "Level", m_entries[i].Level);
        }
        else
        {
            PlayerPrefs::SetString(prefix + "Name", "---");
            PlayerPrefs::SetInt(prefix + "Score", 0);
            PlayerPrefs::SetInt(prefix + "Level", 1);
        }
    }
    PlayerPrefs::Save();
}

bool LeaderboardManager::Qualifies(uint32_t score) const
{
    if (m_entries.size() < MAX_ENTRIES) return true;
    for (const auto& entry : m_entries)
    {
        if (entry.Name == "---") return true;
    }
    return score > m_entries.back().Score;
}

int LeaderboardManager::AddOrUpdateEntry(const std::string& oldName, const std::string& newName, uint32_t score, int level)
{
    std::string safeNew = newName.empty() ? "PLAYER" : newName;
    if (safeNew.length() > 12)
    {
        safeNew = safeNew.substr(0, 12);
    }

    bool updated = false;

    // 1. If oldName was specified and exists, update it
    if (!oldName.empty())
    {
        for (auto& entry : m_entries)
        {
            if (entry.Name == oldName)
            {
                entry.Name = safeNew;
                entry.Score = score;
                entry.Level = level;
                updated = true;
                break;
            }
        }
    }

    // 2. If same name already exists in leaderboard, update if score >= existing
    if (!updated)
    {
        for (auto& entry : m_entries)
        {
            if (entry.Name == safeNew)
            {
                if (score >= entry.Score)
                {
                    entry.Score = score;
                    entry.Level = level;
                }
                updated = true;
                break;
            }
        }
    }

    // 3. Replace an empty/placeholder slot if available
    if (!updated)
    {
        for (auto& entry : m_entries)
        {
            if (entry.Name == "---")
            {
                entry.Name = safeNew;
                entry.Score = score;
                entry.Level = level;
                updated = true;
                break;
            }
        }
    }

    // 4. Otherwise add and let sorting decide
    if (!updated)
    {
        m_entries.push_back(LeaderboardEntry{
            .Name = safeNew,
            .Score = score,
            .Level = level
        });
    }

    // Sort: real names first, higher score first, higher level first
    std::sort(m_entries.begin(), m_entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
        if (a.Name != "---" && b.Name == "---") return true;
        if (a.Name == "---" && b.Name != "---") return false;
        if (a.Score != b.Score) return a.Score > b.Score;
        return a.Level > b.Level;
    });

    if (m_entries.size() > MAX_ENTRIES)
    {
        m_entries.resize(MAX_ENTRIES);
    }

    int rank = -1;
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries[i].Name == safeNew)
        {
            rank = static_cast<int>(i + 1);
            break;
        }
    }

    Save();
    return rank;
}

int LeaderboardManager::AddEntry(const std::string& name, uint32_t score, int level)
{
    return AddOrUpdateEntry("", name, score, level);
}
