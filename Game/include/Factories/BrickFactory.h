#pragma once
#include "ECS/Registry.hpp"
#include "ECS/Entity.h"
#include "ECS/Components/BrickComponent.h"
#include "Data/Color.h"
#include <cstdint>

class BrickFactory
{
public:
    static Color  GetColorForHitPoints(int hitPoints);
    static Entity Create(Registry& registry, float x, float y, BrickType type, bool isSpecial, uint32_t texId, uint8_t armorTier = 0, int customHitPoints = 0);
};