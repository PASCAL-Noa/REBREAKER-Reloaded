#pragma once
#include "ECS/System.h"

class TweenSystem : public System
{
public:
    explicit TweenSystem(Registry& registry);
    void    OnUpdate(float dt);
};