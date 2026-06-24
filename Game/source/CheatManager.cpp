#include "CheatManager.h"

#include "AudioMixer.h"
#include "Core/GameRules.h"
#include "Data/Color.h"

CheatManager::CheatManager(const GameContext& context)
{
    m_konamiSequence = {
        KeyCode::Up, KeyCode::Up, KeyCode::Down, KeyCode::Down,
        KeyCode::Left, KeyCode::Right, KeyCode::Left, KeyCode::Right,
        KeyCode::B, KeyCode::A
    };
    
    m_sfxId = context.Resources.LoadResource("Resources/audio/sfx/ToDelete/bassem.mp3");
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
}

void CheatManager::Update(float dt, const GameContext& context)
{
    if (m_cheatTimer > 0.0f) {
        m_cheatTimer -= dt;
    }

    if (!m_konamiSequence.empty())
    {
        if (context.Input.IsKeyPress(m_konamiSequence[m_konamiIndex]))
        {
            m_konamiIndex++;
            if (m_konamiIndex >= m_konamiSequence.size())
            {
                m_konamiIndex = 0;
                if (m_sfxId != 0) context.Audio.PlaySfx(m_sfxId, 100.0f);
                
                bool inv = context.Rules.GetRule(Rule::Gameplay::Invincible);
                context.Rules.SetRule(Rule::Gameplay::Invincible, !inv);
                m_cheatTimer = 3.0f;
            }
        }
        else
        {
            for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i)
            {
                if (context.Input.IsKeyPress(static_cast<KeyCode>(i)))
                {
                    m_konamiIndex = 0;
                    if (context.Input.IsKeyPress(m_konamiSequence[0])) {
                        m_konamiIndex = 1;
                    }
                    break;
                }
            }
        }
    }
}

void CheatManager::Render(Renderer& renderer) const
{
    if (m_cheatTimer > 0.0f)
    {
        renderer.ResetCamera();
        Vector2f viewSize = renderer.GetLogicalViewSize();

        float alpha = (std::sin(m_cheatTimer * 15.0f) * 0.5f + 0.5f) * 100.0f;
        Transform2D bgTransform{{viewSize.X / 2.0f, viewSize.Y / 2.0f}};
        renderer.DrawRectangle(viewSize.X, viewSize.Y, bgTransform, Color{255, 0, 0, static_cast<uint8_t>(alpha)});
        
        Vector2f textSize = renderer.GetTextSize("CHEAT UNLOCKED!", m_fontId, 80.0f);
        Transform2D textTransform{{viewSize.X / 2.0f - textSize.X / 2.0f, viewSize.Y * 0.2f}};
        renderer.DrawText("CHEAT UNLOCKED!", m_fontId, 80.0f, textTransform, Colors::Yellow);
    }
}
