#pragma once
#include "ECS/System.h"
#include "Core/GameContext.h"
#include <cstdint>

class GameFeelSystem : public System
{
public:
    explicit GameFeelSystem(Registry& registry, GameContext& context);
    void OnUpdate(float dt) override;

private:
    GameContext&    m_context;

    uint32_t    m_sfxBrickHit = 0;
    uint32_t    m_sfxBrickDestroy = 0;
    uint32_t    m_sfxPaddleHit = 0;
    uint32_t    m_sfxBallDeath = 0;
    uint32_t    m_sfxPowerUp = 0;
    uint32_t    m_sfxCombo = 0;
};