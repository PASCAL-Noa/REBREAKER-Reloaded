#pragma once
#include "Entity.h"
#include "ISparseSet.h"
#include "SparseSet.hpp"

#include "CommandBuffer.h"
#include <vector>
#include <queue>
#include <tuple>

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

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) noexcept = default;
    Registry& operator=(Registry&&) noexcept = default;

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

        if (index < m_pendingDestroy.size())
        {
            m_pendingDestroy[index] = false;
        }

        for (ISparseSet* pool : m_pools)
        {
            if (pool) pool->Remove(entity);
        }
        
        m_entityVersions[index] = (m_entityVersions[index] >= MAX_ENTITY_VERSION) ? 1 : (m_entityVersions[index] + 1);
        m_freeEntities.push(index);
    }

    void DestroyEntityDeferred(Entity entity)
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= m_entityVersions.size() || GetEntityVersion(entity) != m_entityVersions[index]) return;

        if (index >= m_pendingDestroy.size())
        {
            m_pendingDestroy.resize(index + 1, false);
        }

        if (m_pendingDestroy[index]) return;

        m_pendingDestroy[index] = true;
        m_deferredDestroyList.push_back(entity);
        m_commandBuffer.DestroyEntity(entity);
    }

    [[nodiscard]] bool IsPendingDestroy(Entity entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= m_pendingDestroy.size()) return false;
        return m_pendingDestroy[index];
    }

    [[nodiscard]] bool IsAlive(Entity entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        return index < m_entityVersions.size() && GetEntityVersion(entity) == m_entityVersions[index] && !IsPendingDestroy(entity);
    }

    template <typename T, typename... Args>
    void AddComponentDeferred(Entity entity, Args&&... args)
    {
        m_commandBuffer.AddComponent<T>(entity, std::forward<Args>(args)...);
    }

    template <typename T>
    void RemoveComponentDeferred(Entity entity)
    {
        m_commandBuffer.RemoveComponent<T>(entity);
    }

    CommandBuffer& GetCommandBuffer()
    {
        return m_commandBuffer;
    }

    void ProcessDeferredCommands()
    {
        for (Entity e : m_deferredDestroyList)
        {
            uint32_t index = GetEntityIndex(e);
            if (index < m_pendingDestroy.size())
            {
                m_pendingDestroy[index] = false;
            }
        }
        m_deferredDestroyList.clear();

        m_commandBuffer.Execute(*this);
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
        SparseSet<T1>* pool1 = GetPool<T1>();
        if (!pool1) return;

        ISparseSet* minPool = pool1;

        if constexpr (sizeof...(Tn) == 0)
        {
            const auto& entities = minPool->GetEntities();
            for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i)
            {
                if (i >= static_cast<int>(entities.size())) 
                {
                    i = static_cast<int>(entities.size()) - 1;
                    if (i < 0) break;
                }

                Entity entity = entities[i];
                if (IsPendingDestroy(entity)) continue;

                if (pool1->Contains(entity))
                {
                    func(entity, pool1->Get(entity));
                }
            }
        }
        else
        {
            std::tuple<SparseSet<Tn>*...> otherPools = { GetPool<Tn>()... };
            bool anyNull = false;
            std::apply([&anyNull, &minPool](auto*... pools) {
                auto check = [&anyNull, &minPool](auto* pool) {
                    if (!pool) { anyNull = true; return; }
                    if (pool->Size() < minPool->Size()) {
                        minPool = pool;
                    }
                };
                (check(pools), ...);
            }, otherPools);

            if (anyNull) return;

            const auto& entities = minPool->GetEntities();
            for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i)
            {
                if (i >= static_cast<int>(entities.size())) 
                {
                    i = static_cast<int>(entities.size()) - 1;
                    if (i < 0) break;
                }

                Entity entity = entities[i];
                if (IsPendingDestroy(entity)) continue;

                bool hasAll = pool1->Contains(entity);
                if (hasAll)
                {
                    std::apply([entity, &hasAll](auto*... pools) {
                        hasAll = (hasAll && (pools->Contains(entity) && ...));
                    }, otherPools);
                }

                if (hasAll)
                {
                    std::apply([entity, &func, pool1](auto*... pools) {
                        func(entity, pool1->Get(entity), pools->Get(entity)...);
                    }, otherPools);
                }
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

    CommandBuffer m_commandBuffer;
    std::vector<bool> m_pendingDestroy;
    std::vector<Entity> m_deferredDestroyList;
};

#include "CommandBuffer.inl"