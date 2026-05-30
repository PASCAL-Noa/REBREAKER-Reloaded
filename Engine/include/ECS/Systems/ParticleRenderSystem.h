#pragma once
#include "ECS/System.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/ColorComponent.h"
#include <vector>

class ParticleRenderSystem : public System
{
public:
    ParticleRenderSystem(Registry& registry, Renderer& renderer, int maxParticles);

    void OnRender() override;

private:
    Renderer& m_renderer;
    std::vector<Vertex> m_vertices;
};