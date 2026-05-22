#pragma once

class Registry;

class System
{
public:
    explicit System(Registry& registry) : m_registry(registry) {}
    virtual ~System() = default;

    virtual void OnInit() {}
    virtual void OnBeginUpdate() {}
    virtual void OnUpdate(float dt) {}
    virtual void OnLateUpdate(float dt) {}
    virtual void OnEndUpdate() {}

protected:
    Registry& m_registry;
};