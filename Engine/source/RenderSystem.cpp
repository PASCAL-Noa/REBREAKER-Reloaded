#include "ECS/Systems/RenderSystem.h"

#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/TrailComponent.h"
#include "ECS/Components/PaddleComponent.h"
#include "ECS/Components/BallComponent.h"
#include "Core/ProfileManager.h"
#include "Graphics/CosmeticRenderer.h"
#include "Graphics/Renderer.h"

#include <algorithm>
#include <cmath>

RenderSystem::RenderSystem(Registry& registry, Renderer& renderer) : System(registry), m_renderer(renderer) {}

void RenderSystem::OnUpdate(float dt)
{
    m_animTime += dt;
}

void RenderSystem::OnRender()
{
    // 1. Render glowing motion trails beneath sprites
    RenderTrails();

    // 2. Render sorted sprites & customized paddle/ball cosmetics
    m_renderQueue.clear();

    m_registry.View<Transform2D, SpriteComponent>([this](const Entity e, const Transform2D& transform, const SpriteComponent& sprite)
    {
        float scaleX = transform.Scale.X;
        float scaleY = transform.Scale.Y;

        Vector2f texSize = m_renderer.GetTextureSize(sprite.TextureId);

        if (texSize.X > 0.0f && texSize.Y > 0.0f)
        {
            if (m_registry.HasComponent<BoxCollider>(e))
            {
                auto& box = m_registry.GetComponent<BoxCollider>(e);
                scaleX = (box.Size.X / texSize.X) * transform.Scale.X;
                scaleY = (box.Size.Y / texSize.Y) * transform.Scale.Y;
            }
            else if (m_registry.HasComponent<CircleCollider>(e))
            {
                auto& circle = m_registry.GetComponent<CircleCollider>(e);
                float diameter = circle.Radius * 2.0f;
                scaleX = (diameter / texSize.X) * transform.Scale.X;
                scaleY = (diameter / texSize.Y) * transform.Scale.Y;
            }
        }

        Transform2D renderTransform = transform;
        renderTransform.Scale = {scaleX, scaleY};

        m_renderQueue.push_back(RenderItem{
            .sprite = &sprite,
            .renderTransform = renderTransform,
            .worldTransform = transform,
            .layer = sprite.Layer,
            .entity = e
        });
    });

    std::stable_sort(m_renderQueue.begin(), m_renderQueue.end(), [](const RenderItem& a, const RenderItem& b)
    {
        if (a.layer != b.layer)
        {
            return a.layer < b.layer;
        }
        return a.entity < b.entity;
    });

    const auto& pm = ProfileManager::Get();
    const Color primaryCol = pm.GetEquippedPrimaryColor();
    const Color secondaryCol = pm.GetEquippedSecondaryColor();
    const PaddleSkinStyle paddleSkin = pm.GetEquippedPaddleSkinStyle();
    const BallSkinStyle ballSkin = pm.GetEquippedBallSkinStyle();

    for (const auto& item : m_renderQueue)
    {
        const float alphaScale = static_cast<float>(item.sprite->Tint.a) / 255.0f;

        if (m_registry.HasComponent<PaddleComponent>(item.entity) && m_registry.HasComponent<BoxCollider>(item.entity))
        {
            if (alphaScale > 0.01f)
            {
                const auto& box = m_registry.GetComponent<BoxCollider>(item.entity);
                const Vector2f effectiveSize = box.GetEffectiveSize(item.worldTransform.Scale);
                CosmeticRenderer::DrawPaddle(
                    m_renderer,
                    item.worldTransform.Position,
                    effectiveSize,
                    item.worldTransform.Rotation,
                    paddleSkin,
                    primaryCol,
                    secondaryCol,
                    m_animTime,
                    alphaScale,
                    item.sprite->TextureId);
            }
            continue;
        }

        if (m_registry.HasComponent<BallComponent>(item.entity) && m_registry.HasComponent<CircleCollider>(item.entity))
        {
            if (alphaScale > 0.01f)
            {
                const auto& circle = m_registry.GetComponent<CircleCollider>(item.entity);
                const auto& bc = m_registry.GetComponent<BallComponent>(item.entity);
                const float radius = circle.GetEffectiveRadius(item.worldTransform.Scale);
                const bool isTempo = (item.sprite->Tint.r == 210 && item.sprite->Tint.g == 160 && item.sprite->Tint.b == 255);
                CosmeticRenderer::DrawBall(
                    m_renderer,
                    item.worldTransform.Position,
                    radius,
                    ballSkin,
                    primaryCol,
                    secondaryCol,
                    m_animTime,
                    alphaScale,
                    bc.IsFireBall,
                    bc.IsGhost,
                    isTempo,
                    item.sprite->TextureId);
            }
            continue;
        }

        m_renderer.DrawSprite(*item.sprite, item.renderTransform, BlendMode::Alpha);
    }
}

void RenderSystem::RenderTrails()
{
    m_registry.View<TrailComponent>([this](Entity /*e*/, TrailComponent& trail)
    {
        CosmeticRenderer::RenderTrail(m_renderer, trail, m_animTime, m_trailVerticesScratch);
    });
}
