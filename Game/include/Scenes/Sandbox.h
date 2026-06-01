#pragma once
#include "Core/Scene.h"
#include "ECS/Components/Camera2D.h"
#include <cstdint>

class SandBox : public Scene
{
public:
    void                    OnInit(GameContext& context) override;
    void                    OnUpdate(float dt, GameContext& context) override;
    void                    OnRender(GameContext& context) override;
    void                    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t  GetPostProcessShader() const override;

private:
    void                    InitPhysicsWorld();
    void                    InitParticles();
    void                    HandleInput(float dt, const GameContext& context);
    void                    SpawnParticles(int count, float x, float y);
    void                    DrawUI(const GameContext& context) const;
    void                    CreateWall(float x, float y, float w, float h);

    Camera2D                m_camera{Vector2f{0.0f, 0.0f}};
    uint32_t                m_fontId = 0;
    uint32_t                m_shaderId = 0;
    bool                    m_enableShader = false;
    Entity                  m_player = 0;
};