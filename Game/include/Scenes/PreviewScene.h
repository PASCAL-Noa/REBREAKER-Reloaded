#pragma once
#include "Scenes/GameScene.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Data/Color.h"
#include <vector>

class PreviewScene : public GameScene
{
public:
    PreviewScene();
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
    void SetEmbeddedMode(bool embedded);
    [[nodiscard]] bool IsEmbeddedMode() const { return m_isEmbeddedMode; }
    void SetPreviewPowerUp(PowerUpType type, int level);

protected:
    void SpawnPowerUp(const Vector2f& position) override;
    void ApplyPowerUp(PowerUpType type) override;

private:
    void SpawnPreviewCapsule();
    void SpawnMagnetDemoCapsule();
    void OnBallLaunched();
    void UpdateAutopilot(float dt);
    [[nodiscard]] bool IsPowerUpEffectActive();

    bool   m_isEmbeddedMode = false;
    float  m_autoLaunchTimer = 0.0f;
    int    m_previewLevel = 1;
    size_t m_currentItemIndex = 0;
    Entity m_previewCapsule = NULL_ENTITY;
    bool   m_capsuleSpawnedForLaunch = false;
    bool   m_wasBallAttached = true;
    float  m_magnetCapsuleTimer = 0.0f;
    bool   m_powerUpActivated = false;
    float  m_autoResetTimer = 0.0f;
    float  m_extraLifeTimer = 0.0f;
};
