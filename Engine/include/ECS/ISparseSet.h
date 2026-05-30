#pragma once
#include "Entity.h"

class ISparseSet
{
public:
    virtual ~ISparseSet() = default;
    virtual void Remove(Entity entity) = 0;
};