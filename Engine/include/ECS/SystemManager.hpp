#pragma once
#include "System.h"
#include <vector>

class SystemManager
{
public:
    SystemManager() = default;

    ~SystemManager()
    {
        for (System* system : m_systems)
        {
            delete system;
        }
        m_systems.clear();
    }

    template <typename T, typename... Args>
    T* AddSystem(Args&&... args)
    {
        T* system = new T(std::forward<Args>(args)...);
        m_systems.push_back(system);
        return system;
    }

    void OnInit() const
    {
        for (System* system : m_systems) system->OnInit();
    }

    void OnBeginUpdate() const
    {
        for (System* system : m_systems) system->OnBeginUpdate();
    }

    void OnUpdate(float dt) const
    {
        for (System* system : m_systems) system->OnUpdate(dt);
    }

    void OnLateUpdate(float dt) const
    {
        for (System* system : m_systems) system->OnLateUpdate(dt);
    }

    void OnEndUpdate() const
    {
        for (System* system : m_systems) system->OnEndUpdate();
    }

    void OnRender() const
    {
        for (System* system : m_systems) system->OnRender();
    }

private:
    std::vector<System*> m_systems;
};