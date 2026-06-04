#pragma once
#include "StateMachine/Condition.h"
#include "Core/InputManager.h"

template<typename T>
class KeyPressCondition : public Condition<T>
{
public:
    KeyPressCondition(KeyCode key) : m_key(key) {}

    bool OnTest(T* pOwner) override
    {
        return pOwner->GetContext()->Input.IsKeyPress(m_key);
    }

private:
    KeyCode m_key;
};