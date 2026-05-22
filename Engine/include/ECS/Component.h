#pragma once
#include "Entity.h"

struct Component
{
    Entity Owner = MAX_ENTITIES;
    bool IsEnabled = true;

    Component() = default;
    explicit Component(Entity owner) : Owner(owner) {}
};