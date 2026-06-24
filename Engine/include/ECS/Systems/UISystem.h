#pragma once
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "ECS/Components/Transform2D.h"
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
    static bool IsPointInsideRect(const Vector2f& point, const Vector2f& rectPos, const Vector2f& rectSize);
    static bool IsParentEnabled(Registry& registry, Entity entity);

    static void UpdateButtons(Registry& registry, const GameContext& context, const Vector2f& logicalMousePos, const Vector2f& viewSize);
    static void UpdateDropdowns(Registry& registry, const GameContext& context, const Vector2f& logicalMousePos, const Vector2f& viewSize);

    static void RenderPanel(Registry& registry, const GameContext& context, Entity entity, const RectTransform& transform, Transform2D& drawTransform);
    static void RenderButton(Registry& registry, const GameContext& context, Entity entity, const RectTransform& transform, Transform2D& drawTransform);
    static void RenderSprite(Registry& registry, const GameContext& context, Entity entity, const RectTransform& transform, Transform2D& drawTransform);
    static void RenderText(Registry& registry, const GameContext& context, Entity entity, const Vector2f& absolutePos);
};
