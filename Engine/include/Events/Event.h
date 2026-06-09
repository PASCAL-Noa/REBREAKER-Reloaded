#pragma once
#include <cstdint>

struct Event
{
protected:
    ~Event() = default;
};

inline uint32_t GetNextEventId()
{
    static uint32_t id = 0;
    return id++;
}

template <typename T>
inline uint32_t GetEventId()
{
    static uint32_t id = GetNextEventId();
    return id;
}

