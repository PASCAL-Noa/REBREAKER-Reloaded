#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/ParticleComponent.h"

ParticleSystem::ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles)
    : System(registry), m_renderer(renderer)
{
    m_vertices.reserve(maxParticles * 6);
}

void ParticleSystem::OnUpdate(float dt)
{
    std::vector<Entity> toDestroy;

    m_registry.View<Transform2D, ParticleComponent>([&](Entity e, Transform2D& transform, ParticleComponent& particle)
    {
        transform.Position.X += particle.Velocity.X * dt;
        transform.Position.Y += particle.Velocity.Y * dt;

        particle.Life -= dt;
        if (particle.Life <= 0.0f)
        {
            toDestroy.push_back(e);
        }
    });

    for (Entity e : toDestroy)
    {
        m_registry.DestroyEntity(e);
    }
}

void ParticleSystem::OnRender()
{
    m_vertices.clear();

    m_registry.View<Transform2D, ParticleComponent>([this](Entity e, const Transform2D& transform, const ParticleComponent& particle)
    {
        if (particle.Life <= 0.0f) return;

        float alphaRatio = particle.Life / particle.MaxLife;
        if (alphaRatio > 1.0f) alphaRatio = 1.0f;
        if (alphaRatio < 0.0f) alphaRatio = 0.0f;

        Color c = particle.Tint;
        c.a = static_cast<uint8_t>(255.0f * alphaRatio);

        float hs = particle.Size * 0.5f;

        float left = transform.Position.X - hs;
        float right = transform.Position.X + hs;
        float top = transform.Position.Y - hs;
        float bottom = transform.Position.Y + hs;

        m_vertices.push_back({ left, top, c, 0.0f, 0.0f });
        m_vertices.push_back({ right, top, c, 0.0f, 0.0f });
        m_vertices.push_back({ right, bottom, c, 0.0f, 0.0f });

        m_vertices.push_back({ right, bottom, c, 0.0f, 0.0f });
        m_vertices.push_back({ left, bottom, c, 0.0f, 0.0f });
        m_vertices.push_back({ left, top, c, 0.0f, 0.0f });
    });

    if (!m_vertices.empty())
    {
        m_renderer.DrawVertices(m_vertices, PrimitiveType::Triangles, 0, BlendMode::Alpha);
    }
}