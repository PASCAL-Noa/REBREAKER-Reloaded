#pragma once
#include "Scenes/DefaultScene.h"
#include <cstdint>

class SandBox : public DefaultScene
{
public:
    void                    OnInit(GameContext& context) override;
    void                    OnUpdate(float dt, GameContext& context) override;
    void                    OnRender(GameContext& context) override;
    void                    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t  GetPostProcessShader() const override;

private:
    void                    InitPhysicsWorld(GameContext& context);
    void                    InitParticles();
    void                    HandleInput(float dt, const GameContext& context);
    void                    SpawnParticles(int count, float x, float y);
    void                    CreateWall(float x, float y, float w, float h);

    uint32_t                m_shaderId = 0;
    bool                    m_enableShader = false;
    Entity                  m_player = 0;
    uint32_t                m_bounceSfxId = 0;
    uint32_t                m_debugTexId = 0;
};