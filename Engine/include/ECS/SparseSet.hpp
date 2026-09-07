#pragma once
#include <vector>
#include <cassert>
#include "Entity.h"
#include "ISparseSet.h"

template <typename T>
class SparseSet : public ISparseSet
{
public:
    SparseSet()
    {
        m_sparse.resize(MAX_ENTITIES, npos);
    }

    void Insert(Entity entity, const T& component)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < MAX_ENTITIES);
        if (Contains(entity)) return;

        m_sparse[index] = m_dense.size();
        m_dense.push_back(component);
        m_entities.push_back(entity);
    }

    void Remove(Entity entity) override
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < MAX_ENTITIES);
        if (!Contains(entity)) return;

        size_t indexOfRemoved = m_sparse[index];
        size_t indexOfLast = m_dense.size() - 1;
        Entity entityOfLast = m_entities[indexOfLast];
        uint32_t indexOfLastEntity = GetEntityIndex(entityOfLast);

        m_dense[indexOfRemoved] = m_dense[indexOfLast];
        m_entities[indexOfRemoved] = entityOfLast;
        m_sparse[indexOfLastEntity] = indexOfRemoved;
        m_sparse[index] = npos;

        m_dense.pop_back();
        m_entities.pop_back();
    }

    T& Get(Entity entity)
    {
        assert(Contains(entity));
        return m_dense[m_sparse[GetEntityIndex(entity)]];
    }

    bool Contains(Entity entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= MAX_ENTITIES || m_sparse[index] == npos) return false;
        return m_entities[m_sparse[index]] == entity; // Check full entity including version
    }

    std::vector<T>& GetDense()
    {
        return m_dense;
    }

    const std::vector<Entity>& GetEntities() const
    {
        return m_entities;
    }

private:
    static constexpr size_t npos = static_cast<size_t>(-1);

    std::vector<T> m_dense;
    std::vector<Entity> m_entities;
    std::vector<size_t> m_sparse;
};