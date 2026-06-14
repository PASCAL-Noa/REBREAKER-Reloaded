#include "../include/ECS/Systems/UISystem.h"

#include "Core/InputManager.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "Graphics/Renderer.h"

Vector2f UISystem::GetAbsolutePosition(Registry& registry, Entity entity, const RectTransform& transform, const Vector2f& viewSize)
{
    Vector2f basePos{0.0f, 0.0f};

    if (transform.Parent != NULL_ENTITY && registry.HasComponent<RectTransform>(transform.Parent))
    {
        const auto& parentTransform = registry.GetComponent<RectTransform>(transform.Parent);
        basePos = GetAbsolutePosition(registry, transform.Parent, parentTransform, viewSize);

        // Anchor relative to parent
        switch (transform.AnchorPoint)
        {
            case Anchor::TopLeft: basePos.X -= parentTransform.Size.X / 2.0f; basePos.Y -= parentTransform.Size.Y / 2.0f; break;
            case Anchor::TopCenter: basePos.Y -= parentTransform.Size.Y / 2.0f; break;
            case Anchor::TopRight: basePos.X += parentTransform.Size.X / 2.0f; basePos.Y -= parentTransform.Size.Y / 2.0f; break;
            case Anchor::MiddleLeft: basePos.X -= parentTransform.Size.X / 2.0f; break;
            case Anchor::Center: break;
            case Anchor::MiddleRight: basePos.X += parentTransform.Size.X / 2.0f; break;
            case Anchor::BottomLeft: basePos.X -= parentTransform.Size.X / 2.0f; basePos.Y += parentTransform.Size.Y / 2.0f; break;
            case Anchor::BottomCenter: basePos.Y += parentTransform.Size.Y / 2.0f; break;
            case Anchor::BottomRight: basePos.X += parentTransform.Size.X / 2.0f; basePos.Y += parentTransform.Size.Y / 2.0f; break;
        }
    }
    else
    {
        // Anchor relative to screen
        switch (transform.AnchorPoint)
        {
            case Anchor::TopLeft: basePos = Vector2f{0.0f, 0.0f}; break;
            case Anchor::TopCenter: basePos = Vector2f{viewSize.X / 2.0f, 0.0f}; break;
            case Anchor::TopRight: basePos = Vector2f{viewSize.X, 0.0f}; break;
            case Anchor::MiddleLeft: basePos = Vector2f{0.0f, viewSize.Y / 2.0f}; break;
            case Anchor::Center: basePos = Vector2f{viewSize.X / 2.0f, viewSize.Y / 2.0f}; break;
            case Anchor::MiddleRight: basePos = Vector2f{viewSize.X, viewSize.Y / 2.0f}; break;
            case Anchor::BottomLeft: basePos = Vector2f{0.0f, viewSize.Y}; break;
            case Anchor::BottomCenter: basePos = Vector2f{viewSize.X / 2.0f, viewSize.Y}; break;
            case Anchor::BottomRight: basePos = Vector2f{viewSize.X, viewSize.Y}; break;
        }
    }

    return Vector2f{basePos.X + transform.Position.X, basePos.Y + transform.Position.Y};
}

bool UISystem::IsPointInsideRect(const Vector2f& point, const Vector2f& rectPos, const Vector2f& rectSize)
{
    float left = rectPos.X - rectSize.X / 2.0f;
    float right = rectPos.X + rectSize.X / 2.0f;
    float top = rectPos.Y - rectSize.Y / 2.0f;
    float bottom = rectPos.Y + rectSize.Y / 2.0f;

    return point.X >= left && point.X <= right && point.Y >= top && point.Y <= bottom;
}

void UISystem::OnUpdate(float dt, Registry& registry, const GameContext& context)
{

    auto [mouseX, mouseY] = context.Input.GetMousePosition();

    context.Render.ResetCamera();
    const Vector2f logicalMousePos = context.Render.MapPixelToCoords(Vector2f{mouseX, mouseY});
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    const bool isLeftMouseDown = context.Input.IsMouseButtonDown(MouseButton::Left);
    const bool isLeftMousePress = context.Input.IsMouseButtonPress(MouseButton::Left);
    const bool isLeftMouseRelease = context.Input.IsMouseButtonRelease(MouseButton::Left);

    registry.View<RectTransform, ButtonComponent>([&](const Entity entity, const RectTransform& transform, ButtonComponent& button)
    {
        if (!transform.IsActive) return;

        Entity canvasParent = transform.Parent;
        bool parentEnabled = true;
        while (canvasParent != NULL_ENTITY)
        {
            if (registry.HasComponent<RectTransform>(canvasParent))
            {
                if (!registry.GetComponent<RectTransform>(canvasParent).IsActive)
                {
                    parentEnabled = false;
                    break;
                }
            }
            if (registry.HasComponent<CanvasComponent>(canvasParent))
            {
                if (!registry.GetComponent<CanvasComponent>(canvasParent).IsEnabled)
                {
                    parentEnabled = false;
                }
                break;
            }
            if (registry.HasComponent<RectTransform>(canvasParent))
                canvasParent = registry.GetComponent<RectTransform>(canvasParent).Parent;
            else
                break;
        }

        if (!parentEnabled) return;

        Vector2f absolutePos = GetAbsolutePosition(registry, entity, transform, viewSize);
        bool isHovered = IsPointInsideRect(logicalMousePos, absolutePos, transform.Size);

        if (isHovered)
        {
            if (isLeftMousePress)
            {
                button.State = ButtonState::Pressed;
            }
            else if (isLeftMouseRelease && button.State == ButtonState::Pressed)
            {
                button.State = ButtonState::Hovered;
                if (button.OnClick) button.OnClick();
            }
            else if (!isLeftMouseDown && button.State != ButtonState::Hovered)
            {
                button.State = ButtonState::Hovered;
                if (button.OnHoverEnter) button.OnHoverEnter();
            }
        }
        else
        {
            if (button.State == ButtonState::Hovered && button.OnHoverExit)
            {
                button.OnHoverExit();
            }
            button.State = ButtonState::Normal;
        }
    });
}

void UISystem::OnRender(Registry& registry, const GameContext& context)
{
    context.Render.ResetCamera();
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    registry.View<RectTransform>([&](Entity entity, const RectTransform& transform)
    {
        if (!transform.IsActive) return;

        Entity canvasParent = transform.Parent;
        bool parentEnabled = true;
        while (canvasParent != NULL_ENTITY)
        {
            if (registry.HasComponent<RectTransform>(canvasParent))
            {
                if (!registry.GetComponent<RectTransform>(canvasParent).IsActive)
                {
                    parentEnabled = false;
                    break;
                }
            }
            if (registry.HasComponent<CanvasComponent>(canvasParent))
            {
                if (!registry.GetComponent<CanvasComponent>(canvasParent).IsEnabled)
                {
                    parentEnabled = false;
                }
                break; 
            }
            if (registry.HasComponent<RectTransform>(canvasParent))
                canvasParent = registry.GetComponent<RectTransform>(canvasParent).Parent;
            else
                break;
        }

        if (!parentEnabled) return;

        const Vector2f absolutePos = GetAbsolutePosition(registry, entity, transform, viewSize);
        Transform2D drawTransform{absolutePos};

        if (registry.HasComponent<PanelComponent>(entity))
        {
            const auto& panel = registry.GetComponent<PanelComponent>(entity);
            Color tint = panel.Tint;
            uint32_t tex = panel.TextureId;

            if (registry.HasComponent<ButtonComponent>(entity))
            {
                const auto& button = registry.GetComponent<ButtonComponent>(entity);
                if (button.State == ButtonState::Hovered)
                {
                    tint = button.HoverColor;
                    if (button.HoverTextureId)
                        tex = button.HoverTextureId;
                    else if (button.DefaultTextureId)
                        tex = button.DefaultTextureId;
                }
                else if (button.State == ButtonState::Pressed)
                {
                    tint = button.PressedColor;
                    if (button.PressedTextureId)
                        tex = button.PressedTextureId;
                    else if (button.DefaultTextureId)
                        tex = button.DefaultTextureId;
                }
                else
                {
                    tint = button.DefaultColor;
                    if (button.DefaultTextureId)
                        tex = button.DefaultTextureId;
                }
            }

            if (tex != 0)
            {
                SpriteComponent sprite{tex};
                sprite.Tint = tint;

                Vector2f texSize = context.Render.GetTextureSize(tex);
                if (texSize.X > 0 && texSize.Y > 0)
                {
                    drawTransform.Scale = Vector2f{transform.Size.X / texSize.X, transform.Size.Y / texSize.Y};
                }
                context.Render.DrawSprite(sprite, drawTransform);
                drawTransform.Scale = Vector2f{1.0f, 1.0f};
            }
            else
            {
                context.Render.DrawRectangle(transform.Size.X, transform.Size.Y, drawTransform, tint);
            }
        }
        else if (registry.HasComponent<SpriteComponent>(entity))
        {
            const auto& sprite = registry.GetComponent<SpriteComponent>(entity);
            float texW = context.Render.GetTextureSize(sprite.TextureId).X;
            float texH = context.Render.GetTextureSize(sprite.TextureId).Y;
            if (sprite.TextureRect)
            {
                texW = static_cast<float>(sprite.TextureRect->Width);
                texH = static_cast<float>(sprite.TextureRect->Height);
            }

            if (texW > 0 && texH > 0)
            {
                drawTransform.Scale = Vector2f{transform.Size.X / texW, transform.Size.Y / texH};
            }
            context.Render.DrawSprite(sprite, drawTransform);
            drawTransform.Scale = Vector2f{1.0f, 1.0f};
        }

        if (registry.HasComponent<TextComponent>(entity))
        {
            const auto& textComp = registry.GetComponent<TextComponent>(entity);
            if (!textComp.Text.empty())
            {
                Vector2f textPos = absolutePos;
                if (textComp.TextCenter)
                {
                    Vector2f textSize = context.Render.GetTextSize(textComp.Text, textComp.FontId, textComp.FontSize);
                    textPos.X -= textSize.X / 2.0f;
                    textPos.Y -= textSize.Y / 2.0f;
                }
                textPos.X += textComp.Offset.X;
                textPos.Y += textComp.Offset.Y;
                context.Render.DrawText(textComp.Text, textComp.FontId, textComp.FontSize, Transform2D{textPos}, textComp.Tint);
            }
        }
    });
}
