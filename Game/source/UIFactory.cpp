#include "UI/UIFactory.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"

Entity UIFactory::CreatePanel(Registry& registry, Entity parent, const PanelDescriptor& desc)
{
    Entity entity = registry.CreateEntity();
    registry.AddComponent<RectTransform>(entity, RectTransform{
        .Position = desc.Position,
        .Size = desc.Size,
        .AnchorPoint = desc.AnchorPoint,
        .Parent = parent
    });
    registry.AddComponent<PanelComponent>(entity, PanelComponent{
        .Tint = desc.Tint
    });
    return entity;
}

Entity UIFactory::CreateText(Registry& registry, Entity parent, const TextDescriptor& desc)
{
    Entity entity = registry.CreateEntity();
    registry.AddComponent<RectTransform>(entity, RectTransform{
        .Position = desc.Position,
        .Size = {0.0f, 0.0f}, // Text generally auto-sizes or doesn't need strict Rect bounds unless word-wrapping
        .AnchorPoint = desc.AnchorPoint,
        .Parent = parent
    });
    registry.AddComponent<TextComponent>(entity, TextComponent{
        .Text = desc.Text,
        .FontId = desc.FontId,
        .FontSize = desc.FontSize,
        .Tint = desc.Tint,
        .TextCenter = desc.TextCenter,
        .Offset = desc.Offset
    });
    return entity;
}

Entity UIFactory::CreateButton(Registry& registry, Entity parent, const ButtonDescriptor& desc)
{
    Entity entity = registry.CreateEntity();
    registry.AddComponent<RectTransform>(entity, RectTransform{
        .Position = desc.Position,
        .Size = desc.Size,
        .AnchorPoint = desc.AnchorPoint,
        .Parent = parent
    });
    registry.AddComponent<PanelComponent>(entity, PanelComponent{});
    registry.AddComponent<ButtonComponent>(entity, ButtonComponent{
        .DefaultColor = desc.DefaultColor,
        .HoverColor = desc.HoverColor,
        .PressedColor = desc.PressedColor,
        .OnClick = desc.OnClick
    });
    
    if (!desc.Text.empty())
    {
        registry.AddComponent<TextComponent>(entity, TextComponent{
            .Text = desc.Text,
            .FontId = desc.FontId,
            .FontSize = desc.FontSize,
            .Tint = desc.TextColor,
            .Offset = desc.TextOffset
        });
    }
    
    return entity;
}

void UIFactory::CreateVolumeControl(Registry& registry, Entity parent, const VolumeControlDescriptor& desc)
{
    // Label
    CreateText(registry, parent, TextDescriptor{
        .Text = desc.Label,
        .Position = {-230.0f, desc.Position.Y - 10.0f},
        .FontId = desc.FontId,
        .FontSize = 30.0f
    });

    // Minus button
    CreateButton(registry, parent, ButtonDescriptor{
        .Text = "-",
        .OnClick = desc.OnMinus,
        .Position = {-100.0f, desc.Position.Y},
        .Size = {40.0f, 40.0f},
        .TextOffset = {-2.0f, -24.0f},
        .FontId = desc.FontId,
        .FontSize = 40.0f
    });

    // Bars
    for (int i = 0; i < 10; ++i)
    {
        Color barColor = Colors::Green;
        if (i >= 5 && i < 8) barColor = Colors::Yellow;
        else if (i == 8) barColor = Colors::Orange;
        else if (i == 9) barColor = Colors::Red;

        if (desc.BarsOut)
        {
            desc.BarsOut->push_back(CreatePanel(registry, parent, PanelDescriptor{
                .Position = {-50.0f + (i * 25.0f), desc.Position.Y},
                .Size = {15.0f, 30.0f},
                .Tint = barColor
            }));
        }
        else
        {
            CreatePanel(registry, parent, PanelDescriptor{
                .Position = {-50.0f + (i * 25.0f), desc.Position.Y},
                .Size = {15.0f, 30.0f},
                .Tint = barColor
            });
        }
    }

    // Plus button
    CreateButton(registry, parent, ButtonDescriptor{
        .Text = "+",
        .OnClick = desc.OnPlus,
        .Position = {230.0f, desc.Position.Y},
        .Size = {40.0f, 40.0f},
        .TextOffset = {-2.0f, -20.0f},
        .FontId = desc.FontId,
        .FontSize = 40.0f
    });
}
