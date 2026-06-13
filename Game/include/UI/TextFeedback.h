#pragma once
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Graphics/Animation.h"
#include "Math/Vector2.h"
#include <vector>
#include <string>

struct FloatingText
{
    Vector2f Position;
    std::string Text;
    float Timer = 1.0f;
    float InitialTimer = 1.0f;
};

class TextFeedback
{
public:
    TextFeedback(Registry& registry, GameContext& context, uint32_t fontId, uint32_t fireTexId);
    ~TextFeedback() = default;

    void OnInit();
    void OnUpdate(float dt, uint32_t comboMultiplier, uint32_t score);
    void OnRender();

    void SpawnComboText(const Vector2f& position, uint32_t comboMultiplier);
    void SetFlamePosition(const Vector2f& position) const;

private:
    Registry& m_registry;
    GameContext& m_context;
    uint32_t m_fontId = 0;
    uint32_t m_fireTexId = 0;

    std::vector<FloatingText> m_floatingTexts;
    Entity m_comboFlame{};

    uint32_t m_lastCombo = 0;
};
