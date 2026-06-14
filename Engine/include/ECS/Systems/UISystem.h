#pragma once
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Math/Vector2.h"
#include "ECS/Components/UI/RectTransform.h"

class UISystem
{
public:
    UISystem() = default;
    ~UISystem() = default;

    static void OnUpdate(float dt, Registry& registry, const GameContext& context);
    static void OnRender(Registry& registry, const GameContext& context);

private:
    static Vector2f GetAbsolutePosition(Registry& registry, Entity entity, const RectTransform& transform, const Vector2f& viewSize);
    static bool IsPointInsideRect(const Vector2f& point, const Vector2f& rectPos, const Vector2f& rectSize) ;
};
