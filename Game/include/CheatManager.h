#pragma once

#include <vector>
#include <cstdint>
#include "Core/InputManager.h"
#include "Core/GameContext.h"
#include "Graphics/Renderer.h"

class CheatManager
{
public:
    CheatManager(const GameContext& context);
    
    void Update(float dt, const GameContext& context);
    void Render(Renderer& renderer) const;

private:
    std::vector<KeyCode> m_konamiSequence;
    int m_konamiIndex = 0;
    
    float m_cheatTimer = 0.0f;
    uint32_t m_sfxId = 0;
    uint32_t m_fontId = 0;
};
