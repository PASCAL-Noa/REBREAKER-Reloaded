#include "LevelManager.h"
#include "Core/PlayerPrefs.h"
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

LevelManager::LevelManager()
{
    Initialize("Resources/levels");
}

LevelManager::LevelManager(const std::string& levelsDirectory)
{
    Initialize(levelsDirectory);
}

void LevelManager::Initialize(const std::string& levelsDirectory)
{
    m_levelsDirectory = levelsDirectory;
    m_levelFiles.clear();

    std::error_code ec;
    if (fs::exists(m_levelsDirectory, ec) && fs::is_directory(m_levelsDirectory, ec))
    {
        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator(m_levelsDirectory, ec))
        {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".txt")
            {
                std::string filename = entry.path().filename().string();
                if (filename.rfind("level", 0) == 0)
                {
                    paths.push_back(entry.path());
                }
            }
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& p : paths)
        {
            m_levelFiles.push_back(p.generic_string());
        }
    }

    if (m_levelFiles.empty())
    {
        for (int i = 1; i <= 5; ++i)
        {
            std::ostringstream oss;
            oss << m_levelsDirectory << "/level" << std::setw(2) << std::setfill('0') << i << ".txt";
            m_levelFiles.push_back(oss.str());
        }
    }

    m_currentLevelIndex = 0;
    LoadFromPrefs();
}

const std::string& LevelManager::GetCurrentLevelPath() const
{
    static const std::string fallback = "Resources/levels/level01.txt";
    if (m_currentLevelIndex >= 0 && m_currentLevelIndex < static_cast<int>(m_levelFiles.size()))
    {
        return m_levelFiles[m_currentLevelIndex];
    }
    return fallback;
}

const std::string& LevelManager::GetLevelPath(int index) const
{
    static const std::string fallback = "Resources/levels/level01.txt";
    if (index >= 0 && index < static_cast<int>(m_levelFiles.size()))
    {
        return m_levelFiles[index];
    }
    return fallback;
}

bool LevelManager::HasNextLevel() const
{
    return (m_currentLevelIndex + 1) < static_cast<int>(m_levelFiles.size());
}

bool LevelManager::NextLevel()
{
    if (HasNextLevel())
    {
        m_currentLevelIndex++;
        UnlockLevel(GetCurrentLevelNumber());
        SaveToPrefs();
        return true;
    }
    return false;
}

void LevelManager::SetLevel(int index)
{
    if (m_levelFiles.empty())
    {
        m_currentLevelIndex = 0;
        return;
    }
    m_currentLevelIndex = std::clamp(index, 0, static_cast<int>(m_levelFiles.size()) - 1);
}

void LevelManager::ResetToFirstLevel()
{
    m_currentLevelIndex = 0;
}

int LevelManager::GetUnlockedLevel() const
{
    return std::max(1, PlayerPrefs::GetInt("UnlockedLevel", m_unlockedLevel));
}

void LevelManager::UnlockLevel(int levelNumber)
{
    int currentUnlocked = GetUnlockedLevel();
    if (levelNumber > currentUnlocked)
    {
        m_unlockedLevel = levelNumber;
        PlayerPrefs::SetInt("UnlockedLevel", m_unlockedLevel);
        PlayerPrefs::Save();
    }
}

bool LevelManager::IsLevelUnlocked(int levelNumber) const
{
    return levelNumber <= GetUnlockedLevel();
}

uint32_t LevelManager::GetHighScoreForLevel(int levelNumber) const
{
    std::string key1 = "HighScore_Level_" + std::to_string(levelNumber);
    std::ostringstream oss;
    oss << "HighScore_Level_" << std::setw(2) << std::setfill('0') << levelNumber;
    std::string key2 = oss.str();

    int s1 = PlayerPrefs::GetInt(key1, 0);
    int s2 = PlayerPrefs::GetInt(key2, 0);
    return static_cast<uint32_t>(std::max(s1, s2));
}

bool LevelManager::SetHighScoreForLevel(int levelNumber, uint32_t score)
{
    uint32_t currentHigh = GetHighScoreForLevel(levelNumber);
    if (score > currentHigh)
    {
        std::string key1 = "HighScore_Level_" + std::to_string(levelNumber);
        std::ostringstream oss;
        oss << "HighScore_Level_" << std::setw(2) << std::setfill('0') << levelNumber;
        std::string key2 = oss.str();

        PlayerPrefs::SetInt(key1, static_cast<int>(score));
        PlayerPrefs::SetInt(key2, static_cast<int>(score));
        PlayerPrefs::Save();
        return true;
    }
    return false;
}

void LevelManager::LoadFromPrefs()
{
    m_unlockedLevel = PlayerPrefs::GetInt("UnlockedLevel", 1);
    if (m_unlockedLevel < 1) m_unlockedLevel = 1;
}

void LevelManager::SaveToPrefs()
{
    PlayerPrefs::SetInt("UnlockedLevel", m_unlockedLevel);
    PlayerPrefs::Save();
}

void LevelManager::SetLevels(const std::vector<std::string>& levelFiles)
{
    m_levelFiles = levelFiles;
    m_currentLevelIndex = 0;
}
