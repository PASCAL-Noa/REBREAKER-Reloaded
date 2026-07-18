#pragma once
#include "Events/Event.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>

class EventDispatcher
{
public:
    using EventCallback = std::function<void(const Event&)>;
    using SubscriptionID = uint64_t;

    template <typename T, typename F>
    SubscriptionID Subscribe(F&& callback)
    {
        uint32_t eventId = GetEventId<T>();
        SubscriptionID subId = ++m_nextSubscriptionId;
        
        m_observers[eventId].push_back({subId, [callback](const Event& e)
        {
            callback(static_cast<const T&>(e));
        }});
        
        return subId;
    }

    void Unsubscribe(uint32_t eventId, SubscriptionID subId)
    {
        auto it = m_observers.find(eventId);
        if (it != m_observers.end())
        {
            auto& callbacks = it->second;
            for (auto cbIt = callbacks.begin(); cbIt != callbacks.end(); ++cbIt)
            {
                if (cbIt->first == subId)
                {
                    callbacks.erase(cbIt);
                    break;
                }
            }
        }
    }

    template <typename T>
    void Publish(const T& event)
    {
        uint32_t eventId = GetEventId<T>();
        auto it = m_observers.find(eventId);
        
        if (it != m_observers.end())
        {
            auto callbacksCopy = it->second;
            for (const auto& observer : callbacksCopy)
            {
                observer.second(event);
            }
        }
    }

    void Clear()
    {
        m_observers.clear();
    }

private:
    SubscriptionID m_nextSubscriptionId = 0;
    std::unordered_map<uint32_t, std::vector<std::pair<SubscriptionID, EventCallback>>> m_observers;
};