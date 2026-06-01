#include "ECS/Systems/ParticleSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/ColorComponent.h"

ParticleSystem::ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles)
    : System(registry), m_renderer(renderer)
{
    m_vertices.reserve(maxParticles);
}

void ParticleSystem::OnUpdate(float dt)
{
    std::vector<Entity> toDestroy;

    m_registry.View<Transform2D, RigidBody>([&](Entity e, Transform2D& transform, const RigidBody& rigidBody)
    {
        transform.Position.X += rigidBody.Velocity.X * dt;
        transform.Position.Y += rigidBody.Velocity.Y * dt;

        if (transform.Position.X < -1000.0f || transform.Position.X > 1000.0f ||
            transform.Position.Y < -1000.0f || transform.Position.Y > 1000.0f)
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

    m_registry.View<Transform2D, ColorComponent>([this](Entity e, const Transform2D& transform, const ColorComponent& color)
    {
        m_vertices.push_back({ transform.Position.X, transform.Position.Y, color.Tint, 0.0f, 0.0f });
    });

    m_renderer.DrawVertices(m_vertices, PrimitiveType::Points, 0, BlendMode::Add);
}