#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Scenes/DefaultScene.h"
#include "ECS/Entity.h"
#include "LevelManager.h"
#include "LevelThumbnail.h"

class LevelSelectScene : public DefaultScene
{
public:
    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;
    [[nodiscard]] uint32_t GetPostProcessShader() const override;

protected:
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    void InitRetroBackground(const GameContext& context);
    void UpdateRetroBackground(float dt, const GameContext& context);
    void RenderRetroBackground(const GameContext& context) const;

    void LoadLevelThumbnails(GameContext& context);
    void RebuildUI(GameContext& context);
    void ClearUI();
    void LaunchLevel(GameContext& context, int levelIndex);
    void RenderThumbnailsAndEffects(const GameContext& context) const;

    Entity m_canvas = NULL_ENTITY;
    LevelManager m_levelManager;

    uint32_t m_clickSfxId = 0;
    uint32_t m_shaderId = 0;

    int m_currentPage = 0;
    int m_hoveredCardIndex = -1;
    int m_selectedLevelIndex = 0;

    std::vector<LevelThumbnailData> m_levelThumbnails;

    struct StarParticle
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Speed = 20.0f;
        float Size = 2.0f;
        float Alpha = 180.0f;
        float TwinkleFreq = 2.0f;
    };

    std::vector<StarParticle> m_stars;
    float m_animTime = 0.0f;
    float m_gridAnimOffset = 0.0f;
};
