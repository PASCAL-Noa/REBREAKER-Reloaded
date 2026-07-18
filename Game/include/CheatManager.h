#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "Core/InputManager.h"
#include "Core/GameContext.h"
#include "Events/EventDispatcher.h"
#include "Graphics/Renderer.h"

class CheatManager
{
public:
    CheatManager(const GameContext& context);
    ~CheatManager();
    
    void Update(float dt, const GameContext& context);
    void Render(Renderer& renderer) const;

    bool IsCheatTabUnlocked() const { return m_cheatTabUnlocked; }
    bool TryCheat(const std::string& code, const GameContext& context);

private:
    std::vector<KeyCode> m_konamiSequence;
    int m_konamiIndex = 0;
    
    float m_cheatTimer = 0.0f;
    uint32_t m_sfxId = 0;
    uint32_t m_errorSfxId = 0;
    uint32_t m_fontId = 0;
    uint32_t m_scannerShaderId = 0;

    bool m_cheatTabUnlocked = false;
    bool m_cheatWasActivated = false;
    std::string m_activeCheatName = "";
    
    EventDispatcher::SubscriptionID m_cheatSubId = 0;
    const GameContext* mp_context = nullptr;
};
