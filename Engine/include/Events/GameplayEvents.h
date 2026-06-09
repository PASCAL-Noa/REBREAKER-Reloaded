#pragma once
#include "ECS/Entity.h"
#include "Events/Event.h"

struct BrickHitEvent : public Event
{
    Entity BrickEntity;
    bool IsDestroyed;
    bool IsSpecial;

    BrickHitEvent(Entity brick, bool destroyed, bool special)
        : BrickEntity(brick), IsDestroyed(destroyed), IsSpecial(special) {}
};

struct PaddleHitEvent : public Event
{
    Entity PaddleEntity;
    Entity BallEntity;

    PaddleHitEvent(Entity paddle, Entity ball)
        : PaddleEntity(paddle), BallEntity(ball) {}
};

struct BallDeathEvent : public Event
{
    Entity BallEntity;

    explicit BallDeathEvent(Entity ball)
        : BallEntity(ball) {}
};

struct PowerUpEvent : public Event
{
    Entity PowerUpEntity;

    explicit PowerUpEvent(Entity powerUp)
        : PowerUpEntity(powerUp) {}
};