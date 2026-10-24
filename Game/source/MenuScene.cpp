#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/LevelSelectScene.h"
#include "Scenes/Sandbox.h"
#include "Scenes/SamplePhysics.h"
#include "Scenes/SampleAudio.h"
#include "Scenes/SampleStateMachine.h"
#include "Scenes/SampleTween.h"
#include "Scenes/SampleUI.h"
#include "Scenes/SampleRenderLayer.h"
#include "Core/GameContext.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Core/InputRemapManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/GameRules.h"
#include "AudioMixer.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "UI/UIFactory.h"
#include "ECS/Systems/UISystem.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "System/Window.h"
#include "Data/WindowConfig.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Tween.h"
#include "Math/EasingFunctions.h"
#include "ECS/Components/UI/TextInputComponent.h"
#include "Events/CheatSubmitEvent.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

void MenuScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_bgColor = Color{8, 10, 16, 255};

    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");

    // Initialize persisted audio & render preferences
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 100.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 100.0f);
    context.Audio.SetSfxVolume(sfxVol);
    context.Audio.SetMusicVolume(musicVol);

    std::string savedDevice = PlayerPrefs::GetString("Audio_Device", "");
    if (!savedDevice.empty())
    {
        context.Audio.SetAudioDevice(savedDevice);
    }

    // Play Main Menu background music smoothly in loop
    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);

    bool vsync = PlayerPrefs::GetBool("VSync", false);
    context.Render.GetWindow().SetVSync(vsync);

    m_levelManager.Initialize("Resources/levels");
    m_leaderboard.Load();

    InitRetroBackground(context);

    CreateMainMenuUI(context);
    CreateSettingsLayout(context);
    CreateAudioTab(context);
    CreateRenderTab(context);
    CreateInputsTab(context);
    CreateGamerulesTab(context);
    CreateCheatsTab(context);
    CreateLeaderboardModal(context);
    CreateDebugModal(context);
    CreateWorkshopModal(context);

    PreloadGameAssets(context);
}

void MenuScene::PreloadGameAssets(GameContext& context)
{
    // Preload all level files asynchronously
    m_levelManager.PreloadLevels(context.Resources);

    // Preload gameplay textures asynchronously
    context.Resources.LoadResourceAsync("Resources/sprite/ball.png");
    context.Resources.LoadResourceAsync("Resources/sprite/paddle.png");
    context.Resources.LoadResourceAsync("Resources/sprite/brick.png");
    context.Resources.LoadResourceAsync("Resources/sprite/brick_crack.png");
    context.Resources.LoadResourceAsync("Resources/sprite/fire.png");
    context.Resources.LoadResourceAsync("Resources/sprite/heart.png");
    context.Resources.LoadResourceAsync("Resources/sprite/debug.jpg");

    // Preload shaders asynchronously
    context.Resources.LoadResourceAsync("Resources/shaders/crack.frag");
    context.Resources.LoadResourceAsync("Resources/shaders/scanner.frag");

    // Preload audio buffers asynchronously
    context.Resources.LoadResourceAsync("Resources/audio/sfx/ball_despawn.wav");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/brick_destroy.wav");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/brick_hit.wav");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/combo.wav");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/game_over.mp3");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/game_win.mp3");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/victory.mp3");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/paddle_hit.wav");
    context.Resources.LoadResourceAsync("Resources/audio/sfx/powerup.wav");

    // Preload font asynchronously
    context.Resources.LoadResourceAsync("Resources/font/arial.ttf");
}

void MenuScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);

    // Rebinding input capture
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
            KeyCode pressed = context.Input.GetLastKeyPressed();
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
                if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                UpdateInputButtons();
                return;
            }
        }
        else
        {
            // Gamepad capture (ESC cancels; Back button 6 cancels after debounce)
            if (context.Input.IsKeyPress(KeyCode::Escape) || (m_rebindTimer < 4.85f && context.Input.IsJoystickButtonPress(0, 6) && m_rebindAction != InputAction::Pause))
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
                int joyInput = context.Input.GetAnyGamepadInputTriggered(0);
                if (joyInput >= 0)
                {
                    InputRemapManager::Get().SetGamepadBinding(m_rebindAction, joyInput);
                    InputRemapManager::Get().SaveToPrefs();

                    m_isRebinding = false;
                    m_rebindAction = InputAction::Count;
                    UISystem::SetNavigationEnabled(true);
                    if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    UpdateInputButtons();
                    return;
                }
            }
        }

        // Live countdown text update
        UpdateInputButtons();
        return;
    }

    // Escape or Gamepad B closes any open modal, or returns to main
    if (context.Input.IsMenuCancelTriggered())
    {
        if (m_activeModal != NULL_ENTITY)
        {
            CloseModals();
            return;
        }
    }

    // Navigation shortcuts when no modal is open
    if (m_activeModal == NULL_ENTITY)
    {
        if (context.Input.IsKeyPress(KeyCode::Enter) || context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsKeyPress(KeyCode::P) || context.Input.IsJoystickButtonPress(0, 7))
        {
            context.Scenes.LoadScene<GameScene>();
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::L))
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            context.Scenes.LoadScene<LevelSelectScene>();
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::W))
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            OpenModal(m_workshopCanvas);
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::C) || context.Input.IsKeyPress(KeyCode::H))
        {
            OpenModal(m_leaderboardCanvas);
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::O))
        {
            OpenModal(m_settingsCanvas);
            return;
        }
        if (context.Input.IsKeyPress(KeyCode::Tab))
        {
            OpenModal(m_debugCanvas);
            return;
        }
    }

    // Direct developer jump keys
    if (context.Input.IsKeyPress(KeyCode::F1) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num1)))
    {
        context.Scenes.LoadScene<SandBox>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F2) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num2)))
    {
        context.Scenes.LoadScene<SamplePhysics>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F3) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num3)))
    {
        context.Scenes.LoadScene<SampleAudio>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F4) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num4)))
    {
        context.Scenes.LoadScene<GameScene>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F5) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num5)))
    {
        context.Scenes.LoadScene<SampleStateMachine>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F6) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num6)))
    {
        context.Scenes.LoadScene<SampleTween>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F7) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num7)))
    {
        context.Scenes.LoadScene<SampleUI>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F8) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num8)))
    {
        context.Scenes.LoadScene<SampleRenderLayer>();
        return;
    }
    if (context.Input.IsKeyPress(KeyCode::F9) || (m_activeModal == NULL_ENTITY && context.Input.IsKeyPress(KeyCode::Num9)))
    {
        context.Scenes.LoadScene<LevelSelectScene>();
        return;
    }

    UpdateRetroBackground(dt, context);

    if (m_cheatsTabBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    UISystem::OnUpdate(dt, m_registry, context);
}

void MenuScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    RenderRetroBackground(context);
    context.Render.ResetCamera();
    UISystem::OnRender(m_registry, context);
}

void MenuScene::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}

uint32_t MenuScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
}

void MenuScene::OpenModal(Entity modalCanvas)
{
    CloseModals();
    m_activeModal = modalCanvas;
    if (m_registry.IsAlive(m_activeModal) && m_registry.HasComponent<CanvasComponent>(m_activeModal))
    {
        m_registry.GetComponent<CanvasComponent>(m_activeModal).IsEnabled = true;
    }
    if (m_registry.IsAlive(m_mainCanvas) && m_registry.HasComponent<CanvasComponent>(m_mainCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_mainCanvas).IsEnabled = false;
    }
    if (modalCanvas == m_settingsCanvas)
    {
        OpenSettingsTab(m_audioCanvas);
    }
    if (modalCanvas == m_workshopCanvas)
    {
        OpenWorkshopTab(m_workshopUpgradesCanvas);
    }
}

void MenuScene::CloseModals()
{
    for (Entity canvas : {m_settingsCanvas, m_leaderboardCanvas, m_debugCanvas, m_workshopCanvas})
    {
        if (canvas != NULL_ENTITY && m_registry.IsAlive(canvas) && m_registry.HasComponent<CanvasComponent>(canvas))
        {
            m_registry.GetComponent<CanvasComponent>(canvas).IsEnabled = false;
        }
    }
    if (m_activeTabCanvas != NULL_ENTITY && m_registry.IsAlive(m_activeTabCanvas) && m_registry.HasComponent<CanvasComponent>(m_activeTabCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
        m_activeTabCanvas = NULL_ENTITY;
    }
    for (Entity subCanvas : {m_workshopUpgradesCanvas, m_workshopUltimatesCanvas, m_workshopProfileCanvas})
    {
        if (subCanvas != NULL_ENTITY && m_registry.IsAlive(subCanvas) && m_registry.HasComponent<CanvasComponent>(subCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(subCanvas).IsEnabled = false;
        }
    }
    m_workshopActiveTabCanvas = NULL_ENTITY;
    m_activeModal = NULL_ENTITY;
    if (m_registry.IsAlive(m_mainCanvas) && m_registry.HasComponent<CanvasComponent>(m_mainCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_mainCanvas).IsEnabled = true;
    }
}

void MenuScene::UpdateVolumeBars(const std::vector<Entity>& bars, float volume)
{
    for (size_t i = 0; i < bars.size(); ++i)
    {
        if (m_registry.HasComponent<PanelComponent>(bars[i]))
        {
            auto& panel = m_registry.GetComponent<PanelComponent>(bars[i]);
            if ((static_cast<float>(i) * 10.0f) < volume)
            {
                panel.Tint.a = 255;
            }
            else
            {
                panel.Tint.a = 50;
            }
        }
    }
}

void MenuScene::InitRetroBackground(const GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    m_stars.clear();
    m_stars.reserve(100);

    uint32_t seed = 123456789;
    auto rnd = [&seed]() -> float {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        return static_cast<float>(seed % 10000) / 10000.0f;
    };

    for (size_t i = 0; i < 100; ++i)
    {
        StarParticle star;
        star.X = rnd() * viewSize.X;
        star.Y = rnd() * viewSize.Y * 0.75f;
        star.Speed = 8.0f + rnd() * 22.0f;
        star.Size = 1.5f + rnd() * 2.0f;
        star.Alpha = 70.0f + rnd() * 140.0f;
        star.TwinkleFreq = 1.5f + rnd() * 3.0f;
        m_stars.push_back(star);
    }
}

void MenuScene::UpdateRetroBackground(float dt, const GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    m_animTime += dt;
    m_gridAnimOffset += dt * 35.0f;
    if (m_gridAnimOffset > 60.0f)
    {
        m_gridAnimOffset -= 60.0f;
    }

    for (auto& star : m_stars)
    {
        star.Y += star.Speed * dt;
        if (star.Y > viewSize.Y * 0.75f)
        {
            star.Y = 0.0f;
            star.X = static_cast<float>(rand() % static_cast<int>(std::max(1.0f, viewSize.X)));
        }
    }
}

void MenuScene::RenderRetroBackground(const GameContext& context) const
{
    context.Render.ResetCamera();
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    // 1. Subtle retro arcade perspective grid at the bottom
    const float horizonY = viewSize.Y * 0.58f;
    const float bottomY = viewSize.Y;
    const float centerX = viewSize.X * 0.5f;

    // Horizon line
    context.Render.DrawLine(Vector2f{0.0f, horizonY}, Vector2f{viewSize.X, horizonY}, Color{45, 80, 150, 70}, 2.0f);

    // Exponentially spaced horizontal grid lines
    constexpr int kHLineCount = 10;
    for (int i = 0; i <= kHLineCount; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(kHLineCount);
        float depthT = t * t;
        float y = horizonY + depthT * (bottomY - horizonY);

        uint8_t alpha = static_cast<uint8_t>(20 + depthT * 60);
        Color lineColor{35, 75, 145, alpha};
        context.Render.DrawLine(Vector2f{0.0f, y}, Vector2f{viewSize.X, y}, lineColor, 1.5f);
    }

    // Radiating vertical perspective lines from horizon vanishing center
    constexpr int kPerspectiveVLines = 16;
    for (int i = -kPerspectiveVLines / 2; i <= kPerspectiveVLines / 2; ++i)
    {
        float bottomOffset = static_cast<float>(i) * (viewSize.X / static_cast<float>(kPerspectiveVLines * 0.8f));
        Vector2f start{centerX + static_cast<float>(i) * 15.0f, horizonY};
        Vector2f end{centerX + bottomOffset, bottomY};
        context.Render.DrawLine(start, end, Color{30, 65, 130, 35}, 1.2f);
    }

    // 2. Subtle starfield in the upper sky
    for (const auto& star : m_stars)
    {
        float twinkle = (std::sin(m_animTime * star.TwinkleFreq + star.X) + 1.0f) * 0.5f;
        uint8_t a = static_cast<uint8_t>(std::clamp(star.Alpha * (0.5f + 0.5f * twinkle), 30.0f, 255.0f));
        Color col{200, 220, 255, a};
        context.Render.DrawRectangle(star.Size, star.Size, Transform2D{Vector2f{star.X, star.Y}}, col);
    }
}

void MenuScene::CreateMainMenuUI(const GameContext& context)
{
    m_mainCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_mainCanvas, CanvasComponent{.IsEnabled = true});

    // Subtle neon underline below title
    UIFactory::CreatePanel(m_registry, m_mainCanvas, PanelDescriptor{
        .Position = {0.0f, -145.0f},
        .Size = {420.0f, 3.0f},
        .Tint = Color{255, 180, 50, 180}
    });

    // Title & Subtitle
    UIFactory::CreateText(m_registry, m_mainCanvas, TextDescriptor{
        .Text = "REBREAKER",
        .Position = {0.0f, -235.0f},
        .FontId = m_fontId,
        .FontSize = 80.0f,
        .Tint = Colors::Gold
    });

    UIFactory::CreateText(m_registry, m_mainCanvas, TextDescriptor{
        .Text = "--- RELOADED ---",
        .Position = {0.0f, -175.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{255, 100, 50, 255}
    });

    // Primary Menu Buttons
    constexpr float btnWidth = 360.0f;
    constexpr float btnHeight = 46.0f;
    constexpr float startY = -115.0f;
    constexpr float stepY = 54.0f;

    // 1. Play
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "PLAY",
        .OnClick = [&context]() {
            context.Scenes.LoadScene<GameScene>();
        },
        .Position = {0.0f, startY},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{30, 80, 45, 255},
        .HoverColor = Color{45, 120, 65, 255},
        .PressedColor = Color{20, 55, 30, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });

    // 2. Level Select
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "LEVEL SELECT",
        .OnClick = [&context]() {
            context.Scenes.LoadScene<LevelSelectScene>();
        },
        .Position = {0.0f, startY + stepY},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{35, 55, 90, 255},
        .HoverColor = Color{50, 80, 130, 255},
        .PressedColor = Color{25, 40, 65, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });

    // 3. Workshop & Upgrades
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "WORKSHOP [W]",
        .OnClick = [this]() {
            OpenModal(m_workshopCanvas);
        },
        .Position = {0.0f, startY + stepY * 2.0f},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{75, 60, 20, 255},
        .HoverColor = Color{110, 90, 30, 255},
        .PressedColor = Color{50, 40, 15, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });

    // 4. Leaderboard
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "LEADERBOARD",
        .OnClick = [this]() {
            OpenModal(m_leaderboardCanvas);
        },
        .Position = {0.0f, startY + stepY * 3.0f},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{40, 45, 65, 255},
        .HoverColor = Color{60, 65, 95, 255},
        .PressedColor = Color{28, 32, 48, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });

    // 5. Settings
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "SETTINGS",
        .OnClick = [this]() {
            OpenModal(m_settingsCanvas);
            OpenSettingsTab(m_audioCanvas);
        },
        .Position = {0.0f, startY + stepY * 4.0f},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{40, 45, 65, 255},
        .HoverColor = Color{60, 65, 95, 255},
        .PressedColor = Color{28, 32, 48, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });

    // 6. Debug / Samples
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "SAMPLES & TESTS [TAB]",
        .OnClick = [this]() {
            OpenModal(m_debugCanvas);
        },
        .Position = {0.0f, startY + stepY * 5.0f},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{50, 45, 75, 255},
        .HoverColor = Color{75, 65, 110, 255},
        .PressedColor = Color{35, 30, 52, 255},
        .FontId = m_fontId,
        .FontSize = 30.0f
    });

    // 7. Quit
    UIFactory::CreateButton(m_registry, m_mainCanvas, ButtonDescriptor{
        .Text = "QUIT",
        .OnClick = [&context]() {
            context.Render.GetWindow().Close();
        },
        .Position = {0.0f, startY + stepY * 6.0f},
        .Size = {btnWidth, btnHeight},
        .DefaultColor = Color{90, 30, 35, 255},
        .HoverColor = Color{130, 45, 50, 255},
        .PressedColor = Color{60, 20, 25, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });
}

void MenuScene::CreateSettingsLayout(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X - 300.0f;
    float viewY = context.Render.GetLogicalViewSize().Y - 350.0f;

    float leftPanelWidth = viewX * 0.25f;
    float leftPanelX = -viewX * 0.375f + 200.0f;

    m_settingsCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_settingsCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(m_registry, m_settingsCanvas, TextDescriptor{
        .Text = "SETTINGS",
        .Position = {leftPanelX, -viewY * 0.3f},
        .FontId = m_fontId,
        .FontSize = 50.0f
    });

    // 4 Tab Buttons
    float startY = -viewY * 0.15f;
    float stepY = viewY * 0.08f;

    UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "AUDIO",
        .OnClick = [this]() { OpenSettingsTab(m_audioCanvas); },
        .Position = {leftPanelX, startY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "RENDER",
        .OnClick = [this]() { OpenSettingsTab(m_renderCanvas); },
        .Position = {leftPanelX, startY + stepY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "INPUTS",
        .OnClick = [this]() { OpenSettingsTab(m_inputsCanvas); },
        .Position = {leftPanelX, startY + stepY * 2.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "GAMERULES",
        .OnClick = [this]() { OpenSettingsTab(m_gamerulesCanvas); },
        .Position = {leftPanelX, startY + stepY * 3.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    m_cheatsTabBtn = UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "CHEATS",
        .OnClick = [this]() { OpenSettingsTab(m_cheatsCanvas); },
        .Position = {leftPanelX, startY + stepY * 4.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });
    if (m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    // Back Button
    UIFactory::CreateButton(m_registry, m_settingsCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            CloseModals();
        },
        .Position = {leftPanelX, viewY * 0.4f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .DefaultColor = Colors::Crimson,
        .HoverColor = Colors::LightCoral,
        .PressedColor = Colors::DarkRed,
        .FontId = m_fontId
    });

    // Dark transparent background for settings modal
    UIFactory::CreatePanel(m_registry, m_settingsCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX + 300.0f, viewY + 350.0f},
        .Tint = Colors::Transparent,
        .AnchorPoint = Anchor::Center
    });

    // Left Tabs Panel
    UIFactory::CreatePanel(m_registry, m_settingsCanvas, PanelDescriptor{
        .Position = {leftPanelX, 0.0f},
        .Size = {leftPanelWidth, viewY - 50.0f},
        .Tint = Colors::Transparent
    });
}

void MenuScene::CreateAudioTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_audioCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_audioCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_audioCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_audioCanvas, TextDescriptor{
        .Text = "AUDIO SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateVolumeControl(m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "SFX",
        .OnMinus = [&context, this]() {
            float vol = std::max(0.0f, context.Audio.GetSfxVolume() - 10.0f);
            context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .OnPlus = [&context, this]() {
            float vol = std::min(100.0f, context.Audio.GetSfxVolume() + 10.0f);
            context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .BarsOut = &m_sfxVolumeBars,
        .Position = {rightPanelX * 0.5f, -viewY * 0.1f + 20.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateVolumeControl(m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "MUSIC",
        .OnMinus = [&context, this]() {
            float vol = std::max(0.0f, context.Audio.GetMusicVolume() - 10.0f);
            context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .OnPlus = [&context, this]() {
            float vol = std::min(100.0f, context.Audio.GetMusicVolume() + 10.0f);
            context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .BarsOut = &m_musicVolumeBars,
        .Position = {rightPanelX * 0.5f, viewY * 0.1f - 50.0f},
        .FontId = m_fontId
    });

    UpdateVolumeBars(m_sfxVolumeBars, context.Audio.GetSfxVolume());
    UpdateVolumeBars(m_musicVolumeBars, context.Audio.GetMusicVolume());

    // Playback Device Selection
    UIFactory::CreateText(m_registry, m_audioCanvas, TextDescriptor{
        .Text = "DEVICE",
        .Position = {rightPanelX * 0.5f - 110.0f, viewY * 0.1f + 45.0f},
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

    m_audioDeviceBtn = UIFactory::CreateButton(m_registry, m_audioCanvas, ButtonDescriptor{
        .Text = formatDeviceText(context.Audio.GetCurrentAudioDevice()),
        .OnClick = [&context, this, formatDeviceText]() {
            auto devices = context.Audio.GetAvailableAudioDevices();
            if (devices.empty()) return;

            std::string current = context.Audio.GetCurrentAudioDevice();
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
            if (context.Audio.SetAudioDevice(newDevice))
            {
                PlayerPrefs::SetString("Audio_Device", newDevice);
                PlayerPrefs::Save();
                if (m_bounceSfxId != 0) context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            }
            if (m_registry.HasComponent<TextComponent>(m_audioDeviceBtn))
            {
                m_registry.GetComponent<TextComponent>(m_audioDeviceBtn).Text = formatDeviceText(newDevice);
            }
        },
        .Position = {rightPanelX * 0.5f + 250.0f, viewY * 0.1f + 45.0f},
        .Size = {480.0f, 54.0f},
        .DefaultColor = Color{40, 55, 80, 255},
        .HoverColor = Color{60, 85, 120, 255},
        .PressedColor = Color{25, 38, 55, 255},
        .FontId = m_fontId,
        .FontSize = 24.0f
    });

    UIFactory::CreatePanel(m_registry, m_audioCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void MenuScene::CreateRenderTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_renderCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_renderCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_renderCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_renderCanvas, TextDescriptor{
        .Text = "RENDER SETTINGS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    bool isFullscreen = context.Render.GetWindow().GetConfig().Mode == WindowMode::Fullscreen;
    std::string fsText = isFullscreen ? "FULL-SCREEN: ON" : "FULL-SCREEN: OFF";
    
    m_fsBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = fsText,
        .OnClick = [&context, this]() {
            WindowConfig config = context.Render.GetWindow().GetConfig();
            if (config.Mode == WindowMode::Fullscreen) {
                config.Mode = WindowMode::Windowed;
                PlayerPrefs::SetBool("Fullscreen", false);
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: OFF";
                }
            } else {
                config.Mode = WindowMode::Fullscreen;
                PlayerPrefs::SetBool("Fullscreen", true);
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: ON";
                }
            }
            PlayerPrefs::Save();
            context.Render.GetWindow().ApplyConfig(config);
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

    std::string currentRes = std::to_string(context.Render.GetWindow().GetConfig().Width) + "x" + std::to_string(context.Render.GetWindow().GetConfig().Height);

    UIFactory::CreateDropdown(m_registry, m_renderCanvas, DropdownDescriptor{
        .DefaultText = currentRes,
        .Options = options,
        .OnSelect = [&context, uniqueModes](int index, const std::string& /*text*/) {
            if (index >= 0 && static_cast<size_t>(index) < uniqueModes.size()) {
                WindowConfig config = context.Render.GetWindow().GetConfig();
                config.Width = uniqueModes[static_cast<size_t>(index)].Width;
                config.Height = uniqueModes[static_cast<size_t>(index)].Height;
                PlayerPrefs::SetInt("WindowWidth", static_cast<int>(config.Width));
                PlayerPrefs::SetInt("WindowHeight", static_cast<int>(config.Height));
                PlayerPrefs::Save();
                context.Render.GetWindow().ApplyConfig(config);
            }
        },
        .Position = {rightPanelX + 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId,
    });
    
    bool isShader = context.Rules.GetRule(Rule::Graphics::EnableShader);
    std::string shaderText = isShader ? "SHADER: ON" : "SHADER: OFF";
    m_shaderBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = shaderText,
        .OnClick = [&context, this]() {
            bool state = context.Rules.GetRule(Rule::Graphics::EnableShader);
            context.Rules.SetRule(Rule::Graphics::EnableShader, !state);
            PlayerPrefs::SetBool("EnableShader", !state);
            PlayerPrefs::Save();
            if (m_registry.HasComponent<TextComponent>(m_shaderBtn)) {
                m_registry.GetComponent<TextComponent>(m_shaderBtn).Text = !state ? "SHADER: ON" : "SHADER: OFF";
            }
        },
        .Position = {rightPanelX - 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });
    
    bool isParticles = context.Rules.GetRule(Rule::Graphics::EnableParticles);
    std::string particlesText = isParticles ? "PARTICLES: ON" : "PARTICLES: OFF";
    m_particlesBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = particlesText,
        .OnClick = [&context, this]() {
            bool state = context.Rules.GetRule(Rule::Graphics::EnableParticles);
            context.Rules.SetRule(Rule::Graphics::EnableParticles, !state);
            PlayerPrefs::SetBool("EnableParticles", !state);
            PlayerPrefs::Save();
            if (m_registry.HasComponent<TextComponent>(m_particlesBtn)) {
                m_registry.GetComponent<TextComponent>(m_particlesBtn).Text = !state ? "PARTICLES: ON" : "PARTICLES: OFF";
            }
        },
        .Position = {rightPanelX + 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(m_registry, m_renderCanvas, PanelDescriptor{
       .Position = {rightPanelX, 0.0f},
       .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void MenuScene::CreateInputsTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_inputsCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_inputsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_inputsCanvas, TweenComponent{});

    // 1. Title strictly aligned with AUDIO & RENDER (-viewY * 0.2f)
    UIFactory::CreateText(m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "INPUT CONTROLS & REBINDING",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    // 2. Target Device Selector Button
    m_inputDeviceBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "TARGET DEVICE: [ KEYBOARD ]",
        .OnClick = [this]() {
            m_rebindGamepadMode = !m_rebindGamepadMode;
            if (m_isRebinding)
            {
                m_isRebinding = false;
                m_rebindAction = InputAction::Count;
            }
            if (m_registry.HasComponent<TextComponent>(m_inputDeviceBtn))
            {
                m_registry.GetComponent<TextComponent>(m_inputDeviceBtn).Text =
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
    m_leftRebindBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
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
    m_rightRebindBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
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
    m_launchRebindBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
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
    m_pauseRebindBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
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
    m_resetDefaultsBtn = UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
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
    m_rebindStatusText = UIFactory::CreateText(m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "Click a binding button to remap (5s timeout)",
        .Position = {rightPanelX, startY + stepY * 5.1f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Color{255, 230, 100, 230}
    });

    UpdateInputButtons();

    UIFactory::CreatePanel(m_registry, m_inputsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void MenuScene::UpdateInputButtons()
{
    const int secondsLeft = std::max(1, static_cast<int>(std::ceil(m_rebindTimer)));

    const auto formatBtnText = [this, secondsLeft](InputAction act, const std::string& prefix, Entity btn)
    {
        if (!m_registry.HasComponent<TextComponent>(btn)) return;
        auto& textComp = m_registry.GetComponent<TextComponent>(btn);

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

    if (m_registry.HasComponent<TextComponent>(m_rebindStatusText))
    {
        auto& status = m_registry.GetComponent<TextComponent>(m_rebindStatusText);
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

void MenuScene::CreateGamerulesTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_gamerulesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_gamerulesCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_gamerulesCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_gamerulesCanvas, TextDescriptor{
        .Text = "GAMERULES",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateButton(m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = "Difficulty",
        .Position = {rightPanelX, -viewY * 0.1f},
        .FontId = m_fontId
    });
    
    UIFactory::CreatePanel(m_registry, m_gamerulesCanvas, PanelDescriptor{
      .Position = {rightPanelX, 0.0f},
      .Size = {rightPanelWidth, viewY},
      .Tint = Colors::Transparent
    });
}

void MenuScene::CreateCheatsTab(const GameContext& context)
{
    float viewX = context.Render.GetLogicalViewSize().X;
    float viewY = context.Render.GetLogicalViewSize().Y;
    float rightPanelWidth = viewX * 0.75f;
    float rightPanelX = viewX * 0.125f;

    m_cheatsCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_cheatsCanvas, CanvasComponent{.IsEnabled = false});
    m_registry.AddComponent<TweenComponent>(m_cheatsCanvas, TweenComponent{});

    UIFactory::CreateText(m_registry, m_cheatsCanvas, TextDescriptor{
        .Text = "CHEATS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateTextInput(m_registry, m_cheatsCanvas, TextInputDescriptor{
        .Placeholder = "Enter Cheat Code...",
        .OnSubmit = [&context](const std::string& code) {
            context.Events.Publish(CheatSubmitEvent(code));
        },
        .Position = {rightPanelX, 0.0f},
        .Size = {600.0f, 60.0f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreatePanel(m_registry, m_cheatsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
}

void MenuScene::OpenSettingsTab(Entity targetCanvas)
{
    if (m_activeTabCanvas == targetCanvas) {
        return;
    }

    if (m_activeTabCanvas != NULL_ENTITY && m_registry.IsAlive(m_activeTabCanvas) && m_registry.HasComponent<CanvasComponent>(m_activeTabCanvas)) {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    m_activeTabCanvas = targetCanvas;
    if (m_activeTabCanvas != NULL_ENTITY && m_registry.IsAlive(m_activeTabCanvas) && m_registry.HasComponent<CanvasComponent>(m_activeTabCanvas)) {
        m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = true;

        if (m_registry.HasComponent<TweenComponent>(targetCanvas))
        {
            auto& tweenComp = m_registry.GetComponent<TweenComponent>(targetCanvas);
            tweenComp.Clear();
            
            m_registry.View<RectTransform>([&](Entity e, RectTransform& rect) {
                if (rect.Parent == targetCanvas) {
                    Vector2f originalPos = rect.Position;
                    Vector2f startPos = {originalPos.X, originalPos.Y - 50.0f};
                    
                    tweenComp.AddTween(TweenConfig<Vector2f>{
                        .Start = startPos,
                        .End = originalPos,
                        .Duration = 0.25f,
                        .Setter = [this, e](Vector2f pos) {
                            if (m_registry.HasComponent<RectTransform>(e)) {
                                m_registry.GetComponent<RectTransform>(e).Position = pos;
                            }
                        },
                        .Ease = EasingFunctions::EasingType::EaseOutQuad
                    });
                }
            });
        }
    }
}

void MenuScene::CreateLeaderboardModal(const GameContext& /*context*/)
{
    m_leaderboardCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_leaderboardCanvas, CanvasComponent{.IsEnabled = false});

    // Subtle header accent line
    UIFactory::CreatePanel(m_registry, m_leaderboardCanvas, PanelDescriptor{
        .Position = {0.0f, -200.0f},
        .Size = {720.0f, 2.0f},
        .Tint = Color{255, 200, 50, 180}
    });

    // Header
    UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
        .Text = "HIGH SCORES",
        .Position = {0.0f, -250.0f},
        .FontId = m_fontId,
        .FontSize = 46.0f,
        .Tint = Colors::Gold
    });

    // Column titles (aligned horizontally)
    constexpr float colRankX   = -240.0f;
    constexpr float colPlayerX =    0.0f;
    constexpr float colScoreX  =  240.0f;

    UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
        .Text = "RANK",
        .Position = {colRankX, -170.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{180, 210, 255, 255}
    });

    UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
        .Text = "PLAYER",
        .Position = {colPlayerX, -170.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{180, 210, 255, 255}
    });

    UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
        .Text = "SCORE",
        .Position = {colScoreX, -170.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{180, 210, 255, 255}
    });

    const auto& entries = m_leaderboard.GetEntries();
    constexpr float rowStartY = -110.0f;
    constexpr float rowStepY = 52.0f;

    for (size_t i = 0; i < 5; ++i)
    {
        std::string rankStr = "#" + std::to_string(i + 1);
        std::string nameStr = (i < entries.size()) ? entries[i].Name : "---";
        std::string scoreStr = (i < entries.size()) ? std::to_string(entries[i].Score) : "0";

        Color rowColor = (i == 0) ? Colors::Gold : (i == 1 ? Colors::White : Color{180, 185, 200, 255});
        float rowY = rowStartY + static_cast<float>(i) * rowStepY;

        // Rank column
        UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
            .Text = rankStr,
            .Position = {colRankX, rowY},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = rowColor
        });

        // Player column
        UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
            .Text = nameStr,
            .Position = {colPlayerX, rowY},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = rowColor
        });

        // Score column
        UIFactory::CreateText(m_registry, m_leaderboardCanvas, TextDescriptor{
            .Text = scoreStr,
            .Position = {colScoreX, rowY},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = rowColor
        });
    }

    // Back Button
    UIFactory::CreateButton(m_registry, m_leaderboardCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            CloseModals();
        },
        .Position = {0.0f, 240.0f},
        .Size = {320.0f, 54.0f},
        .DefaultColor = Color{100, 35, 40, 255},
        .HoverColor = Color{140, 50, 60, 255},
        .PressedColor = Color{70, 25, 30, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });
}

void MenuScene::CreateDebugModal(const GameContext& context)
{
    m_debugCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_debugCanvas, CanvasComponent{.IsEnabled = false});

    // Subtle header accent line
    UIFactory::CreatePanel(m_registry, m_debugCanvas, PanelDescriptor{
        .Position = {0.0f, -200.0f},
        .Size = {620.0f, 2.0f},
        .Tint = Color{200, 100, 240, 180}
    });

    // Header
    UIFactory::CreateText(m_registry, m_debugCanvas, TextDescriptor{
        .Text = "TEST & DEBUG SCENES",
        .Position = {0.0f, -250.0f},
        .FontId = m_fontId,
        .FontSize = 46.0f,
        .Tint = Colors::Gold
    });

    constexpr float btnW = 360.0f;
    constexpr float btnH = 50.0f;
    constexpr float col1X = -190.0f;
    constexpr float col2X = 190.0f;

    // Column 1
    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "1. SandBox [F1]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SandBox>(); },
        .Position = {col1X, -160.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "2. Physics [F2]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SamplePhysics>(); },
        .Position = {col1X, -95.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "3. Audio & Music [F3]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleAudio>(); },
        .Position = {col1X, -30.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "4. State Machine [F5]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleStateMachine>(); },
        .Position = {col1X, 35.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    // Column 2
    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "5. Tween Effects [F6]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleTween>(); },
        .Position = {col2X, -160.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "6. UI Showcase [F7]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleUI>(); },
        .Position = {col2X, -95.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "7. Render Layer [F8]",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleRenderLayer>(); },
        .Position = {col2X, -30.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "8. Game Direct [F4]",
        .OnClick = [&context]() { context.Scenes.LoadScene<GameScene>(); },
        .Position = {col2X, 35.0f},
        .Size = {btnW, btnH},
        .DefaultColor = Color{35, 75, 45, 255},
        .HoverColor = Color{50, 110, 65, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    // Hint
    UIFactory::CreateText(m_registry, m_debugCanvas, TextDescriptor{
        .Text = "Press F1 to F8 to jump directly to any test scene.",
        .Position = {0.0f, 130.0f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Colors::Yellow
    });

    // Back Button
    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            CloseModals();
        },
        .Position = {0.0f, 230.0f},
        .Size = {320.0f, 54.0f},
        .DefaultColor = Color{100, 35, 40, 255},
        .HoverColor = Color{140, 50, 60, 255},
        .PressedColor = Color{70, 25, 30, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f
    });
}

void MenuScene::CreateWorkshopModal(const GameContext& /*context*/)
{
    m_workshopCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopCanvas, CanvasComponent{.IsEnabled = false});

    // Header Title
    UIFactory::CreateText(m_registry, m_workshopCanvas, TextDescriptor{
        .Text = "WORKSHOP & ARMORED LAB",
        .Position = {0.0f, -270.0f},
        .FontId = m_fontId,
        .FontSize = 44.0f,
        .Tint = Colors::Gold
    });

    // Accent line
    UIFactory::CreatePanel(m_registry, m_workshopCanvas, PanelDescriptor{
        .Position = {0.0f, -235.0f},
        .Size = {840.0f, 2.0f},
        .Tint = Color{255, 200, 50, 180}
    });

    // Bank & Pilot status line
    m_workshopBankText = UIFactory::CreateText(m_registry, m_workshopCanvas, TextDescriptor{
        .Text = "BANK: 0 CREDITS  |  PILOT: PLAYER",
        .Position = {0.0f, -205.0f},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .Tint = Color{100, 240, 255, 255}
    });

    // Sub-tab Navigation Buttons
    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "UPGRADES",
        .OnClick = [this]() {
            OpenWorkshopTab(m_workshopUpgradesCanvas);
        },
        .Position = {-260.0f, -160.0f},
        .Size = {230.0f, 42.0f},
        .DefaultColor = Color{45, 55, 80, 255},
        .HoverColor = Color{65, 80, 115, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "ULTIMATES",
        .OnClick = [this]() {
            OpenWorkshopTab(m_workshopUltimatesCanvas);
        },
        .Position = {0.0f, -160.0f},
        .Size = {230.0f, 42.0f},
        .DefaultColor = Color{45, 55, 80, 255},
        .HoverColor = Color{65, 80, 115, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "PROFILE & STATS",
        .OnClick = [this]() {
            OpenWorkshopTab(m_workshopProfileCanvas);
        },
        .Position = {260.0f, -160.0f},
        .Size = {230.0f, 42.0f},
        .DefaultColor = Color{45, 55, 80, 255},
        .HoverColor = Color{65, 80, 115, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
    });

    // Sub-canvases
    m_workshopUpgradesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopUpgradesCanvas, CanvasComponent{.IsEnabled = false});

    m_workshopUltimatesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopUltimatesCanvas, CanvasComponent{.IsEnabled = false});

    m_workshopProfileCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopProfileCanvas, CanvasComponent{.IsEnabled = false});

    // --- Tab 1: Upgrades & Power-Up Lab ---
    // Paging Controls
    UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "< PREV",
        .OnClick = [this]() {
            m_workshopCurrentPage = (m_workshopCurrentPage + 2) % 3;
            RefreshWorkshopUI();
        },
        .Position = {-260.0f, -112.0f},
        .Size = {130.0f, 34.0f},
        .DefaultColor = Color{35, 40, 55, 255},
        .HoverColor = Color{55, 65, 90, 255},
        .FontId = m_fontId,
        .FontSize = 20.0f
    });

    m_workshopPageText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "PAGE 1 / 3: TACTICAL POWER-UPS",
        .Position = {0.0f, -112.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Colors::Gold
    });

    UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "NEXT >",
        .OnClick = [this]() {
            m_workshopCurrentPage = (m_workshopCurrentPage + 1) % 3;
            RefreshWorkshopUI();
        },
        .Position = {260.0f, -112.0f},
        .Size = {130.0f, 34.0f},
        .DefaultColor = Color{35, 40, 55, 255},
        .HoverColor = Color{55, 65, 90, 255},
        .FontId = m_fontId,
        .FontSize = 20.0f
    });

    // 4 Upgrade Cards
    m_upgradeCards.clear();
    for (int i = 0; i < 4; ++i)
    {
        UpgradeCardWidgets card;
        float x = (i % 2 == 0) ? -230.0f : 230.0f;
        float y = (i < 2) ? -20.0f : 125.0f;

        card.RootPanel = UIFactory::CreatePanel(m_registry, m_workshopUpgradesCanvas, PanelDescriptor{
            .Position = {x, y},
            .Size = {440.0f, 125.0f},
            .Tint = Color{25, 28, 42, 240}
        });

        card.TitleText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "CARD TITLE",
            .Position = {x, y - 40.0f},
            .FontId = m_fontId,
            .FontSize = 24.0f,
            .Tint = Colors::Gold
        });

        card.DescText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "Card Description",
            .Position = {x, y - 18.0f},
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .Tint = Color{200, 215, 235, 255}
        });

        card.CurrentTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "Current Tier",
            .Position = {x, y + 4.0f},
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .Tint = Color{120, 220, 255, 255}
        });

        card.NextTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "Next Tier",
            .Position = {x, y + 24.0f},
            .FontId = m_fontId,
            .FontSize = 16.0f,
            .Tint = Color{255, 200, 100, 255}
        });

        int cardIndex = i;
        card.UpgradeBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
            .Text = "UPGRADE",
            .OnClick = [this, cardIndex]() {
                if (cardIndex < static_cast<int>(m_upgradeCards.size()))
                {
                    PowerUpType t = m_upgradeCards[cardIndex].Type;
                    if (PowerUpManager::Get().CanUpgrade(t))
                    {
                        PowerUpManager::Get().Upgrade(t);
                        if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
                        RefreshWorkshopUI();
                    }
                }
            },
            .Position = {x, y + 48.0f},
            .Size = {240.0f, 30.0f},
            .DefaultColor = Color{35, 75, 45, 255},
            .HoverColor = Color{50, 110, 65, 255},
            .PressedColor = Color{20, 45, 28, 255},
            .FontId = m_fontId,
            .FontSize = 19.0f
        });

        m_upgradeCards.push_back(card);
    }

    // --- Tab 2: Ultimates ---
    UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
        .Text = "SELECT ACTIVE ULTIMATE ABILITY (CHARGE IN-STAGE AND DISCHARGE)",
        .Position = {0.0f, -112.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Colors::Gold
    });

    const UltimateAbilityType ultTypes[3] = {
        UltimateAbilityType::NovaShockwave,
        UltimateAbilityType::ThunderTempest,
        UltimateAbilityType::TimeStasis
    };

    m_ultimateCards.clear();
    for (int i = 0; i < 3; ++i)
    {
        UltimateCardWidgets card;
        card.Type = ultTypes[i];
        float x = -280.0f + static_cast<float>(i) * 280.0f;
        float y = 40.0f;

        card.RootPanel = UIFactory::CreatePanel(m_registry, m_workshopUltimatesCanvas, PanelDescriptor{
            .Position = {x, y},
            .Size = {260.0f, 260.0f},
            .Tint = Color{25, 28, 42, 240}
        });

        card.TitleText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateName(card.Type),
            .Position = {x, y - 90.0f},
            .FontId = m_fontId,
            .FontSize = 24.0f,
            .Tint = Colors::Gold
        });

        card.DescText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateDescription(card.Type),
            .Position = {x, y - 15.0f},
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .Tint = Color{200, 220, 245, 255}
        });

        UltimateAbilityType ultType = card.Type;
        card.EquipBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "EQUIP",
            .OnClick = [this, ultType]() {
                ProfileManager::Get().SetEquippedUltimate(ultType);
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 85.0f);
                RefreshWorkshopUI();
            },
            .Position = {x, y + 95.0f},
            .Size = {180.0f, 40.0f},
            .DefaultColor = Color{40, 60, 95, 255},
            .HoverColor = Color{60, 90, 140, 255},
            .PressedColor = Color{25, 40, 65, 255},
            .FontId = m_fontId,
            .FontSize = 24.0f
        });

        m_ultimateCards.push_back(card);
    }

    // --- Tab 3: Profile & Stats ---
    UIFactory::CreatePanel(m_registry, m_workshopProfileCanvas, PanelDescriptor{
        .Position = {0.0f, 40.0f},
        .Size = {780.0f, 270.0f},
        .Tint = Color{22, 25, 38, 240}
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "PILOT IDENTIFICATION & CALLSIGN",
        .Position = {0.0f, -65.0f},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .Tint = Colors::Gold
    });

    UIFactory::CreateTextInput(m_registry, m_workshopProfileCanvas, TextInputDescriptor{
        .Placeholder = "ENTER PILOT PSEUDO...",
        .OnSubmit = [this](const std::string& text) {
            ProfileManager::Get().SetUsername(text);
            if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
            RefreshWorkshopUI();
        },
        .Position = {0.0f, -20.0f},
        .Size = {360.0f, 44.0f},
        .DefaultColor = Color{35, 40, 55, 255},
        .FocusedColor = Color{55, 65, 90, 255},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .MaxLength = 16
    });

    m_workshopProfileStatsText = UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "CAREER STATS",
        .Position = {0.0f, 45.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{180, 220, 255, 255}
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "EQUIPPED: NEON CYBER PADDLE | PLASMA RIBBON TRAIL",
        .Position = {0.0f, 105.0f},
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .Tint = Color{255, 200, 80, 255}
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "(Advanced hulls, energy shields & audio packs unlockable in upcoming ops)",
        .Position = {0.0f, 135.0f},
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .Tint = Color{140, 150, 175, 255}
    });

    // Back Button (Bottom)
    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            CloseModals();
        },
        .Position = {0.0f, 305.0f},
        .Size = {240.0f, 46.0f},
        .DefaultColor = Color{100, 35, 40, 255},
        .HoverColor = Color{140, 50, 60, 255},
        .PressedColor = Color{70, 25, 30, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });
}

void MenuScene::OpenWorkshopTab(Entity targetCanvas)
{
    for (Entity subCanvas : {m_workshopUpgradesCanvas, m_workshopUltimatesCanvas, m_workshopProfileCanvas})
    {
        if (subCanvas != NULL_ENTITY && m_registry.IsAlive(subCanvas) && m_registry.HasComponent<CanvasComponent>(subCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(subCanvas).IsEnabled = false;
        }
    }

    if (targetCanvas != NULL_ENTITY && m_registry.IsAlive(targetCanvas) && m_registry.HasComponent<CanvasComponent>(targetCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = true;
        m_workshopActiveTabCanvas = targetCanvas;
    }

    RefreshWorkshopUI();
}

void MenuScene::RefreshWorkshopUI()
{
    // Update Bank & Pilot Header
    if (m_workshopBankText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopBankText))
    {
        auto& textComp = m_registry.GetComponent<TextComponent>(m_workshopBankText);
        textComp.Text = "BANK: " + std::to_string(PowerUpManager::Get().GetCoins()) +
                        " CREDITS  |  PILOT: " + ProfileManager::Get().GetUsername();
    }

    // Upgrades Tab Refresh
    const PowerUpType pagePowerUps[3][4] = {
        {PowerUpType::StickyPaddle, PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall},
        {PowerUpType::Storm, PowerUpType::FireBall, PowerUpType::BigBall, PowerUpType::MultiBall},
        {PowerUpType::TempoBall, PowerUpType::ExpandPaddle, PowerUpType::ExtraLife, PowerUpType::ItemRegen}
    };

    const std::string pageTitles[3] = {
        "PAGE 1 / 3: TACTICAL POWER-UPS",
        "PAGE 2 / 3: OFFENSIVE POWER-UPS",
        "PAGE 3 / 3: UTILITY & PERKS"
    };

    if (m_workshopPageText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopPageText))
    {
        m_registry.GetComponent<TextComponent>(m_workshopPageText).Text = pageTitles[m_workshopCurrentPage];
    }

    for (size_t i = 0; i < m_upgradeCards.size() && i < 4; ++i)
    {
        PowerUpType type = pagePowerUps[m_workshopCurrentPage][i];
        m_upgradeCards[i].Type = type;

        int curLvl = PowerUpManager::Get().GetLevel(type);
        int maxLvl = PowerUpManager::Get().GetMaxLevel(type);

        if (m_registry.HasComponent<TextComponent>(m_upgradeCards[i].TitleText))
        {
            m_registry.GetComponent<TextComponent>(m_upgradeCards[i].TitleText).Text =
                PowerUpManager::Get().GetName(type) + "  [LV. " + std::to_string(curLvl) + "/" + std::to_string(maxLvl) + "]";
        }

        if (m_registry.HasComponent<TextComponent>(m_upgradeCards[i].DescText))
        {
            m_registry.GetComponent<TextComponent>(m_upgradeCards[i].DescText).Text =
                PowerUpManager::Get().GetDescription(type);
        }

        if (m_registry.HasComponent<TextComponent>(m_upgradeCards[i].CurrentTierText))
        {
            m_registry.GetComponent<TextComponent>(m_upgradeCards[i].CurrentTierText).Text =
                PowerUpManager::Get().GetCurrentTierInfo(type);
        }

        if (m_registry.HasComponent<TextComponent>(m_upgradeCards[i].NextTierText))
        {
            m_registry.GetComponent<TextComponent>(m_upgradeCards[i].NextTierText).Text =
                PowerUpManager::Get().GetNextTierInfo(type);
        }

        if (m_registry.HasComponent<TextComponent>(m_upgradeCards[i].UpgradeBtn))
        {
            auto& btnText = m_registry.GetComponent<TextComponent>(m_upgradeCards[i].UpgradeBtn);
            if (curLvl >= maxLvl)
            {
                btnText.Text = "MAX LEVEL";
            }
            else
            {
                int cost = PowerUpManager::Get().GetUpgradeCost(type);
                btnText.Text = "UPGRADE (" + std::to_string(cost) + " CR)";
            }
        }
    }

    // Ultimates Tab Refresh
    UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();
    for (auto& card : m_ultimateCards)
    {
        if (m_registry.HasComponent<TextComponent>(card.EquipBtn))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(card.EquipBtn);
            if (card.Type == equipped)
            {
                textComp.Text = "[EQUIPPED]";
                textComp.Tint = Colors::Gold;
            }
            else
            {
                textComp.Text = "EQUIP";
                textComp.Tint = Colors::White;
            }
        }
    }

    // Profile & Stats Refresh
    if (m_workshopProfileStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopProfileStatsText))
    {
        auto& textComp = m_registry.GetComponent<TextComponent>(m_workshopProfileStatsText);
        textComp.Text = "LIFETIME SCORE: " + std::to_string(ProfileManager::Get().GetLifetimeScore()) + " PTS  |  " +
                        "LIFETIME CREDITS: " + std::to_string(ProfileManager::Get().GetLifetimeCredits()) + " CR\n" +
                        "CURRENT BANK: " + std::to_string(PowerUpManager::Get().GetCoins()) + " CR  |  " +
                        "HIGHEST STAGE: STAGE " + std::to_string(m_levelManager.GetHighestClearedLevel());
    }
}
