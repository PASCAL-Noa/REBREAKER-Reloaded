#pragma once
#include "Core/Scene.h"
#include <cstdint>

class MenuScene : public Scene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

private:
    uint32_t m_fontId = 0;
};