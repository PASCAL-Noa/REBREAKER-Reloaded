#pragma once
#include "StateMachine/Action.h"

template<typename T>
class GameOverAction : public Action<T>
{
public:
    void Start(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnGameOverEnter();
        }
    }

    void Update(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnGameOverUpdate();
        }
    }

    void End(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnGameOverExit();
        }
    }
};
