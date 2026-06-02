#pragma once
#include "Events/Event.h"
#include <unordered_map>
#include <vector>
#include <functional>

class EventDispatcher
{
public:
    using EventCallback = std::function<void(const Event&)>;

    template <typename T, typename F>
    void Subscribe(F&& callback)
    {
        m_observers[GetEventId<T>()].push_back([callback](const Event& e)
        {
            callback(static_cast<const T&>(e));
        });
    }

    template <typename T>
    void Publish(const T& event)
    {
        uint32_t eventId = GetEventId<T>();
        auto it = m_observers.find(eventId);
        
        if (it != m_observers.end())
        {
            for (const auto& observer : it->second)
            {
                observer(event);
            }
        }
    }

    void Clear()
    {
        m_observers.clear();
    }

private:
    std::unordered_map<uint32_t, std::vector<EventCallback>> m_observers;
};