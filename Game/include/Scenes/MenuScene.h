#pragma once
#include <cstdint>
#include "Scenes/DefaultScene.h"

class MenuScene : public DefaultScene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;

};