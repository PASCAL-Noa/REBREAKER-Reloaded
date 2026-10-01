#include "Scenes/DefaultScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/SandBox.h"
#include "Scenes/SamplePhysics.h"
#include "Scenes/SampleAudio.h"
#include "Scenes/GameScene.h"
#include "Scenes/SampleStateMachine.h"
#include "Scenes/SampleTween.h"
#include "Scenes/SampleUI.h"
#include "Scenes/SampleRenderLayer.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "ECS/Components/Transform2D.h"
#include "Data/Color.h"
#include "AudioMixer.h"
#include "ECS/Components/TweenComponent.h"

void DefaultScene::OnInit(GameContext& context)
{
    Scene::OnInit(context);
    mp_context = &context;
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");

    m_camera = m_registry.CreateEntity();
    m_registry.AddComponent<Camera2D>(m_camera, Camera2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<TweenComponent>(m_camera, TweenComponent{});
}

void DefaultScene::OnUpdate(float dt, GameContext& context)
{
    Scene::OnUpdate(dt, context);
    mp_context = &context;

    // Universal quick scene navigation from anywhere
    if (context.Input.IsKeyPress(KeyCode::F1))      context.Scenes.LoadScene<SandBox>();
    else if (context.Input.IsKeyPress(KeyCode::F2)) context.Scenes.LoadScene<SamplePhysics>();
    else if (context.Input.IsKeyPress(KeyCode::F3)) context.Scenes.LoadScene<SampleAudio>();
    else if (context.Input.IsKeyPress(KeyCode::F4)) context.Scenes.LoadScene<GameScene>();
    else if (context.Input.IsKeyPress(KeyCode::F5)) context.Scenes.LoadScene<SampleStateMachine>();
    else if (context.Input.IsKeyPress(KeyCode::F6)) context.Scenes.LoadScene<SampleTween>();
    else if (context.Input.IsKeyPress(KeyCode::F7)) context.Scenes.LoadScene<SampleUI>();
    else if (context.Input.IsKeyPress(KeyCode::F8)) context.Scenes.LoadScene<SampleRenderLayer>();
    else if (context.Input.IsKeyPress(KeyCode::F12) || context.Input.IsKeyPress(KeyCode::Num0))
    {
        context.Scenes.LoadScene<MenuScene>();
    }
}

void DefaultScene::OnRender(GameContext& context)
{
    Scene::OnRender(context);
    mp_context = &context;
}

void DefaultScene::OnDestroy(GameContext& context)
{
    StopAllAudio();
    Scene::OnDestroy(context);
}

void DefaultScene::DrawDefaultUI(const GameContext& context, const std::string& sceneName, const std::string& instructions) const
{
    std::string text = sceneName + "\n\n";
    text += "FPS : " + std::to_string(context.Data.FPS) + "\n";
    text += "Entities : " + std::to_string(m_registry.GetActiveEntityCount()) + "\n\n";
    text += instructions + "\n\n";
    text += "[0/F12] Menu | [F1]SandBox [F2]Physics [F3]Audio [F4]Game [F5]State [F6]Tween [F7]UI [F8]Render [F9]PowerUps";

    context.Render.DrawText(text, m_fontId, 24.0f, Transform2D{Vector2f{10.0f, 10.0f}}, Colors::White);
}

void DefaultScene::PlaySfx(uint32_t id, float volume, float pitch) const
{
    if (mp_context) mp_context->Audio.PlaySfx(id, volume, pitch);
}

void DefaultScene::PlayMusic(const std::string& filepath, float volume, bool loop) const
{
    if (mp_context) mp_context->Audio.PlayMusic(filepath, volume, loop);
}

void DefaultScene::StopMusic() const
{
    if (mp_context) mp_context->Audio.StopMusic();
}

void DefaultScene::StopAllAudio() const
{
    if (mp_context) mp_context->Audio.StopAll();
}