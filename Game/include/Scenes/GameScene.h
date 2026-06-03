#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"

class GameScene : public DefaultScene
{
public:
    void                    OnInit(GameContext& context) override;
    void                    OnUpdate(float dt, GameContext& context) override;
    void                    OnRender(GameContext& context) override;
    void                    OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t  GetPostProcessShader() const override;

private:
    void                    CreateWall(float x, float y, float w, float h);
    Entity                  m_ball{};
    uint32_t                m_shaderId = 0;
    uint32_t                m_ballTexId = 0;
    uint32_t                m_bounceSfxId = 0;
    bool                    m_enableShader = false;
    bool                    m_showDebug = false;
};