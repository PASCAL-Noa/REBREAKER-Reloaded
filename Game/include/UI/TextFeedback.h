#pragma once
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Graphics/Animation.h"
#include "Math/Vector2.h"
#include <vector>
#include <string>

#include "ECS/Components/Camera2D.h"

class TextFeedback
{
public:
    TextFeedback(Registry& registry, GameContext& context, uint32_t fontId, uint32_t fireTexId);
    ~TextFeedback() = default;

    void OnInit(Entity canvasParent);
    void OnUpdate(float dt, uint32_t comboMultiplier, uint32_t score);

    void SpawnComboText(Entity canvasParent, const Vector2f& worldPosition, uint32_t comboMultiplier, const Camera2D& worldCamera);
    void SetFlamePosition(const struct RectTransform& referenceTransform, float xOffset, float yOffset = 0.0f) const;

private:
    Registry& m_registry;
    GameContext& m_context;
    uint32_t m_fontId = 0;
    uint32_t m_fireTexId = 0;

    Entity m_comboFlame{};
    uint32_t m_lastCombo = 0;
};
