#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"

class SampleAudio : public DefaultScene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

private:
    void PlaySound(uint32_t soundId);

    uint32_t m_bassem = 0;
    uint32_t m_putine = 0;
    uint32_t m_electric_zoo = 0;
    uint32_t m_faaah = 0;

    Entity m_musicEntity = 0;
    Entity m_sfxEntity = 0;
    
    float m_currentVolume = 100.0f;
    float m_sfxCooldown = 0.0f;
};