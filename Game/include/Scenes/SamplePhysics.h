#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"

class SamplePhysics : public DefaultScene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

private:
    void CreateBox(float x, float y, float w, float h, float vx, float vy, bool isTrigger, bool isKinematic);
    void HandleInput(float dt, const GameContext& context);

    Entity m_player = 0;
};