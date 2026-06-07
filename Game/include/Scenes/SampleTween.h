#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"

class SampleTween : public DefaultScene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

private:
    Entity m_agent;
    uint32_t m_texId = 0;
};