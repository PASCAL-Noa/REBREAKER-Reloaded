#include "Scenes/SampleStateMachine.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "ECS/Entity.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "Graphics/Renderer.h"
#include "StateMachine/Action.h"
#include "StateMachine/Condition.h"
#include "StateMachine/Transition.h"
#include "ECS/Systems/RenderSystem.h"
#include "Actions/ChangeColorAction.h"
#include "Conditions/KeyPressCondition.h"
#include <string>

void SampleStateMachine::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_camera.Zoom = 1.25f;
    m_texId = context.Resources.LoadResource("Resources/sprite/debug.jpg");

    m_agent = m_registry.CreateEntity();
    m_registry.AddComponent<Transform2D>(m_agent, Transform2D{Vector2f{0.0f, 0.0f}});
    m_registry.AddComponent<SpriteComponent>(m_agent, SpriteComponent{m_texId, Colors::White});
    m_registry.GetComponent<Transform2D>(m_agent).Scale = Vector2f{1.0f, 1.0f};

    mp_stateMachine = new StateMachine<SampleStateMachine>(this, 2);

    State<SampleStateMachine>* idleState = mp_stateMachine->CreateState(0);
    idleState->AddAction(new ChangeColorAction<SampleStateMachine>(m_agent, Colors::Red));
    idleState->AddTransition(new Transition<SampleStateMachine>(new KeyPressCondition<SampleStateMachine>(KeyCode::Space), 1));

    State<SampleStateMachine>* moveState = mp_stateMachine->CreateState(1);
    moveState->AddAction(new ChangeColorAction<SampleStateMachine>(m_agent, Colors::Green));
    moveState->AddTransition(new Transition<SampleStateMachine>(new KeyPressCondition<SampleStateMachine>(KeyCode::Space), 0));

    mp_stateMachine->SetState(0);

    m_systemManager.AddSystem<RenderSystem>(m_registry, context.Render);
    m_systemManager.OnInit();
}

void SampleStateMachine::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    mp_currentContext = &context;
    m_dt = dt;

    if (mp_stateMachine)
    {
        mp_stateMachine->Update();
    }

    m_systemManager.OnUpdate(dt);
}

void SampleStateMachine::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    context.Render.SetCamera(m_camera);

    m_systemManager.OnRender();

    context.Render.ResetCamera();

    std::string stateStr = mp_stateMachine ? std::to_string(mp_stateMachine->GetCurrentState()) : "NONE";
    std::string stats = "Active State ID: " + stateStr;
    DrawDefaultUI(context, "SAMPLE : STATE-MACHINE", stats);
}

void SampleStateMachine::OnDestroy(GameContext& context)
{
    delete mp_stateMachine;
    mp_stateMachine = nullptr;

    DefaultScene::OnDestroy(context);
}