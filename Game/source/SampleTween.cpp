#include "Scenes/SampleTween.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Systems/TweenSystem.h"
#include "Graphics/Renderer.h"
#include "TweenEffects/TweenEffects.h"
#include <string>

void SampleTween::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_registry.GetComponent<Camera2D>(m_camera).Zoom = 1.25f;
    m_texId = context.Resources.LoadResource("Resources/sprite/debug.jpg");

    m_agent = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_agent, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<SpriteComponent>(m_agent, SpriteComponent{m_texId, Colors::White});
    m_registry.GetComponent<Transform2D>(m_agent).Scale = Vector2f{1.0f, 1.0f};

    m_registry.AddComponent<TweenComponent>(m_agent, TweenComponent{});

    m_systemManager.AddSystem<TweenSystem>(m_registry);
    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    
    m_systemManager.OnInit();
}

void SampleTween::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    if (context.Input.IsKeyPress(KeyCode::Num1))
    {
        auto& tween = m_registry.GetComponent<TweenComponent>(m_agent);
        auto& transform = m_registry.GetComponent<Transform2D>(m_agent);
        TweenEffects::Shake(tween, transform, 0.4f, 25.0f);
    }

    if (context.Input.IsKeyPress(KeyCode::Num2))
    {
        auto& tween = m_registry.GetComponent<TweenComponent>(m_agent);
        auto& transform = m_registry.GetComponent<Transform2D>(m_agent);
        TweenEffects::Spin(tween, transform, 0.8f);
    }

    m_systemManager.OnUpdate(dt);
}

void SampleTween::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_registry.GetComponent<Camera2D>(m_camera));
    m_systemManager.OnRender();
    
    context.Render.ResetCamera();

    std::string stats = "[1] : Shake\n[2] : Spin";
    DrawDefaultUI(context, "SAMPLE : TWEEN", stats);
}

void SampleTween::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}