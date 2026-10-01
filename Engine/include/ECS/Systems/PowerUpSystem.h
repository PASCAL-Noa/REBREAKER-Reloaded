#pragma once
#include "ECS/System.h"
#include "Events/EventDispatcher.h"

class PowerUpSystem : public System
{
public:
    PowerUpSystem(Registry& registry, EventDispatcher& events);
    ~PowerUpSystem() override = default;

    void OnUpdate(float dt) override;

private:
    EventDispatcher& m_events;
};
