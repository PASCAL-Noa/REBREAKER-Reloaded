#include "Factories/BrickFactory.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include <algorithm>

Color BrickFactory::GetColorForHitPoints(int hitPoints)
{
    const int clamped = std::clamp(hitPoints, 1, 9);
    switch (clamped)
    {
        case 1:  return Color{65, 225, 95, 255};   // 1 PV : Emerald Green
        case 2:  return Color{250, 215, 45, 255};  // 2 PV : Solar Yellow
        case 3:  return Color{245, 65, 65, 255};   // 3 PV : Crimson Red
        case 4:  return Color{255, 135, 35, 255};  // 4 PV : Blaze Orange
        case 5:  return Color{40, 225, 255, 255};  // 5 PV : Electric Cyan
        case 6:  return Color{55, 125, 255, 255};  // 6 PV : Royal Azure
        case 7:  return Color{175, 70, 255, 255};  // 7 PV : Plasma Violet
        case 8:  return Color{255, 65, 195, 255};  // 8 PV : Neon Magenta
        case 9:  return Color{255, 235, 140, 255}; // 9 PV : Radiant Diamond Gold
        default: return Colors::White;
    }
}

Entity BrickFactory::Create(Registry& registry, float x, float y, BrickType type, bool isSpecial, uint32_t texId, uint8_t armorTier, int customHitPoints)
{
    Entity brick = registry.CreateEntity();
    registry.AddComponent<Transform2D>(brick, Transform2D{Vector2f{x, y}});
    registry.AddComponent<BoxCollider>(brick, BoxCollider{Vector2f{100.0f, 30.0f}, Vector2f{0.0f, 0.0f}, false, false});
    registry.AddComponent<RigidBody>(brick, RigidBody{Vector2f{0.0f, 0.0f}, 1.0f, 1.0f, true});
    registry.AddComponent<TweenComponent>(brick, TweenComponent{});

    BrickComponent comp;
    comp.Type = type;
    comp.IsSpecial = isSpecial;
    comp.ArmorTier = armorTier;
    Color tint = Colors::White;

    switch (type)
    {
        case BrickType::Light:
            comp.HitPoints = 1 + armorTier;
            comp.ScoreValue = 50 + armorTier * 25;
            if (armorTier == 0) tint = Colors::Green;
            else if (armorTier == 1) tint = Color{50, 240, 255, 255};      // Electric Cyan
            else if (armorTier == 2) tint = Color{255, 120, 240, 255};     // Amethyst
            else tint = Color{255, 195, 50, 255};                           // Solar Gold
            break;

        case BrickType::Medium:
            comp.HitPoints = 2 + armorTier;
            comp.ScoreValue = 100 + armorTier * 50;
            if (armorTier == 0) tint = Colors::Yellow;
            else if (armorTier == 1) tint = Color{0, 215, 175, 255};       // Cyber Turquoise
            else if (armorTier == 2) tint = Color{200, 70, 255, 255};      // Plasma Violet
            else tint = Color{255, 75, 45, 255};                            // Crimson Nova
            break;

        case BrickType::Hard:
            comp.HitPoints = 3 + armorTier * 2;
            comp.ScoreValue = 200 + armorTier * 100;
            if (armorTier == 0) tint = Colors::Red;
            else if (armorTier == 1) tint = Color{30, 140, 255, 255};      // Azure Blue
            else if (armorTier == 2) tint = Color{150, 30, 235, 255};      // Deep Titanium Purple
            else tint = Color{130, 15, 45, 255};                            // Obsidian Onyx
            break;

        case BrickType::Special:
            comp.HitPoints = 1 + armorTier;
            comp.ScoreValue = 0;
            comp.IsSpecial = true;
            comp.RegenCooldown = 30.0f;
            tint = Color{255, 0, 255, 255};
            break;

        case BrickType::Metal:
            comp.HitPoints = 999999;
            comp.ScoreValue = 500;
            comp.IsUnbreakable = true;
            tint = Color{180, 195, 210, 255};
            break;

        case BrickType::Bomb:
            comp.HitPoints = 1;
            comp.ScoreValue = 150;
            comp.IsBomb = true;
            tint = (armorTier >= 2) ? Color{255, 40, 120, 255} : Color{255, 80, 20, 255};
            break;

        case BrickType::Lantern:
            comp.HitPoints = 1 + armorTier;
            comp.ScoreValue = 120;
            comp.IsLantern = true;
            tint = Color{255, 235, 120, 255};
            break;

        case BrickType::Event:
            comp.HitPoints = 1 + armorTier;
            comp.ScoreValue = 120;
            comp.IsEventBrick = true;
            comp.RandomEvent = true;
            tint = Color{45, 220, 235, 255};
            break;

        default:
            tint = Colors::White;
            break;
    }

    if (customHitPoints > 0 && !comp.IsUnbreakable)
    {
        const int hp = std::clamp(customHitPoints, 1, 9);
        comp.HitPoints = static_cast<uint32_t>(hp);
        comp.ScoreValue = static_cast<uint32_t>(hp * 60);
        if (type == BrickType::Light || type == BrickType::Medium || type == BrickType::Hard)
        {
            tint = GetColorForHitPoints(hp);
        }
    }

    comp.MaxHitPoints = comp.HitPoints;

    registry.AddComponent<BrickComponent>(brick, comp);
    registry.AddComponent<SpriteComponent>(brick, SpriteComponent{texId, tint});

    if (comp.IsLantern)
    {
        LightSourceComponent lanternLight{};
        lanternLight.Type = LightType::Point;
        lanternLight.LightColor = Color{255, 235, 130, 255};
        lanternLight.Radius = 230.0f;
        lanternLight.Intensity = 1.25f;
        lanternLight.InnerRadius = 20.0f;
        registry.AddComponent<LightSourceComponent>(brick, lanternLight);
    }

    return brick;
}
