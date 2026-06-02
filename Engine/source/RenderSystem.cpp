#include "ECS/Systems/RenderSystem.h"

#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BoxCollider.h"
#include "Graphics/Renderer.h"

RenderSystem::RenderSystem(Registry& registry, Renderer& renderer)
    : System(registry), m_renderer(renderer)
{
}

void RenderSystem::OnRender()
{
    m_registry.View<Transform2D, SpriteComponent>([this](Entity e, Transform2D& transform, SpriteComponent& sprite)
    {
        float scaleX = transform.Scale.X;
        float scaleY = transform.Scale.Y;

        if (m_registry.HasComponent<BoxCollider>(e))
        {
            auto& box = m_registry.GetComponent<BoxCollider>(e);
            Vector2f texSize = m_renderer.GetTextureSize(sprite.TextureId);

            if (texSize.X > 0.0f && texSize.Y > 0.0f)
            {
                scaleX = box.Size.X / texSize.X;
                scaleY = box.Size.Y / texSize.Y;
            }
        }

        Transform2D renderTransform = transform;
        renderTransform.Scale = {scaleX, scaleY};

        m_renderer.DrawSprite(sprite.TextureId, renderTransform, sprite.Tint);
    });
}