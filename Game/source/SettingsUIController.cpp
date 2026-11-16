#include "UI/SettingsUIController.h"
#include "UI/UIFactory.h"
#include "Core/PlayerPrefs.h"
#include "Core/InputRemapManager.h"
#include "System/Window.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/RectTransform.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Systems/UISystem.h"
#include "Events/GameplayEvents.h"
#include "Events/EventDispatcher.h"
#include "Events/CheatSubmitEvent.h"
#include "Core/GameRules.h"
#include "Core/InputManager.h"
#include "AudioMixer.h"
#include "Graphics/Renderer.h"
#include <algorithm>
#include <cmath>

void SettingsUIController::Create(Registry& registry,
                                  GameContext& context,
                                  uint32_t fontId,
                                  uint32_t clickSfxId,
                                  std::function<void()> onClose)
{
    m_registry = &registry;
    m_context = &context;
    m_fontId = fontId;
    m_clickSfxId = clickSfxId;
    m_onClose = std::move(onClose);

    CreateSettingsLayout();
    CreateAudioTab();
    CreateRenderTab();
    CreateInputsTab();
    CreateGamerulesTab();
    CreateCheatsTab();

    // Default to Audio tab on initial creation
    OpenSettingsTab(m_audioCanvas);
}

void SettingsUIController::Open()
{
    if (!m_registry || m_settingsCanvas == NULL_ENTITY) return;

    if (m_registry->HasComponent<CanvasComponent>(m_settingsCanvas))
    {
        m_registry->GetComponent<CanvasComponent>(m_settingsCanvas).IsEnabled = true;
    }

    // Refresh Cheats tab visibility dynamically
    if (m_cheatsTabBtn != NULL_ENTITY && m_registry->HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry->GetComponent<RectTransform>(m_cheatsTabBtn).IsActive =
            m_context->Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    // Ensure audio volume bars match current mixer state
    SyncAudioVolume();

    // Refresh active tab or fallback to audio tab
    if (m_activeTabCanvas != NULL_ENTITY)
    {
        OpenSettingsTab(m_activeTabCanvas);
    }
    else
    {
        OpenSettingsTab(m_audioCanvas);
    }
}

void SettingsUIController::Close()
{
    if (!m_registry || m_settingsCanvas == NULL_ENTITY) return;

    if (m_registry->HasComponent<CanvasComponent>(m_settingsCanvas))
    {
        m_registry->GetComponent<CanvasComponent>(m_settingsCanvas).IsEnabled = false;
    }

    // Deactivate all tab canvases
    if (m_activeTabCanvas != NULL_ENTITY && m_registry->HasComponent<CanvasComponent>(m_activeTabCanvas))
    {
        m_registry->GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    // Cancel any active rebinding session
    if (m_isRebinding)
    {
        m_isRebinding = false;
        m_rebindAction = InputAction::Count;
        UISystem::SetNavigationEnabled(true);
        UpdateInputButtons();
    }

    if (m_onClose)
    {
        m_onClose();
    }
}

bool SettingsUIController::IsOpen() const
{
    if (!m_registry || m_settingsCanvas == NULL_ENTITY) return false;
    if (!m_registry->HasComponent<CanvasComponent>(m_settingsCanvas)) return false;
    return m_registry->GetComponent<CanvasComponent>(m_settingsCanvas).IsEnabled;
}

void SettingsUIController::SyncAudioVolume()
{
    if (!m_context) return;
    UpdateVolumeBars(m_masterVolumeBars, m_context->Audio.GetMasterVolume());
    UpdateVolumeBars(m_sfxVolumeBars, m_context->Audio.GetSfxVolume());
    UpdateVolumeBars(m_musicVolumeBars, m_context->Audio.GetMusicVolume());
}

void SettingsUIController::OpenSettingsTab(Entity targetCanvas)
{
    if (!m_registry) return;

    if (m_activeTabCanvas == targetCanvas && targetCanvas != NULL_ENTITY)
    {
        if (m_registry->HasComponent<CanvasComponent>(targetCanvas))
        {
            m_registry->GetComponent<CanvasComponent>(targetCanvas).IsEnabled = true;
        }
        return;
    }

    if (m_activeTabCanvas != NULL_ENTITY && m_registry->HasComponent<CanvasComponent>(m_activeTabCanvas))
    {
        m_registry->GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    m_activeTabCanvas = targetCanvas;

    if (m_activeTabCanvas != NULL_ENTITY && m_registry->HasComponent<CanvasComponent>(m_activeTabCanvas))
    {
        m_registry->GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = true;
    }
}

void SettingsUIController::Update(float dt)
{
    if (!m_registry || !m_context) return;

    if (m_isRebinding && m_rebindAction != InputAction::Count)
    {
        m_rebindTimer -= dt;
        if (m_rebindTimer <= 0.0f)
        {
            // 5-second timeout expired -> cancel rebinding gracefully
            m_isRebinding = false;
            m_rebindAction = InputAction::Count;
            UISystem::SetNavigationEnabled(true);
            UpdateInputButtons();
            return;
        }

        UISystem::SetNavigationEnabled(false);

        if (!m_rebindGamepadMode)
        {
            // Keyboard capture
            KeyCode pressed = m_context->Input.GetLastKeyPressed();
            if (pressed != KeyCode::Count)
            {
                if (pressed == KeyCode::Escape)
                {
                    m_isRebinding = false;
                    m_rebindAction = InputAction::Count;
                    UISystem::SetNavigationEnabled(true);
                    UpdateInputButtons();
                    return;
                }

                InputRemapManager::Get().SetBinding(m_rebindAction, pressed);
                InputRemapManager::Get().SaveToPrefs();

                m_isRebinding = false;
                m_rebindAction = InputAction::Count;
                UISystem::SetNavigationEnabled(true);
                UpdateInputButtons();
            }
        }
        else
        {
            // Gamepad capture (ESC cancels; Back button 6 cancels after debounce)
            if (m_context->Input.IsKeyPress(KeyCode::Escape) || (m_rebindTimer < 4.85f && m_context->Input.IsJoystickButtonPress(0, 6) && m_rebindAction != InputAction::Pause))
            {
                m_isRebinding = false;
                m_rebindAction = InputAction::Count;
                UISystem::SetNavigationEnabled(true);
                UpdateInputButtons();
                return;
            }

            // Debounce 150ms so activation button is not bound immediately
            if (m_rebindTimer < 4.85f)
            {
                int joyInput = m_context->Input.GetAnyGamepadInputTriggered(0);
                if (joyInput >= 0)
                {
                    InputRemapManager::Get().SetGamepadBinding(m_rebindAction, joyInput);
                    InputRemapManager::Get().SaveToPrefs();

                    m_isRebinding = false;
                    m_rebindAction = InputAction::Count;
                    UISystem::SetNavigationEnabled(true);
                    if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
                    UpdateInputButtons();
                    return;
                }
            }
        }

        UpdateInputButtons();
    }
}

void SettingsUIController::UpdateVolumeBars(const std::vector<Entity>& bars, float volume)
{
    if (!m_registry) return;

    for (size_t i = 0; i < bars.size(); ++i)
    {
        if (m_registry->HasComponent<PanelComponent>(bars[i]))
        {
            auto& panel = m_registry->GetComponent<PanelComponent>(bars[i]);
            bool isLit = ((i * 10.0f) < volume);
            if (isLit)
            {
                if (i < 5) panel.Tint = Color{50, 255, 100, 255}; // Glowing neon green
                else if (i < 8) panel.Tint = Color{255, 240, 50, 255}; // Glowing neon yellow
                else if (i == 8) panel.Tint = Color{255, 140, 25, 255}; // Glowing neon orange
                else panel.Tint = Color{255, 45, 55, 255}; // Glowing neon red
            }
            else
            {
                // Dim unlit LED state
                if (i < 5) panel.Tint = Color{20, 45, 30, 140};
                else if (i < 8) panel.Tint = Color{45, 40, 20, 140};
                else if (i == 8) panel.Tint = Color{45, 30, 20, 140};
                else panel.Tint = Color{45, 20, 25, 140};
            }
        }
    }
}

void SettingsUIController::CreateSettingsLayout()
{
    float viewX = m_context->Render.GetLogicalViewSize().X - 300.0f;
    float viewY = m_context->Render.GetLogicalViewSize().Y - 350.0f;

    float leftPanelWidth = viewX * 0.25f;
    float leftPanelX = -viewX * 0.375f + 200.0f;

    m_settingsCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_settingsCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(*m_registry, m_settingsCanvas, TextDescriptor{
        .Text = "SETTINGS",
        .Position = {leftPanelX, -viewY * 0.3f},
        .FontId = m_fontId,
        .FontSize = 50.0f
    });

    // 4 Tab Buttons
    float startY = -viewY * 0.15f;
    float stepY = viewY * 0.08f;

    UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "AUDIO",
        .OnClick = [this]() { OpenSettingsTab(m_audioCanvas); },
        .Position = {leftPanelX, startY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "RENDER",
        .OnClick = [this]() { OpenSettingsTab(m_renderCanvas); },
        .Position = {leftPanelX, startY + stepY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "INPUTS",
        .OnClick = [this]() { OpenSettingsTab(m_inputsCanvas); },
        .Position = {leftPanelX, startY + stepY * 2.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "GAMERULES",
        .OnClick = [this]() { OpenSettingsTab(m_gamerulesCanvas); },
        .Position = {leftPanelX, startY + stepY * 3.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    m_cheatsTabBtn = UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "CHEATS",
        .OnClick = [this]() { OpenSettingsTab(m_cheatsCanvas); },
        .Position = {leftPanelX, startY + stepY * 4.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });
    if (m_registry->HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry->GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = m_context->Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    // Back Button
    UIFactory::CreateButton(*m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            Close();
        },
        .Position = {leftPanelX, viewY * 0.4f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .DefaultColor = Colors::Crimson,
        .HoverColor = Colors::LightCoral,
        .PressedColor = Colors::DarkRed,
        .FontId = m_fontId
    });

    // Dark transparent background for settings modal
    UIFactory::CreatePanel(*m_registry, m_settingsCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX + 300.0f, viewY + 350.0f},
        .Tint = Colors::Transparent,
        .AnchorPoint = Anchor::Center
    });

    // Left Tabs Panel
    UIFactory::CreatePanel(*m_registry, m_settingsCanvas, PanelDescriptor{
        .Position = {leftPanelX, 0.0f},
        .Size = {leftPanelWidth, viewY - 50.0f},
        .Tint = Colors::Transparent
    });
}

void SettingsUIController::CreateAudioTab()
{
    float viewX = m_context->Render.GetLogicalViewSize().X;
    float viewY = m_context->Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_audioCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_audioCanvas, CanvasComponent{.IsEnabled = false});
    m_registry->AddComponent<TweenComponent>(m_audioCanvas, TweenComponent{});

    UIFactory::CreateText(*m_registry, m_audioCanvas, TextDescriptor{
        .Text = "AUDIO SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateVolumeControl(*m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "MASTER",
        .OnMinus = [this]() {
            float vol = std::max(0.0f, m_context->Audio.GetMasterVolume() - 10.0f);
            m_context->Audio.SetMasterVolume(vol);
            PlayerPrefs::SetFloat("MasterVolume", vol);
            PlayerPrefs::Save();
            if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 35.0f);
            UpdateVolumeBars(m_masterVolumeBars, vol);
        },
        .OnPlus = [this]() {
            float vol = std::min(100.0f, m_context->Audio.GetMasterVolume() + 10.0f);
            m_context->Audio.SetMasterVolume(vol);
            PlayerPrefs::SetFloat("MasterVolume", vol);
            PlayerPrefs::Save();
            if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 35.0f);
            UpdateVolumeBars(m_masterVolumeBars, vol);
        },
        .BarsOut = &m_masterVolumeBars,
        .Position = {rightPanelX * 0.5f, -viewY * 0.12f},
        .FontId = m_fontId
    });

    UIFactory::CreateVolumeControl(*m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "SFX",
        .OnMinus = [this]() {
            float vol = std::max(0.0f, m_context->Audio.GetSfxVolume() - 10.0f);
            m_context->Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 35.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .OnPlus = [this]() {
            float vol = std::min(100.0f, m_context->Audio.GetSfxVolume() + 10.0f);
            m_context->Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 35.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .BarsOut = &m_sfxVolumeBars,
        .Position = {rightPanelX * 0.5f, -viewY * 0.01f},
        .FontId = m_fontId
    });

    UIFactory::CreateVolumeControl(*m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "MUSIC",
        .OnMinus = [this]() {
            float vol = std::max(0.0f, m_context->Audio.GetMusicVolume() - 10.0f);
            m_context->Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .OnPlus = [this]() {
            float vol = std::min(100.0f, m_context->Audio.GetMusicVolume() + 10.0f);
            m_context->Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .BarsOut = &m_musicVolumeBars,
        .Position = {rightPanelX * 0.5f, viewY * 0.10f},
        .FontId = m_fontId
    });

    UpdateVolumeBars(m_masterVolumeBars, m_context->Audio.GetMasterVolume());
    UpdateVolumeBars(m_sfxVolumeBars, m_context->Audio.GetSfxVolume());
    UpdateVolumeBars(m_musicVolumeBars, m_context->Audio.GetMusicVolume());

    // Playback Device Selection
    UIFactory::CreateText(*m_registry, m_audioCanvas, TextDescriptor{
        .Text = "DEVICE",
        .Position = {rightPanelX * 0.5f - 110.0f, viewY * 0.21f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::White
    });

    auto formatDeviceText = [](const std::string& rawName) -> std::string {
        if (rawName.empty()) return "< DEFAULT DEVICE >";
        std::string shortName = rawName;
        if (shortName.length() > 24)
        {
            shortName = shortName.substr(0, 22) + "...";
        }
        return "< " + shortName + " >";
    };

    m_audioDeviceBtn = UIFactory::CreateButton(*m_registry, m_audioCanvas, ButtonDescriptor{
        .Text = formatDeviceText(m_context->Audio.GetCurrentAudioDevice()),
        .OnClick = [this, formatDeviceText]() {
            auto devices = m_context->Audio.GetAvailableAudioDevices();
            if (devices.empty()) return;

            std::string current = m_context->Audio.GetCurrentAudioDevice();
            size_t currentIdx = 0;
            for (size_t i = 0; i < devices.size(); ++i)
            {
                if (devices[i] == current)
                {
                    currentIdx = i;
                    break;
                }
            }
            size_t nextIdx = (currentIdx + 1) % devices.size();
            std::string newDevice = devices[nextIdx];
            if (m_context->Audio.SetAudioDevice(newDevice))
            {
                PlayerPrefs::SetString("Audio_Device", newDevice);
                PlayerPrefs::Save();
                if (m_clickSfxId != 0) m_context->Audio.PlaySfx(m_clickSfxId, 35.0f);
            }
            if (m_registry->HasComponent<TextComponent>(m_audioDeviceBtn))
            {
                m_registry->GetComponent<TextComponent>(m_audioDeviceBtn).Text = formatDeviceText(newDevice);
            }
        },
        .Position = {rightPanelX * 0.5f + 250.0f, viewY * 0.21f},
        .Size = {480.0f, 54.0f},
        .DefaultColor = Color{40, 55, 80, 255},
        .HoverColor = Color{60, 85, 120, 255},
        .PressedColor = Color{25, 38, 55, 255},
        .FontId = m_fontId,
        .FontSize = 24.0f
    });

    UIFactory::CreatePanel(*m_registry, m_audioCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void SettingsUIController::CreateRenderTab()
{
    float viewX = m_context->Render.GetLogicalViewSize().X;
    float viewY = m_context->Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_renderCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_renderCanvas, CanvasComponent{.IsEnabled = false});
    m_registry->AddComponent<TweenComponent>(m_renderCanvas, TweenComponent{});

    UIFactory::CreateText(*m_registry, m_renderCanvas, TextDescriptor{
        .Text = "RENDER SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    bool isFullscreen = m_context->Render.GetWindow().GetConfig().Mode == WindowMode::Fullscreen;
    std::string fsText = isFullscreen ? "FULL-SCREEN: ON" : "FULL-SCREEN: OFF";

    m_fsBtn = UIFactory::CreateButton(*m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = fsText,
        .OnClick = [this]() {
            WindowConfig config = m_context->Render.GetWindow().GetConfig();
            if (config.Mode == WindowMode::Fullscreen) {
                config.Mode = WindowMode::Windowed;
                int resW = PlayerPrefs::GetInt("ResolutionWidth", 1920);
                int resH = PlayerPrefs::GetInt("ResolutionHeight", 1080);
                if (resW > 0 && resH > 0) {
                    config.Width = resW;
                    config.Height = resH;
                }
                PlayerPrefs::SetBool("Fullscreen", false);
                if (m_registry->HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry->GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: OFF";
                }
            } else {
                config.Mode = WindowMode::Fullscreen;
                PlayerPrefs::SetBool("Fullscreen", true);
                if (m_registry->HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry->GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: ON";
                }
            }
            PlayerPrefs::Save();
            m_context->Render.GetWindow().ApplyConfig(config);
            m_context->Render.OnWindowResized(config.Width, config.Height);
        },
        .Position = {rightPanelX - 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    std::vector<Resolution> uniqueModes = Window::GetSupportedResolutions();
    std::vector<std::string> options;

    for (const auto& res : uniqueModes) {
        options.push_back(std::to_string(res.Width) + "x" + std::to_string(res.Height));
    }

    std::string currentRes = std::to_string(m_context->Render.GetWindow().GetConfig().Width) + "x" + std::to_string(m_context->Render.GetWindow().GetConfig().Height);

    UIFactory::CreateDropdown(*m_registry, m_renderCanvas, DropdownDescriptor{
        .DefaultText = currentRes,
        .Options = options,
        .OnSelect = [this, uniqueModes](int index, const std::string& /*text*/) {
            if (index >= 0 && static_cast<size_t>(index) < uniqueModes.size()) {
                WindowConfig config = m_context->Render.GetWindow().GetConfig();
                config.Width = uniqueModes[static_cast<size_t>(index)].Width;
                config.Height = uniqueModes[static_cast<size_t>(index)].Height;
                PlayerPrefs::SetInt("ResolutionWidth", static_cast<int>(config.Width));
                PlayerPrefs::SetInt("WindowWidth", static_cast<int>(config.Width));
                PlayerPrefs::SetInt("ResolutionHeight", static_cast<int>(config.Height));
                PlayerPrefs::SetInt("WindowHeight", static_cast<int>(config.Height));
                PlayerPrefs::Save();
                m_context->Render.GetWindow().ApplyConfig(config);
                m_context->Render.OnWindowResized(config.Width, config.Height);
            }
        },
        .Position = {rightPanelX + 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId,
    });

    bool isShader = m_context->Rules.GetRule(Rule::Graphics::EnableShader);
    std::string shaderText = isShader ? "SHADER: ON" : "SHADER: OFF";
    m_shaderBtn = UIFactory::CreateButton(*m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = shaderText,
        .OnClick = [this]() {
            bool state = m_context->Rules.GetRule(Rule::Graphics::EnableShader);
            m_context->Rules.SetRule(Rule::Graphics::EnableShader, !state);
            if (m_registry->HasComponent<TextComponent>(m_shaderBtn)) {
                m_registry->GetComponent<TextComponent>(m_shaderBtn).Text = !state ? "SHADER: ON" : "SHADER: OFF";
            }
        },
        .Position = {rightPanelX - 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    bool isParticles = m_context->Rules.GetRule(Rule::Graphics::EnableParticles);
    std::string particlesText = isParticles ? "PARTICLES: ON" : "PARTICLES: OFF";
    m_particlesBtn = UIFactory::CreateButton(*m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = particlesText,
        .OnClick = [this]() {
            bool state = m_context->Rules.GetRule(Rule::Graphics::EnableParticles);
            m_context->Rules.SetRule(Rule::Graphics::EnableParticles, !state);
            if (m_registry->HasComponent<TextComponent>(m_particlesBtn)) {
                m_registry->GetComponent<TextComponent>(m_particlesBtn).Text = !state ? "PARTICLES: ON" : "PARTICLES: OFF";
            }
        },
        .Position = {rightPanelX + 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(*m_registry, m_renderCanvas, PanelDescriptor{
       .Position = {rightPanelX, 0.0f},
       .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void SettingsUIController::CreateInputsTab()
{
    float viewX = m_context->Render.GetLogicalViewSize().X;
    float viewY = m_context->Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_inputsCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_inputsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry->AddComponent<TweenComponent>(m_inputsCanvas, TweenComponent{});

    // 1. Title strictly aligned with AUDIO & RENDER (-viewY * 0.2f)
    UIFactory::CreateText(*m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "INPUT CONTROLS & REBINDING",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    // 2. Target Device Selector Button
    m_inputDeviceBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "TARGET DEVICE: [ KEYBOARD ]",
        .OnClick = [this]() {
            m_rebindGamepadMode = !m_rebindGamepadMode;
            if (m_isRebinding)
            {
                m_isRebinding = false;
                m_rebindAction = InputAction::Count;
            }
            if (m_registry->HasComponent<TextComponent>(m_inputDeviceBtn))
            {
                m_registry->GetComponent<TextComponent>(m_inputDeviceBtn).Text =
                    m_rebindGamepadMode ? "TARGET DEVICE: [ GAMEPAD CONTROLLER ]" : "TARGET DEVICE: [ KEYBOARD ]";
            }
            UpdateInputButtons();
        },
        .Position = {rightPanelX, -viewY * 0.11f},
        .Size = {500.0f, 48.0f},
        .DefaultColor = Color{45, 65, 95, 255},
        .HoverColor = Color{65, 95, 140, 255},
        .PressedColor = Color{30, 45, 70, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    const float startY = -viewY * 0.03f;
    const float stepY = 56.0f;
    const Vector2f btnSize{500.0f, 46.0f};

    // Move Left
    m_leftRebindBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "MOVE LEFT: ...",
        .OnClick = [this]() {
            m_isRebinding = true;
            m_rebindAction = InputAction::MoveLeft;
            m_rebindTimer = 5.0f;
            UpdateInputButtons();
        },
        .Position = {rightPanelX, startY},
        .Size = btnSize,
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Move Right
    m_rightRebindBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "MOVE RIGHT: ...",
        .OnClick = [this]() {
            m_isRebinding = true;
            m_rebindAction = InputAction::MoveRight;
            m_rebindTimer = 5.0f;
            UpdateInputButtons();
        },
        .Position = {rightPanelX, startY + stepY},
        .Size = btnSize,
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Launch Ball / Storm
    m_launchRebindBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "LAUNCH / STORM: ...",
        .OnClick = [this]() {
            m_isRebinding = true;
            m_rebindAction = InputAction::LaunchBall;
            m_rebindTimer = 5.0f;
            UpdateInputButtons();
        },
        .Position = {rightPanelX, startY + stepY * 2.0f},
        .Size = btnSize,
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Pause Game
    m_pauseRebindBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "PAUSE: ...",
        .OnClick = [this]() {
            m_isRebinding = true;
            m_rebindAction = InputAction::Pause;
            m_rebindTimer = 5.0f;
            UpdateInputButtons();
        },
        .Position = {rightPanelX, startY + stepY * 3.0f},
        .Size = btnSize,
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Reset Defaults
    m_resetDefaultsBtn = UIFactory::CreateButton(*m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "RESET DEFAULT BINDINGS",
        .OnClick = [this]() {
            InputRemapManager::Get().LoadDefaults();
            InputRemapManager::Get().SaveToPrefs();
            m_isRebinding = false;
            m_rebindAction = InputAction::Count;
            UpdateInputButtons();
        },
        .Position = {rightPanelX, startY + stepY * 4.2f},
        .Size = btnSize,
        .DefaultColor = Color{120, 50, 55, 255},
        .HoverColor = Color{160, 70, 75, 255},
        .PressedColor = Color{80, 30, 35, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Status / instructions text
    m_rebindStatusText = UIFactory::CreateText(*m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "Click a binding button to remap (5s timeout)",
        .Position = {rightPanelX, startY + stepY * 5.1f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Color{255, 230, 100, 230}
    });

    UpdateInputButtons();

    UIFactory::CreatePanel(*m_registry, m_inputsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void SettingsUIController::UpdateInputButtons()
{
    if (!m_registry) return;

    const int secondsLeft = std::max(1, static_cast<int>(std::ceil(m_rebindTimer)));

    const auto formatBtnText = [this, secondsLeft](InputAction act, const std::string& prefix, Entity btn)
    {
        if (!m_registry->HasComponent<TextComponent>(btn)) return;
        auto& textComp = m_registry->GetComponent<TextComponent>(btn);

        if (m_isRebinding && m_rebindAction == act)
        {
            if (m_rebindGamepadMode)
            {
                textComp.Text = prefix + ": [PRESS GAMEPAD BTN (" + std::to_string(secondsLeft) + "s)...]";
            }
            else
            {
                textComp.Text = prefix + ": [PRESS A KEY (" + std::to_string(secondsLeft) + "s)...]";
            }
            return;
        }

        if (m_rebindGamepadMode)
        {
            int joyBtn = InputRemapManager::Get().GetGamepadBinding(act);
            textComp.Text = prefix + ": " + InputRemapManager::GamepadButtonToString(joyBtn);
        }
        else
        {
            const auto& b = InputRemapManager::Get().GetBinding(act);
            std::string keyStr(KeyCodeUtils::ToString(b.PrimaryKey));
            if (b.SecondaryKey != KeyCode::Count)
            {
                keyStr += " / " + std::string(KeyCodeUtils::ToString(b.SecondaryKey));
            }
            textComp.Text = prefix + ": " + keyStr;
        }
    };

    formatBtnText(InputAction::MoveLeft, "MOVE LEFT", m_leftRebindBtn);
    formatBtnText(InputAction::MoveRight, "MOVE RIGHT", m_rightRebindBtn);
    formatBtnText(InputAction::LaunchBall, "LAUNCH / STORM", m_launchRebindBtn);
    formatBtnText(InputAction::Pause, "PAUSE", m_pauseRebindBtn);

    if (m_registry->HasComponent<TextComponent>(m_rebindStatusText))
    {
        auto& status = m_registry->GetComponent<TextComponent>(m_rebindStatusText);
        if (m_isRebinding)
        {
            status.Text = m_rebindGamepadMode
                ? ("PRESS ANY GAMEPAD BUTTON (" + std::to_string(secondsLeft) + "s) | ESC / BACK TO CANCEL")
                : ("PRESS ANY KEY (" + std::to_string(secondsLeft) + "s) | ESC TO CANCEL");
            status.Tint = Color{255, 110, 110, 255};
        }
        else
        {
            status.Text = m_rebindGamepadMode
                ? "Click an action then press a Gamepad Button (5s limit)"
                : "Click an action then press any Keyboard Key (5s limit)";
            status.Tint = Color{255, 230, 100, 230};
        }
    }
}

void SettingsUIController::CreateGamerulesTab()
{
    float viewX = m_context->Render.GetLogicalViewSize().X;
    float viewY = m_context->Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_gamerulesCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_gamerulesCanvas, CanvasComponent{.IsEnabled = false});
    m_registry->AddComponent<TweenComponent>(m_gamerulesCanvas, TweenComponent{});

    UIFactory::CreateText(*m_registry, m_gamerulesCanvas, TextDescriptor{
        .Text = "GAMERULES",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateButton(*m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = "Difficulty",
        .Position = {rightPanelX, -viewY * 0.1f},
        .FontId = m_fontId
    });

    const bool isEvents = m_context->Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled);
    m_arenaEventsBtn = UIFactory::CreateButton(*m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = isEvents ? "ARENA EVENTS: ON" : "ARENA EVENTS: OFF",
        .OnClick = [this]() {
            bool state = m_context->Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled);
            m_context->Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, !state);
            if (m_arenaEventsBtn != NULL_ENTITY && m_registry->HasComponent<TextComponent>(m_arenaEventsBtn)) {
                m_registry->GetComponent<TextComponent>(m_arenaEventsBtn).Text = !state ? "ARENA EVENTS: ON" : "ARENA EVENTS: OFF";
            }
        },
        .Position = {rightPanelX, 0.0f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(*m_registry, m_gamerulesCanvas, PanelDescriptor{
      .Position = {rightPanelX, 0.0f},
      .Size = {rightPanelWidth, viewY},
      .Tint = Colors::Transparent
    });
}

void SettingsUIController::CreateCheatsTab()
{
    float viewX = m_context->Render.GetLogicalViewSize().X;
    float viewY = m_context->Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_cheatsCanvas = m_registry->CreateEntity();
    m_registry->AddComponent<CanvasComponent>(m_cheatsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry->AddComponent<TweenComponent>(m_cheatsCanvas, TweenComponent{});

    UIFactory::CreateText(*m_registry, m_cheatsCanvas, TextDescriptor{
        .Text = "CHEATS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateTextInput(*m_registry, m_cheatsCanvas, TextInputDescriptor{
        .Placeholder = "Enter Cheat Code...",
        .OnSubmit = [this](const std::string& code) {
            m_context->Events.Publish(CheatSubmitEvent(code));
        },
        .Position = {rightPanelX, 0.0f},
        .Size = {600.0f, 60.0f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreatePanel(*m_registry, m_cheatsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}
