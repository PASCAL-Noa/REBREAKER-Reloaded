#pragma once
#include "ECS/System.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/ColorComponent.h"
#include <vector>

class ParticleRenderSystem : public System
{
public:
    ParticleRenderSystem(Registry& registry, Renderer& renderer, int maxParticles)
        : System(registry), m_renderer(renderer)
    {
        m_vertices.reserve(maxParticles);
    }

    void OnUpdate(float dt) override
    {
        m_vertices.clear();

        m_registry.View<Transform2D, ColorComponent>([this](Entity e, Transform2D& transform, ColorComponent& color) 
        {
            m_vertices.push_back({ transform.X, transform.Y, color.Tint, 0.0f, 0.0f });
        });

        m_renderer.DrawVertices(m_vertices, PrimitiveType::Points, 0, BlendMode::Add);
    }

private:
    Renderer& m_renderer;
    std::vector<Vertex> m_vertices;
};