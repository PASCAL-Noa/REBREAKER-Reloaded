#pragma once
#include "StateMachine/Action.h"

template<typename T>
class VictoryAction : public Action<T>
{
public:
    void Start(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnVictoryEnter();
        }
    }

    void Update(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnVictoryUpdate();
        }
    }

    void End(T* pOwner) override
    {
        if (pOwner)
        {
            pOwner->OnVictoryExit();
        }
    }
};
