#include "Scenes/MenuScene.h"
#include "Scenes/SandBox.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "Scenes/GameScene.h"
#include "Scenes/SampleAudio.h"
#include "Scenes/SamplePhysics.h"
#include "Scenes/SampleStateMachine.h"
#include "Scenes/SampleTween.h"
#include "Scenes/SampleUI.h"
#include "Scenes/SampleRenderLayer.h"
#include "Scenes/SamplePowerUps.h"

void MenuScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
}

void MenuScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    if (context.Input.IsKeyPress(KeyCode::Num1) || context.Input.IsKeyPress(KeyCode::F1))
    {
        context.Scenes.LoadScene<SandBox>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num2) || context.Input.IsKeyPress(KeyCode::F2))
    {
        context.Scenes.LoadScene<SamplePhysics>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num3) || context.Input.IsKeyPress(KeyCode::F3))
    {
        context.Scenes.LoadScene<SampleAudio>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num4) || context.Input.IsKeyPress(KeyCode::F4))
    {
        context.Scenes.LoadScene<GameScene>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num5) || context.Input.IsKeyPress(KeyCode::F5))
    {
        context.Scenes.LoadScene<SampleStateMachine>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num6) || context.Input.IsKeyPress(KeyCode::F6))
    {
        context.Scenes.LoadScene<SampleTween>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num7) || context.Input.IsKeyPress(KeyCode::F7))
    {
        context.Scenes.LoadScene<SampleUI>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num8) || context.Input.IsKeyPress(KeyCode::F8))
    {
        context.Scenes.LoadScene<SampleRenderLayer>();
    }
    if (context.Input.IsKeyPress(KeyCode::Num9) || context.Input.IsKeyPress(KeyCode::F9))
    {
        context.Scenes.LoadScene<SamplePowerUps>();
    }
}

void MenuScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    context.Render.ResetCamera();

    context.Render.DrawText("Press '1' / 'F1' for SandBox", m_fontId, 32.0f, Transform2D{200.0f, 250.0f}, Colors::White);
    context.Render.DrawText("Press '2' / 'F2' for Physics", m_fontId, 32.0f, Transform2D{200.0f, 300.0f}, Colors::White);
    context.Render.DrawText("Press '3' / 'F3' for Audio", m_fontId, 32.0f, Transform2D{200.0f, 350.0f}, Colors::White);
    context.Render.DrawText("Press '4' / 'F4' for Game", m_fontId, 32.0f, Transform2D{200.0f, 400.0f}, Colors::Green);
    context.Render.DrawText("Press '5' / 'F5' for StateMachine", m_fontId, 32.0f, Transform2D{200.0f, 450.0f}, Colors::White);
    context.Render.DrawText("Press '6' / 'F6' for Tween", m_fontId, 32.0f, Transform2D{200.0f, 500.0f}, Colors::White);
    context.Render.DrawText("Press '7' / 'F7' for UI", m_fontId, 32.0f, Transform2D{200.0f, 550.0f}, Colors::White);
    context.Render.DrawText("Press '8' / 'F8' for Render Layer", m_fontId, 32.0f, Transform2D{200.0f, 600.0f}, Colors::White);
    context.Render.DrawText("Press '9' / 'F9' for Power-Ups Sample", m_fontId, 32.0f, Transform2D{200.0f, 650.0f}, Color{0, 220, 255, 255});
    context.Render.DrawText("Quick Scene Jump: [F1-F9] Jump Directly from Any Scene | [0]/[F12] Menu", m_fontId, 24.0f, Transform2D{200.0f, 720.0f}, Colors::Yellow);
}
