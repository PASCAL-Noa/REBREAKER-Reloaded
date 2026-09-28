#include "Scenes/SampleRenderLayer.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Systems/RenderSystem.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include <algorithm>
#include <string>

void SampleRenderLayer::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_registry.GetComponent<Camera2D>(m_camera).Zoom = 1.0f;
    m_texId = context.Resources.LoadResource("Resources/sprite/debug.jpg");

    // =========================================================================
    // Demonstration 1: Inverted creation order (Left side)
    // m_invertedFirst is created FIRST, with Layer = 10.
    // m_invertedSecond is created SECOND, with Layer = 0.
    // Result: m_invertedFirst (Cyan) renders ON TOP of m_invertedSecond (Magenta).
    // =========================================================================
    m_invertedFirst = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_invertedFirst, Transform2D{
        .Position = Vector2f{-500.0f, 150.0f},
        .Scale = Vector2f{0.9f, 0.9f}
    });
    m_registry.AddComponent<SpriteComponent>(m_invertedFirst, SpriteComponent{
        .TextureId = m_texId,
        .Tint = Colors::Cyan,
        .Layer = 10
    });

    m_invertedSecond = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_invertedSecond, Transform2D{
        .Position = Vector2f{-430.0f, 80.0f},
        .Scale = Vector2f{0.9f, 0.9f}
    });
    m_registry.AddComponent<SpriteComponent>(m_invertedSecond, SpriteComponent{
        .TextureId = m_texId,
        .Tint = Colors::Magenta,
        .Layer = 0
    });

    // =========================================================================
    // Demonstration 2: Normal creation order (Right side)
    // m_normalFirst is created FIRST, with Layer = 0.
    // m_normalSecond is created SECOND, with Layer = 10.
    // Result: m_normalSecond (Green) renders ON TOP of m_normalFirst (Yellow).
    // =========================================================================
    m_normalFirst = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_normalFirst, Transform2D{
        .Position = Vector2f{430.0f, 150.0f},
        .Scale = Vector2f{0.9f, 0.9f}
    });
    m_registry.AddComponent<SpriteComponent>(m_normalFirst, SpriteComponent{
        .TextureId = m_texId,
        .Tint = Colors::Yellow,
        .Layer = 0
    });

    m_normalSecond = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_normalSecond, Transform2D{
        .Position = Vector2f{500.0f, 80.0f},
        .Scale = Vector2f{0.9f, 0.9f}
    });
    m_registry.AddComponent<SpriteComponent>(m_normalSecond, SpriteComponent{
        .TextureId = m_texId,
        .Tint = Colors::Green,
        .Layer = 10
    });

    // =========================================================================
    // Demonstration 3: Interactive Stacking Playground (Center)
    // 3 overlapping cards whose layers can be manipulated at runtime.
    // =========================================================================
    const std::array<Color, 3> colors = {
        Color{230, 60, 60, 255},   // Red
        Color{60, 210, 60, 255},   // Green
        Color{60, 130, 240, 255}   // Blue
    };

    const std::array<Vector2f, 3> positions = {
        Vector2f{-70.0f, -80.0f},
        Vector2f{0.0f, -30.0f},
        Vector2f{70.0f, 20.0f}
    };

    for (size_t i = 0; i < 3; ++i)
    {
        m_interactiveEntities[i] = m_registry.CreateEntity();
        m_registry.AddComponent<Transform2D>(m_interactiveEntities[i], Transform2D{
            .Position = positions[i],
            .Scale = Vector2f{1.1f, 1.1f}
        });
        m_registry.AddComponent<SpriteComponent>(m_interactiveEntities[i], SpriteComponent{
            .TextureId = m_texId,
            .Tint = colors[i],
            .Layer = static_cast<int>(i + 1)
        });
    }

    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.OnInit();
}

void SampleRenderLayer::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    // Sprite selection
    if (context.Input.IsKeyPress(KeyCode::Num1))
    {
        m_selectedSpriteIndex = 0;
    }
    if (context.Input.IsKeyPress(KeyCode::Num2))
    {
        m_selectedSpriteIndex = 1;
    }
    if (context.Input.IsKeyPress(KeyCode::Num3))
    {
        m_selectedSpriteIndex = 2;
    }
    if (context.Input.IsKeyPress(KeyCode::Tab))
    {
        m_selectedSpriteIndex = (m_selectedSpriteIndex + 1) % 3;
    }

    // Modify selected sprite's layer
    if (context.Input.IsKeyPress(KeyCode::Up) || context.Input.IsKeyPress(KeyCode::W))
    {
        auto& sprite = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[m_selectedSpriteIndex]);
        sprite.Layer++;
    }
    if (context.Input.IsKeyPress(KeyCode::Down) || context.Input.IsKeyPress(KeyCode::S))
    {
        auto& sprite = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[m_selectedSpriteIndex]);
        sprite.Layer--;
    }

    // Bring selected sprite to front
    if (context.Input.IsKeyPress(KeyCode::Space))
    {
        int maxLayer = -999999;
        for (size_t i = 0; i < 3; ++i)
        {
            const auto& s = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[i]);
            if (s.Layer > maxLayer)
            {
                maxLayer = s.Layer;
            }
        }
        auto& sprite = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[m_selectedSpriteIndex]);
        sprite.Layer = maxLayer + 1;
    }

    // Reset interactive layers
    if (context.Input.IsKeyPress(KeyCode::R))
    {
        for (size_t i = 0; i < 3; ++i)
        {
            auto& sprite = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[i]);
            sprite.Layer = static_cast<int>(i + 1);
        }
    }

    m_systemManager.OnUpdate(dt);
}

void SampleRenderLayer::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    context.Render.SetCamera(m_registry.GetComponent<Camera2D>(m_camera));
    m_systemManager.OnRender();

    // World-space labels for the static comparison tests
    context.Render.DrawText("Inverted Order:\nCyan (1st, Layer 10)\nMagenta (2nd, Layer 0)\n-> Cyan ON TOP",
        m_fontId, 22.0f, Transform2D{Vector2f{-620.0f, 260.0f}}, Colors::Cyan);

    context.Render.DrawText("Normal Order:\nYellow (1st, Layer 0)\nGreen (2nd, Layer 10)\n-> Green ON TOP",
        m_fontId, 22.0f, Transform2D{Vector2f{320.0f, 260.0f}}, Colors::Green);

    context.Render.ResetCamera();

    const std::array<std::string, 3> spriteNames = {"Red", "Green", "Blue"};
    int layer0 = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[0]).Layer;
    int layer1 = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[1]).Layer;
    int layer2 = m_registry.GetComponent<SpriteComponent>(m_interactiveEntities[2]).Layer;

    std::string stats = "=== INTERACTIVE SPRITE STACK (Center) ===\n";
    stats += "Selected: Sprite " + std::to_string(m_selectedSpriteIndex + 1) + " (" + spriteNames[m_selectedSpriteIndex] + ")\n";
    stats += "  [1] Sprite 1 (Red)   : Layer = " + std::to_string(layer0) + (m_selectedSpriteIndex == 0 ? "  <-- ACTIVE" : "") + "\n";
    stats += "  [2] Sprite 2 (Green) : Layer = " + std::to_string(layer1) + (m_selectedSpriteIndex == 1 ? "  <-- ACTIVE" : "") + "\n";
    stats += "  [3] Sprite 3 (Blue)  : Layer = " + std::to_string(layer2) + (m_selectedSpriteIndex == 2 ? "  <-- ACTIVE" : "") + "\n\n";

    stats += "CONTROLS:\n";
    stats += "  [1] / [2] / [3] / [Tab] : Select Sprite\n";
    stats += "  [Up] / [Down] or [W] / [S] : Increase / Decrease Layer\n";
    stats += "  [Space] : Bring Selected to Front\n";
    stats += "  [R]     : Reset Layers to 1, 2, 3\n\n";

    stats += "=== ACCEPTANCE CRITERIA VERIFICATION ===\n";
    stats += "Left  : 1st created has Layer 10, 2nd created has Layer 0 -> Layer 10 (Cyan) renders on top\n";
    stats += "Right : 1st created has Layer 0,  2nd created has Layer 10 -> Layer 10 (Green) renders on top\n";
    stats += "=> Sprites with higher layer indices ALWAYS render on top, regardless of creation order.";

    DrawDefaultUI(context, "SAMPLE : RENDER LAYERING & SPRITE STACKING", stats);
}

void SampleRenderLayer::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}
