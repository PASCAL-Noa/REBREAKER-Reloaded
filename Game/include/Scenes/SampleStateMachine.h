#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "StateMachine/StateMachine.h"

class SampleStateMachine : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;

    auto&   GetRegistry() { return m_registry; }
    [[nodiscard]] GameContext*  GetContext() const { return mp_currentContext; }
    [[nodiscard]] float   GetDeltaTime() const { return m_dt; }

private:
    Entity  m_agent = 0;
    uint32_t    m_texId = 0;
    
    GameContext*    mp_currentContext = nullptr;
    float m_dt = 0.0f;

    StateMachine<SampleStateMachine>*   mp_stateMachine = nullptr;
};