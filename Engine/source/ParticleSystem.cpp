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

    size_t targetIndex = 0;
    if (m_activeCount < m_poolCapacity)
    {
        targetIndex = m_activeCount++;
    }
    else
    {
        targetIndex = m_nextPoolIndex;
        m_nextPoolIndex = (m_nextPoolIndex + 1) % m_poolCapacity;
    }

    Particle& p = m_pool[targetIndex];
    p.Position = position;
    p.Velocity = velocity;
    p.Life = life;
    p.MaxLife = life;
    p.Size = size;
    p.Tint = tint;
}

void ParticleSystem::Clear()
{
    for (size_t i = 0; i < m_activeCount; ++i)
    {
        m_pool[i].Life = 0.0f;
    }
    m_activeCount = 0;
    m_nextPoolIndex = 0;
}

void ParticleSystem::OnUpdate(float dt)
{
    // 1. Update internal pooled particles (Zero dead iterations, O(1) removal)
    for (size_t i = 0; i < m_activeCount; )
    {
        auto& p = m_pool[i];
        p.Position.X += p.Velocity.X * dt;
        p.Position.Y += p.Velocity.Y * dt;
        p.Life -= dt;

        if (p.Life <= 0.0f)
        {
            // Swap with last active particle
            if (m_activeCount > 1 && i < m_activeCount - 1)
            {
                p = m_pool[m_activeCount - 1];
            }
            --m_activeCount;
        }
        else
        {
            ++i;
        }
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

    // 1. Render internal pooled particles (only active count)
    for (size_t i = 0; i < m_activeCount; ++i)
    {
        const auto& p = m_pool[i];
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