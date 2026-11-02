#pragma once
#include "ECS/System.h"
#include "Core/GameContext.h"
#include "Events/EventDispatcher.h"
#include <cstdint>
#include <chrono>
#include <algorithm>

#include "ECS/Components/Transform2D.h"

class GameFeelSystem : public System
{
public:
    explicit GameFeelSystem(Registry& registry, GameContext& context);
    ~GameFeelSystem() override = default;
    void OnUpdate(float dt) override;

    void SetAudioEnabled(bool enabled) { m_audioEnabled = enabled; }
    [[nodiscard]] bool IsAudioEnabled() const { return m_audioEnabled; }
    void SetVolumeMultiplier(float mult) { m_volumeMultiplier = std::clamp(mult, 0.0f, 1.0f); }
    [[nodiscard]] float GetVolumeMultiplier() const { return m_volumeMultiplier; }

private:
    GameContext&    m_context;
    bool            m_audioEnabled = true;
    float           m_volumeMultiplier = 1.0f;
    std::chrono::steady_clock::time_point m_lastPowerUpSfxTime{};

    ScopedSubscription m_brickHitSub;
    ScopedSubscription m_paddleHitSub;
    ScopedSubscription m_ballDeathSub;
    ScopedSubscription m_powerUpSub;

    uint32_t    m_sfxBrickHit = 0;
    uint32_t    m_sfxBrickDestroy = 0;
    uint32_t    m_sfxPaddleHit = 0;
    uint32_t    m_sfxBallDeath = 0;
    uint32_t    m_sfxPowerUp = 0;
    uint32_t    m_sfxPowerUpStorm = 0;
    uint32_t    m_sfxPowerUpLaser = 0;
    uint32_t    m_sfxPowerUpFireball = 0;
    uint32_t    m_sfxPowerUpBuff = 0;
    uint32_t    m_sfxPowerUpShrink = 0;
    uint32_t    m_sfxPowerUpLife = 0;
    uint32_t    m_sfxPowerUpTempo = 0;
    uint32_t    m_sfxCombo = 0;
};

