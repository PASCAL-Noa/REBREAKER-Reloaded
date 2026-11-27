#pragma once
#include "ECS/System.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Entity.h"
#include "Graphics/Renderer.h"
#include <vector>

struct SpriteComponent;

class RenderSystem : public System
{
public:
    RenderSystem(Registry& registry, Renderer& renderer);
    void OnUpdate(float dt) override;
    void OnRender() override;

private:
    struct RenderItem
    {
        const SpriteComponent* sprite = nullptr;
        Transform2D renderTransform;
        Transform2D worldTransform;
        int layer = 0;
        Entity entity = 0;
    };

    Renderer& m_renderer;
    std::vector<RenderItem> m_renderQueue;
    std::vector<Vertex> m_trailVerticesScratch;
    float m_animTime = 0.0f;

    void RenderTrails();
};
