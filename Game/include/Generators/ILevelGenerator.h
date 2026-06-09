#pragma once
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include <cstdint>

class ILevelGenerator
{
public:
    virtual ~ILevelGenerator() = default;
    virtual int Generate(Registry& registry, GameContext& context, uint32_t brickTexId) = 0;
    virtual void Update(float dt, Registry& registry, GameContext& context) = 0;
};