#pragma once
#include "Math/Vector2.h"
#include "ECS/Entity.h"

enum class Anchor
{
    TopLeft, TopCenter, TopRight,
    MiddleLeft, Center, MiddleRight,
    BottomLeft, BottomCenter, BottomRight
};

struct RectTransform
{
    Vector2f    Position{0.0f, 0.0f};
    Vector2f    Size{100.0f, 100.0f};
    Anchor    AnchorPoint = Anchor::Center;
    Entity    Parent = NULL_ENTITY;
    bool    IsActive = true;
};
