#include "ECS/Systems/ParticleRenderSystem.h"
#include "ECS/Registry.hpp"

ParticleRenderSystem::ParticleRenderSystem(Registry& registry, Renderer& renderer, int maxParticles)
    : System(registry), m_renderer(renderer)
{
    m_vertices.reserve(maxParticles);
}

void ParticleRenderSystem::OnRender()
{
    m_vertices.clear();

    m_registry.View<Transform2D, ColorComponent>([this](Entity e, Transform2D& transform, ColorComponent& color)
    {
        m_vertices.push_back({ transform.X, transform.Y, color.Tint, 0.0f, 0.0f });
    });

    m_renderer.DrawVertices(m_vertices, PrimitiveType::Points, 0, BlendMode::Add);
}