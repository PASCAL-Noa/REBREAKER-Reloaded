#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/ParticleComponent.h"
#include <algorithm>

ParticleSystem::ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles)
    : System(registry), m_renderer(renderer), m_poolCapacity(static_cast<size_t>(std::max(maxParticles, 1)))
{
    m_pool.resize(m_poolCapacity);
    m_vertices.reserve(m_poolCapacity * 6);
}

void ParticleSystem::Spawn(const Vector2f& position, const Vector2f& velocity, float life, float size, const Color& tint)
{
    if (m_pool.empty()) return;

    // Fast O(1) circular ring buffer insertion
    Particle& p = m_pool[m_nextPoolIndex];
    p.Position = position;
    p.Velocity = velocity;
    p.Life = life;
    p.MaxLife = life;
    p.Size = size;
    p.Tint = tint;

    m_nextPoolIndex = (m_nextPoolIndex + 1) % m_poolCapacity;
}

void ParticleSystem::Clear()
{
    for (auto& p : m_pool)
    {
        p.Life = 0.0f;
    }
    m_nextPoolIndex = 0;
}

void ParticleSystem::OnUpdate(float dt)
{
    // 1. Update internal pooled particles (Zero ECS overhead)
    for (auto& p : m_pool)
    {
        if (p.Life <= 0.0f) continue;

        p.Position.X += p.Velocity.X * dt;
        p.Position.Y += p.Velocity.Y * dt;
        p.Life -= dt;
    }

    // 2. Legacy / backward compatibility: Update any ParticleComponent remaining in ECS
    m_registry.View<Transform2D, ParticleComponent>([this, dt](Entity e, Transform2D& transform, ParticleComponent& particle)
    {
        transform.Position.X += particle.Velocity.X * dt;
        transform.Position.Y += particle.Velocity.Y * dt;

        particle.Life -= dt;
        if (particle.Life <= 0.0f)
        {
            m_registry.DestroyEntityDeferred(e);
        }
    });

    m_registry.ProcessDeferredCommands();
}

void ParticleSystem::OnRender()
{
    m_vertices.clear();

    auto appendQuad = [this](const Vector2f& pos, float size, float life, float maxLife, const Color& tint)
    {
        if (life <= 0.0f || maxLife <= 0.0f) return;

        float alphaRatio = life / maxLife;
        if (alphaRatio > 1.0f) alphaRatio = 1.0f;
        if (alphaRatio < 0.0f) alphaRatio = 0.0f;

        Color c = tint;
        c.a = static_cast<uint8_t>(255.0f * alphaRatio);

        float hs = size * 0.5f;

        float left = pos.X - hs;
        float right = pos.X + hs;
        float top = pos.Y - hs;
        float bottom = pos.Y + hs;

        m_vertices.push_back({ left, top, c, 0.0f, 0.0f });
        m_vertices.push_back({ right, top, c, 0.0f, 0.0f });
        m_vertices.push_back({ right, bottom, c, 0.0f, 0.0f });

        m_vertices.push_back({ right, bottom, c, 0.0f, 0.0f });
        m_vertices.push_back({ left, bottom, c, 0.0f, 0.0f });
        m_vertices.push_back({ left, top, c, 0.0f, 0.0f });
    };

    // 1. Render internal pooled particles
    for (const auto& p : m_pool)
    {
        if (p.Life <= 0.0f) continue;
        appendQuad(p.Position, p.Size, p.Life, p.MaxLife, p.Tint);
    }

    // 2. Render any legacy ECS particles
    m_registry.View<Transform2D, ParticleComponent>([&appendQuad](Entity, const Transform2D& transform, const ParticleComponent& particle)
    {
        appendQuad(transform.Position, particle.Size, particle.Life, particle.MaxLife, particle.Tint);
    });

    if (!m_vertices.empty())
    {
        m_renderer.DrawVertices(m_vertices, PrimitiveType::Triangles, 0, BlendMode::Alpha);
    }
}