#pragma once
#include "StateMachine/Action.h"

template<typename T>
class ResetGameAction : public Action<T>
{
public:
    void    Start(T* pOwner) override {}
    void    Update(T* pOwner) override {}
    void    End(T* pOwner) override {pOwner->FullReset(); }
};