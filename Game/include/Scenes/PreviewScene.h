#pragma once
#include "Scenes/GameScene.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Core/ProfileManager.h"
#include "Data/Color.h"
#include <vector>
#include <functional>

enum class PreviewMode
{
    PowerUp,
    Ultimate
};

class PreviewScene : public GameScene
{
public:
    PreviewScene();
    virtual ~PreviewScene() = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;
    void StartCountdown() override;
    [[nodiscard]] bool PreservesMusicOnDestroy() const override { return true; }

    void NextPowerUp();
    void PrevPowerUp();
    void NextItem();
    void PrevItem();
    void LevelUp();
    void LevelDown();
    void ResetPreview();
    void TogglePreviewMode();

    static void SetInitialPowerUp(PowerUpType type);
    static void SetInitialUltimate(UltimateAbilityType type, int level = 1);

    void SetEmbeddedMode(bool embedded);
    [[nodiscard]] bool IsEmbeddedMode() const { return m_isEmbeddedMode; }
    void SetAudioEnabled(bool enabled) override;
    void SetMusicEnabled(bool enabled) override;
    void RestorePostUltimateMusic() override;
    void SetOnResetCallback(std::function<void()> cb) { m_onResetCallback = std::move(cb); }
    [[nodiscard]] bool IsAudioEnabled() const { return m_audioEnabled; }
    void SetPreviewPowerUp(PowerUpType type, int level);
    void SetPreviewUltimate(UltimateAbilityType type, int level);

    [[nodiscard]] bool HasNextLevel() const override { return false; }
    void OnVictoryEnter() override {}
    void OnVictoryUpdate() override {}
    void OnVictoryExit() override {}
    void TryTriggerUltimate() override;

    [[nodiscard]] PreviewMode GetPreviewMode() const { return m_previewMode; }
    [[nodiscard]] UltimateAbilityType GetCurrentUltimate() const { return m_currentUltimate; }
    [[nodiscard]] int GetPreviewLevel() const { return m_previewLevel; }

    [[nodiscard]] bool IsEffectActive();
    [[nodiscard]] bool IsPowerUpEffectActive();
    [[nodiscard]] bool IsUltimateEffectActive();
    [[nodiscard]] bool IsAnyPowerUpEffectActive();

protected:
    void SpawnPowerUp(const Vector2f& position) override;
    void ApplyPowerUp(PowerUpType type) override;

private:
    void SpawnPreviewCapsule();
    void SpawnMagnetDemoCapsule();
    void OnBallLaunched();
    void UpdateAutopilot(float dt);

    PreviewMode m_previewMode = PreviewMode::PowerUp;
    UltimateAbilityType m_currentUltimate = UltimateAbilityType::PlasmaOrb;

    bool   m_isEmbeddedMode = false;
    float  m_autoLaunchTimer = 0.0f;
    int    m_previewLevel = 1;
    size_t m_currentItemIndex = 0;
    Entity m_previewCapsule = NULL_ENTITY;
    bool   m_capsuleSpawnedForLaunch = false;
    bool   m_wasBallAttached = true;
    float  m_magnetCapsuleTimer = 0.0f;
    bool   m_powerUpActivated = false;
    bool   m_ultimateActivated = false;
    float  m_autoResetTimer = 0.0f;
    float  m_extraLifeTimer = 0.0f;
    float  m_powerUpRainPreviewTimer = 0.0f;
    float  m_resetTransitionTimer = 0.0f;
    std::function<void()> m_onResetCallback;
};
