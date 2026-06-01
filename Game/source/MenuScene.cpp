#include "Scenes/MenuScene.h"
#include "Scenes/SandBox.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"

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
}

void MenuScene::OnRender(GameContext& context)
{
    context.Render.DrawText("MENU SCENE - Appuie sur '1' pour la SandBox", m_fontId, 32.0f, Transform2D{200.0f, 300.0f}, Colors::White);
}