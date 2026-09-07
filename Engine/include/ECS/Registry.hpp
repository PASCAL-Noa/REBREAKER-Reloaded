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
            uint32_t index = m_freeEntities.front();
            m_freeEntities.pop();
            return MakeEntity(index, m_entityVersions[index]);
        }
        uint32_t index = m_entityCount++;
        m_entityVersions.push_back(1);
        return MakeEntity(index, m_entityVersions[index]);
    }

    void DestroyEntity(Entity entity)
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= m_entityVersions.size() || GetEntityVersion(entity) != m_entityVersions[index]) return;

        for (ISparseSet* pool : m_pools)
        {
            if (pool) pool->Remove(entity);
        }
        
        m_entityVersions[index]++;
        m_freeEntities.push(index);
    }

    size_t GetActiveEntityCount() const
    {
        return m_entityCount - m_freeEntities.size();
    }

    template <typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < m_entityVersions.size() && GetEntityVersion(entity) == m_entityVersions[index] && "Entity is invalid!");

        SparseSet<T>* pool = GetOrCreatePool<T>();
        if (!pool->Contains(entity))
        {
            pool->Insert(entity, T(std::forward<Args>(args)...));
        }
        return pool->Get(entity);
    }

    template <typename T>
    void RemoveComponent(Entity entity)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < m_entityVersions.size() && GetEntityVersion(entity) == m_entityVersions[index] && "Entity is invalid!");

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

        auto& entities = primaryPool->GetEntities();
        for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i)
        {
            if (i >= static_cast<int>(entities.size())) 
            {
                i = static_cast<int>(entities.size()) - 1;
                if (i < 0) break;
            }

            Entity entity = entities[i];
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
    size_t m_entityCount = 0;
    std::vector<uint32_t> m_entityVersions;
    std::queue<uint32_t> m_freeEntities;
};