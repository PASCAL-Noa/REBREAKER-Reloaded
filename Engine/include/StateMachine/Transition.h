#pragma once
#include "Condition.h"

template<typename T>
class Transition
{
public:
    Transition(Condition<T>* condition, int targetState) : m_condition(condition), m_targetState(targetState) {}

    ~Transition()
    {
        delete m_condition;
    }

    bool Check(T* pOwner)
    {
        if (!m_condition) return false;
        return m_condition->Test(pOwner);
    }

    int GetTargetState() const
    {
        return m_targetState;
    }

private:
    Condition<T>* m_condition;
    int m_targetState;
};