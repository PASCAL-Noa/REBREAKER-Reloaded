#pragma once
#include <string>
#include <vector>

struct LoreEntry
{
    std::string Id;
    std::string Title;
    std::string Chapter;
    std::string Content;
    bool        Unlocked = false;
};

class LoreManager
{
public:
    static LoreManager& Get();

    [[nodiscard]] const std::vector<LoreEntry>& GetAllEntries();
    [[nodiscard]] bool IsLoreUnlocked(const std::string& id) const;
    void UnlockLore(const std::string& id);
    [[nodiscard]] int GetUnlockedCount() const;
    [[nodiscard]] int GetTotalCount() const;
    void ResetToDefaults();

private:
    LoreManager();
    void InitEntries();

    std::vector<LoreEntry> m_entries;
};
