#include "Scenes/DefaultScene.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/SceneManager.h"
#include "InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/Transform2D.h"
#include "Data/Color.h"

void DefaultScene::OnInit(GameContext& context)
{
    Scene::OnInit(context);
    m_fontId = context.Resources.LoadResource("Resources/font/arial.ttf");
}

void DefaultScene::OnUpdate(float dt, GameContext& context)
{
    Scene::OnUpdate(dt, context);

    if (context.Input.IsKeyDown(KeyCode::Num0))
    {
        context.Scenes.LoadScene<MenuScene>();
    }
}

void DefaultScene::OnRender(GameContext& context)
{
    Scene::OnRender(context);
}

void DefaultScene::DrawDefaultUI(const GameContext& context, const std::string& sceneName, const std::string& instructions) const
{
    std::string text = sceneName + "\n\n";
    text += "FPS : " + std::to_string(context.Data.FPS) + "\n";
    text += "Entities : " + std::to_string(m_registry.GetActiveEntityCount()) + "\n\n";
    text += instructions + "\n\n";
    text += "[0] Retour au menu";

    context.Render.DrawText(text, m_fontId, 24.0f, Transform2D{Vector2f{10.0f, 10.0f}}, Colors::White);
}