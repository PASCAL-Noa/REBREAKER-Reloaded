#pragma once
#include "ECS/System.h"

class AnimatorSystem : public System
{
public:
    AnimatorSystem(Registry& registry);
    ~AnimatorSystem() override = default;

    void OnUpdate(float dt) override;
};
