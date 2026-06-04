#pragma once
#include "StateMachine/Action.h"
#include "ECS/Entity.h"
#include "ECS/Components/SpriteComponent.h"
#include "Data/Color.h"

template<typename T>
class ChangeColorAction : public Action<T>
{
public:
    ChangeColorAction(Entity e, Color c) : m_entity(e), m_color(c) {}

    void    Start(T* pOwner) override
    {
        pOwner->GetRegistry().template GetComponent<SpriteComponent>(m_entity).Tint = m_color;
    }

    void    Update(T* pOwner) override {}
    void    End(T* pOwner) override {}

private:
    Entity  m_entity;
    Color   m_color;
};