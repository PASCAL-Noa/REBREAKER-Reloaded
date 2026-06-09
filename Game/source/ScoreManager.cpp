#include "ScoreManager.h"
#include <fstream>

constexpr float COMBO_DURATION = 3.0f;

ScoreManager::ScoreManager(const std::string& savePath) : m_savePath(savePath)
{
    LoadHighScore();
}

void ScoreManager::Update(float dt)
{
    if (m_comboMultiplier > 0)
    {
        m_comboTimer -= dt;
        if (m_comboTimer <= 0.0f)
        {
            BreakCombo();
        }
    }
}

void ScoreManager::AddScore(uint32_t score)
{
    m_comboMultiplier++;
    m_comboScore += score * m_comboMultiplier;
    m_comboTimer = COMBO_DURATION;
}

void ScoreManager::BreakCombo()
{
    m_score += m_comboScore;
    m_comboScore = 0;
    m_comboMultiplier = 0;
    m_comboTimer = 0.0f;

    if (m_score > m_highScore)
    {
        m_highScore = m_score;
        SaveHighScore();
    }
}

void ScoreManager::Reset()
{
    BreakCombo();
    m_score = 0;
}

uint32_t ScoreManager::GetScore() const { return m_score; }
uint32_t ScoreManager::GetComboMultiplier() const { return m_comboMultiplier; }
uint32_t ScoreManager::GetComboScore() const { return m_comboScore; }
uint32_t ScoreManager::GetHighScore() const { return m_highScore; }
float ScoreManager::GetComboTimer() const { return m_comboTimer; }

void ScoreManager::LoadHighScore()
{
    std::ifstream file(m_savePath);
    if (file.is_open())
    {
        file >> m_highScore;
    }
}

void ScoreManager::SaveHighScore()
{
    std::ofstream file(m_savePath);
    if (file.is_open())
    {
        file << m_highScore;
    }
}