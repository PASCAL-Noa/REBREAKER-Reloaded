#pragma once
#include "StateMachine/Action.h"

template<typename T>
class UpdateGameplayAction : public Action<T>
{
public:
    void    Start(T* pOwner) override {}
    
    void    Update(T* pOwner) override
    {
        pOwner->UpdateGameplay();
    }
    
    void    End(T* pOwner) override {}
};