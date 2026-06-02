#pragma once
#include "ECS/System.h"

class Renderer;

class RenderSystem : public System
{
public:
    RenderSystem(Registry& registry, Renderer& renderer);
    void OnRender() override;

private:
    Renderer& m_renderer;
};