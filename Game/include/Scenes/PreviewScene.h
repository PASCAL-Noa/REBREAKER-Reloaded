#pragma once
#include "Scenes/GameScene.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Data/Color.h"
#include <vector>

class PreviewScene : public GameScene
{
public:
    virtual ~PreviewScene() = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;

    void NextPowerUp();
    void PrevPowerUp();
    void LevelUp();
    void LevelDown();
    void ResetPreview();

    static void SetInitialPowerUp(PowerUpType type);

protected:
    void SpawnPowerUp(const Vector2f& position) override;
    void ApplyPowerUp(PowerUpType type) override;

private:
    void SpawnPreviewCapsule();
    void SpawnMagnetDemoCapsule();
    void OnBallLaunched();

    size_t m_currentItemIndex = 0;
    Entity m_previewCapsule = NULL_ENTITY;
    bool   m_capsuleSpawnedForLaunch = false;
    bool   m_wasBallAttached = true;
    float  m_magnetCapsuleTimer = 0.0f;
};
