#pragma once
#include "ECS/Component.h"
#include "Data/Color.h"

struct ColorComponent : public Component
{
    Color Tint = {255, 255, 255, 255};

    ColorComponent() = default;
    
    ColorComponent(Entity owner, Color color) 
        : Component(owner), Tint(color) {}
};