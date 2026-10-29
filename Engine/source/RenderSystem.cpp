#include "ECS/Systems/RenderSystem.h"

#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/CircleCollider.h"
#include "ECS/Components/TrailComponent.h"
#include "Graphics/Renderer.h"

#include <algorithm>
#include <cmath>

RenderSystem::RenderSystem(Registry& registry, Renderer& renderer) : System(registry), m_renderer(renderer) {}

void RenderSystem::OnRender()
{
    // 1. Render glowing motion trails beneath sprites
    RenderTrails();

    // 2. Render sorted sprites
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

    for (const auto& item : m_renderQueue)
    {
        m_renderer.DrawSprite(*item.sprite, item.renderTransform, BlendMode::Alpha);
    }
}

void RenderSystem::RenderTrails()
{
    m_registry.View<TrailComponent>([this](Entity /*e*/, TrailComponent& trail)
    {
        if (trail.Points.size() < 2) return;

        const auto& cfg = trail.Config;
        const size_t numPoints = trail.Points.size();

        if (cfg.Shape == TrailShape::Particles)
        {
            // Discrete particle/disk streak
            for (size_t i = 0; i < numPoints; ++i)
            {
                const auto& pt = trail.Points[i];
                float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;

                float width = cfg.FadeWidth ? ((1.0f - t) * pt.Width + t * cfg.EndWidth) : pt.Width;
                float radius = width * 0.5f;

                Color c = cfg.StartColor;
                if (cfg.CosmeticId == TrailCosmeticId::Rainbow)
                {
                    float hue = std::fmod(t * 360.0f + pt.Age * 180.0f, 360.0f);
                    c = TrailPresets::HsvToRgb(hue, 0.9f, 1.0f);
                }
                else
                {
                    c.r = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.r + t * cfg.EndColor.r);
                    c.g = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.g + t * cfg.EndColor.g);
                    c.b = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.b + t * cfg.EndColor.b);
                }
                if (cfg.FadeAlpha)
                {
                    c.a = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.a + t * cfg.EndColor.a);
                }

                m_renderer.DrawCircle(radius, Transform2D{pt.Position}, c, cfg.Blend);
            }
        }
        else
        {
            // Ribbon Quad Strip (TriangleStrip) - zero per-frame heap allocation
            m_trailVerticesScratch.clear();
            m_trailVerticesScratch.reserve(numPoints * 2);

            for (size_t i = 0; i < numPoints; ++i)
            {
                const auto& pt = trail.Points[i];
                float t = (cfg.Lifetime > 0.0f) ? std::clamp(pt.Age / cfg.Lifetime, 0.0f, 1.0f) : 1.0f;

                Vector2f dir{0.0f, 0.0f};
                if (i == 0)
                {
                    dir = trail.Points[0].Position - trail.Points[1].Position;
                }
                else if (i == numPoints - 1)
                {
                    dir = trail.Points[i - 1].Position - trail.Points[i].Position;
                }
                else
                {
                    dir = trail.Points[i - 1].Position - trail.Points[i + 1].Position;
                }

                float len = dir.Length();
                Vector2f normal = (len > 0.001f) ? Vector2f{-dir.Y / len, dir.X / len} : Vector2f{0.0f, 1.0f};

                float width = cfg.FadeWidth ? ((1.0f - t) * pt.Width + t * cfg.EndWidth) : pt.Width;
                float halfWidth = width * 0.5f;

                Color c = cfg.StartColor;
                if (cfg.CosmeticId == TrailCosmeticId::Rainbow)
                {
                    float hue = std::fmod(t * 360.0f + pt.Age * 180.0f, 360.0f);
                    c = TrailPresets::HsvToRgb(hue, 0.9f, 1.0f);
                }
                else
                {
                    c.r = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.r + t * cfg.EndColor.r);
                    c.g = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.g + t * cfg.EndColor.g);
                    c.b = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.b + t * cfg.EndColor.b);
                }

                if (cfg.FadeAlpha)
                {
                    c.a = static_cast<uint8_t>((1.0f - t) * cfg.StartColor.a + t * cfg.EndColor.a);
                }
                else
                {
                    c.a = cfg.StartColor.a;
                }

                Vector2f left = pt.Position + normal * halfWidth;
                Vector2f right = pt.Position - normal * halfWidth;

                m_trailVerticesScratch.push_back(Vertex{left.X, left.Y, c, 0.0f, t});
                m_trailVerticesScratch.push_back(Vertex{right.X, right.Y, c, 1.0f, t});
            }

            if (m_trailVerticesScratch.size() >= 4)
            {
                m_renderer.DrawVertices(m_trailVerticesScratch, PrimitiveType::TriangleStrip, 0, cfg.Blend);
            }
        }
    });
}


