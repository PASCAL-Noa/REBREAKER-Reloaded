#pragma once
#include "Core/Scene.h"
#include "Core/GameContext.h"
#include "InputManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"

class SandBox : public Scene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t GetPostProcessShader() const override;

private:
    void InitParticles();
    void HandleInput(float dt, const InputManager& input);
    void SpawnParticles(int count, float x, float y);
    static void DrawLights(Renderer& renderer);
    void DrawUI(const GameContext& context) const;

    Transform2D m_player{0, 0.0f, 0.0f};
    Camera2D m_camera{0, 0.0f, 0.0f};

    uint32_t m_fontId = 0;
    uint32_t m_shaderId = 0;
    bool m_enableShader = false;
};