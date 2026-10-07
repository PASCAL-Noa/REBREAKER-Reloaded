#pragma once

#include <string>
#include <vector>
#include <cstdint>

class LevelManager
{
public:
    LevelManager();
    explicit LevelManager(const std::string& levelsDirectory);

    void Initialize(const std::string& levelsDirectory = "Resources/levels");

    // Level Navigation
    [[nodiscard]] int GetCurrentLevelIndex() const { return m_currentLevelIndex; } // 0-based: 0, 1, 2...
    [[nodiscard]] int GetCurrentLevelNumber() const { return m_currentLevelIndex + 1; } // 1-based: 1, 2, 3...
    [[nodiscard]] int GetLevelCount() const { return static_cast<int>(m_levelFiles.size()); }
    [[nodiscard]] const std::string& GetCurrentLevelPath() const;
    [[nodiscard]] const std::string& GetLevelPath(int index) const;

    [[nodiscard]] bool HasNextLevel() const;
    bool NextLevel();
    void SetLevel(int index);
    void ResetToFirstLevel();

    // Progression & Unlocks (PlayerPrefs persisted)
    [[nodiscard]] int GetUnlockedLevel() const; // 1-based (highest unlocked level)
    void UnlockLevel(int levelNumber);
    [[nodiscard]] bool IsLevelUnlocked(int levelNumber) const;

    // High Scores per level (PlayerPrefs persisted)
    [[nodiscard]] uint32_t GetHighScoreForLevel(int levelNumber) const;
    bool SetHighScoreForLevel(int levelNumber, uint32_t score);

    // Save & Load
    void LoadFromPrefs();
    void SaveToPrefs();

    // Direct configuration (for tests or custom playlists)
    void SetLevels(const std::vector<std::string>& levelFiles);

private:
    std::string m_levelsDirectory = "Resources/levels";
    std::vector<std::string> m_levelFiles;
    int m_currentLevelIndex = 0;
    int m_unlockedLevel = 1;
};
