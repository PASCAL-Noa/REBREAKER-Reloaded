#pragma once

#include <string>
#include <cstdint>

class ScoreManager
{
public:
    explicit ScoreManager(const std::string& savePath);

    void    Update(float dt);
    void    AddScore(uint32_t score);
    void    BreakCombo();
    void    Reset();

    [[nodiscard]] uint32_t  GetScore() const;
    [[nodiscard]] uint32_t  GetComboMultiplier() const;
    [[nodiscard]] uint32_t  GetComboScore() const;
    [[nodiscard]] uint32_t  GetHighScore() const;
    [[nodiscard]] float     GetComboTimer() const;

private:
    void    LoadHighScore();
    void    SaveHighScore();

    std::string     m_savePath;
    uint32_t    m_score = 0;
    uint32_t    m_highScore = 0;
    uint32_t    m_comboScore = 0;
    uint32_t    m_comboMultiplier = 0;
    float   m_comboTimer = 0.0f;
};