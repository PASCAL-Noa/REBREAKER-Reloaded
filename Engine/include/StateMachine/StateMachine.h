#pragma once
#include <vector>
#include <memory>
#include "State.h"

template<typename T>
class StateMachine
{
public:
    StateMachine(T* pOwner, int stateCount) : m_owner(pOwner), m_currentState(-1)
    {
        m_states.resize(stateCount);
    }

    ~StateMachine() = default;

    State<T>* CreateState(int stateIndex)
    {
        m_states[stateIndex] = std::make_unique<State<T>>(m_owner);
        return m_states[stateIndex].get();
    }

    void SetState(int stateIndex)
    {
        if (m_currentState >= 0 && m_currentState < static_cast<int>(m_states.size()))
        {
            if (m_states[m_currentState])
            {
                m_states[m_currentState]->End();
            }
        }

        m_currentState = stateIndex;

        if (m_currentState >= 0 && m_currentState < static_cast<int>(m_states.size()))
        {
            if (m_states[m_currentState])
            {
                m_states[m_currentState]->Start();
            }
        }
    }

    void Update()
    {
        if (m_currentState < 0 || m_currentState >= static_cast<int>(m_states.size())) return;
        if (!m_states[m_currentState]) return;

        int nextState = m_states[m_currentState]->Update();

        if (nextState != -1 && nextState != m_currentState)
        {
            SetState(nextState);
        }
    }

    int GetCurrentState() const
    {
        return m_currentState;
    }

private:
    T* m_owner;
    std::vector<std::unique_ptr<State<T>>> m_states;
    int m_currentState;
};