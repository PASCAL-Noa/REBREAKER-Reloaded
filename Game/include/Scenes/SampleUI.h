#pragma once
#include "Scenes/DefaultScene.h"
#include "../../../Engine/include/ECS/Systems/UISystem.h"
#include "ECS/Entity.h"

class SampleUI : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;
    void    OnDestroy(GameContext& context) override;

private:
    UISystem    m_uiSystem;
    Entity      m_canvas = NULL_ENTITY;
    uint32_t    m_fontId = 0;
};
