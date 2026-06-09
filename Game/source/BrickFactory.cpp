#include "Factories/BrickFactory.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"

Entity BrickFactory::Create(Registry& registry, float x, float y, BrickType type, bool isSpecial, uint32_t texId)
{
    Entity brick = registry.CreateEntity();
    registry.AddComponent<Transform2D>(brick, Transform2D{Vector2f{x, y}});
    registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{100.0f, 30.0f}, Vector2f{0.0f, 0.0f}, false, false});
    registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    registry.AddComponent<TweenComponent>(brick, TweenComponent{});

    BrickComponent comp;
    comp.Type = type;
    comp.IsSpecial = isSpecial;
    Color tint = Colors::White;

    switch (type)
    {
        case BrickType::Light:   comp.HitPoints = 1; comp.ScoreValue = 50;  tint = Colors::Green; break;
        case BrickType::Medium:  comp.HitPoints = 2; comp.ScoreValue = 100; tint = Colors::Yellow; break;
        case BrickType::Hard:    comp.HitPoints = 3; comp.ScoreValue = 200; tint = Colors::Red; break;
        case BrickType::Special: comp.HitPoints = 1; comp.ScoreValue = 500; comp.IsSpecial = true; tint = Color{255, 0, 255, 255}; break;
        default:                 tint = Colors::White;
    }

    registry.AddComponent<BrickComponent>(brick, comp);
    registry.AddComponent<SpriteComponent>(brick, SpriteComponent{texId, tint});

    return brick;
}
