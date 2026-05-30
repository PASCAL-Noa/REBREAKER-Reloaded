#pragma once
#include "ECS/System.h"
#include "Graphics/Renderer.h"
#include <vector>

class ParticleSystem : public System
{
public:
    ParticleSystem(Registry& registry, Renderer& renderer, int maxParticles);

    void OnUpdate(float dt) override;
    void OnRender() override;

private:
    Renderer& m_renderer;
    std::vector<Vertex> m_vertices;
};