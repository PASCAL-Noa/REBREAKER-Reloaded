#pragma once
#include "Entity.h"
#include "ISparseSet.h"
#include "SparseSet.hpp"
#include <vector>
#include <queue>

class ComponentCounter
{
public:
    template <typename T>
    static size_t GetId()
    {
        static size_t id = s_counter++;
        return id;
    }
private:
    static inline size_t s_counter = 0;
};

class Registry
{
public:
    Registry() = default;
    ~Registry()
    {
        for (ISparseSet* pool : m_pools)
        {
            delete pool;
        }
    }

    Entity CreateEntity()
    {
        if (!m_freeEntities.empty())
        {
            Entity entity = m_freeEntities.front();
            m_freeEntities.pop();
            return entity;
        }
        return m_entityCount++;
    }

    void DestroyEntity(Entity entity)
    {
        for (ISparseSet* pool : m_pools)
        {
            if (pool) pool->Remove(entity);
        }
        m_freeEntities.push(entity);
    }

    template <typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args)
    {
        SparseSet<T>* pool = GetOrCreatePool<T>();
        pool->Insert(entity, T(entity, std::forward<Args>(args)...));
        return pool->Get(entity);
    }

    template <typename T>
    void RemoveComponent(Entity entity)
    {
        if (SparseSet<T>* pool = GetPool<T>())
        {
            pool->Remove(entity);
        }
    }

    template <typename T>
    bool HasComponent(Entity entity) const
    {
        SparseSet<T>* pool = GetPool<T>();
        return pool && pool->Contains(entity);
    }

    template <typename T>
    T& GetComponent(Entity entity)
    {
        return GetPool<T>()->Get(entity);
    }

    template <typename T1, typename... Tn, typename Func>
    void View(Func&& func)
    {
        SparseSet<T1>* primaryPool = GetPool<T1>();
        if (!primaryPool) return;

        for (Entity entity : primaryPool->GetEntities())
        {
            if ((HasComponent<Tn>(entity) && ...))
            {
                func(entity, primaryPool->Get(entity), GetComponent<Tn>(entity)...);
            }
        }
    }

private:
    template <typename T>
    SparseSet<T>* GetPool() const
    {
        size_t id = ComponentCounter::GetId<T>();
        if (id >= m_pools.size() || !m_pools[id]) return nullptr;
        return static_cast<SparseSet<T>*>(m_pools[id]);
    }

    template <typename T>
    SparseSet<T>* GetOrCreatePool()
    {
        size_t id = ComponentCounter::GetId<T>();
        if (id >= m_pools.size()) m_pools.resize(id + 1, nullptr);

        if (!m_pools[id]) m_pools[id] = new SparseSet<T>();

        return static_cast<SparseSet<T>*>(m_pools[id]);
    }

    std::vector<ISparseSet*> m_pools;
    std::queue<Entity> m_freeEntities;
    Entity m_entityCount = 0;
};