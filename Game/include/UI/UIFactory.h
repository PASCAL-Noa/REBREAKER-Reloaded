#pragma once
#include "ECS/Registry.hpp"
#include "ECS/Entity.h"
#include "Math/Vector2.h"
#include "Data/Color.h"
#include "ECS/Components/UI/RectTransform.h"
#include <string>
#include <functional>

struct PanelDescriptor
{
    Vector2f    Position = {0.0f, 0.0f};
    Vector2f    Size = {100.0f, 100.0f};
    Color       Tint = Colors::White;
    Anchor  AnchorPoint = Anchor::Center;
};

struct TextDescriptor
{
    std::string     Text = "";
    Vector2f    Offset = {0.0f, 0.0f};
    Vector2f    Position = {0.0f, 0.0f};
    uint32_t    FontId = 0;
    float   FontSize = 30.0f;
    Color   Tint = Colors::White;
    Anchor  AnchorPoint = Anchor::Center;
    bool    TextCenter  = true;
    uint8_t     _padding[7] = {0};
};

struct ButtonDescriptor
{
    std::string     Text = "";
    std::function<void()>   OnClick = nullptr;
    Vector2f    Position = {0.0f, 0.0f};
    Vector2f    Size = {300.0f, 60.0f};
    Vector2f    TextOffset = {0.0f, -10.0f};
    Color   DefaultColor = Color{50, 50, 50, 255};
    Color   HoverColor   = Color{100, 100, 100, 255};
    Color   PressedColor = Color{30, 30, 30, 255};
    Color   TextColor    = Colors::White;
    uint32_t    FontId = 0;
    float   FontSize = 30.0f;
    Anchor  AnchorPoint  = Anchor::Center;
    uint8_t     _padding[4] = {0};
};

struct VolumeControlDescriptor
{
    std::string     Label = "";
    std::function<void()>   OnMinus = nullptr;
    std::function<void()>   OnPlus = nullptr;
    std::vector<Entity>*    BarsOut  = nullptr;
    Vector2f    Position = {0.0f, 0.0f};
    uint32_t    FontId = 0;
    uint8_t     _padding[4] = {0};
};

class UIFactory
{
public:
    static Entity   CreatePanel(Registry& registry, Entity parent, const PanelDescriptor& desc);
    static Entity   CreateText(Registry& registry, Entity parent, const TextDescriptor& desc);
    static Entity   CreateButton(Registry& registry, Entity parent, const ButtonDescriptor& desc);
    
    // Complex Widgets
    static void     CreateVolumeControl(Registry& registry, Entity parent, const VolumeControlDescriptor& desc);
};
