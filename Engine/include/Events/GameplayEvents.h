#pragma once
#include "ECS/Entity.h"
#include "Events/Event.h"

struct BrickHitEvent : public Event
{
    Entity  BrickEntity;
    bool    IsDestroyed;
    bool    IsSpecial;
    uint32_t    Combo;

    BrickHitEvent(const Entity brick, const bool destroyed, const bool special, const uint32_t combo)
        : BrickEntity(brick), IsDestroyed(destroyed), IsSpecial(special), Combo(combo) {}
};

struct PaddleHitEvent : public Event
{
    Entity PaddleEntity;
    Entity BallEntity;

    PaddleHitEvent(const Entity paddle, const Entity ball)
        : PaddleEntity(paddle), BallEntity(ball) {}
};

struct BallDeathEvent : public Event
{
    Entity BallEntity;

    explicit BallDeathEvent(const Entity ball)
        : BallEntity(ball) {}
};

struct PowerUpEvent : public Event
{
    Entity PowerUpEntity;

    explicit PowerUpEvent(const Entity powerUp)
        : PowerUpEntity(powerUp) {}
};