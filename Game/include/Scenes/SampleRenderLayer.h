#pragma once
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include <array>

class SampleRenderLayer : public DefaultScene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

private:
    uint32_t m_texId = 0;

    // Static test: Inverted creation order (1st created has higher layer)
    Entity m_invertedFirst{};
    Entity m_invertedSecond{};

    // Static test: Normal creation order (2nd created has higher layer)
    Entity m_normalFirst{};
    Entity m_normalSecond{};

    // Interactive stack
    std::array<Entity, 3> m_interactiveEntities{};
    size_t m_selectedSpriteIndex = 0;
};
