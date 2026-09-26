#pragma once
#include "Registry.hpp"

inline void CommandBuffer::DestroyEntity(Entity entity)
{
    Push([entity](Registry& registry) {
        registry.DestroyEntity(entity);
    });
}

inline void CommandBuffer::Execute(Registry& registry)
{
    if (m_commands.empty()) return;

    auto commandsToExecute = std::move(m_commands);
    m_commands.clear();

    for (auto& cmd : commandsToExecute)
    {
        if (cmd)
        {
            cmd(registry);
        }
    }
}

template <typename T, typename... Args>
inline void CommandBuffer::AddComponent(Entity entity, Args&&... args)
{
    auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
    Push([entity, argsTuple = std::move(argsTuple)](Registry& registry) mutable {
        std::apply([&registry, entity](auto&&... unpackedArgs) {
            registry.AddComponent<T>(entity, std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
        }, std::move(argsTuple));
    });
}

template <typename T>
inline void CommandBuffer::RemoveComponent(Entity entity)
{
    Push([entity](Registry& registry) {
        registry.RemoveComponent<T>(entity);
    });
}
