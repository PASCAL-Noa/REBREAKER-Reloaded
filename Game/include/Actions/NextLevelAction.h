#pragma once
#include "StateMachine/Action.h"

template<typename T>
class NextLevelAction : public Action<T>
{
public:
    void Start(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->StartLevelTransition();
        }
    }

    void Update(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->UpdateLevelTransition();
        }
    }

    void End(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->CompleteLevelTransition();
        }
    }
};
