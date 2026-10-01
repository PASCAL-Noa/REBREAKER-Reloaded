#pragma once
#include <cstdint>

using Entity = uint32_t;
constexpr Entity MAX_ENTITIES = 1000000;
constexpr Entity NULL_ENTITY = 0xFFFFFFFF;

constexpr uint32_t ENTITY_INDEX_MASK = 0xFFFFF;
constexpr uint32_t ENTITY_VERSION_MASK = 0xFFF00000;
constexpr uint32_t ENTITY_VERSION_SHIFT = 20;
constexpr uint32_t MAX_ENTITY_VERSION = 0xFFF;

inline uint32_t GetEntityIndex(Entity e) { return e & ENTITY_INDEX_MASK; }
inline uint32_t GetEntityVersion(Entity e) { return (e & ENTITY_VERSION_MASK) >> ENTITY_VERSION_SHIFT; }
inline Entity MakeEntity(uint32_t index, uint32_t version) { return (index & ENTITY_INDEX_MASK) | (((version & MAX_ENTITY_VERSION) << ENTITY_VERSION_SHIFT) & ENTITY_VERSION_MASK); }