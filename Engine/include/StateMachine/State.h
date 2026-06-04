#pragma once
#include <vector>
#include "Action.h"
#include "Transition.h"

template<typename T>
class State
{
public:
    State(T* pOwner) : m_owner(pOwner) {}

    ~State()
    {
        for (Action<T>* action : m_actions)
        {
            delete action;
        }
        for (Transition<T>* transition : m_transitions)
        {
            delete transition;
        }
    }

    void Start()
    {
        for (Action<T>* action : m_actions)
        {
            action->Start(m_owner);
        }
    }

    int Update()
    {
        for (Action<T>* action : m_actions)
        {
            action->Update(m_owner);
        }

        for (Transition<T>* transition : m_transitions)
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
        for (Action<T>* action : m_actions)
        {
            action->End(m_owner);
        }
    }

    void AddAction(Action<T>* action)
    {
        m_actions.push_back(action);
    }

    void AddTransition(Transition<T>* transition)
    {
        m_transitions.push_back(transition);
    }

private:
    T* m_owner;
    std::vector<Action<T>*> m_actions;
    std::vector<Transition<T>*> m_transitions;
};