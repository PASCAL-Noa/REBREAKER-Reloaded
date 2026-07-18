#include "CheatManager.h"

#include "AudioMixer.h"
#include "Core/GameRules.h"
#include "Data/Color.h"
#include "Events/CheatSubmitEvent.h"
#include "Events/EventDispatcher.h"
#include "Math/EasingFunctions.h"

CheatManager::CheatManager(const GameContext& context)
    : mp_context(&context)
{
    m_konamiSequence = {
        KeyCode::Up, KeyCode::Up, KeyCode::Down, KeyCode::Down,
        KeyCode::Left, KeyCode::Right, KeyCode::Left, KeyCode::Right,
        KeyCode::B, KeyCode::A
    };
    
    m_sfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup.wav");
    m_errorSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_despawn.wav");
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_scannerShaderId = context.Resources.LoadResource("Resources/shaders/scanner.frag");

    m_cheatSubId = context.Events.Subscribe<CheatSubmitEvent>([this, &context](const CheatSubmitEvent& e) {
        TryCheat(e.CheatCode, context);
    });
}

CheatManager::~CheatManager()
{
    if (mp_context)
    {
        const_cast<GameContext*>(mp_context)->Events.Unsubscribe(GetEventId<CheatSubmitEvent>(), m_cheatSubId);
    }
}

void CheatManager::Update(float dt, const GameContext& context)
{
    if (m_cheatTimer > 0.0f)
        m_cheatTimer -= dt;

    if (!m_konamiSequence.empty())
    {
        if (context.Input.IsKeyPress(m_konamiSequence[m_konamiIndex]))
        {
            m_konamiIndex++;
            if (m_konamiIndex >= m_konamiSequence.size())
            {
                m_konamiIndex = 0;
                if (!m_cheatTabUnlocked)
                {
                    m_cheatTabUnlocked = true;
                    context.Rules.SetRule(Rule::Gameplay::CheatsUnlocked, true);
                    m_activeCheatName = "CHEATS UNLOCKED";
                    m_cheatWasActivated = true;
                    m_cheatTimer = 5.0f;
                    if (m_sfxId != 0) context.Audio.PlaySfx(m_sfxId, 100.0f);
                }
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

        Color scannerColor = m_cheatWasActivated ? Color{0, 255, 0, 255} : Color{255, 0, 0, 255};
        Transform2D bgTransform{{viewSize.X / 2.0f, viewSize.Y / 2.0f}};
        
        float elapsedTime = 5.0f - m_cheatTimer;
        if (elapsedTime <= 2.5f)
        {
            float p = std::min(elapsedTime / 2.5f, 1.0f);
            float progress = EasingFunctions::GetFunction(EasingFunctions::EasingType::EaseOutQuart)(p);
            
            if (m_scannerShaderId != 0)
            {
                renderer.DrawScannerEffect(viewSize.X, viewSize.Y, bgTransform, scannerColor, progress, m_scannerShaderId);
            }
            else
            {
                const float alpha = (1.0f - progress) * 100.0f;
                renderer.DrawRectangle(viewSize.X, viewSize.Y, bgTransform, Color{scannerColor.r, scannerColor.g, scannerColor.b, static_cast<uint8_t>(alpha)});
            }
        }
        
        Vector2f textSize = renderer.GetTextSize(m_activeCheatName, m_fontId, 80.0f);
        const Transform2D textTransform{{viewSize.X / 2.0f - textSize.X / 2.0f, viewSize.Y * 0.2f}};
        renderer.DrawText(m_activeCheatName, m_fontId, 80.0f, textTransform, Colors::Yellow);
    }
}

bool CheatManager::TryCheat(const std::string& code, const GameContext& context)
{
    std::string upperCode = code;
    for (char& c : upperCode) c = static_cast<char>(toupper(c));

    bool success = false;

    if (upperCode == "GOD")
    {
        const bool inv = context.Rules.GetRule(Rule::Gameplay::Invincible);
        context.Rules.SetRule(Rule::Gameplay::Invincible, !inv);
        m_activeCheatName = inv ? "GODMODE OFF" : "GODMODE ON";
        m_cheatWasActivated = !inv;
        success = true;
    }
    else if (upperCode == "VIEW")
    {
        const bool show = context.Rules.GetRule(Rule::Debug::ShowCollider);
        context.Rules.SetRule(Rule::Debug::ShowCollider, !show);
        m_activeCheatName = show ? "HITBOXES HIDDEN" : "HITBOXES SHOWN";
        m_cheatWasActivated = !show;
        success = true;
    }
    else if (upperCode == "SUPER")
    {
        const bool inf = context.Rules.GetRule(Rule::Gameplay::InfiniteLives);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, !inf);
        m_activeCheatName = inf ? "INFINITE LIVES OFF" : "INFINITE LIVES ON";
        m_cheatWasActivated = !inf;
        success = true;
    }

    if (success)
    {
        if (m_sfxId != 0) context.Audio.PlaySfx(m_sfxId, 100.0f);
        m_cheatTimer = 5.0f;
    }
    else
    {
        if (m_errorSfxId != 0) context.Audio.PlaySfx(m_errorSfxId, 100.0f);
    }

    return success;
}