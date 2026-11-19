#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include "ECS/Entity.h"
#include "ECS/Registry.hpp"
#include "Core/GameContext.h"
#include "Core/InputActions.h"

class SettingsUIController
{
public:
    SettingsUIController() = default;
    ~SettingsUIController() = default;

    void Create(Registry& registry,
                GameContext& context,
                uint32_t fontId,
                uint32_t clickSfxId = 0,
                std::function<void()> onClose = nullptr);

    void Open();
    void Close();
    [[nodiscard]] bool IsOpen() const;
    [[nodiscard]] bool IsRebinding() const noexcept { return m_isRebinding; }

    void Update(float dt);
    void SyncAudioVolume();

    void OpenSettingsTab(Entity targetCanvas);
    [[nodiscard]] Entity GetRootCanvas() const noexcept { return m_settingsCanvas; }

private:
    void CreateSettingsLayout();
    void CreateAudioTab();
    void CreateRenderTab();
    void CreateInputsTab();
    void CreateGamerulesTab();
    void CreateCheatsTab();
    void UpdateInputButtons();
    void UpdateVolumeBars(const std::vector<Entity>& bars, float volume);

    Registry*           m_registry = nullptr;
    GameContext*        m_context = nullptr;
    uint32_t            m_fontId = 0;
    uint32_t            m_clickSfxId = 0;
    std::function<void()> m_onClose = nullptr;

    Entity              m_settingsCanvas = NULL_ENTITY;
    Entity              m_audioCanvas = NULL_ENTITY;
    Entity              m_renderCanvas = NULL_ENTITY;
    Entity              m_inputsCanvas = NULL_ENTITY;
    Entity              m_gamerulesCanvas = NULL_ENTITY;
    Entity              m_cheatsCanvas = NULL_ENTITY;
    Entity              m_activeTabCanvas = NULL_ENTITY;
    Entity              m_cheatsTabBtn = NULL_ENTITY;

    // Audio Tab Widgets
    Entity              m_audioDeviceBtn = NULL_ENTITY;
    std::vector<Entity> m_masterVolumeBars;
    std::vector<Entity> m_sfxVolumeBars;
    std::vector<Entity> m_musicVolumeBars;

    // Render Tab Widgets
    Entity              m_fsBtn = NULL_ENTITY;
    Entity              m_shaderBtn = NULL_ENTITY;
    Entity              m_particlesBtn = NULL_ENTITY;

    // Inputs Tab Widgets
    Entity              m_inputDeviceBtn = NULL_ENTITY;
    Entity              m_leftRebindBtn = NULL_ENTITY;
    Entity              m_rightRebindBtn = NULL_ENTITY;
    Entity              m_launchRebindBtn = NULL_ENTITY;
    Entity              m_pauseRebindBtn = NULL_ENTITY;
    Entity              m_resetDefaultsBtn = NULL_ENTITY;
    Entity              m_rebindStatusText = NULL_ENTITY;
    InputAction         m_rebindAction = InputAction::Count;
    bool                m_isRebinding = false;
    bool                m_rebindGamepadMode = false;
    float               m_rebindTimer = 0.0f;

    void RefreshGamerulesUI();

    // Gamerules Tab Widgets
    Entity              m_gamerulesModeBtn = NULL_ENTITY;
    Entity              m_arenaEventsBtn = NULL_ENTITY;
    Entity              m_hazardZonesBtn = NULL_ENTITY;
    Entity              m_ultimateDrainBtn = NULL_ENTITY;
    Entity              m_screenGlitchBtn = NULL_ENTITY;
    Entity              m_cameraTiltBtn = NULL_ENTITY;
    Entity              m_zoomPulseBtn = NULL_ENTITY;
    Entity              m_audioWarpBtn = NULL_ENTITY;
    Entity              m_empLockdownBtn = NULL_ENTITY;
    Entity              m_creepingCeilingBtn = NULL_ENTITY;
    Entity              m_infiniteLivesBtn = NULL_ENTITY;
    Entity              m_infiniteUltimateBtn = NULL_ENTITY;

    bool                m_isClosing = false;
};
