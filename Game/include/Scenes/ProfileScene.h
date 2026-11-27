#pragma once

#include "Scenes/DefaultScene.h"
#include "Core/SceneTransition.h"
#include "UI/ProfileGarageRenderer.h"
#include "ECS/Components/TrailComponent.h"
#include "Graphics/CosmeticRenderer.h"
#include "Data/Vertex.h"
#include "Math/Vector2.h"
#include <memory>
#include <vector>

class ProfileGarageState;
class SceneManager;

class ProfileScene : public DefaultScene
{
public:
    ProfileScene();
    ~ProfileScene() override;

    static void Launch(SceneManager& scenes, SceneTransitionType type = SceneTransitionType::StaticSnow, float duration = 0.55f);

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

private:
    void UpdateShowcase(float dt, const Vector2f& viewSize);
    void SpawnShowcaseBurst(const Vector2f& pos, float scale);
    void ReturnToMenu(GameContext& context);
    void ClampCareerScroll(const Vector2f& viewSize);

    std::unique_ptr<ProfileGarageState> m_state;
    ProfileRenderAssets m_assets;

    uint32_t m_clickSfxId = 0;
    uint32_t m_switchSfxId = 0;
    uint32_t m_explosionSfxId = 0;

    float m_animTime = 0.0f;
    Vector2f m_ballPos{0.0f, 0.0f};
    TrailComponent m_ballTrail{};
    TrailStyle m_cachedTrailStyle = TrailStyle::ClassicWhite;
    Color m_cachedTrailPri{0, 230, 255, 255};
    Color m_cachedTrailSec{255, 255, 255, 255};
    std::vector<Vertex> m_trailScratch;
    CosmeticExplosionPool m_explosions;
    float m_autoBurstTimer = 0.8f;
};
