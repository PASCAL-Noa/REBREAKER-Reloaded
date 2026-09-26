#pragma once
#include "Events/Event.h"
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <algorithm>

class EventDispatcher;

class ScopedSubscription
{
public:
    ScopedSubscription() = default;

    ScopedSubscription(EventDispatcher& dispatcher, uint32_t eventId, uint64_t subId)
        : m_dispatcher(&dispatcher), m_eventId(eventId), m_subId(subId)
    {
    }

    ~ScopedSubscription()
    {
        Reset();
    }

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    ScopedSubscription(ScopedSubscription&& other) noexcept
        : m_dispatcher(other.m_dispatcher), m_eventId(other.m_eventId), m_subId(other.m_subId)
    {
        other.m_dispatcher = nullptr;
        other.m_eventId = 0;
        other.m_subId = 0;
    }

    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_dispatcher = other.m_dispatcher;
            m_eventId = other.m_eventId;
            m_subId = other.m_subId;
            other.m_dispatcher = nullptr;
            other.m_eventId = 0;
            other.m_subId = 0;
        }
        return *this;
    }

    void Reset();

    void Release()
    {
        m_dispatcher = nullptr;
        m_eventId = 0;
        m_subId = 0;
    }

    [[nodiscard]] bool IsActive() const
    {
        return m_dispatcher != nullptr && m_subId != 0;
    }

    [[nodiscard]] uint64_t GetId() const
    {
        return m_subId;
    }

    [[nodiscard]] uint32_t GetEventId() const
    {
        return m_eventId;
    }

private:
    EventDispatcher* m_dispatcher = nullptr;
    uint32_t m_eventId = 0;
    uint64_t m_subId = 0;
};

class EventDispatcher
{
public:
    using EventCallback = std::function<void(const Event&)>;
    using SubscriptionID = uint64_t;

    struct ObserverInfo
    {
        SubscriptionID Id = 0;
        const void* Owner = nullptr;
        EventCallback Callback;
    };

    template <typename T, typename F>
    SubscriptionID Subscribe(F&& callback)
    {
        return SubscribeForOwner<T>(nullptr, std::forward<F>(callback));
    }

    template <typename T, typename F>
    SubscriptionID SubscribeForOwner(const void* owner, F&& callback)
    {
        uint32_t eventId = GetEventId<T>();
        SubscriptionID subId = ++m_nextSubscriptionId;
        
        m_observers[eventId].push_back(ObserverInfo{
            subId,
            owner,
            [callback](const Event& e)
            {
                callback(static_cast<const T&>(e));
            }
        });
        
        return subId;
    }

    template <typename T, typename F>
    [[nodiscard]] ScopedSubscription SubscribeScoped(F&& callback)
    {
        uint32_t eventId = GetEventId<T>();
        SubscriptionID subId = Subscribe<T>(std::forward<F>(callback));
        return ScopedSubscription(*this, eventId, subId);
    }

    void Unsubscribe(uint32_t eventId, SubscriptionID subId)
    {
        auto it = m_observers.find(eventId);
        if (it != m_observers.end())
        {
            auto& callbacks = it->second;
            for (auto cbIt = callbacks.begin(); cbIt != callbacks.end(); ++cbIt)
            {
                if (cbIt->Id == subId)
                {
                    callbacks.erase(cbIt);
                    break;
                }
            }
            if (callbacks.empty())
            {
                m_observers.erase(it);
            }
        }
    }

    void ClearForOwner(const void* owner)
    {
        if (!owner) return;
        for (auto it = m_observers.begin(); it != m_observers.end();)
        {
            auto& callbacks = it->second;
            std::erase_if(callbacks, [owner](const ObserverInfo& obs) {
                return obs.Owner == owner;
            });
            if (callbacks.empty())
            {
                it = m_observers.erase(it);
            }
            else
            {
                ++it;
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
                observer.Callback(event);
            }
        }
    }

    void Clear()
    {
        m_observers.clear();
    }

    [[nodiscard]] size_t GetObserverCount() const
    {
        size_t count = 0;
        for (const auto& [id, list] : m_observers)
        {
            count += list.size();
        }
        return count;
    }

    template <typename T>
    [[nodiscard]] size_t GetObserverCount() const
    {
        uint32_t eventId = GetEventId<T>();
        auto it = m_observers.find(eventId);
        return (it != m_observers.end()) ? it->second.size() : 0;
    }

private:
    SubscriptionID m_nextSubscriptionId = 0;
    std::unordered_map<uint32_t, std::vector<ObserverInfo>> m_observers;
};

inline void ScopedSubscription::Reset()
{
    if (m_dispatcher && m_subId != 0)
    {
        m_dispatcher->Unsubscribe(m_eventId, m_subId);
        m_dispatcher = nullptr;
        m_eventId = 0;
        m_subId = 0;
    }
}