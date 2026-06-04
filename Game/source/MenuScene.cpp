#include "Scenes/MenuScene.h"
#include "Scenes/SandBox.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "Scenes/SampleAudio.h"
#include "Scenes/SamplePhysics.h"
#include "Scenes/SampleStateMachine.h"

void MenuScene::OnInit(GameContext& context)
{
    m_fontId = context.Resources.LoadResource("Resources/font/arial.ttf");
}

void MenuScene::OnUpdate(float dt, GameContext& context)
{
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

    if (context.Input.IsKeyDown(KeyCode::Num5))
    {
        context.Scenes.LoadScene<SampleStateMachine>();
    }

}

void MenuScene::OnRender(GameContext& context)
{
    context.Render.DrawText("Press '1' for SandBox", m_fontId, 32.0f, Transform2D{200.0f, 300.0f}, Colors::White);
    context.Render.DrawText("Press '2' for Physics", m_fontId, 32.0f, Transform2D{200.0f, 350.0f}, Colors::White);
    context.Render.DrawText("Press '3' for Audio", m_fontId, 32.0f, Transform2D{200.0f, 400.0f}, Colors::White);
    context.Render.DrawText("Press '5' for State Machine", m_fontId, 32.0f, Transform2D{200.0f, 500.0f}, Colors::White);
}