#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct LeaderboardEntry
{
    std::string Name;
    uint32_t Score = 0;
    int Level = 1;
};

class LeaderboardManager
{
public:
    static constexpr size_t MAX_ENTRIES = 5;

    LeaderboardManager();

    void Load();
    void Save();

    [[nodiscard]] const std::vector<LeaderboardEntry>& GetEntries() const { return m_entries; }
    [[nodiscard]] bool Qualifies(uint32_t score) const;
    int AddEntry(const std::string& name, uint32_t score, int level);
    int AddOrUpdateEntry(const std::string& oldName, const std::string& newName, uint32_t score, int level);
    void ResetToDefaults();

private:
    std::vector<LeaderboardEntry> m_entries;
};
