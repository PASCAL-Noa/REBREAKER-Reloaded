#pragma once
#include <vector>
#include <memory>
#include "Action.h"
#include "Transition.h"

template<typename T>
class State
{
public:
    State(T* pOwner) : m_owner(pOwner) {}

    ~State() = default;

    void Start()
    {
        for (auto& action : m_actions)
        {
            action->Start(m_owner);
        }
    }

    int Update()
    {
        for (auto& action : m_actions)
        {
            action->Update(m_owner);
        }

        for (auto& transition : m_transitions)
        {
            if (transition->Check(m_owner))
            {
                return transition->GetTargetState();
            }
        }

        return -1;
    }

    void End()
    {
        for (auto& action : m_actions)
        {
            action->End(m_owner);
        }
    }

    void AddAction(Action<T>* action)
    {
        m_actions.push_back(std::unique_ptr<Action<T>>(action));
    }

    void AddTransition(Transition<T>* transition)
    {
        m_transitions.push_back(std::unique_ptr<Transition<T>>(transition));
    }

private:
    T* m_owner;
    std::vector<std::unique_ptr<Action<T>>> m_actions;
    std::vector<std::unique_ptr<Transition<T>>> m_transitions;
};