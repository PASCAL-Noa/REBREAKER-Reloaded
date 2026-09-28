#pragma once
#include "ECS/System.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Entity.h"
#include <vector>

class Renderer;
struct SpriteComponent;

class RenderSystem : public System
{
public:
    RenderSystem(Registry& registry, Renderer& renderer);
    void OnRender() override;

private:
    struct RenderItem
    {
        const SpriteComponent* sprite = nullptr;
        Transform2D renderTransform;
        int layer = 0;
        Entity entity = 0;
    };

    Renderer& m_renderer;
    std::vector<RenderItem> m_renderQueue;
};