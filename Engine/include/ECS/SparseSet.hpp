#pragma once
#include <vector>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <utility>
#include "Entity.h"
#include "ISparseSet.h"

template <typename T>
class SparseSet : public ISparseSet
{
public:
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t PAGE_SHIFT = 12; // 2^12 = 4096
    static constexpr size_t PAGE_MASK = PAGE_SIZE - 1; // 0xFFF

    static_assert((1 << PAGE_SHIFT) == PAGE_SIZE, "PAGE_SIZE must equal 1 << PAGE_SHIFT");

    SparseSet() = default;

    ~SparseSet() override
    {
        ClearPages();
    }

    SparseSet(const SparseSet& other)
        : m_dense(other.m_dense),
          m_entities(other.m_entities)
    {
        m_pages.resize(other.m_pages.size(), nullptr);
        for (size_t i = 0; i < other.m_pages.size(); ++i)
        {
            if (other.m_pages[i])
            {
                m_pages[i] = new size_t[PAGE_SIZE];
                std::memcpy(m_pages[i], other.m_pages[i], PAGE_SIZE * sizeof(size_t));
            }
        }
    }

    SparseSet& operator=(const SparseSet& other)
    {
        if (this != &other)
        {
            ClearPages();
            m_dense = other.m_dense;
            m_entities = other.m_entities;
            m_pages.resize(other.m_pages.size(), nullptr);
            for (size_t i = 0; i < other.m_pages.size(); ++i)
            {
                if (other.m_pages[i])
                {
                    m_pages[i] = new size_t[PAGE_SIZE];
                    std::memcpy(m_pages[i], other.m_pages[i], PAGE_SIZE * sizeof(size_t));
                }
            }
        }
        return *this;
    }

    SparseSet(SparseSet&& other) noexcept
        : m_dense(std::move(other.m_dense)),
          m_entities(std::move(other.m_entities)),
          m_pages(std::move(other.m_pages))
    {
    }

    SparseSet& operator=(SparseSet&& other) noexcept
    {
        if (this != &other)
        {
            ClearPages();
            m_dense = std::move(other.m_dense);
            m_entities = std::move(other.m_entities);
            m_pages = std::move(other.m_pages);
        }
        return *this;
    }

    void Insert(Entity entity, const T& component)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < MAX_ENTITIES);
        if (Contains(entity)) return;

        size_t* pagePtr = AssurePage(index >> PAGE_SHIFT);
        size_t offset = index & PAGE_MASK;

        pagePtr[offset] = m_dense.size();
        m_dense.push_back(component);
        m_entities.push_back(entity);
    }

    void Insert(Entity entity, T&& component)
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < MAX_ENTITIES);
        if (Contains(entity)) return;

        size_t* pagePtr = AssurePage(index >> PAGE_SHIFT);
        size_t offset = index & PAGE_MASK;

        pagePtr[offset] = m_dense.size();
        m_dense.push_back(std::move(component));
        m_entities.push_back(entity);
    }

    void Remove(Entity entity) override
    {
        uint32_t index = GetEntityIndex(entity);
        assert(index < MAX_ENTITIES);
        if (!Contains(entity)) return;

        size_t page = index >> PAGE_SHIFT;
        size_t offset = index & PAGE_MASK;

        size_t indexOfRemoved = m_pages[page][offset];
        size_t indexOfLast = m_dense.size() - 1;
        Entity entityOfLast = m_entities[indexOfLast];
        uint32_t indexOfLastEntity = GetEntityIndex(entityOfLast);

        size_t lastPage = indexOfLastEntity >> PAGE_SHIFT;
        size_t lastOffset = indexOfLastEntity & PAGE_MASK;

        m_dense[indexOfRemoved] = std::move(m_dense[indexOfLast]);
        m_entities[indexOfRemoved] = entityOfLast;
        m_pages[lastPage][lastOffset] = indexOfRemoved;
        m_pages[page][offset] = npos;

        m_dense.pop_back();
        m_entities.pop_back();
    }

    T& Get(Entity entity)
    {
        assert(Contains(entity));
        uint32_t index = GetEntityIndex(entity);
        size_t page = index >> PAGE_SHIFT;
        size_t offset = index & PAGE_MASK;
        return m_dense[m_pages[page][offset]];
    }

    const T& Get(Entity entity) const
    {
        assert(Contains(entity));
        uint32_t index = GetEntityIndex(entity);
        size_t page = index >> PAGE_SHIFT;
        size_t offset = index & PAGE_MASK;
        return m_dense[m_pages[page][offset]];
    }

    T* TryGet(Entity entity)
    {
        if (!Contains(entity)) return nullptr;
        uint32_t index = GetEntityIndex(entity);
        size_t page = index >> PAGE_SHIFT;
        size_t offset = index & PAGE_MASK;
        return &m_dense[m_pages[page][offset]];
    }

    const T* TryGet(Entity entity) const
    {
        if (!Contains(entity)) return nullptr;
        uint32_t index = GetEntityIndex(entity);
        size_t page = index >> PAGE_SHIFT;
        size_t offset = index & PAGE_MASK;
        return &m_dense[m_pages[page][offset]];
    }

    bool Contains(Entity entity) const
    {
        uint32_t index = GetEntityIndex(entity);
        if (index >= MAX_ENTITIES) return false;

        size_t page = index >> PAGE_SHIFT;
        if (page >= m_pages.size() || m_pages[page] == nullptr) return false;

        size_t offset = index & PAGE_MASK;
        size_t denseIndex = m_pages[page][offset];
        if (denseIndex == npos || denseIndex >= m_entities.size()) return false;

        return m_entities[denseIndex] == entity; // Check full entity including version
    }

    std::vector<T>& GetDense()
    {
        return m_dense;
    }

    const std::vector<T>& GetDense() const
    {
        return m_dense;
    }

    size_t Size() const override
    {
        return m_entities.size();
    }

    const std::vector<Entity>& GetEntities() const override
    {
        return m_entities;
    }

    void Clear()
    {
        m_dense.clear();
        m_entities.clear();
        for (size_t* page : m_pages)
        {
            if (page)
            {
                std::fill_n(page, PAGE_SIZE, npos);
            }
        }
    }

    size_t GetAllocatedPageCount() const
    {
        size_t count = 0;
        for (const size_t* page : m_pages)
        {
            if (page) ++count;
        }
        return count;
    }

    size_t GetMemoryUsage() const
    {
        return sizeof(*this)
             + m_dense.capacity() * sizeof(T)
             + m_entities.capacity() * sizeof(Entity)
             + m_pages.capacity() * sizeof(size_t*)
             + GetAllocatedPageCount() * PAGE_SIZE * sizeof(size_t);
    }

private:
    static constexpr size_t npos = static_cast<size_t>(-1);

    size_t* AssurePage(size_t page)
    {
        if (page >= m_pages.size())
        {
            m_pages.resize(page + 1, nullptr);
        }
        if (m_pages[page] == nullptr)
        {
            m_pages[page] = new size_t[PAGE_SIZE];
            std::fill_n(m_pages[page], PAGE_SIZE, npos);
        }
        return m_pages[page];
    }

    void ClearPages()
    {
        for (size_t* page : m_pages)
        {
            delete[] page;
        }
        m_pages.clear();
    }

    std::vector<T> m_dense;
    std::vector<Entity> m_entities;
    std::vector<size_t*> m_pages;
};