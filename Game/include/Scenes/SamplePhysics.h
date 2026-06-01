#pragma once
#include "Core/Scene.h"
#include "ECS/Components/Camera2D.h"
#include <cstdint>

class SamplePhysics : public Scene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

private:
    void CreateBox(float x, float y, float w, float h, float vx, float vy, bool isTrigger, bool isKinematic);
    void HandleInput(float dt, const GameContext& context);

    Camera2D m_camera{Vector2f{0.0f, 0.0f}};
    uint32_t m_fontId = 0;
    Entity m_player = 0;
};