#pragma once
#include "StateMachine/Condition.h"

template<typename T>
class LivesCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override {return pOwner->GetLives() <= 0; }
};

template<typename T>
class VictoryCondition : public Condition<T>
{
public:
    bool OnTest(T* pOwner) override {return pOwner->GetBrickCount() <= 0; }
};
