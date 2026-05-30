#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Velocity2D.h"
#include "ECS/Components/ColorComponent.h"

ParticleSystem::ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles)
    : System(registry), m_renderer(renderer)
{
    m_vertices.reserve(maxParticles);
}

void ParticleSystem::OnUpdate(float dt)
{
    std::vector<Entity> toDestroy;

    m_registry.View<Transform2D, Velocity2D>([&](Entity e, Transform2D& transform, Velocity2D& velocity)
    {
        transform.X += velocity.X * dt;
        transform.Y += velocity.Y * dt;

        if (transform.X < -1000.0f || transform.X > 1000.0f ||
            transform.Y < -1000.0f || transform.Y > 1000.0f)
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

    m_registry.View<Transform2D, ColorComponent>([this](Entity e, Transform2D& transform, ColorComponent& color)
    {
        m_vertices.push_back({ transform.X, transform.Y, color.Tint, 0.0f, 0.0f });
    });

    m_renderer.DrawVertices(m_vertices, PrimitiveType::Points, 0, BlendMode::Add);
}