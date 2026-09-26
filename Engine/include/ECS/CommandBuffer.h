#pragma once

#include <vector>
#include <functional>
#include <tuple>
#include <utility>
#include "Entity.h"

class Registry;

class CommandBuffer
{
public:
    CommandBuffer() = default;
    ~CommandBuffer() = default;

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) noexcept = default;
    CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

    void Push(std::function<void(Registry&)> command)
    {
        m_commands.push_back(std::move(command));
    }

    void DestroyEntity(Entity entity);

    template <typename T, typename... Args>
    void AddComponent(Entity entity, Args&&... args);

    template <typename T>
    void RemoveComponent(Entity entity);

    void Execute(Registry& registry);

    void Clear()
    {
        m_commands.clear();
    }

    [[nodiscard]] bool IsEmpty() const
    {
        return m_commands.empty();
    }

    [[nodiscard]] size_t Size() const
    {
        return m_commands.size();
    }

private:
    std::vector<std::function<void(Registry&)>> m_commands;
};
