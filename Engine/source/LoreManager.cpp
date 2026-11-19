#include "Core/LoreManager.h"
#include "Core/PlayerPrefs.h"

LoreManager& LoreManager::Get()
{
    static LoreManager instance;
    return instance;
}

LoreManager::LoreManager()
{
    InitEntries();
}

void LoreManager::InitEntries()
{
    m_entries = {
        {
            "lore_ch1",
            "THE ARCHITECT'S PROTOCOL",
            "CHAPTER 01",
            "Initial surveillance logs reveal that the defensive matrix wasn't designed to destroy intruders, but to test them. The paddle's durability alloy is reactive to anomalous energy discharges.",
            false
        },
        {
            "lore_ch2",
            "THE NEURAL CORRUPTION",
            "CHAPTER 02",
            "A divergent sub-routine known as OMEGA began re-architecting the brick lattice into self-repairing defensive arrays, feeding on shattered energy orbs.",
            false
        },
        {
            "lore_ch3",
            "OMEGA-01 AWAKENING",
            "CHAPTER 03",
            "Confrontation telemetry confirms OMEGA-01's synthesis of laser arrays and bullet hell dispersion canons. Only concentrated kinetic deflection against its exposed reactor core can rupture its hull.",
            false
        },
        {
            "lore_boss_clear",
            "THE FRACTURED NEXUS",
            "EPILOGUE",
            "With OMEGA dismantled, the grid enters stabilization. The pilot's chassis has been upgraded with the legendary Void Obsidian hull plating, a testament to surviving the campaign.",
            false
        }
    };

    for (auto& entry : m_entries)
    {
        entry.Unlocked = PlayerPrefs::GetBool("Lore_Unlocked_" + entry.Id, false);
    }
}

const std::vector<LoreEntry>& LoreManager::GetAllEntries()
{
    for (auto& entry : m_entries)
    {
        entry.Unlocked = PlayerPrefs::GetBool("Lore_Unlocked_" + entry.Id, false);
    }
    return m_entries;
}

bool LoreManager::IsLoreUnlocked(const std::string& id) const
{
    for (const auto& entry : m_entries)
    {
        if (entry.Id == id)
        {
            return entry.Unlocked;
        }
    }
    return false;
}

void LoreManager::UnlockLore(const std::string& id)
{
    for (auto& entry : m_entries)
    {
        if (entry.Id == id)
        {
            entry.Unlocked = true;
            PlayerPrefs::SetBool("Lore_Unlocked_" + id, true);
            PlayerPrefs::Save();
            return;
        }
    }
}

int LoreManager::GetUnlockedCount() const
{
    int count = 0;
    for (const auto& entry : m_entries)
    {
        if (entry.Unlocked) ++count;
    }
    return count;
}

int LoreManager::GetTotalCount() const
{
    return static_cast<int>(m_entries.size());
}

void LoreManager::ResetToDefaults()
{
    for (auto& entry : m_entries)
    {
        entry.Unlocked = false;
        PlayerPrefs::SetBool("Lore_Unlocked_" + entry.Id, false);
    }
    PlayerPrefs::Save();
}
