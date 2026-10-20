#pragma once
#include "ECS/Entity.h"
#include "Events/Event.h"
#include "Math/Vector2.h"

struct BrickHitEvent : public Event
{
    Entity      BrickEntity;
    bool        IsDestroyed;
    bool        IsSpecial;
    uint32_t    Combo;
    Vector2f    ContactPoint;

    BrickHitEvent(const Entity brick, const bool destroyed, const bool special, const uint32_t combo, const Vector2f& contactPoint = Vector2f{0.0f, 0.0f})
        : BrickEntity(brick), IsDestroyed(destroyed), IsSpecial(special), Combo(combo), ContactPoint(contactPoint) {}
};

struct PaddleHitEvent : public Event
{
    Entity      PaddleEntity;
    Entity      BallEntity;
    Vector2f    ContactPoint;

    PaddleHitEvent(const Entity paddle, const Entity ball, const Vector2f& contactPoint = Vector2f{0.0f, 0.0f})
        : PaddleEntity(paddle), BallEntity(ball), ContactPoint(contactPoint) {}
};

struct BallDeathEvent : public Event
{
    Entity BallEntity;

    explicit BallDeathEvent(const Entity ball)
        : BallEntity(ball) {}
};

#include "ECS/Components/PowerUpComponent.h"

struct PowerUpEvent : public Event
{
    Entity      PowerUpEntity;
    PowerUpType Type;

    explicit PowerUpEvent(const Entity powerUp, const PowerUpType type = PowerUpType::MultiBall)
        : PowerUpEntity(powerUp), Type(type) {}
};

struct NextLevelEvent : public Event
{
};

enum class GameUIState
{
    Playing,
    Paused,
    GameOver,
    Victory,
    LevelTransition,
    Countdown
};

struct GameStateChangedEvent : public Event
{
    GameUIState State;

    explicit GameStateChangedEvent(GameUIState state)
        : State(state) {}
};

struct ScoreChangedEvent : public Event
{
    uint32_t Score;
    uint32_t Multiplier;

    ScoreChangedEvent(uint32_t score, uint32_t multiplier)
        : Score(score), Multiplier(multiplier) {}
};

struct LivesChangedEvent : public Event
{
    int Lives;

    explicit LivesChangedEvent(int lives)
        : Lives(lives) {}
};