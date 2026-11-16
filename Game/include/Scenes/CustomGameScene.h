#pragma once

#include "Scenes/DefaultScene.h"
#include "Data/Color.h"
#include <string>
#include <vector>

enum class CustomDifficultyPreset : int
{
    Easy = 0,
    Normal = 1,
    Hard = 2,
    Nightmare = 3,
    Custom = 4,
    Count = 5
};

enum class CustomTargetGameMode : int
{
    Endless = 0,
    Campaign = 1,
    Tetris = 2
};

struct CustomRuleOption
{
    std::string Name;
    std::string Description;
    bool*       pFlag = nullptr;
    Color       ColorOn{60, 240, 140, 255};
    Color       ColorOff{180, 50, 60, 255};
};

class CustomGameScene : public DefaultScene
{
public:
    CustomGameScene();
    ~CustomGameScene() override = default;

    void OnInit(GameContext& context) override;
    void OnUpdate(float dt, GameContext& context) override;
    void OnRender(GameContext& context) override;
    void OnDestroy(GameContext& context) override;

    void ApplyDifficultyPreset(CustomDifficultyPreset preset, GameContext& context);
    void LaunchCustomMatch(GameContext& context);

private:
    void InitOptions();
    void HandleInput(GameContext& context);
    void RenderUI(GameContext& context) const;

    CustomDifficultyPreset m_difficultyPreset = CustomDifficultyPreset::Normal;
    CustomTargetGameMode   m_gameMode = CustomTargetGameMode::Endless;

    // Custom Rules Toggles
    bool m_hazardZones = true;
    bool m_ultimateDrain = true;
    bool m_screenGlitch = true;
    bool m_cameraTilt = true;
    bool m_zoomPulse = true;
    bool m_audioWarp = true;
    bool m_arenaEvents = true;
    bool m_ultimateLockdown = true;
    bool m_infiniteLives = false;
    bool m_infiniteUltimate = false;
    bool m_creepingCeiling = true;

    // Navigation state
    int   m_selectedRow = 0; // 0: GameMode, 1: Preset, 2..N: Rules, N+1: Launch, N+2: Return
    float m_animTimer = 0.0f;
    uint32_t m_fontId = 0;
    uint32_t m_clickSfxId = 0;
    uint32_t m_hoverSfxId = 0;

    std::vector<CustomRuleOption> m_options;
};

