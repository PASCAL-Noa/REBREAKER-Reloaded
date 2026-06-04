#pragma once
#include "Core/Scene.h"
#include "ECS/Components/Camera2D.h"
#include <string>
#include <cstdint>

class DefaultScene : public Scene
{
public:
    void    OnInit(GameContext& context) override;
    void    OnUpdate(float dt, GameContext& context) override;
    void    OnRender(GameContext& context) override;

protected:
    void    DrawDefaultUI(const GameContext& context, const std::string& sceneName, const std::string& instructions) const;

    Camera2D    m_camera{Vector2f{0.0f, 0.0f}};
    uint32_t    m_fontId = 0;
};