#pragma once
#include "Entity.h"
#include <vector>

class ISparseSet
{
public:
    virtual ~ISparseSet() = default;
    virtual void Remove(Entity entity) = 0;
    virtual size_t Size() const = 0;
    virtual const std::vector<Entity>& GetEntities() const = 0;
};