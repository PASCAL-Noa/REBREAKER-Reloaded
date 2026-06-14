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

void MenuScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
}

void MenuScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    if (context.Input.IsKeyDown(KeyCode::Num1))
    {
        context.Scenes.LoadScene<SandBox>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num2))
    {
        context.Scenes.LoadScene<SamplePhysics>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num3))
    {
        context.Scenes.LoadScene<SampleAudio>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num4))
    {
        context.Scenes.LoadScene<GameScene>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num5))
    {
        context.Scenes.LoadScene<SampleStateMachine>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num6))
    {
        context.Scenes.LoadScene<SampleTween>();
    }

    if (context.Input.IsKeyDown(KeyCode::Num7))
    {
        context.Scenes.LoadScene<SampleUI>();
    }
}

void MenuScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    context.Render.ResetCamera();

    context.Render.DrawText("Press '1' for SandBox", m_fontId, 32.0f, Transform2D{200.0f, 300.0f}, Colors::White);
    context.Render.DrawText("Press '2' for Physics", m_fontId, 32.0f, Transform2D{200.0f, 350.0f}, Colors::White);
    context.Render.DrawText("Press '3' for Audio", m_fontId, 32.0f, Transform2D{200.0f, 400.0f}, Colors::White);
    context.Render.DrawText("Press '4' for Game", m_fontId, 32.0f, Transform2D{200.0f, 450.0f}, Colors::Green);
    context.Render.DrawText("Press '5' for StateMachine", m_fontId, 32.0f, Transform2D{200.0f, 500.0f}, Colors::White);
    context.Render.DrawText("Press '6' for Tween", m_fontId, 32.0f, Transform2D{200.0f, 550.0f}, Colors::White);
    context.Render.DrawText("Press '7' for UI", m_fontId, 32.0f, Transform2D{200.0f, 600.0f}, Colors::White);
}
