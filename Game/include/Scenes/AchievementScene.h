#pragma once

#include "Core/AchievementManager.h"
#include "Core/SceneManager.h"
#include "Data/Color.h"
#include "Math/Vector2.h"
#include "Scenes/DefaultScene.h"
#include <cstdint>
#include <vector>

class AchievementScene : public DefaultScene
{
public:
    AchievementScene() = default;
    ~AchievementScene() override = default;

    static void Launch(SceneManager& scenes,
                       SceneTransitionType type = SceneTransitionType::None,
                       float duration = 0.0f);

    void OnInit(GameContext& context) override;
    void OnDestroy(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    void RefreshFilteredList();
    void ClampScroll(const Vector2f& viewSize);
    void ReturnToProfile(GameContext& context);

    std::vector<AchievementDefinition> m_filteredDefs;
    int   m_activeFilter = 0;
    float m_scrollY = 0.0f;
    float m_animTime = 0.0f;

    uint32_t m_fontId = 0;
    uint32_t m_clickSfxId = 0;
    uint32_t m_switchSfxId = 0;
};
