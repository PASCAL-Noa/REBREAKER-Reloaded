#pragma once
#include "ECS/Registry.hpp"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include <cstdint>

class BrickFactory
{
public:
    static Entity Create(Registry& registry, float x, float y, BrickType type, bool isSpecial, uint32_t texId);
};