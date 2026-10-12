#pragma once
#include "ECS/System.h"
#include "Graphics/Renderer.h"
#include "Math/Vector2.h"
#include "Data/Color.h"
#include <vector>

class ParticleSystem : public System
{
public:
    struct Particle
    {
        Vector2f Position{0.0f, 0.0f};
        Vector2f Velocity{0.0f, 0.0f};
        float    Life = 0.0f;
        float    MaxLife = 1.0f;
        float    Size = 10.0f;
        Color    Tint = Colors::White;
    };

    ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles);

    void Spawn(const Vector2f& position, const Vector2f& velocity, float life, float size, const Color& tint);
    void Clear();

    void OnUpdate(float dt) override;
    void OnRender() override;

private:
    Renderer&               m_renderer;
    std::vector<Particle>   m_pool;
    size_t                  m_poolCapacity = 0;
    size_t                  m_nextPoolIndex = 0;
    std::vector<Vertex>     m_vertices;
};