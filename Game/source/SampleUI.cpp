#include "Scenes/SampleUI.h"
#include "Scenes/MenuScene.h"
#include "Core/InputManager.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "Core/Timer.h"
#include <iostream>

#include "Core/SceneManager.h"
#include "Core/Timer.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include <algorithm>

void SampleUI::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");

    m_canvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_canvas, CanvasComponent{.IsEnabled = true});

    Entity title = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(title, RectTransform{
        .Position = {0, 60.0f},
        .Size = {800, 100},
        .AnchorPoint = Anchor::TopCenter,
        .Parent = m_canvas
    });
    m_registry.AddComponent<TextComponent>(title, TextComponent{
        .Text = "SAMPLE UI",
        .FontId = m_fontId,
        .FontSize = 60.0f,
        .Tint = Colors::Gold,
        .Offset = {0.0f, -10.0f}
    });

    auto createAnchorButton = [&](Anchor anchor, const Vector2f& pos, const std::string& text, Color baseColor) {
        Entity btn = m_registry.CreateEntity();
        m_registry.AddComponent<RectTransform>(btn, RectTransform{
            .Position = pos,
            .Size = {200, 60},
            .AnchorPoint = anchor,
            .Parent = m_canvas
        });
        m_registry.AddComponent<PanelComponent>(btn, PanelComponent{});
        m_registry.AddComponent<ButtonComponent>(btn, ButtonComponent{
            .DefaultColor = baseColor,
            .HoverColor = Color{static_cast<uint8_t>(std::min(255, baseColor.r + 50)), static_cast<uint8_t>(std::min(255, baseColor.g + 50)), static_cast<uint8_t>(std::min(255, baseColor.b + 50)), 255},
            .PressedColor = Color{static_cast<uint8_t>(std::max(0, baseColor.r - 50)), static_cast<uint8_t>(std::max(0, baseColor.g - 50)), static_cast<uint8_t>(std::max(0, baseColor.b - 50)), 255},
            .OnClick = [text]() { std::cout << text << " clicked!\n"; }
        });
        m_registry.AddComponent<TextComponent>(btn, TextComponent{
            .Text = text,
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = Colors::White,
            .Offset = {0.0f, -10.0f}
        });
        return btn;
    };


    createAnchorButton(Anchor::TopLeft, {120.0f, 160.0f}, "TOP-LEFT", Colors::SteelBlue);
    createAnchorButton(Anchor::TopCenter, {0.0f, 160.0f}, "TOP-CENTER", Colors::SteelBlue);
    createAnchorButton(Anchor::TopRight, {-120.0f, 160.0f}, "TOP-RIGHT", Colors::SteelBlue);

    createAnchorButton(Anchor::MiddleLeft, {120.0f, 0.0f}, "MID-LEFT", Colors::DarkCyan);
    createAnchorButton(Anchor::Center, {0.0f, 0.0f}, "CENTER", Colors::ForestGreen);
    createAnchorButton(Anchor::MiddleRight, {-120.0f, 0.0f}, "MID-RIGHT", Colors::DarkCyan);

    createAnchorButton(Anchor::BottomLeft, {120.0f, -60.0f}, "BOT-LEFT", Colors::Sienna);
    createAnchorButton(Anchor::BottomCenter, {0.0f, -60.0f}, "BOT-CENTER", Colors::Sienna);
    createAnchorButton(Anchor::BottomRight, {-120.0f, -60.0f}, "BOT-Right", Colors::Sienna);


    Entity nestedPanel = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(nestedPanel, RectTransform{
        .Position = {0.0f, 120.0f},
        .Size = {400.0f, 100.0f},
        .AnchorPoint = Anchor::Center,
        .Parent = m_canvas
    });
    m_registry.AddComponent<PanelComponent>(nestedPanel, PanelComponent{.Tint = Color{30, 30, 30, 200}});
    
    Entity nestedText = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(nestedText, RectTransform{
        .Position = {0.0f, 0.0f},
        .Size = {400.0f, 100.0f},
        .AnchorPoint = Anchor::Center,
        .Parent = nestedPanel
    });
    m_registry.AddComponent<TextComponent>(nestedText, TextComponent{
        .Text = "Nested Panel Showcase",
        .FontId = m_fontId,
        .FontSize = 30.0f,
        .Tint = Colors::Cyan,
        .Offset = {0.0f, -10.0f}
    });

    Entity bg = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(bg, RectTransform{
        .Position = {0, 0},
        .Size = {context.Render.GetLogicalViewSize().X, context.Render.GetLogicalViewSize().Y},
        .AnchorPoint = Anchor::Center,
        .Parent = m_canvas
    });
    m_registry.AddComponent<PanelComponent>(bg, PanelComponent{.Tint = Colors::DarkSlateGray});
}

void SampleUI::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        context.Scenes.LoadScene<MenuScene>();
        return;
    }

    m_uiSystem.OnUpdate(dt, m_registry, context);
}

void SampleUI::OnRender(GameContext& context)
{
    m_uiSystem.OnRender(m_registry, context);

    std::string stats = "FPS: " + std::to_string(static_cast<int>(context.Time.GetFPS()));
    DrawDefaultUI(context, "Sample UI", stats);
}

void SampleUI::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}
