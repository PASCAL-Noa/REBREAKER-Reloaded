#pragma once
#include "Core/InputActions.h"
#include <array>
#include <utility>

class InputRemapManager
{
public:
    struct ActionBinding
    {
        KeyCode PrimaryKey = KeyCode::Count;
        KeyCode SecondaryKey = KeyCode::Count;
    };

    static InputRemapManager& Get();

    void LoadDefaults();
    void LoadFromPrefs();
    void SaveToPrefs();

    void SetBinding(InputAction action, KeyCode primary, KeyCode secondary = KeyCode::Count);
    [[nodiscard]] const ActionBinding& GetBinding(InputAction action) const;

    [[nodiscard]] bool IsActionActive(InputAction action, const class InputManager& input) const;
    [[nodiscard]] bool IsActionTriggered(InputAction action, const class InputManager& input) const;

private:
    InputRemapManager();
    std::array<ActionBinding, static_cast<size_t>(InputAction::Count)> m_bindings;
};
