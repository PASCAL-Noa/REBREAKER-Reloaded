#pragma once
#include "Scenes/GameScene.h"
#include <vector>
#include <string>

enum class PlaygroundLayout : int
{
    TacticalMix = 0,
    MetalChains = 1,
    BombClusters = 2,
    RegeneratingItemLab = 3
};

enum class LightingMode : int
{
    Lit = 0,
    Atmospheric = 1,
    PitchBlack = 2
};

class SandboxScene : public GameScene
{
public:
    SandboxScene();
    ~SandboxScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    // Safety and Loop Overrides
    void OnGameOverEnter() override;
    void OnVictoryEnter() override;
    [[nodiscard]] bool HasNextLevel() const override { return false; }

    // Environment & Lighting Controls
    void SetLightingMode(LightingMode mode);
    void NextLightingMode();
    void PreviousLightingMode();
    void ToggleLightingMode();
    [[nodiscard]] LightingMode GetLightingMode() const noexcept { return m_lightingMode; }

    // Safety Features
    void SetSafetyBarrier(bool enabled);
    void ToggleSafetyBarrier();
    [[nodiscard]] bool IsSafetyBarrierEnabled() const noexcept { return m_safetyBarrierEnabled; }
    void RespawnBall();

    // Playground Layouts
    void LoadPlaygroundLayout(PlaygroundLayout layout);
    void NextPlaygroundLayout();
    void PreviousPlaygroundLayout();
    [[nodiscard]] PlaygroundLayout GetCurrentLayout() const noexcept { return m_currentLayout; }
    [[nodiscard]] const char* GetCurrentLayoutName() const noexcept;
    void ClearAllBricks();
    void RespawnCurrentLayout();

    // Power-Up Spawner & Controls (Carousel Streamer Deck)
    void NextPowerUp();
    void PreviousPowerUp();
    void TriggerSelectedPowerUp();
    void SpawnPowerUpCapsule(PowerUpType type, const Vector2f& position);
    void SetSpawnCapsuleMode(bool spawnCapsule) noexcept { m_spawnCapsuleMode = spawnCapsule; }
    [[nodiscard]] bool IsSpawnCapsuleMode() const noexcept { return m_spawnCapsuleMode; }
    void ToggleSpawnCapsuleMode() noexcept { m_spawnCapsuleMode = !m_spawnCapsuleMode; }
    void ClearAllPowerUpEffects();
    void MaximizeAllPowerUpLevels();

    // Ultimate Abilities Module
    void NextUltimate();
    void PreviousUltimate();
    void FireSelectedUltimate();
    void ClearActiveUltimatesDebug();

    // UI Stream Deck Toggle
    void TogglePalette();
    [[nodiscard]] bool IsPaletteVisible() const noexcept { return m_paletteVisible; }

protected:
    void HandleSandboxInput(const GameContext& context);
    void CreateStreamDeckUI(const GameContext& context);
    void UpdateSandboxUIDisplay();

private:
    PlaygroundLayout m_currentLayout = PlaygroundLayout::TacticalMix;
    LightingMode     m_lightingMode = LightingMode::Lit;
    bool             m_safetyBarrierEnabled = true;
    bool             m_spawnCapsuleMode = false;
    bool             m_paletteVisible = true;

    int              m_selectedPowerUpIdx = 0;
    int              m_selectedUltimateIdx = 0;

    Entity           m_sandboxCanvas = NULL_ENTITY;
    Entity           m_miniToggleCanvas = NULL_ENTITY;

    // Stream Deck Display Buttons / Pads
    Entity           m_btnPowerUpLCD = NULL_ENTITY;
    Entity           m_btnSpawnMode = NULL_ENTITY;
    Entity           m_btnUltimateLCD = NULL_ENTITY;
    Entity           m_btnLayoutLCD = NULL_ENTITY;
    Entity           m_btnLightingLCD = NULL_ENTITY;
    Entity           m_btnBarrierPad = NULL_ENTITY;
};
