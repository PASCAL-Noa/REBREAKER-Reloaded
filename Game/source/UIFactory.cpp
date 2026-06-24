#include "UI/UIFactory.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/CanvasComponent.h"

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
        .Tint = desc.Tint,
        .TextureId = desc.TextureId
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
    registry.AddComponent<PanelComponent>(entity, PanelComponent{
        .TextureId = desc.TextureId
    });
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
        .Position = {desc.Position.X, desc.Position.Y - 50.0f},
        .FontId = desc.FontId,
        .FontSize = 30.0f
    });

    // Minus button
    CreateButton(registry, parent, ButtonDescriptor{
        .Text = "-",
        .OnClick = desc.OnMinus,
        .Position = {desc.Position.X - 165.0f, desc.Position.Y},
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
                .Position = {desc.Position.X - 115.0f + (i * 25.0f), desc.Position.Y},
                .Size = {15.0f, 30.0f},
                .Tint = barColor
            }));
        }
        else
        {
            CreatePanel(registry, parent, PanelDescriptor{
                .Position = {desc.Position.X - 115.0f + (i * 25.0f), desc.Position.Y},
                .Size = {15.0f, 30.0f},
                .Tint = barColor
            });
        }
    }

    // Plus button
    CreateButton(registry, parent, ButtonDescriptor{
        .Text = "+",
        .OnClick = desc.OnPlus,
        .Position = {desc.Position.X + 165.0f, desc.Position.Y},
        .Size = {40.0f, 40.0f},
        .TextOffset = {-2.0f, -20.0f},
        .FontId = desc.FontId,
        .FontSize = 40.0f
    });
}

Entity UIFactory::CreateDropdown(Registry& registry, Entity parent, const DropdownDescriptor& desc)
{
    Entity dropdownHead = CreateButton(registry, parent, ButtonDescriptor{
        .Text = desc.DefaultText,
        .Position = desc.Position,
        .Size = desc.Size,
        .DefaultColor = desc.DefaultColor,
        .HoverColor = desc.HoverColor,
        .PressedColor = desc.PressedColor,
        .TextColor = desc.TextColor,
        .FontId = desc.FontId,
        .FontSize = desc.FontSize,
        .AnchorPoint = desc.AnchorPoint,
        .TextureId = desc.TextureId
    });

    Entity menuCanvas = registry.CreateEntity();
    registry.AddComponent<CanvasComponent>(menuCanvas, CanvasComponent{.IsEnabled = false});
    registry.AddComponent<RectTransform>(menuCanvas, RectTransform{
        .Position = {0.0f, desc.Size.Y / 2.0f},
        .Size = {desc.Size.X, desc.Size.Y * desc.Options.size()},
        .AnchorPoint = Anchor::BottomCenter,
        .Parent = dropdownHead
    });

    registry.GetComponent<RectTransform>(menuCanvas).AnchorPoint = Anchor::BottomCenter;

    auto onSelect = desc.OnSelect;
    for (size_t i = 0; i < desc.Options.size(); ++i)
    {
        float yPos = desc.Size.Y * i + desc.Size.Y / 2.0f;
        std::string optionText = desc.Options[i];

        CreateButton(registry, menuCanvas, ButtonDescriptor{
            .Text = optionText,
            .OnClick = [&registry, dropdownHead, menuCanvas, onSelect, i, optionText]() {
                if (onSelect) onSelect(static_cast<int>(i), optionText);
                if (registry.HasComponent<TextComponent>(dropdownHead)) {
                    registry.GetComponent<TextComponent>(dropdownHead).Text = optionText;
                }
                registry.GetComponent<CanvasComponent>(menuCanvas).IsEnabled = false;
            },
            .Position = {0.0f, yPos},
            .Size = {desc.Size.X, desc.Size.Y},
            .DefaultColor = Color{40, 40, 40, 255},
            .HoverColor = Color{80, 80, 80, 255},
            .PressedColor = Color{20, 20, 20, 255},
            .TextColor = Colors::White,
            .FontId = desc.FontId,
            .FontSize = desc.FontSize * 0.8f,
            .AnchorPoint = Anchor::TopCenter
        });
    }
    
    CreatePanel(registry, menuCanvas, PanelDescriptor{
        .Position = {0.0f, (desc.Size.Y * desc.Options.size()) / 2.0f},
        .Size = {desc.Size.X, desc.Size.Y * desc.Options.size()},
        .Tint = Color{20, 20, 20, 240},
        .AnchorPoint = Anchor::TopCenter
    });

    // Toggle menu
    registry.GetComponent<ButtonComponent>(dropdownHead).OnClick = [&registry, menuCanvas]() {
        if (registry.HasComponent<CanvasComponent>(menuCanvas)) {
            auto& canvas = registry.GetComponent<CanvasComponent>(menuCanvas);
            canvas.IsEnabled = !canvas.IsEnabled;
        }
    };

    return dropdownHead;
}
