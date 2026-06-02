#pragma once
#include "Events/Event.h"
#include "ECS/Entity.h"

struct CollisionEvent : public Event
{
    Entity EntityA;
    Entity EntityB;
    CollisionEvent(Entity entityA, Entity entityB) : EntityA(entityA), EntityB(entityB) {}
};