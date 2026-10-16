#include "Core/InputRemapManager.h"
#include "Core/InputManager.h"
#include "Core/PlayerPrefs.h"

InputRemapManager& InputRemapManager::Get()
{
    static InputRemapManager instance;
    return instance;
}

InputRemapManager::InputRemapManager()
{
    LoadDefaults();
    LoadFromPrefs();
}

void InputRemapManager::LoadDefaults()
{
    // Default QWERTY / AZERTY friendly:
    // MoveLeft: Q (AZERTY A/Q) or Left arrow
    // MoveRight: D or Right arrow
    // LaunchBall: Space or Enter
    // FireLaser: Space or Up arrow
    // Pause: Escape or P
    m_bindings[static_cast<size_t>(InputAction::MoveLeft)] = { KeyCode::Q, KeyCode::Left };
    m_bindings[static_cast<size_t>(InputAction::MoveRight)] = { KeyCode::D, KeyCode::Right };
    m_bindings[static_cast<size_t>(InputAction::LaunchBall)] = { KeyCode::Space, KeyCode::Enter };
    m_bindings[static_cast<size_t>(InputAction::FireLaser)] = { KeyCode::Space, KeyCode::Up };
    m_bindings[static_cast<size_t>(InputAction::Pause)] = { KeyCode::Escape, KeyCode::P };
}

void InputRemapManager::LoadFromPrefs()
{
    for (size_t i = 0; i < static_cast<size_t>(InputAction::Count); ++i)
    {
        auto action = static_cast<InputAction>(i);
        std::string actName(KeyCodeUtils::ActionToString(action));
        std::string primKeyName = "Input_" + actName + "_Primary";
        std::string secKeyName = "Input_" + actName + "_Secondary";

        std::string savedPrim = PlayerPrefs::GetString(primKeyName, "");
        if (!savedPrim.empty())
        {
            KeyCode k = KeyCodeUtils::FromString(savedPrim);
            if (k != KeyCode::Count)
            {
                m_bindings[i].PrimaryKey = k;
            }
        }

        std::string savedSec = PlayerPrefs::GetString(secKeyName, "");
        if (!savedSec.empty())
        {
            KeyCode k = KeyCodeUtils::FromString(savedSec);
            if (k != KeyCode::Count)
            {
                m_bindings[i].SecondaryKey = k;
            }
        }
    }
}

void InputRemapManager::SaveToPrefs()
{
    for (size_t i = 0; i < static_cast<size_t>(InputAction::Count); ++i)
    {
        auto action = static_cast<InputAction>(i);
        std::string actName(KeyCodeUtils::ActionToString(action));
        std::string primKeyName = "Input_" + actName + "_Primary";
        std::string secKeyName = "Input_" + actName + "_Secondary";

        if (m_bindings[i].PrimaryKey != KeyCode::Count)
        {
            PlayerPrefs::SetString(primKeyName, std::string(KeyCodeUtils::ToString(m_bindings[i].PrimaryKey)));
        }
        else
        {
            PlayerPrefs::SetString(primKeyName, "");
        }

        if (m_bindings[i].SecondaryKey != KeyCode::Count)
        {
            PlayerPrefs::SetString(secKeyName, std::string(KeyCodeUtils::ToString(m_bindings[i].SecondaryKey)));
        }
        else
        {
            PlayerPrefs::SetString(secKeyName, "");
        }
    }
    PlayerPrefs::Save();
}

void InputRemapManager::SetBinding(InputAction action, KeyCode primary, KeyCode secondary)
{
    const size_t index = static_cast<size_t>(action);
    if (index < m_bindings.size())
    {
        m_bindings[index].PrimaryKey = primary;
        m_bindings[index].SecondaryKey = secondary;
    }
}

const InputRemapManager::ActionBinding& InputRemapManager::GetBinding(InputAction action) const
{
    return m_bindings[static_cast<size_t>(action)];
}

bool InputRemapManager::IsActionActive(InputAction action, const InputManager& input) const
{
    const auto& b = GetBinding(action);
    if (b.PrimaryKey != KeyCode::Count && input.IsKeyDown(b.PrimaryKey))
    {
        return true;
    }
    if (b.SecondaryKey != KeyCode::Count && input.IsKeyDown(b.SecondaryKey))
    {
        return true;
    }
    return false;
}

bool InputRemapManager::IsActionTriggered(InputAction action, const InputManager& input) const
{
    const auto& b = GetBinding(action);
    if (b.PrimaryKey != KeyCode::Count && input.IsKeyPress(b.PrimaryKey))
    {
        return true;
    }
    if (b.SecondaryKey != KeyCode::Count && input.IsKeyPress(b.SecondaryKey))
    {
        return true;
    }
    return false;
}
