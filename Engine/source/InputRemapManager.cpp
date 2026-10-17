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
    // Default QWERTY / AZERTY friendly keyboard & standard Gamepad bindings:
    m_bindings[static_cast<size_t>(InputAction::MoveLeft)]   = { KeyCode::Q, KeyCode::Left, 4 };   // LB
    m_bindings[static_cast<size_t>(InputAction::MoveRight)]  = { KeyCode::D, KeyCode::Right, 5 };  // RB
    m_bindings[static_cast<size_t>(InputAction::LaunchBall)] = { KeyCode::Space, KeyCode::Enter, 0 }; // A
    m_bindings[static_cast<size_t>(InputAction::FireLaser)]  = { KeyCode::Space, KeyCode::Up, 0 };  // A
    m_bindings[static_cast<size_t>(InputAction::Pause)]      = { KeyCode::Escape, KeyCode::P, 7 };  // Start / Menu
}

void InputRemapManager::LoadFromPrefs()
{
    for (size_t i = 0; i < static_cast<size_t>(InputAction::Count); ++i)
    {
        auto action = static_cast<InputAction>(i);
        std::string actName(KeyCodeUtils::ActionToString(action));
        std::string primKeyName = "Input_" + actName + "_Primary";
        std::string secKeyName = "Input_" + actName + "_Secondary";
        std::string joyBtnName = "Input_" + actName + "_JoyBtn";

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

        m_bindings[i].GamepadButton = PlayerPrefs::GetInt(joyBtnName, m_bindings[i].GamepadButton);
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
        std::string joyBtnName = "Input_" + actName + "_JoyBtn";

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

        PlayerPrefs::SetInt(joyBtnName, m_bindings[i].GamepadButton);
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

void InputRemapManager::SetGamepadBinding(InputAction action, int button)
{
    const size_t index = static_cast<size_t>(action);
    if (index < m_bindings.size())
    {
        m_bindings[index].GamepadButton = button;
    }
}

const InputRemapManager::ActionBinding& InputRemapManager::GetBinding(InputAction action) const
{
    return m_bindings[static_cast<size_t>(action)];
}

int InputRemapManager::GetGamepadBinding(InputAction action) const
{
    return m_bindings[static_cast<size_t>(action)].GamepadButton;
}

std::string InputRemapManager::GamepadButtonToString(int button)
{
    switch (button)
    {
        case 0: return "A (BTN 0)";
        case 1: return "B (BTN 1)";
        case 2: return "X (BTN 2)";
        case 3: return "Y (BTN 3)";
        case 4: return "LB (BTN 4)";
        case 5: return "RB (BTN 5)";
        case 6: return "BACK / VIEW (BTN 6)";
        case 7: return "START / MENU (BTN 7)";
        case 8: return "L3 (STICK L)";
        case 9: return "R3 (STICK R)";
        case 100: return "D-PAD LEFT";
        case 101: return "D-PAD RIGHT";
        case 102: return "D-PAD UP";
        case 103: return "D-PAD DOWN";
        case 104: return "STICK LEFT";
        case 105: return "STICK RIGHT";
        case 106: return "STICK UP";
        case 107: return "STICK DOWN";
        case 108: return "LT (L TRIGGER)";
        case 109: return "RT (R TRIGGER)";
        default:
            if (button < 0) return "UNBOUND";
            return "BTN " + std::to_string(button);
    }
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
