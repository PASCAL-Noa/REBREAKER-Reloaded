#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/PreviewScene.h"
#include <SFML/Graphics/RenderTexture.hpp>
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
#include "ECS/Components/SpriteComponent.h"
#include "Events/CheatSubmitEvent.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

MenuScene::MenuScene() = default;
MenuScene::~MenuScene() = default;

void MenuScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_bgColor = Color{8, 10, 16, 255};

    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
    m_bounceSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_brickHitSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");
    m_brickDestroySfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_destroy.wav");
    m_lightningSfxId = context.Resources.LoadResource("Resources/audio/sfx/lightning.wav");
    m_conductionSfxId = context.Resources.LoadResource("Resources/audio/sfx/conduction.wav");
    m_fireballBurnSfxId = context.Resources.LoadResource("Resources/audio/sfx/fireball_burn.wav");
    m_fireballExplosionSfxId = context.Resources.LoadResource("Resources/audio/sfx/fireball_explode.wav");
    m_laserSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_laser.wav");
    m_powerupSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup.wav");
    m_powerupBuffSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_buff.wav");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/menu_fx.frag");
    m_previewShaderId = context.Resources.LoadResource("Resources/shaders/crt_preview.frag");
    m_luckyShaderId = context.Resources.LoadResource("Resources/shaders/lucky.frag");
    m_channelSwitchSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_brickTexId = context.Resources.LoadResource("Resources/sprite/brick.png");
    m_brickCrackTexId = context.Resources.LoadResource("Resources/sprite/brick_crack.png");
    m_fireTexId = context.Resources.LoadResource("Resources/sprite/fire.png");
    m_heartTexId = context.Resources.LoadResource("Resources/sprite/heart.png");

    // Initialize persisted audio & render preferences
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 100.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 100.0f);
    context.Audio.SetSfxVolume(sfxVol);
    context.Audio.SetMusicVolume(musicVol);

    std::string savedDevice = PlayerPrefs::GetString("Audio_Device", "");
    if (!savedDevice.empty() && context.Audio.GetCurrentAudioDevice() != savedDevice)
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

    // Initialize Workshop Embedded Preview Simulation
    m_previewTexture = std::make_unique<sf::RenderTexture>();
    sf::ContextSettings rtSettings;
    rtSettings.antiAliasingLevel = 4;
    (void)m_previewTexture->resize(sf::Vector2u(1280, 600), rtSettings);
    m_previewTexture->setSmooth(true);

    m_ultimatePreviewTexture = std::make_unique<sf::RenderTexture>();
    (void)m_ultimatePreviewTexture->resize(sf::Vector2u(1160, 540), rtSettings);
    m_ultimatePreviewTexture->setSmooth(true);

    m_previewScene = std::make_unique<PreviewScene>();
    m_previewScene->OnInit(context);
    m_previewScene->SetEmbeddedMode(true);
    m_lastPreviewPowerUp = m_selectedPowerUp;
    m_lastPreviewLevel = m_inspectedLevel;
    int realLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
    m_previewScene->SetPreviewPowerUp(m_selectedPowerUp, m_inspectedLevel);
    PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLvl);

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

    // Preview scene shortcut (accessible anywhere from menu / workshop)
    if (!m_isRebinding && !m_showDescriptionModal && (context.Input.IsKeyPress(KeyCode::P) || context.Input.IsKeyPress(KeyCode::F1) || context.Input.IsKeyPress(KeyCode::F10)))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
        PreviewScene::SetInitialPowerUp(m_selectedPowerUp);
        context.Scenes.LoadScene<PreviewScene>();
        return;
    }

    // Description Modal Exclusive Interaction Mode
    if (m_showDescriptionModal)
    {
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const float screenCenterX = viewSize.X * 0.5f;
        const float screenCenterY = viewSize.Y * 0.5f;

        // Escape or Gamepad B closes description modal
        if (context.Input.IsMenuCancelTriggered())
        {
            CloseDescriptionModal();
            return;
        }

        // Left mouse click handling
        if (context.Input.IsMouseButtonPress(MouseButton::Left))
        {
            auto [mx, my] = context.Input.GetMousePosition();
            Vector2f mousePos{mx, my};

            // 1. RETOUR button: center {screenCenterX, screenCenterY + 330.0f}, size {360.0f, 60.0f}
            if (std::abs(mousePos.X - screenCenterX) <= 180.0f && std::abs(mousePos.Y - (screenCenterY + 330.0f)) <= 30.0f)
            {
                CloseDescriptionModal();
                return;
            }

            // 2. [X] button: center {screenCenterX + 610.0f, screenCenterY - 370.0f}, size {52.0f, 52.0f}
            if (std::abs(mousePos.X - (screenCenterX + 610.0f)) <= 26.0f && std::abs(mousePos.Y - (screenCenterY - 370.0f)) <= 26.0f)
            {
                CloseDescriptionModal();
                return;
            }

            // 3. Click outside dialog window (dialog size 1360 x 860 centered at screenCenter)
            if (std::abs(mousePos.X - screenCenterX) > 680.0f || std::abs(mousePos.Y - screenCenterY) > 430.0f)
            {
                CloseDescriptionModal();
                return;
            }

            // Clicks inside dialog content are consumed without closing
            return;
        }

        m_previewAnimTime += dt;
        return; // Exclusivity: skip all other scene logic and UISystem::OnUpdate
    }

    // Escape or Gamepad B closes any open sub-panel or modal, or returns to main
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
        if (context.Input.IsKeyPress(KeyCode::Enter) || context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsJoystickButtonPress(0, 7))
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

    // Workshop Upgrades Scrolling (Mouse Wheel & Keyboard)
    if (m_activeModal == m_workshopCanvas && m_workshopActiveTabCanvas == m_workshopUpgradesCanvas && !m_showDescriptionModal)
    {
        float wheel = context.Input.GetMouseWheelDelta();
        if (wheel > 0.05f)
        {
            if (m_workshopScrollRow > 0)
            {
                m_workshopScrollRow--;
                RefreshWorkshopUI();
            }
        }
        else if (wheel < -0.05f)
        {
            if (m_workshopScrollRow < 2)
            {
                m_workshopScrollRow++;
                RefreshWorkshopUI();
            }
        }

        if (context.Input.IsKeyPress(KeyCode::Up) || context.Input.IsKeyPress(KeyCode::PageUp))
        {
            if (m_workshopScrollRow > 0)
            {
                m_workshopScrollRow--;
                RefreshWorkshopUI();
            }
        }
        else if (context.Input.IsKeyPress(KeyCode::Down) || context.Input.IsKeyPress(KeyCode::PageDown))
        {
            if (m_workshopScrollRow < 2)
            {
                m_workshopScrollRow++;
                RefreshWorkshopUI();
            }
        }
    }

    // Workshop Upgrades Live Simulation Autopilot & Physics Update
    if (m_activeModal == m_workshopCanvas && m_workshopActiveTabCanvas == m_workshopUpgradesCanvas)
    {
        if (m_previewScene)
        {
            int realLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
            PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, m_inspectedLevel);

            m_previewScene->OnUpdate(dt, context);

            PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLevel);
        }
    }

    UpdateRetroBackground(dt, context);
    m_previewAnimTime += dt;
    if (m_previewSwitchTimer > 0.0f)
    {
        m_previewSwitchTimer -= dt;
        if (m_previewSwitchTimer < 0.0f)
        {
            m_previewSwitchTimer = 0.0f;
        }
    }
    for (auto& card : m_ultimateCards)
    {
        if (card.SwitchTimer > 0.0f)
        {
            card.SwitchTimer -= dt;
            if (card.SwitchTimer < 0.0f) card.SwitchTimer = 0.0f;
        }
    }

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

    if (m_activeModal == m_workshopCanvas)
    {
        RenderWorkshopCustomVisuals(context);
    }
}

void MenuScene::OnDestroy(GameContext& context)
{
    if (m_previewScene)
    {
        m_previewScene->OnDestroy(context);
        m_previewScene.reset();
    }
    m_previewTexture.reset();
    m_ultimatePreviewTexture.reset();
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

PostProcessParams MenuScene::GetPostProcessParams() const
{
    PostProcessParams params;
    params.ChromaticAberration = 0.005f;
    params.VignetteIntensity = 0.25f;
    params.VignetteColor = Color{0, 0, 0, 255};
    return params;
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
    if (m_activeModal == m_workshopCanvas)
    {
        if (m_previewScene)
        {
            m_previewScene->ResetPreview();
        }
        if (mp_context)
        {
            mp_context->Audio.StopSfx();
        }
    }

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
    for (Entity subCanvas : {m_workshopUpgradesCanvas, m_workshopUltimatesCanvas, m_workshopProfileCanvas, m_workshopDescCanvas})
    {
        if (subCanvas != NULL_ENTITY && m_registry.IsAlive(subCanvas) && m_registry.HasComponent<CanvasComponent>(subCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(subCanvas).IsEnabled = false;
        }
    }
    m_showDescriptionModal = false;
    m_workshopActiveTabCanvas = NULL_ENTITY;
    m_activeModal = NULL_ENTITY;
    if (m_registry.IsAlive(m_mainCanvas) && m_registry.HasComponent<CanvasComponent>(m_mainCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_mainCanvas).IsEnabled = true;
    }
}

void MenuScene::OpenDescriptionModal()
{
    m_showDescriptionModal = true;
    if (m_workshopDescCanvas != NULL_ENTITY && m_registry.IsAlive(m_workshopDescCanvas) && m_registry.HasComponent<CanvasComponent>(m_workshopDescCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_workshopDescCanvas).IsEnabled = true;
    }
    if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
    RefreshWorkshopUI();
}

void MenuScene::CloseDescriptionModal()
{
    m_showDescriptionModal = false;
    if (m_workshopDescCanvas != NULL_ENTITY && m_registry.IsAlive(m_workshopDescCanvas) && m_registry.HasComponent<CanvasComponent>(m_workshopDescCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_workshopDescCanvas).IsEnabled = false;
    }
    if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
    RefreshWorkshopUI();
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

    UIFactory::CreateButton(m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = "RESET SAVE DATA",
        .OnClick = [this]() {
            ResetSaveData();
            if (m_brickDestroySfxId != 0 && mp_context)
            {
                mp_context->Audio.PlaySfx(m_brickDestroySfxId, 85.0f);
            }
        },
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth * 0.5f, 60.0f},
        .DefaultColor = Color{140, 35, 45, 255},
        .HoverColor = Color{195, 50, 65, 255},
        .PressedColor = Color{90, 20, 30, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f
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

static constexpr PowerUpType kAllWorkshopPowerUps[13] = {
    PowerUpType::Storm,
    PowerUpType::FireBall,
    PowerUpType::BigBall,
    PowerUpType::MultiBall,
    PowerUpType::StickyPaddle,
    PowerUpType::BottomBarrier,
    PowerUpType::MagnetPaddle,
    PowerUpType::GhostBall,
    PowerUpType::TempoBall,
    PowerUpType::ExpandPaddle,
    PowerUpType::ShrinkPaddle,
    PowerUpType::ExtraLife,
    PowerUpType::ItemRegen
};

void MenuScene::CreateWorkshopModal(const GameContext& /*context*/)
{
    m_workshopCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopCanvas, CanvasComponent{.IsEnabled = false});

    // 1. Massive Sleek Modal Backdrop Frame (2400 x 1400 px)
    UIFactory::CreatePanel(m_registry, m_workshopCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {2400.0f, 1400.0f},
        .Tint = Color{14, 18, 30, 252},
        .ZOrder = 0
    });

    // Top Header Banner
    UIFactory::CreatePanel(m_registry, m_workshopCanvas, PanelDescriptor{
        .Position = {0.0f, -635.0f},
        .Size = {2360.0f, 84.0f},
        .Tint = Color{20, 26, 44, 255},
        .ZOrder = 1
    });

    // Left Player Name in Header (without "PLAYER: " prefix)
    m_workshopPlayerText = UIFactory::CreateText(m_registry, m_workshopCanvas, TextDescriptor{
        .Text = ProfileManager::Get().GetUsername(),
        .Position = {-900.0f, -635.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{80, 220, 255, 255},
        .ZOrder = 2
    });

    // Center Nav Tabs (Clash Royale Style Switchers)
    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "POWER-UPS",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            OpenWorkshopTab(m_workshopUpgradesCanvas);
        },
        .Position = {-240.0f, -635.0f},
        .Size = {260.0f, 54.0f},
        .DefaultColor = Color{35, 50, 85, 255},
        .HoverColor = Color{55, 80, 135, 255},
        .PressedColor = Color{25, 35, 60, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });

    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "ULTIMATES",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            OpenWorkshopTab(m_workshopUltimatesCanvas);
        },
        .Position = {30.0f, -635.0f},
        .Size = {240.0f, 54.0f},
        .DefaultColor = Color{35, 50, 85, 255},
        .HoverColor = Color{55, 80, 135, 255},
        .PressedColor = Color{25, 35, 60, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });

    UIFactory::CreateButton(m_registry, m_workshopCanvas, ButtonDescriptor{
        .Text = "PROFILE",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            OpenWorkshopTab(m_workshopProfileCanvas);
        },
        .Position = {310.0f, -635.0f},
        .Size = {270.0f, 54.0f},
        .DefaultColor = Color{35, 50, 85, 255},
        .HoverColor = Color{55, 80, 135, 255},
        .PressedColor = Color{25, 35, 60, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });

    // Right Bank Display (Golden Pill)
    m_workshopBankText = UIFactory::CreateText(m_registry, m_workshopCanvas, TextDescriptor{
        .Text = "BANK: 0 CREDITS",
        .Position = {800.0f, -635.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::Gold,
        .ZOrder = 2
    });

    // Sub-canvases
    m_workshopUpgradesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopUpgradesCanvas, CanvasComponent{.IsEnabled = false});

    m_workshopUltimatesCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopUltimatesCanvas, CanvasComponent{.IsEnabled = false});

    m_workshopProfileCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopProfileCanvas, CanvasComponent{.IsEnabled = false});

    // =========================================================================
    // --- TAB 1: UPGRADES LAB (CLASH ROYALE CARD GRID + INSPECTOR PREVIEW) ---
    // =========================================================================

    // 9 Visible Cards in a 3x3 CSS-style Grid on the Left
    m_upgradeCards.clear();
    const float cardColX[3] = {-930.0f, -670.0f, -410.0f};
    const float cardRowY[3] = {-350.0f, -50.0f, 250.0f};

    for (int i = 0; i < 9; ++i)
    {
        UpgradeCardWidgets card;
        float x = cardColX[i % 3];
        float y = cardRowY[i / 3];
        int slotIndex = i;

        card.CardBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
            .Text = "",
            .OnClick = [this, slotIndex]() {
                if (m_showDescriptionModal) return;
                int pwrIdx = m_workshopScrollRow * 3 + slotIndex;
                if (pwrIdx >= 0 && pwrIdx < 13)
                {
                    PowerUpType newType = kAllWorkshopPowerUps[pwrIdx];
                    bool changed = (m_selectedPowerUp != newType);
                    m_selectedPowerUp = newType;
                    m_inspectedLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
                    m_workshopPreviewLevel = m_inspectedLevel;
                    m_previewAnimTime = 0.0f;
                    m_simAudioTriggerMask = 0;
                    m_simLastLoop = -1;
                    if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
                    if (!changed && m_previewScene)
                    {
                        m_previewScene->ResetPreview();
                        TriggerPreviewChannelSwitch();
                    }
                    RefreshWorkshopUI();
                }
            },
            .Position = {x, y},
            .Size = {240.0f, 280.0f},
            .DefaultColor = Color{24, 30, 48, 250},
            .HoverColor = Color{35, 48, 78, 255},
            .PressedColor = Color{18, 22, 36, 255},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .ZOrder = 2
        });

        card.LevelText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "NIV. 1",
            .Position = {x, y + 84.0f},
            .FontId = m_fontId,
            .FontSize = 18.0f,
            .Tint = Colors::Gold,
            .ZOrder = 3
        });

        card.TitleText = NULL_ENTITY;
        card.CategoryText = NULL_ENTITY;
        card.CostText = NULL_ENTITY;

        m_upgradeCards.push_back(card);
    }

    // Scrollbar Buttons (Up / Down)
    m_workshopScrollUpBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "^",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            if (m_workshopScrollRow > 0)
            {
                m_workshopScrollRow--;
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 70.0f);
                RefreshWorkshopUI();
            }
        },
        .Position = {-250.0f, -440.0f},
        .Size = {44.0f, 44.0f},
        .DefaultColor = Color{35, 45, 65, 255},
        .HoverColor = Color{55, 75, 110, 255},
        .PressedColor = Color{25, 35, 50, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .ZOrder = 3
    });

    m_workshopScrollDownBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "v",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            if (m_workshopScrollRow < 2)
            {
                m_workshopScrollRow++;
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 70.0f);
                RefreshWorkshopUI();
            }
        },
        .Position = {-250.0f, 340.0f},
        .Size = {44.0f, 44.0f},
        .DefaultColor = Color{35, 45, 65, 255},
        .HoverColor = Color{55, 75, 110, 255},
        .PressedColor = Color{25, 35, 50, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .ZOrder = 3
    });

    // Back to Menu Button
    UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "BACK TO MENU",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            CloseModals();
        },
        .Position = {-670.0f, 490.0f},
        .Size = {360.0f, 54.0f},
        .DefaultColor = Color{120, 35, 45, 255},
        .HoverColor = Color{165, 50, 65, 255},
        .PressedColor = Color{85, 25, 35, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });

    // Right Column Inspector Panel (Frame neatly enclosing preview and stats controls)
    UIFactory::CreatePanel(m_registry, m_workshopUpgradesCanvas, PanelDescriptor{
        .Position = {600.0f, -35.0f},
        .Size = {1040.0f, 900.0f},
        .Tint = Color{18, 22, 36, 245},
        .ZOrder = 1
    });

    // Inspector Title at the Top (Clean, aligned with cards)
    m_inspectorTitleText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "POWER-UP NAME",
        .Position = {600.0f, -440.0f},
        .FontId = m_fontId,
        .FontSize = 38.0f,
        .Tint = Colors::Gold,
        .ZOrder = 3
    });

    m_inspectorCategoryText = NULL_ENTITY;

    // Level Selector / Inspection Buttons (LVL. 1 to LVL. 5)
    m_levelPipButtons.clear();
    constexpr float pipBtnW = 96.0f;
    constexpr float pipBtnH = 34.0f;
    constexpr float pipBtnGap = 12.0f;
    float startPipX = 600.0f - (5.0f * (pipBtnW + pipBtnGap) - pipBtnGap) * 0.5f + (pipBtnW * 0.5f);
    float pipBtnY = -65.0f;

    for (int p = 0; p < 5; ++p)
    {
        float bx = startPipX + static_cast<float>(p) * (pipBtnW + pipBtnGap);
        int lvl = p + 1;
        Entity pipBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
            .Text = "LVL. " + std::to_string(lvl),
            .OnClick = [this, lvl]() {
                if (m_showDescriptionModal) return;
                bool levelChanged = (m_inspectedLevel != lvl);
                m_inspectedLevel = lvl;
                m_workshopPreviewLevel = lvl;
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
                if (!levelChanged && m_previewScene)
                {
                    m_previewScene->ResetPreview();
                    TriggerPreviewChannelSwitch();
                }
                RefreshWorkshopUI();
            },
            .Position = {bx, pipBtnY},
            .Size = {pipBtnW, pipBtnH},
            .DefaultColor = Color{35, 45, 65, 220},
            .HoverColor = Color{55, 75, 110, 240},
            .PressedColor = Color{25, 35, 50, 255},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .ZOrder = 3
        });
        m_levelPipButtons.push_back(pipBtn);
    }

    // Description & Stats Toggle Button
    m_inspectorDescBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "DESCRIPTION & STATS",
        .OnClick = [this]() {
            if (m_showDescriptionModal)
            {
                CloseDescriptionModal();
            }
            else
            {
                OpenDescriptionModal();
            }
        },
        .Position = {600.0f, -10.0f},
        .Size = {420.0f, 40.0f},
        .DefaultColor = Color{30, 60, 95, 240},
        .HoverColor = Color{45, 90, 145, 255},
        .PressedColor = Color{20, 45, 75, 255},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .ZOrder = 3
    });

    // Stat & Description Info Box (Compacted to align with cards)
    UIFactory::CreatePanel(m_registry, m_workshopUpgradesCanvas, PanelDescriptor{
        .Position = {600.0f, 150.0f},
        .Size = {840.0f, 240.0f},
        .Tint = Color{24, 30, 48, 240},
        .ZOrder = 2
    });

    m_inspectorDescText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "Description.",
        .Position = {600.0f, 65.0f},
        .FontId = m_fontId,
        .FontSize = 21.0f,
        .Tint = Color{225, 235, 250, 255},
        .MaxWidth = 780.0f,
        .ZOrder = 3
    });

    m_inspectorCurTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "CURRENT: ...",
        .Position = {600.0f, 125.0f},
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .Tint = Color{100, 220, 255, 255},
        .MaxWidth = 800.0f,
        .ZOrder = 3
    });

    m_inspectorNextTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "VIEWING: ...",
        .Position = {600.0f, 180.0f},
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .Tint = Color{255, 215, 80, 255},
        .MaxWidth = 800.0f,
        .ZOrder = 3
    });

    m_inspectorTierCostText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "COST: ...",
        .Position = {600.0f, 235.0f},
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .Tint = Color{160, 210, 255, 255},
        .MaxWidth = 800.0f,
        .ZOrder = 3
    });

    // Big Upgrade Action Button (Bottom aligned with 3x3 cards grid)
    m_inspectorUpgradeBtn = UIFactory::CreateButton(m_registry, m_workshopUpgradesCanvas, ButtonDescriptor{
        .Text = "UPGRADE",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            PowerUpType t = m_selectedPowerUp;
            if (PowerUpManager::Get().CanUpgrade(t))
            {
                PowerUpManager::Get().Upgrade(t);
                m_inspectedLevel = PowerUpManager::Get().GetLevel(t);
                m_workshopPreviewLevel = m_inspectedLevel;
                if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
                RefreshWorkshopUI();
            }
        },
        .Position = {600.0f, 345.0f},
        .Size = {640.0f, 70.0f},
        .DefaultColor = Color{35, 145, 60, 255},
        .HoverColor = Color{50, 190, 80, 255},
        .PressedColor = Color{22, 95, 40, 255},
        .FontId = m_fontId,
        .FontSize = 34.0f,
        .ZOrder = 3
    });

    // Description Modal Canvas Placeholder
    m_workshopDescCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopDescCanvas, CanvasComponent{.IsEnabled = false});
    m_workshopDescBackdrop = NULL_ENTITY;
    m_descModalCloseBtn = NULL_ENTITY;
    m_descModalXBtn = NULL_ENTITY;
    m_descModalTitleText = NULL_ENTITY;
    m_descModalCategoryText = NULL_ENTITY;
    m_descModalDescText = NULL_ENTITY;
    for (int lvl = 0; lvl < 5; ++lvl)
    {
        m_descModalTierPanels[lvl] = NULL_ENTITY;
        m_descModalTierTags[lvl] = NULL_ENTITY;
        m_descModalTierTexts[lvl] = NULL_ENTITY;
    }

    // =========================================================================
    // --- TAB 2: FLAGSHIP ULTIMATES HANGAR (3 ALL-IN-ONE COLUMNS) ---
    // =========================================================================
    UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
        .Text = "FLAGSHIP ULTIMATE ABILITIES (SMASH BRICKS TO CHARGE AND UNLEASH IN COMBAT)",
        .Position = {0.0f, -540.0f},
        .FontId = m_fontId,
        .FontSize = 30.0f,
        .Tint = Colors::Gold,
        .ZOrder = 2
    });

    const UltimateAbilityType ultTypes[3] = {
        UltimateAbilityType::NovaShockwave,
        UltimateAbilityType::ThunderTempest,
        UltimateAbilityType::TimeStasis
    };

    m_ultimateCards.clear();
    const float ultColX[3] = {-720.0f, 0.0f, 720.0f};

    for (int i = 0; i < 3; ++i)
    {
        UltimateCardWidgets card;
        card.Type = ultTypes[i];
        card.InspectedLevel = ProfileManager::Get().GetUltimateLevel(card.Type);
        card.SwitchTimer = 0.0f;
        float x = ultColX[i];
        float y = -35.0f;
        int cardIdx = i;

        card.RootPanel = UIFactory::CreatePanel(m_registry, m_workshopUltimatesCanvas, PanelDescriptor{
            .Position = {x, y},
            .Size = {680.0f, 960.0f},
            .Tint = Color{20, 26, 42, 250},
            .ZOrder = 1
        });

        // 1. Title & Role Header
        card.TitleText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateName(card.Type),
            .Position = {x, -455.0f},
            .FontId = m_fontId,
            .FontSize = 32.0f,
            .Tint = Colors::Gold,
            .ZOrder = 3
        });

        card.CategoryText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateCategory(card.Type),
            .Position = {x, -420.0f},
            .FontId = m_fontId,
            .FontSize = 18.0f,
            .Tint = Color{120, 205, 255, 230},
            .ZOrder = 3
        });

        // 2. Interactive Level Inspection Buttons (NIV. 1 to NIV. 5)
        constexpr float ultPipW = 96.0f;
        constexpr float ultPipH = 34.0f;
        constexpr float ultPipGap = 12.0f;
        float startUltPipX = x - (5.0f * (ultPipW + ultPipGap) - ultPipGap) * 0.5f + (ultPipW * 0.5f);
        float ultPipY = -95.0f;

        for (int p = 0; p < 5; ++p)
        {
            float bx = startUltPipX + static_cast<float>(p) * (ultPipW + ultPipGap);
            int lvl = p + 1;
            Entity pipBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
                .Text = "LVL. " + std::to_string(lvl),
                .OnClick = [this, cardIdx, lvl]() {
                    if (m_showDescriptionModal) return;
                    m_ultimateCards[cardIdx].InspectedLevel = lvl;
                    m_ultimateCards[cardIdx].SwitchTimer = 0.38f;
                    if (m_channelSwitchSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_channelSwitchSfxId, 85.0f, 1.25f);
                    else if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 80.0f);
                    RefreshWorkshopUI();
                },
                .Position = {bx, ultPipY},
                .Size = {ultPipW, ultPipH},
                .DefaultColor = Color{35, 45, 65, 220},
                .HoverColor = Color{55, 75, 110, 240},
                .PressedColor = Color{25, 35, 50, 255},
                .FontId = m_fontId,
                .FontSize = 20.0f,
                .ZOrder = 3
            });
            card.LevelPipButtons[p] = pipBtn;
        }

        // 3. Stats & Description Info Box
        UIFactory::CreatePanel(m_registry, m_workshopUltimatesCanvas, PanelDescriptor{
            .Position = {x, 40.0f},
            .Size = {580.0f, 200.0f},
            .Tint = Color{24, 30, 48, 240},
            .ZOrder = 2
        });

        card.DescText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateDescription(card.Type),
            .Position = {x, -25.0f},
            .FontId = m_fontId,
            .FontSize = 18.0f,
            .Tint = Color{215, 230, 250, 255},
            .MaxWidth = 540.0f,
            .ZOrder = 3
        });

        card.CurTierText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "CURRENT LEVEL: ...",
            .Position = {x, 30.0f},
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .Tint = Color{100, 220, 255, 255},
            .MaxWidth = 540.0f,
            .ZOrder = 3
        });

        card.NextTierText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "VIEWING LEVEL: ...",
            .Position = {x, 70.0f},
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .Tint = Colors::Gold,
            .MaxWidth = 540.0f,
            .ZOrder = 3
        });

        card.CostText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "NEXT TIER COST: ...",
            .Position = {x, 110.0f},
            .FontId = m_fontId,
            .FontSize = 18.0f,
            .Tint = Color{80, 240, 140, 255},
            .MaxWidth = 540.0f,
            .ZOrder = 3
        });

        // 4. Big Upgrade Action Button
        UltimateAbilityType ultType = card.Type;
        card.UpgradeBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "UPGRADE",
            .OnClick = [this, cardIdx, ultType]() {
                if (m_showDescriptionModal) return;
                if (ProfileManager::Get().CanUpgradeUltimate(ultType))
                {
                    ProfileManager::Get().UpgradeUltimate(ultType);
                    m_ultimateCards[cardIdx].InspectedLevel = ProfileManager::Get().GetUltimateLevel(ultType);
                    m_ultimateCards[cardIdx].SwitchTimer = 0.38f;
                    if (m_channelSwitchSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_channelSwitchSfxId, 85.0f, 1.25f);
                    if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
                    RefreshWorkshopUI();
                }
            },
            .Position = {x, 185.0f},
            .Size = {580.0f, 54.0f},
            .DefaultColor = Color{35, 145, 60, 255},
            .HoverColor = Color{50, 195, 85, 255},
            .PressedColor = Color{22, 95, 40, 255},
            .FontId = m_fontId,
            .FontSize = 26.0f,
            .ZOrder = 3
        });

        // 5. Equip Action Button
        card.EquipBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "EQUIP ULTIMATE",
            .OnClick = [this, ultType]() {
                if (m_showDescriptionModal) return;
                ProfileManager::Get().SetEquippedUltimate(ultType);
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 85.0f);
                RefreshWorkshopUI();
            },
            .Position = {x, 255.0f},
            .Size = {580.0f, 54.0f},
            .DefaultColor = Color{40, 70, 130, 255},
            .HoverColor = Color{60, 105, 195, 255},
            .PressedColor = Color{25, 45, 85, 255},
            .FontId = m_fontId,
            .FontSize = 26.0f,
            .ZOrder = 3
        });

        m_ultimateCards.push_back(card);
    }

    // Back to Menu Button (Ultimates Tab)
    UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
        .Text = "BACK TO MENU",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            CloseModals();
        },
        .Position = {0.0f, 520.0f},
        .Size = {360.0f, 54.0f},
        .DefaultColor = Color{120, 35, 45, 255},
        .HoverColor = Color{165, 50, 65, 255},
        .PressedColor = Color{85, 25, 35, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });

    // =========================================================================
    // --- TAB 3: PLAYER PROFILE & CAREER RECORD ---
    // =========================================================================
    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "PLAYER PROFILE & CAREER RECORD",
        .Position = {0.0f, -540.0f},
        .FontId = m_fontId,
        .FontSize = 30.0f,
        .Tint = Colors::Gold,
        .ZOrder = 2
    });

    // Left Card: Player Identity
    UIFactory::CreatePanel(m_registry, m_workshopProfileCanvas, PanelDescriptor{
        .Position = {-580.0f, -30.0f},
        .Size = {1060.0f, 960.0f},
        .Tint = Color{20, 26, 44, 250},
        .ZOrder = 1
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "PLAYER PROFILE DOSSIER",
        .Position = {-580.0f, -440.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Colors::Gold,
        .ZOrder = 3
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "ENTER NICKNAME (PRE-FILLS LEADERBOARDS AUTOMATICALLY):",
        .Position = {-580.0f, -390.0f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Color{80, 220, 255, 255},
        .ZOrder = 3
    });

    UIFactory::CreateTextInput(m_registry, m_workshopProfileCanvas, TextInputDescriptor{
        .Placeholder = "ENTER NICKNAME...",
        .OnSubmit = [this](const std::string& text) {
            ProfileManager::Get().SetUsername(text);
            if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
            RefreshWorkshopUI();
        },
        .Position = {-580.0f, -320.0f},
        .Size = {540.0f, 64.0f},
        .DefaultColor = Color{35, 45, 68, 255},
        .FocusedColor = Color{55, 75, 115, 255},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .MaxLength = 16,
        .ZOrder = 3
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "VESSEL EQUIPPED: RETRO CYBER-PADDLE MK-II",
        .Position = {-580.0f, -210.0f},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .Tint = Color{255, 215, 80, 255},
        .ZOrder = 3
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "BALL CORE: HIGH-VELOCITY PLASMA ORB",
        .Position = {-580.0f, -150.0f},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .Tint = Color{80, 220, 255, 255},
        .ZOrder = 3
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "TRAIL FX: NEON CHROMATIC STREAM",
        .Position = {-580.0f, -90.0f},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .Tint = Color{255, 120, 180, 255},
        .ZOrder = 3
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "Cosmetic hulls, custom particle ribbons & sound packs unlock in upcoming operations.",
        .Position = {-580.0f, 50.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{150, 165, 195, 255},
        .ZOrder = 3
    });

    // Right Card: Career Log
    UIFactory::CreatePanel(m_registry, m_workshopProfileCanvas, PanelDescriptor{
        .Position = {580.0f, -30.0f},
        .Size = {1060.0f, 960.0f},
        .Tint = Color{20, 26, 44, 250},
        .ZOrder = 1
    });

    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "CAREER METRICS & FLIGHT LOG",
        .Position = {580.0f, -440.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Colors::Gold,
        .ZOrder = 3
    });

    m_workshopProfileStatsText = UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "CAREER STATS",
        .Position = {580.0f, -160.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::White,
        .ZOrder = 3
    });

    // Reset Save Data Warning & Button (Profile Tab)
    UIFactory::CreateText(m_registry, m_workshopProfileCanvas, TextDescriptor{
        .Text = "RESET ALL CREDITS, UPGRADES, ULTIMATES AND PROGRESSION",
        .Position = {580.0f, 215.0f},
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .Tint = Color{255, 120, 120, 220},
        .ZOrder = 3
    });

    m_resetSaveBtn = UIFactory::CreateButton(m_registry, m_workshopProfileCanvas, ButtonDescriptor{
        .Text = "RESET SAVE DATA",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            ResetSaveData();
            if (m_brickDestroySfxId != 0 && mp_context)
            {
                mp_context->Audio.PlaySfx(m_brickDestroySfxId, 85.0f);
            }
            if (m_resetSaveBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_resetSaveBtn))
            {
                m_registry.GetComponent<TextComponent>(m_resetSaveBtn).Text = "[ SAVE DATA RESET ! ]";
                m_registry.GetComponent<TextComponent>(m_resetSaveBtn).Tint = Colors::Gold;
            }
        },
        .Position = {580.0f, 280.0f},
        .Size = {540.0f, 60.0f},
        .DefaultColor = Color{140, 35, 45, 255},
        .HoverColor = Color{195, 50, 65, 255},
        .PressedColor = Color{90, 20, 30, 255},
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .ZOrder = 3
    });

    // Back to Menu Button (Profile Tab)
    UIFactory::CreateButton(m_registry, m_workshopProfileCanvas, ButtonDescriptor{
        .Text = "BACK TO MENU",
        .OnClick = [this]() {
            if (m_showDescriptionModal) return;
            CloseModals();
        },
        .Position = {0.0f, 520.0f},
        .Size = {360.0f, 54.0f},
        .DefaultColor = Color{120, 35, 45, 255},
        .HoverColor = Color{165, 50, 65, 255},
        .PressedColor = Color{85, 25, 35, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .ZOrder = 2
    });
}

void MenuScene::OpenWorkshopTab(Entity targetCanvas)
{
    if (targetCanvas != m_workshopUpgradesCanvas)
    {
        if (m_previewScene)
        {
            m_previewScene->ResetPreview();
        }
        if (mp_context)
        {
            mp_context->Audio.StopSfx();
        }
    }

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

    m_previewAnimTime = 0.0f;
    m_simAudioTriggerMask = 0;
    m_simLastLoop = -1;
    m_inspectedLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
    m_workshopPreviewLevel = m_inspectedLevel;

    if (targetCanvas == m_workshopUpgradesCanvas)
    {
        TriggerPreviewChannelSwitch();
    }
    else if (targetCanvas == m_workshopUltimatesCanvas)
    {
        for (auto& card : m_ultimateCards)
        {
            card.SwitchTimer = 0.38f;
        }
    }

    if (m_resetSaveBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_resetSaveBtn))
    {
        m_registry.GetComponent<TextComponent>(m_resetSaveBtn).Text = "RESET SAVE DATA";
        m_registry.GetComponent<TextComponent>(m_resetSaveBtn).Tint = Colors::White;
    }

    RefreshWorkshopUI();
}

void MenuScene::ResetSaveData()
{
    PowerUpManager::Get().ResetToDefaults();
    ProfileManager::Get().ResetToDefaults();
    m_levelManager.ResetProgression();
    m_leaderboard.ResetToDefaults();
    PlayerPrefs::Save();
    m_inspectedLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
    m_workshopPreviewLevel = m_inspectedLevel;
    RefreshWorkshopUI();
}

void MenuScene::RefreshWorkshopUI()
{
    // 1. Update Header Call & Bank
    if (m_workshopBankText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopBankText))
    {
        m_registry.GetComponent<TextComponent>(m_workshopBankText).Text =
            "[C] " + std::to_string(PowerUpManager::Get().GetCoins()) + " CREDITS";
    }

    if (m_workshopPlayerText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopPlayerText))
    {
        m_registry.GetComponent<TextComponent>(m_workshopPlayerText).Text =
            ProfileManager::Get().GetUsername();
    }

    // 2. Upgrades Tab Refresh (3x3 Clash Royale Grid)
    for (size_t slot = 0; slot < m_upgradeCards.size() && slot < 9; ++slot)
    {
        int pwrIdx = m_workshopScrollRow * 3 + static_cast<int>(slot);
        if (pwrIdx < 13)
        {
            PowerUpType type = kAllWorkshopPowerUps[pwrIdx];
            m_upgradeCards[slot].Type = type;

            if (m_registry.HasComponent<RectTransform>(m_upgradeCards[slot].CardBtn))
            {
                m_registry.GetComponent<RectTransform>(m_upgradeCards[slot].CardBtn).IsActive = true;
            }

            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].LevelText))
            {
                // Blank out text so the 5 gold squares fill the rectangle with no overlapping text
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].LevelText).Text = "";
            }
        }
        else
        {
            if (m_registry.HasComponent<RectTransform>(m_upgradeCards[slot].CardBtn))
            {
                m_registry.GetComponent<RectTransform>(m_upgradeCards[slot].CardBtn).IsActive = false;
            }
            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].LevelText))
            {
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].LevelText).Text = "";
            }
        }
    }

    // Scrollbar Buttons state
    if (m_workshopScrollUpBtn != NULL_ENTITY && m_registry.HasComponent<ButtonComponent>(m_workshopScrollUpBtn))
    {
        auto& btn = m_registry.GetComponent<ButtonComponent>(m_workshopScrollUpBtn);
        btn.DefaultColor = (m_workshopScrollRow > 0) ? Color{45, 65, 105, 255} : Color{25, 32, 48, 160};
    }
    if (m_workshopScrollDownBtn != NULL_ENTITY && m_registry.HasComponent<ButtonComponent>(m_workshopScrollDownBtn))
    {
        auto& btn = m_registry.GetComponent<ButtonComponent>(m_workshopScrollDownBtn);
        btn.DefaultColor = (m_workshopScrollRow < 2) ? Color{45, 65, 105, 255} : Color{25, 32, 48, 160};
    }

    // Refresh Inspector for Currently Selected Power-Up (Right Column)
    int selCurLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
    int selMaxLvl = PowerUpManager::Get().GetMaxLevel(m_selectedPowerUp);
    if (m_inspectedLevel < 1) m_inspectedLevel = 1;
    if (m_inspectedLevel > selMaxLvl) m_inspectedLevel = selMaxLvl;

    if (m_inspectorTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorTitleText))
    {
        m_registry.GetComponent<TextComponent>(m_inspectorTitleText).Text =
            PowerUpManager::Get().GetName(m_selectedPowerUp);
    }

    if (m_inspectorCategoryText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorCategoryText))
    {
        m_registry.GetComponent<TextComponent>(m_inspectorCategoryText).Text = "";
    }

    if (m_inspectorDescText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorDescText))
    {
        m_registry.GetComponent<TextComponent>(m_inspectorDescText).Text =
            PowerUpManager::Get().GetDescription(m_selectedPowerUp);
    }

    if (m_inspectorCurTierText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorCurTierText))
    {
        m_registry.GetComponent<TextComponent>(m_inspectorCurTierText).Text =
            "CURRENT LEVEL (" + std::to_string(selCurLvl) + "): " + PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, selCurLvl);
    }

    if (m_inspectorNextTierText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorNextTierText))
    {
        std::string status = (m_inspectedLevel == selCurLvl) ? " [CURRENT]" : ((m_inspectedLevel < selCurLvl) ? " [UNLOCKED]" : " [LOCKED]");
        m_registry.GetComponent<TextComponent>(m_inspectorNextTierText).Text =
            "VIEWING LEVEL (" + std::to_string(m_inspectedLevel) + status + "): " + PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, m_inspectedLevel);
        m_registry.GetComponent<TextComponent>(m_inspectorNextTierText).Tint = (m_inspectedLevel <= selCurLvl) ? Colors::Gold : Color{255, 175, 100, 255};
    }

    if (m_inspectorTierCostText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorTierCostText))
    {
        if (selCurLvl >= selMaxLvl)
        {
            m_registry.GetComponent<TextComponent>(m_inspectorTierCostText).Text = "ALL TIERS UNLOCKED (MAX LEVEL)";
            m_registry.GetComponent<TextComponent>(m_inspectorTierCostText).Tint = Colors::Gold;
        }
        else
        {
            int cost = PowerUpManager::Get().GetUpgradeCost(m_selectedPowerUp);
            m_registry.GetComponent<TextComponent>(m_inspectorTierCostText).Text =
                "NEXT TIER COST (LVL. " + std::to_string(selCurLvl + 1) + "): " + std::to_string(cost) + " CREDITS";
            m_registry.GetComponent<TextComponent>(m_inspectorTierCostText).Tint = (PowerUpManager::Get().GetCoins() >= cost) ? Color{80, 240, 140, 255} : Color{255, 140, 100, 255};
        }
    }

    // Refresh Big Upgrade Button
    if (m_inspectorUpgradeBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorUpgradeBtn))
    {
        auto& btnText = m_registry.GetComponent<TextComponent>(m_inspectorUpgradeBtn);
        if (m_registry.HasComponent<ButtonComponent>(m_inspectorUpgradeBtn))
        {
            auto& btnComp = m_registry.GetComponent<ButtonComponent>(m_inspectorUpgradeBtn);
            if (selCurLvl >= selMaxLvl)
            {
                btnText.Text = "[ MAXIMUM LEVEL REACHED ]";
                btnText.Tint = Colors::Gold;
                btnComp.DefaultColor = Color{45, 50, 70, 255};
                btnComp.HoverColor = Color{45, 50, 70, 255};
            }
            else
            {
                int cost = PowerUpManager::Get().GetUpgradeCost(m_selectedPowerUp);
                if (PowerUpManager::Get().GetCoins() >= cost)
                {
                    btnText.Text = "UPGRADE  [" + std::to_string(cost) + " CREDITS]";
                    btnText.Tint = Colors::White;
                    btnComp.DefaultColor = Color{35, 145, 60, 255};
                    btnComp.HoverColor = Color{50, 195, 85, 255};
                }
                else
                {
                    btnText.Text = "INSUFFICIENT CREDITS  (" + std::to_string(cost) + " CR)";
                    btnText.Tint = Color{255, 180, 120, 255};
                    btnComp.DefaultColor = Color{55, 45, 40, 255};
                    btnComp.HoverColor = Color{70, 55, 50, 255};
                }
            }
        }
    }

    // Refresh interactive Level Pip buttons
    for (int p = 0; p < 5 && p < static_cast<int>(m_levelPipButtons.size()); ++p)
    {
        Entity btn = m_levelPipButtons[p];
        int lvl = p + 1;
        bool isOwned = (lvl <= selCurLvl);
        bool isInspected = (lvl == m_inspectedLevel);

        if (m_registry.HasComponent<ButtonComponent>(btn))
        {
            auto& bComp = m_registry.GetComponent<ButtonComponent>(btn);
            if (isInspected)
            {
                bComp.DefaultColor = Color{0, 160, 220, 255};
                bComp.HoverColor = Color{20, 190, 255, 255};
            }
            else if (isOwned)
            {
                bComp.DefaultColor = Color{25, 80, 120, 220};
                bComp.HoverColor = Color{35, 110, 165, 240};
            }
            else
            {
                bComp.DefaultColor = Color{30, 38, 54, 200};
                bComp.HoverColor = Color{45, 58, 80, 220};
            }
        }
        if (m_registry.HasComponent<TextComponent>(btn))
        {
            auto& tComp = m_registry.GetComponent<TextComponent>(btn);
            tComp.Text = (isInspected ? "[ LVL. " : "LVL. ") + std::to_string(lvl) + (isInspected ? " ]" : "");
            tComp.Tint = isInspected ? Colors::Gold : (isOwned ? Color{100, 220, 255, 255} : Color{140, 155, 175, 200});
        }
    }

    // 3. Ultimates Tab Refresh (All 3 Columns)
    UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();
    for (auto& card : m_ultimateCards)
    {
        int curLvl = ProfileManager::Get().GetUltimateLevel(card.Type);
        int maxLvl = ProfileManager::Get().GetUltimateMaxLevel(card.Type);
        if (card.InspectedLevel < 1) card.InspectedLevel = 1;
        if (card.InspectedLevel > maxLvl) card.InspectedLevel = maxLvl;

        // Pip buttons update
        for (int p = 0; p < 5; ++p)
        {
            Entity btn = card.LevelPipButtons[p];
            int lvl = p + 1;
            bool isOwned = (lvl <= curLvl);
            bool isInspected = (lvl == card.InspectedLevel);

            if (m_registry.HasComponent<ButtonComponent>(btn))
            {
                auto& bComp = m_registry.GetComponent<ButtonComponent>(btn);
                if (isInspected)
                {
                    bComp.DefaultColor = Color{0, 160, 220, 255};
                    bComp.HoverColor = Color{20, 190, 255, 255};
                }
                else if (isOwned)
                {
                    bComp.DefaultColor = Color{25, 80, 120, 220};
                    bComp.HoverColor = Color{35, 110, 165, 240};
                }
                else
                {
                    bComp.DefaultColor = Color{30, 38, 54, 200};
                    bComp.HoverColor = Color{45, 58, 80, 220};
                }
            }
            if (m_registry.HasComponent<TextComponent>(btn))
            {
                auto& tComp = m_registry.GetComponent<TextComponent>(btn);
                tComp.Text = (isInspected ? "[ LVL. " : "LVL. ") + std::to_string(lvl) + (isInspected ? " ]" : "");
                tComp.Tint = isInspected ? Colors::Gold : (isOwned ? Color{100, 220, 255, 255} : Color{140, 155, 175, 200});
            }
        }

        // CurTierText
        if (m_registry.HasComponent<TextComponent>(card.CurTierText))
        {
            m_registry.GetComponent<TextComponent>(card.CurTierText).Text =
                "CURRENT LEVEL (" + std::to_string(curLvl) + "): " + ProfileManager::GetUltimateTierInfo(card.Type, curLvl);
        }

        // NextTierText
        if (m_registry.HasComponent<TextComponent>(card.NextTierText))
        {
            std::string status = (card.InspectedLevel == curLvl) ? " [CURRENT]" : ((card.InspectedLevel < curLvl) ? " [UNLOCKED]" : " [LOCKED]");
            auto& nextT = m_registry.GetComponent<TextComponent>(card.NextTierText);
            nextT.Text = "VIEWING LEVEL (" + std::to_string(card.InspectedLevel) + status + "): " + ProfileManager::GetUltimateTierInfo(card.Type, card.InspectedLevel);
            nextT.Tint = (card.InspectedLevel <= curLvl) ? Colors::Gold : Color{255, 175, 100, 255};
        }

        // CostText
        if (m_registry.HasComponent<TextComponent>(card.CostText))
        {
            auto& costT = m_registry.GetComponent<TextComponent>(card.CostText);
            if (curLvl >= maxLvl)
            {
                costT.Text = "ALL TIERS UNLOCKED (MAX LEVEL)";
                costT.Tint = Colors::Gold;
            }
            else
            {
                int cost = ProfileManager::Get().GetUltimateUpgradeCost(card.Type);
                costT.Text = "NEXT TIER COST (LVL. " + std::to_string(curLvl + 1) + "): " + std::to_string(cost) + " CREDITS";
                costT.Tint = (PowerUpManager::Get().GetCoins() >= cost) ? Color{80, 240, 140, 255} : Color{255, 140, 100, 255};
            }
        }

        // UpgradeBtn
        if (m_registry.HasComponent<TextComponent>(card.UpgradeBtn) && m_registry.HasComponent<ButtonComponent>(card.UpgradeBtn))
        {
            auto& btnText = m_registry.GetComponent<TextComponent>(card.UpgradeBtn);
            auto& btnComp = m_registry.GetComponent<ButtonComponent>(card.UpgradeBtn);

            if (curLvl >= maxLvl)
            {
                btnText.Text = "[ MAXIMUM LEVEL REACHED ]";
                btnText.Tint = Colors::Gold;
                btnComp.DefaultColor = Color{45, 50, 70, 255};
                btnComp.HoverColor = Color{45, 50, 70, 255};
            }
            else
            {
                int cost = ProfileManager::Get().GetUltimateUpgradeCost(card.Type);
                if (PowerUpManager::Get().GetCoins() >= cost)
                {
                    btnText.Text = "UPGRADE  [" + std::to_string(cost) + " CREDITS]";
                    btnText.Tint = Colors::White;
                    btnComp.DefaultColor = Color{35, 145, 60, 255};
                    btnComp.HoverColor = Color{50, 195, 85, 255};
                }
                else
                {
                    btnText.Text = "INSUFFICIENT CREDITS  (" + std::to_string(cost) + " CR)";
                    btnText.Tint = Color{255, 180, 120, 255};
                    btnComp.DefaultColor = Color{55, 45, 40, 255};
                    btnComp.HoverColor = Color{70, 55, 50, 255};
                }
            }
        }

        // EquipBtn
        if (m_registry.HasComponent<TextComponent>(card.EquipBtn) && m_registry.HasComponent<ButtonComponent>(card.EquipBtn))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(card.EquipBtn);
            auto& btnComp = m_registry.GetComponent<ButtonComponent>(card.EquipBtn);
            if (card.Type == equipped)
            {
                textComp.Text = "[ EQUIPPED & ACTIVE ]";
                textComp.Tint = Colors::Gold;
                btnComp.DefaultColor = Color{35, 95, 55, 255};
                btnComp.HoverColor = Color{45, 125, 75, 255};
            }
            else
            {
                textComp.Text = "EQUIP ULTIMATE";
                textComp.Tint = Colors::White;
                btnComp.DefaultColor = Color{40, 70, 130, 255};
                btnComp.HoverColor = Color{60, 105, 195, 255};
            }
        }
    }

    // 4. Profile & Stats Refresh
    if (m_workshopProfileStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_workshopProfileStatsText))
    {
        auto& textComp = m_registry.GetComponent<TextComponent>(m_workshopProfileStatsText);
        std::stringstream ss;
        ss << "LIFETIME COMBAT SCORE: " << ProfileManager::Get().GetLifetimeScore() << " PTS\n\n";
        ss << "TOTAL CREDITS EARNED:  " << ProfileManager::Get().GetLifetimeCredits() << " CR\n\n";
        ss << "CURRENT BANK BALANCE:  " << PowerUpManager::Get().GetCoins() << " CR\n\n";
        ss << "HIGHEST STAGE CLEARED: STAGE " << m_levelManager.GetHighestClearedLevel() << "\n\n";
        
        int totalPurchased = 0;
        const PowerUpType allTypes[] = {
            PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::Storm, PowerUpType::TempoBall,
            PowerUpType::ExtraLife, PowerUpType::BigBall, PowerUpType::FireBall, PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall, PowerUpType::ItemRegen
        };
        for (PowerUpType pt : allTypes)
        {
            totalPurchased += (PowerUpManager::Get().GetLevel(pt) - 1);
        }
        ss << "UPGRADES INSTALLED:    " << totalPurchased << " / 48 TIERS";

        textComp.Text = ss.str();
    }

    // 5. Description Modal Refresh
    if (m_descModalTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_descModalTitleText))
    {
        m_registry.GetComponent<TextComponent>(m_descModalTitleText).Text =
            PowerUpManager::Get().GetName(m_selectedPowerUp) + "  -  TIERS & SPECIFICATIONS";
    }

    if (m_descModalCategoryText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_descModalCategoryText))
    {
        std::string cat = "[OFFENSIVE POWER-UP]";
        if (m_selectedPowerUp == PowerUpType::StickyPaddle || m_selectedPowerUp == PowerUpType::BottomBarrier || m_selectedPowerUp == PowerUpType::MagnetPaddle || m_selectedPowerUp == PowerUpType::ExpandPaddle)
            cat = "[DEFENSIVE POWER-UP]";
        else if (m_selectedPowerUp == PowerUpType::GhostBall || m_selectedPowerUp == PowerUpType::TempoBall || m_selectedPowerUp == PowerUpType::ExtraLife || m_selectedPowerUp == PowerUpType::ItemRegen || m_selectedPowerUp == PowerUpType::ShrinkPaddle)
            cat = "[TACTICAL & SPECIAL POWER-UP]";
        m_registry.GetComponent<TextComponent>(m_descModalCategoryText).Text = cat;
    }

    if (m_descModalDescText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_descModalDescText))
    {
        m_registry.GetComponent<TextComponent>(m_descModalDescText).Text =
            PowerUpManager::Get().GetDescription(m_selectedPowerUp);
    }

    for (int lvl = 1; lvl <= 5; ++lvl)
    {
        int idx = lvl - 1;
        bool isCur = (lvl == selCurLvl);
        bool isIns = (lvl == m_inspectedLevel);
        bool isUnlocked = (lvl <= selCurLvl);

        if (m_descModalTierPanels[idx] != NULL_ENTITY && m_registry.HasComponent<PanelComponent>(m_descModalTierPanels[idx]))
        {
            auto& panel = m_registry.GetComponent<PanelComponent>(m_descModalTierPanels[idx]);
            if (isCur)
                panel.Tint = Color{25, 65, 110, 250};
            else if (isIns)
                panel.Tint = Color{55, 45, 20, 240};
            else if (isUnlocked)
                panel.Tint = Color{20, 32, 50, 230};
            else
                panel.Tint = Color{16, 20, 32, 230};
        }

        if (m_descModalTierTags[idx] != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_descModalTierTags[idx]))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(m_descModalTierTags[idx]);
            std::string tag = "LVL. " + std::to_string(lvl);
            if (isCur)
            {
                tag += " [CURRENT]";
                textComp.Tint = Color{80, 220, 255, 255};
            }
            else if (isUnlocked)
            {
                tag += " [UNLOCKED]";
                textComp.Tint = Colors::Gold;
            }
            else
            {
                tag += " [LOCKED]";
                textComp.Tint = Color{140, 150, 170, 220};
            }
            textComp.Text = tag;
        }

        if (m_descModalTierTexts[idx] != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_descModalTierTexts[idx]))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(m_descModalTierTexts[idx]);
            textComp.Text = PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, lvl);
            textComp.Tint = isUnlocked ? Colors::White : Color{180, 190, 205, 200};
        }
    }

    // 6. Update Embedded Preview Scene if inspected item or tier changed
    if (m_previewScene)
    {
        if (m_selectedPowerUp != m_lastPreviewPowerUp || m_inspectedLevel != m_lastPreviewLevel)
        {
            m_lastPreviewPowerUp = m_selectedPowerUp;
            m_lastPreviewLevel = m_inspectedLevel;
            int realLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
            m_previewScene->SetPreviewPowerUp(m_selectedPowerUp, m_inspectedLevel);
            PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLevel);
            TriggerPreviewChannelSwitch();
        }
    }
}

void MenuScene::RenderWorkshopCustomVisuals(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    // 1. Modal Frame Cyber Outline & Accents
    context.Render.DrawRectangleOutline(2400.0f, 1400.0f,
        Transform2D{Vector2f{screenCenterX, screenCenterY}},
        Color{45, 95, 175, 230}, 3.0f);

    // Corner brackets on modal
    constexpr float cw = 40.0f;
    constexpr float halfW = 1200.0f;
    constexpr float halfH = 700.0f;
    Color bracketCol{255, 215, 60, 220};
    // Top-Left
    context.Render.DrawLine(Vector2f{screenCenterX - halfW, screenCenterY - halfH}, Vector2f{screenCenterX - halfW + cw, screenCenterY - halfH}, bracketCol, 3.0f);
    context.Render.DrawLine(Vector2f{screenCenterX - halfW, screenCenterY - halfH}, Vector2f{screenCenterX - halfW, screenCenterY - halfH + cw}, bracketCol, 3.0f);
    // Top-Right
    context.Render.DrawLine(Vector2f{screenCenterX + halfW, screenCenterY - halfH}, Vector2f{screenCenterX + halfW - cw, screenCenterY - halfH}, bracketCol, 3.0f);
    context.Render.DrawLine(Vector2f{screenCenterX + halfW, screenCenterY - halfH}, Vector2f{screenCenterX + halfW, screenCenterY - halfH + cw}, bracketCol, 3.0f);
    // Bottom-Left
    context.Render.DrawLine(Vector2f{screenCenterX - halfW, screenCenterY + halfH}, Vector2f{screenCenterX - halfW + cw, screenCenterY + halfH}, bracketCol, 3.0f);
    context.Render.DrawLine(Vector2f{screenCenterX - halfW, screenCenterY + halfH}, Vector2f{screenCenterX - halfW, screenCenterY + halfH - cw}, bracketCol, 3.0f);
    // Bottom-Right
    context.Render.DrawLine(Vector2f{screenCenterX + halfW, screenCenterY + halfH}, Vector2f{screenCenterX + halfW - cw, screenCenterY + halfH}, bracketCol, 3.0f);
    context.Render.DrawLine(Vector2f{screenCenterX + halfW, screenCenterY + halfH}, Vector2f{screenCenterX + halfW, screenCenterY + halfH - cw}, bracketCol, 3.0f);

    // 2. Tab 1 Custom Visuals: Clash Royale Grid Cards, Scrollbar & Compact Simulation Monitor
    if (m_workshopActiveTabCanvas == m_workshopUpgradesCanvas)
    {
        const float cardColX[3] = {-930.0f, -670.0f, -410.0f};
        const float cardRowY[3] = {-350.0f, -50.0f, 250.0f};

        for (size_t slot = 0; slot < 9; ++slot)
        {
            int pwrIdx = m_workshopScrollRow * 3 + static_cast<int>(slot);
            if (pwrIdx >= 13) continue;

            PowerUpType type = kAllWorkshopPowerUps[pwrIdx];
            float x = screenCenterX + cardColX[slot % 3];
            float y = screenCenterY + cardRowY[slot / 3];
            bool isSelected = (type == m_selectedPowerUp);

            // 1. Clash Royale Card Base Frame (240 x 280 px)
            if (isSelected)
            {
                float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 4.0f);
                uint8_t alpha = static_cast<uint8_t>(180.0f + pulse * 75.0f);
                context.Render.DrawRectangleOutline(240.0f, 280.0f,
                    Transform2D{Vector2f{x, y}},
                    Color{255, 215, 60, alpha}, 3.5f);

                // Corner glowing pips
                constexpr float cLen = 14.0f;
                Color goldColor{255, 215, 60, 255};
                context.Render.DrawLine(Vector2f{x - 120.0f, y - 140.0f}, Vector2f{x - 120.0f + cLen, y - 140.0f}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x - 120.0f, y - 140.0f}, Vector2f{x - 120.0f, y - 140.0f + cLen}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x + 120.0f, y - 140.0f}, Vector2f{x + 120.0f - cLen, y - 140.0f}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x + 120.0f, y - 140.0f}, Vector2f{x + 120.0f, y - 140.0f + cLen}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x - 120.0f, y + 140.0f}, Vector2f{x - 120.0f + cLen, y + 140.0f}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x - 120.0f, y + 140.0f}, Vector2f{x - 120.0f, y + 140.0f - cLen}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x + 120.0f, y + 140.0f}, Vector2f{x + 120.0f - cLen, y + 140.0f}, goldColor, 2.5f);
                context.Render.DrawLine(Vector2f{x + 120.0f, y + 140.0f}, Vector2f{x + 120.0f, y + 140.0f - cLen}, goldColor, 2.5f);
            }
            else
            {
                context.Render.DrawRectangleOutline(240.0f, 280.0f,
                    Transform2D{Vector2f{x, y}},
                    Color{50, 75, 120, 200}, 1.5f);
            }

            // 2. Artwork Placeholder Frame (216 x 190 px) in the upper card area
            Vector2f artCenter{x, y - 25.0f};
            Vector2f artSize{216.0f, 190.0f};

            Color artBg = Color{20, 26, 42, 255};
            if (type == PowerUpType::Storm || type == PowerUpType::FireBall || type == PowerUpType::BigBall || type == PowerUpType::MultiBall)
                artBg = Color{48, 20, 25, 255};
            else if (type == PowerUpType::StickyPaddle || type == PowerUpType::BottomBarrier || type == PowerUpType::MagnetPaddle)
                artBg = Color{18, 38, 55, 255};
            else if (type == PowerUpType::GhostBall || type == PowerUpType::TempoBall)
                artBg = Color{38, 22, 50, 255};
            else
                artBg = Color{20, 44, 30, 255};

            context.Render.DrawRectangle(artSize.X, artSize.Y, Transform2D{artCenter}, artBg);
            context.Render.DrawRectangleOutline(artSize.X, artSize.Y, Transform2D{artCenter}, isSelected ? Color{255, 215, 60, 180} : Color{60, 90, 140, 200}, 1.5f);

            // Inner subtle cross guidelines
            Color guideCol{255, 255, 255, 18};
            context.Render.DrawLine(Vector2f{artCenter.X - artSize.X * 0.5f, artCenter.Y - artSize.Y * 0.5f}, Vector2f{artCenter.X + artSize.X * 0.5f, artCenter.Y + artSize.Y * 0.5f}, guideCol, 1.0f);
            context.Render.DrawLine(Vector2f{artCenter.X - artSize.X * 0.5f, artCenter.Y + artSize.Y * 0.5f}, Vector2f{artCenter.X + artSize.X * 0.5f, artCenter.Y - artSize.Y * 0.5f}, guideCol, 1.0f);

            // Center Badge Pill
            std::string badge = "PWR";
            if (type == PowerUpType::Storm) badge = "STM";
            else if (type == PowerUpType::MultiBall) badge = "MLB";
            else if (type == PowerUpType::FireBall) badge = "FIR";
            else if (type == PowerUpType::BigBall) badge = "BIG";
            else if (type == PowerUpType::StickyPaddle) badge = "STK";
            else if (type == PowerUpType::BottomBarrier) badge = "BAR";
            else if (type == PowerUpType::MagnetPaddle) badge = "MAG";
            else if (type == PowerUpType::GhostBall) badge = "GHO";
            else if (type == PowerUpType::TempoBall) badge = "TMP";
            else if (type == PowerUpType::ExpandPaddle) badge = "EXP";
            else if (type == PowerUpType::ShrinkPaddle) badge = "SHR";
            else if (type == PowerUpType::ExtraLife) badge = "LIF";
            else if (type == PowerUpType::ItemRegen) badge = "REG";

            context.Render.DrawRectangle(64.0f, 64.0f, Transform2D{Vector2f{artCenter.X, artCenter.Y - 10.0f}}, Color{10, 14, 24, 230});
            context.Render.DrawRectangleOutline(64.0f, 64.0f, Transform2D{Vector2f{artCenter.X, artCenter.Y - 10.0f}}, isSelected ? Colors::Gold : Colors::White, 2.0f);
            context.Render.DrawTextCentered(badge, m_fontId, 22.0f, Transform2D{Vector2f{artCenter.X, artCenter.Y - 10.0f}}, Colors::White);
            context.Render.DrawTextCentered("[ ARTWORK ]", m_fontId, 15.0f, Transform2D{Vector2f{artCenter.X, artCenter.Y + 45.0f}}, Color{150, 175, 215, 180});

            // 3. Level Banner Plate at the bottom
            Vector2f bannerCenter{x, y + 104.0f};
            Vector2f bannerSize{216.0f, 48.0f};
            context.Render.DrawRectangle(bannerSize.X, bannerSize.Y, Transform2D{bannerCenter}, Color{14, 18, 30, 255});
            context.Render.DrawRectangleOutline(bannerSize.X, bannerSize.Y, Transform2D{bannerCenter}, isSelected ? Colors::Gold : Color{50, 75, 120, 220}, 1.5f);

            // 5 Level Indicator Squares (Gold: unlit base, lit per purchased tier)
            // Centered and filling the entire banner rectangle
            int curLvl = PowerUpManager::Get().GetLevel(type);
            constexpr float sqSize = 36.0f;
            constexpr float sqGap = 6.0f;
            float sqStartX = x - (5.0f * sqSize + 4.0f * sqGap) * 0.5f + (sqSize * 0.5f);
            float sqY = y + 104.0f;

            for (int p = 0; p < 5; ++p)
            {
                Vector2f sqCenter{sqStartX + static_cast<float>(p) * (sqSize + sqGap), sqY};
                bool isLit = (p < curLvl);

                if (isLit)
                {
                    // Radiant lit gold square
                    context.Render.DrawRectangle(sqSize, sqSize, Transform2D{sqCenter}, Color{255, 215, 60, 255});
                    context.Render.DrawRectangleOutline(sqSize, sqSize, Transform2D{sqCenter}, Color{255, 250, 205, 255}, 2.0f);
                    context.Render.DrawLine(Vector2f{sqCenter.X - 14.0f, sqCenter.Y - 14.0f}, Vector2f{sqCenter.X + 14.0f, sqCenter.Y - 14.0f}, Color{255, 255, 230, 255}, 1.5f);
                }
                else
                {
                    // Base unlit square: dark interior filling the socket with dim gold/amber border
                    context.Render.DrawRectangle(sqSize, sqSize, Transform2D{sqCenter}, Color{20, 24, 38, 240});
                    context.Render.DrawRectangleOutline(sqSize, sqSize, Transform2D{sqCenter}, Color{75, 65, 40, 180}, 1.5f);
                }
            }
        }

        // Vertical Scrollbar track and thumb on the left grid
        const float scrollTrackX = screenCenterX - 250.0f;
        const float trackStartY = screenCenterY - 400.0f;
        const float trackEndY = screenCenterY + 300.0f;
        const float trackLength = trackEndY - trackStartY;

        // Track bar
        context.Render.DrawRectangle(6.0f, trackLength, Transform2D{Vector2f{scrollTrackX, (trackStartY + trackEndY) * 0.5f}}, Color{30, 42, 65, 220});
        context.Render.DrawRectangleOutline(6.0f, trackLength, Transform2D{Vector2f{scrollTrackX, (trackStartY + trackEndY) * 0.5f}}, Color{55, 80, 125, 200}, 1.0f);

        // Thumb indicator (3 of 5 rows visible -> 240px thumb)
        constexpr float thumbHeight = 240.0f;
        float travel = trackLength - thumbHeight;
        float thumbY = trackStartY + (thumbHeight * 0.5f) + (static_cast<float>(m_workshopScrollRow) / 2.0f) * travel;
        context.Render.DrawRectangle(10.0f, thumbHeight, Transform2D{Vector2f{scrollTrackX, thumbY}}, Color{0, 190, 255, 220});
        context.Render.DrawRectangleOutline(10.0f, thumbHeight, Transform2D{Vector2f{scrollTrackX, thumbY}}, Colors::White, 1.5f);

        // Preview Window in Right Column (Enhanced 640x300 Widescreen CRT Monitor)
        const Vector2f monitorCenter{screenCenterX + 600.0f, screenCenterY - 260.0f};
        const Vector2f monitorSize{640.0f, 300.0f};
        RenderPowerUpSimulation(context, m_selectedPowerUp, monitorCenter, monitorSize);

        // Reticle on Inspected Level Button
        constexpr float pipW = 96.0f;
        constexpr float pipH = 34.0f;
        constexpr float pipGap = 12.0f;
        float startX = screenCenterX + 600.0f - (5.0f * (pipW + pipGap) - pipGap) * 0.5f + (pipW * 0.5f);
        float barY = screenCenterY - 65.0f;
        float activeBx = startX + static_cast<float>(m_inspectedLevel - 1) * (pipW + pipGap);

        float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 5.0f);
        uint8_t glowAlpha = static_cast<uint8_t>(180.0f + pulse * 75.0f);
        context.Render.DrawRectangleOutline(pipW + 6.0f, pipH + 6.0f,
            Transform2D{Vector2f{activeBx, barY}}, Color{255, 215, 60, glowAlpha}, 2.5f);

        // Description Modal Overlay (100% Solid & Opaque, Blocks everything underneath)
        if (m_showDescriptionModal)
        {
            Vector2f modalCenter{screenCenterX, screenCenterY};
            Vector2f modalSize{1360.0f, 860.0f};

            // 1. Fullscreen Dark Dimming Backdrop (solid opaque dimming)
            context.Render.DrawRectangle(viewSize.X * 2.0f, viewSize.Y * 2.0f, Transform2D{modalCenter}, Color{6, 10, 20, 245});

            // 2. Main Dialog Solid Window Panel
            context.Render.DrawRectangle(modalSize.X, modalSize.Y, Transform2D{modalCenter}, Color{14, 18, 30, 255});
            context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{modalCenter}, Color{45, 75, 130, 255}, 1.5f);

            // Header Banner Panel
            context.Render.DrawRectangle(1320.0f, 76.0f, Transform2D{Vector2f{modalCenter.X, modalCenter.Y - 370.0f}}, Color{22, 30, 52, 255});
            context.Render.DrawRectangleOutline(1320.0f, 76.0f, Transform2D{Vector2f{modalCenter.X, modalCenter.Y - 370.0f}}, Color{40, 65, 110, 220}, 1.0f);

            // Glowing cyber gold outline for the dialog window
            float modalPulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 4.0f);
            uint8_t modalGlowAlpha = static_cast<uint8_t>(180.0f + modalPulse * 75.0f);
            context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{modalCenter}, Color{255, 215, 60, modalGlowAlpha}, 2.5f);

            // Corner decorative cyber brackets
            constexpr float mcw = 32.0f;
            float mhw = modalSize.X * 0.5f;
            float mhh = modalSize.Y * 0.5f;
            Color cGold{255, 225, 90, 255};

            // Top-Left
            context.Render.DrawLine(Vector2f{modalCenter.X - mhw, modalCenter.Y - mhh}, Vector2f{modalCenter.X - mhw + mcw, modalCenter.Y - mhh}, cGold, 3.0f);
            context.Render.DrawLine(Vector2f{modalCenter.X - mhw, modalCenter.Y - mhh}, Vector2f{modalCenter.X - mhw, modalCenter.Y - mhh + mcw}, cGold, 3.0f);
            // Top-Right
            context.Render.DrawLine(Vector2f{modalCenter.X + mhw, modalCenter.Y - mhh}, Vector2f{modalCenter.X + mhw - mcw, modalCenter.Y - mhh}, cGold, 3.0f);
            context.Render.DrawLine(Vector2f{modalCenter.X + mhw, modalCenter.Y - mhh}, Vector2f{modalCenter.X + mhw, modalCenter.Y - mhh + mcw}, cGold, 3.0f);
            // Bottom-Left
            context.Render.DrawLine(Vector2f{modalCenter.X - mhw, modalCenter.Y + mhh}, Vector2f{modalCenter.X - mhw + mcw, modalCenter.Y + mhh}, cGold, 3.0f);
            context.Render.DrawLine(Vector2f{modalCenter.X - mhw, modalCenter.Y + mhh}, Vector2f{modalCenter.X - mhw, modalCenter.Y + mhh - mcw}, cGold, 3.0f);
            // Bottom-Right
            context.Render.DrawLine(Vector2f{modalCenter.X + mhw, modalCenter.Y + mhh}, Vector2f{modalCenter.X + mhw - mcw, modalCenter.Y + mhh}, cGold, 3.0f);
            context.Render.DrawLine(Vector2f{modalCenter.X + mhw, modalCenter.Y + mhh}, Vector2f{modalCenter.X + mhw, modalCenter.Y + mhh - mcw}, cGold, 3.0f);

            // Dialog Title
            std::string title = PowerUpManager::Get().GetName(m_selectedPowerUp);
            context.Render.DrawTextCentered(title, m_fontId, 34.0f, Transform2D{Vector2f{modalCenter.X - 40.0f, modalCenter.Y - 370.0f}}, Colors::Gold);

            // [X] Close button in top-right of dialog
            auto [mx, my] = context.Input.GetMousePosition();
            Vector2f mousePos{mx, my};
            Vector2f xBtnPos{modalCenter.X + 610.0f, modalCenter.Y - 370.0f};
            bool xHover = (std::abs(mousePos.X - xBtnPos.X) <= 26.0f && std::abs(mousePos.Y - xBtnPos.Y) <= 26.0f);
            context.Render.DrawRectangle(52.0f, 52.0f, Transform2D{xBtnPos}, xHover ? Color{210, 50, 65, 255} : Color{140, 35, 45, 255});
            context.Render.DrawRectangleOutline(52.0f, 52.0f, Transform2D{xBtnPos}, Colors::White, 1.5f);
            context.Render.DrawTextCentered("X", m_fontId, 32.0f, Transform2D{xBtnPos}, Colors::White);

            // Full Description text
            std::string desc = PowerUpManager::Get().GetDescription(m_selectedPowerUp);
            std::string wrappedDesc = context.Render.WrapText(desc, 1200.0f, m_fontId, 23.0f);
            context.Render.DrawTextCentered(wrappedDesc, m_fontId, 23.0f, Transform2D{Vector2f{modalCenter.X, modalCenter.Y - 265.0f}}, Color{225, 235, 250, 255});

            // 5 Tier Rows for NIV. 1 to NIV. 5
            int selCurLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
            constexpr float rowCenterY[5] = {-160.0f, -75.0f, 10.0f, 95.0f, 180.0f};
            for (int lvl = 1; lvl <= 5; ++lvl)
            {
                float ry = modalCenter.Y + rowCenterY[lvl - 1];
                bool isCur = (lvl == selCurLvl);
                bool isIns = (lvl == m_inspectedLevel);
                bool isUnlocked = (lvl <= selCurLvl);

                Color rowBg = isCur ? Color{25, 65, 110, 250} : (isIns ? Color{55, 45, 20, 240} : (isUnlocked ? Color{20, 32, 50, 230} : Color{16, 20, 32, 230}));
                Color rowBorder = isCur ? Color{80, 220, 255, 255} : (isIns ? Colors::Gold : Color{45, 60, 95, 180});

                context.Render.DrawRectangle(1260.0f, 68.0f, Transform2D{Vector2f{modalCenter.X, ry}}, rowBg);
                context.Render.DrawRectangleOutline(1260.0f, 68.0f, Transform2D{Vector2f{modalCenter.X, ry}}, rowBorder, (isCur || isIns) ? 2.0f : 1.0f);

                std::string tag = "LVL. " + std::to_string(lvl);
                Color tagCol = Colors::Gold;
                if (isCur) { tag += " [CURRENT]"; tagCol = Color{80, 220, 255, 255}; }
                else if (isUnlocked) { tag += " [UNLOCKED]"; tagCol = Colors::Gold; }
                else { tag += " [LOCKED]"; tagCol = Color{140, 150, 170, 220}; }

                context.Render.DrawTextCentered(tag, m_fontId, 22.0f, Transform2D{Vector2f{modalCenter.X - 480.0f, ry}}, tagCol);

                std::string tierInfo = PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, lvl);
                context.Render.DrawText(tierInfo, m_fontId, 20.0f, Transform2D{Vector2f{modalCenter.X - 320.0f, ry - 10.0f}}, isUnlocked ? Colors::White : Color{180, 190, 205, 200});
            }

            // Prominent "BACK" Button at the bottom
            Vector2f retBtnPos{modalCenter.X, modalCenter.Y + 330.0f};
            bool retHover = (std::abs(mousePos.X - retBtnPos.X) <= 180.0f && std::abs(mousePos.Y - retBtnPos.Y) <= 30.0f);
            context.Render.DrawRectangle(360.0f, 60.0f, Transform2D{retBtnPos}, retHover ? Color{165, 50, 65, 255} : Color{120, 35, 45, 255});
            context.Render.DrawRectangleOutline(360.0f, 60.0f, Transform2D{retBtnPos}, Colors::White, 1.5f);
            context.Render.DrawTextCentered("BACK", m_fontId, 30.0f, Transform2D{retBtnPos}, Colors::White);
        }
    }

    // 3. Tab 2 Custom Visuals: Ultimates Ability Simulation Viewports
    if (m_workshopActiveTabCanvas == m_workshopUltimatesCanvas)
    {
        const float ultColX[3] = {-720.0f, 0.0f, 720.0f};
        UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();

        constexpr float ultPipW = 96.0f;
        constexpr float ultPipH = 34.0f;
        constexpr float ultPipGap = 12.0f;

        for (int i = 0; i < 3; ++i)
        {
            float x = screenCenterX + ultColX[i];
            float y = screenCenterY - 30.0f;

            UltimateAbilityType ultType = m_ultimateCards[i].Type;
            bool isEquipped = (ultType == equipped);

            // Outer card border
            Color borderCol = isEquipped ? Colors::Gold : Color{50, 80, 135, 200};
            context.Render.DrawRectangleOutline(680.0f, 960.0f, Transform2D{Vector2f{x, y}}, borderCol, isEquipped ? 3.5f : 1.5f);

            // Simulation Viewport on the card
            Vector2f simCenter{x, screenCenterY - 265.0f};
            Vector2f simSize{580.0f, 260.0f};

            RenderUltimateSimulation(context, ultType, m_ultimateCards[i].InspectedLevel, simCenter, simSize, m_ultimateCards[i].SwitchTimer);

            // Top tag
            context.Render.DrawText("ULTIMATE ABILITY", m_fontId, 20.0f,
                Transform2D{Vector2f{simCenter.X - (simSize.X * 0.5f) + 15.0f, simCenter.Y - (simSize.Y * 0.5f) + 16.0f}},
                isEquipped ? Colors::Gold : Color{120, 200, 255, 255});

            // Glowing reticle on currently inspected level pip button
            float startUltPipX = x - (5.0f * (ultPipW + ultPipGap) - ultPipGap) * 0.5f + (ultPipW * 0.5f);
            float activePipX = startUltPipX + static_cast<float>(m_ultimateCards[i].InspectedLevel - 1) * (ultPipW + ultPipGap);
            float pipY = screenCenterY - 95.0f;

            float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 5.0f);
            uint8_t glowAlpha = static_cast<uint8_t>(180.0f + pulse * 75.0f);
            context.Render.DrawRectangleOutline(ultPipW + 6.0f, ultPipH + 6.0f,
                Transform2D{Vector2f{activePipX, pipY}}, Color{255, 215, 60, glowAlpha}, 2.5f);
        }
    }
}

void MenuScene::TriggerSimSfx(GameContext& context, uint32_t sfxId, int eventId, float volume)
{
    if (sfxId == 0) return;
    uint32_t bit = 1u << (static_cast<uint32_t>(eventId) % 32u);
    if (!(m_simAudioTriggerMask & bit))
    {
        m_simAudioTriggerMask |= bit;
        context.Audio.PlaySfx(sfxId, volume);
    }
}

void MenuScene::TriggerPreviewChannelSwitch()
{
    m_previewSwitchTimer = 0.38f;
    if (m_channelSwitchSfxId != 0 && mp_context)
    {
        mp_context->Audio.PlaySfx(m_channelSwitchSfxId, 85.0f, 1.25f);
    }
}

void MenuScene::RenderPowerUpSimulation(GameContext& context, PowerUpType type, const Vector2f& center, const Vector2f& size)
{
    (void)type;
    // 1. Draw live preview scene into preview offscreen texture
    if (m_previewScene && m_previewTexture)
    {
        m_previewTexture->clear(sf::Color(8, 10, 16, 255));

        int realLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
        PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, m_inspectedLevel);

        context.Render.PushRenderTarget(*m_previewTexture);
        m_previewScene->OnRender(context);
        context.Render.PopRenderTarget();

        PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLevel);

        m_previewTexture->display();

        constexpr float kSwitchDuration = 0.38f;
        float switchProgress = std::clamp(m_previewSwitchTimer / kSwitchDuration, 0.0f, 1.0f);
        uint32_t previewShader = (context.Rules.GetRule(Rule::Graphics::EnableShader)) ? m_previewShaderId : 0;

        context.Render.DrawRawTexture(
            m_previewTexture->getTexture(),
            size.X - 4.0f,
            size.Y - 4.0f,
            Transform2D{center},
            Colors::White,
            previewShader,
            m_previewAnimTime,
            switchProgress
        );

        if (previewShader == 0 && switchProgress > 0.001f)
        {
            // Software static flicker overlay fallback when shaders are disabled
            uint8_t noiseAlpha = static_cast<uint8_t>(switchProgress * 150.0f);
            context.Render.DrawRectangle(size.X - 4.0f, size.Y - 4.0f, Transform2D{center}, Color{200, 225, 255, noiseAlpha});
        }
    }
    else
    {
        // Sleek dark placeholder background
        context.Render.DrawRectangle(size.X, size.Y, Transform2D{center}, Color{10, 14, 24, 255});
    }

    // Cyber border outline
    context.Render.DrawRectangleOutline(size.X, size.Y, Transform2D{center}, Color{50, 100, 185, 240}, 2.5f);

    // Subtle corner brackets for high-tech HUD feel
    constexpr float cw = 20.0f;
    float halfW = size.X * 0.5f;
    float halfH = size.Y * 0.5f;
    Color bracketCol{255, 215, 60, 220};
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y - halfH}, Vector2f{center.X - halfW + cw, center.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y - halfH}, Vector2f{center.X - halfW, center.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y - halfH}, Vector2f{center.X + halfW - cw, center.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y - halfH}, Vector2f{center.X + halfW, center.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y + halfH}, Vector2f{center.X - halfW + cw, center.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y + halfH}, Vector2f{center.X - halfW, center.Y + halfH - cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y + halfH}, Vector2f{center.X + halfW - cw, center.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y + halfH}, Vector2f{center.X + halfW, center.Y + halfH - cw}, bracketCol, 2.5f);
}

void MenuScene::RenderUltimateSimulation(GameContext& context, UltimateAbilityType type, int level, const Vector2f& center, const Vector2f& size, float switchTimer)
{
    if (m_ultimatePreviewTexture)
    {
        m_ultimatePreviewTexture->clear(sf::Color(8, 11, 18, 255));

        sf::View view(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(1160.0f, 540.0f));
        m_ultimatePreviewTexture->setView(view);

        context.Render.PushRenderTarget(*m_ultimatePreviewTexture);

        const Vector2f simCenter{0.0f, 0.0f};
        const Vector2f simSize{1160.0f, 540.0f};

        switch (type)
        {
        case UltimateAbilityType::PlasmaOrb:
            RenderPlasmaOrbPreview(context, level, simCenter, simSize, m_previewAnimTime);
            break;
        case UltimateAbilityType::DiscoBall:
            RenderDiscoBallPreview(context, level, simCenter, simSize, m_previewAnimTime);
            break;
        case UltimateAbilityType::PowerUpRain:
            RenderPowerUpRainPreview(context, level, simCenter, simSize, m_previewAnimTime);
            break;
        default:
            break;
        }

        context.Render.PopRenderTarget();
        m_ultimatePreviewTexture->display();

        constexpr float kSwitchDuration = 0.38f;
        float switchProgress = std::clamp(switchTimer / kSwitchDuration, 0.0f, 1.0f);
        uint32_t previewShader = (context.Rules.GetRule(Rule::Graphics::EnableShader)) ? m_previewShaderId : 0;

        context.Render.DrawRawTexture(
            m_ultimatePreviewTexture->getTexture(),
            size.X - 4.0f,
            size.Y - 4.0f,
            Transform2D{center},
            Colors::White,
            previewShader,
            m_previewAnimTime,
            switchProgress
        );

        if (previewShader == 0 && switchProgress > 0.001f)
        {
            uint8_t noiseAlpha = static_cast<uint8_t>(switchProgress * 150.0f);
            context.Render.DrawRectangle(size.X - 4.0f, size.Y - 4.0f, Transform2D{center}, Color{200, 225, 255, noiseAlpha});
        }
    }
    else
    {
        context.Render.DrawRectangle(size.X, size.Y, Transform2D{center}, Color{10, 14, 24, 255});
    }

    // Outer cyber border & brackets
    context.Render.DrawRectangleOutline(size.X, size.Y, Transform2D{center}, Color{50, 100, 185, 240}, 2.5f);

    constexpr float cw = 20.0f;
    float halfW = size.X * 0.5f;
    float halfH = size.Y * 0.5f;
    Color bracketCol{255, 215, 60, 220};
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y - halfH}, Vector2f{center.X - halfW + cw, center.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y - halfH}, Vector2f{center.X - halfW, center.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y - halfH}, Vector2f{center.X + halfW - cw, center.Y - halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y - halfH}, Vector2f{center.X + halfW, center.Y - halfH + cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y + halfH}, Vector2f{center.X - halfW + cw, center.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X - halfW, center.Y + halfH}, Vector2f{center.X - halfW, center.Y + halfH - cw}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y + halfH}, Vector2f{center.X + halfW - cw, center.Y + halfH}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{center.X + halfW, center.Y + halfH}, Vector2f{center.X + halfW, center.Y + halfH - cw}, bracketCol, 2.5f);
}

void MenuScene::RenderPlasmaOrbPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime)
{
    (void)center;
    (void)size;
    float cycle = std::fmod(animTime, 3.5f);

    // Subtle plasma grid / ambient energy lines
    for (float gy = -200.0f; gy <= 200.0f; gy += 80.0f)
    {
        context.Render.DrawLine(Vector2f{-560.0f, gy}, Vector2f{560.0f, gy}, Color{30, 20, 60, 40}, 1.0f);
    }

    // Target Bricks formation at top (2 rows, 9 columns)
    const float brickW = 85.0f;
    const float brickH = 26.0f;
    for (int row = 0; row < 2; ++row)
    {
        float by = (row == 0) ? -130.0f : -85.0f;
        for (int col = 0; col < 9; ++col)
        {
            float bx = -440.0f + static_cast<float>(col) * 110.0f;
            bool hit = false;
            if (cycle >= 1.6f && cycle < 3.2f)
            {
                hit = true;
            }
            Color bCol = hit ? Color{80, 40, 120, 140} : ((row == 0) ? Color{160, 80, 240, 230} : Color{100, 140, 255, 230});
            context.Render.DrawRectangle(brickW, brickH, Transform2D{Vector2f{bx, by}}, bCol);
            context.Render.DrawRectangleOutline(brickW, brickH, Transform2D{Vector2f{bx, by}}, hit ? Color{180, 100, 255, 180} : Colors::Cyan, 1.5f);
        }
    }

    // Paddle at bottom
    const Vector2f paddlePos{0.0f, 175.0f};
    context.Render.DrawRectangle(140.0f, 26.0f, Transform2D{paddlePos}, Color{35, 30, 65, 255});
    context.Render.DrawRectangleOutline(140.0f, 26.0f, Transform2D{paddlePos}, Color{160, 100, 255, 255}, 2.0f);

    int initialCount = (level == 1) ? 1 : ((level <= 3) ? 2 : 3);
    float baseScale = (level <= 2) ? 1.0f : ((level <= 4) ? 1.5f : 2.0f);
    float baseR = 20.0f * baseScale;

    if (cycle < 0.6f)
    {
        // Charging phase on paddle
        float charge = cycle / 0.6f;
        float curR = baseR * charge;
        context.Render.DrawCircle(curR, Transform2D{Vector2f{0.0f, 145.0f}}, Color{100, 160, 255, static_cast<uint8_t>(charge * 220.0f)});
        context.Render.DrawCircleOutline(curR + 6.0f, Transform2D{Vector2f{0.0f, 145.0f}}, Color{200, 80, 255, 255}, 2.0f);
    }
    else if (cycle < 1.7f)
    {
        // Ascent phase: primary orbs travel upward
        float flyProg = (cycle - 0.6f) / 1.1f;
        float curY = 145.0f - flyProg * 220.0f;

        for (int i = 0; i < initialCount; ++i)
        {
            float xOffset = 0.0f;
            if (initialCount == 2)
            {
                xOffset = (i == 0) ? -70.0f : 70.0f;
            }
            else if (initialCount == 3)
            {
                xOffset = (i == 0) ? -120.0f : ((i == 1) ? 0.0f : 120.0f);
            }
            Vector2f orbPos{xOffset, curY};

            float pulse = 1.0f + 0.15f * std::sin(animTime * 18.0f + static_cast<float>(i));
            context.Render.DrawCircle(baseR * 1.35f * pulse, Transform2D{orbPos}, Color{120, 40, 255, 90});
            context.Render.DrawCircle(baseR * pulse, Transform2D{orbPos}, Color{80, 120, 255, 200});
            context.Render.DrawCircle(baseR * 0.55f * pulse, Transform2D{orbPos}, Color{220, 240, 255, 255});
            context.Render.DrawCircleOutline(baseR * 1.35f * pulse, Transform2D{orbPos}, Color{180, 100, 255, 200}, 2.0f);
        }
    }
    else if (cycle < 2.9f)
    {
        // Division phase: orbs split into child orbs
        float divProg = (cycle - 1.7f) / 1.2f;
        int childCount = (level == 1) ? 2 : ((level == 2) ? 4 : ((level <= 4) ? 6 : 8));
        float childR = baseR * 0.55f;

        for (int c = 0; c < childCount; ++c)
        {
            float ang = static_cast<float>(c) * (6.2831853f / static_cast<float>(childCount)) + animTime * 4.0f;
            float spreadDist = divProg * 140.0f;
            Vector2f childPos{std::cos(ang) * spreadDist * 1.6f, -75.0f + std::sin(ang) * spreadDist * 0.7f};

            uint8_t childAlpha = static_cast<uint8_t>(std::clamp((1.0f - divProg * 0.6f) * 255.0f, 0.0f, 255.0f));
            context.Render.DrawCircle(childR, Transform2D{childPos}, Color{100, 180, 255, childAlpha});
            context.Render.DrawCircle(childR * 0.45f, Transform2D{childPos}, Color{240, 245, 255, childAlpha});
            context.Render.DrawCircleOutline(childR + 3.0f, Transform2D{childPos}, Color{220, 90, 255, childAlpha}, 1.5f);

            Vector2f trailPt = childPos - Vector2f{std::cos(ang) * 16.0f, std::sin(ang) * 16.0f};
            context.Render.DrawLine(trailPt, childPos, Color{160, 100, 255, childAlpha}, 2.0f);
        }
    }
    else
    {
        // Final explosions & dissipation
        float expProg = (cycle - 2.9f) / 0.6f;
        float expR = expProg * 120.0f;
        uint8_t expAlpha = static_cast<uint8_t>((1.0f - expProg) * 220.0f);
        context.Render.DrawCircleOutline(expR, Transform2D{Vector2f{-100.0f, -80.0f}}, Color{180, 80, 255, expAlpha}, 3.0f);
        context.Render.DrawCircleOutline(expR * 0.8f, Transform2D{Vector2f{100.0f, -80.0f}}, Color{100, 200, 255, expAlpha}, 2.5f);
    }

    // Dynamic HUD Overlay
    context.Render.DrawText("[ PLASMA ORB ]  MK-" + std::to_string(level), m_fontId, 24.0f, Transform2D{Vector2f{-540.0f, -240.0f}}, Color{160, 120, 255, 255});
    context.Render.DrawText("PLASMA HIGH-ENERGY DIVISION BURST", m_fontId, 16.0f, Transform2D{Vector2f{-540.0f, -212.0f}}, Color{140, 180, 255, 220});

    std::string divStr = (level == 1) ? "DIVISION: 1 -> 2 ORBS" :
                         ((level == 2) ? "DIVISION: 2 -> 4 ORBS" :
                         ((level == 3) ? "DIVISION: 2 LARGE -> 4 -> 2" :
                         ((level == 4) ? "DIVISION: 3 LARGE -> 4 -> 2" : "DIVISION: 3 MEGA -> 8 -> 4 -> 2")));
    context.Render.DrawText(divStr, m_fontId, 20.0f, Transform2D{Vector2f{240.0f, -240.0f}}, Color{180, 140, 255, 240});

    std::string scaleStr = "SCALE: x" + std::to_string(baseScale).substr(0, 3) + " | BURST FLUX: MAX";
    context.Render.DrawText(scaleStr, m_fontId, 20.0f, Transform2D{Vector2f{-540.0f, 220.0f}}, Color{130, 210, 255, 240});
    context.Render.DrawText("AUTO-DETONATION: ENABLED", m_fontId, 16.0f, Transform2D{Vector2f{250.0f, 220.0f}}, Color{180, 160, 220, 200});
}

namespace
{
Color DiscoRainbowColor(float hue, uint8_t alpha = 255)
{
    float r = (std::sin(hue * 6.2831853f + 0.00f) * 0.5f + 0.5f) * 255.0f;
    float g = (std::sin(hue * 6.2831853f + 2.0943951f) * 0.5f + 0.5f) * 255.0f;
    float b = (std::sin(hue * 6.2831853f + 4.1887902f) * 0.5f + 0.5f) * 255.0f;
    return Color{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), alpha};
}

void DrawMusicNote(GameContext& context, const Vector2f& pos, float scale, float rotAngle, const Color& color, bool doubleNote)
{
    float r = 5.0f * scale;
    float stemH = 20.0f * scale;
    float lineThick = std::max(1.5f, 2.0f * scale);

    auto rotateVec = [rotAngle](const Vector2f& v) -> Vector2f {
        float cosA = std::cos(rotAngle);
        float sinA = std::sin(rotAngle);
        return Vector2f{v.X * cosA - v.Y * sinA, v.X * sinA + v.Y * cosA};
    };

    if (!doubleNote)
    {
        Vector2f headCenter = pos;
        Transform2D headTrans{headCenter, rotAngle - 0.35f, Vector2f{1.35f * scale, 0.95f * scale}};
        context.Render.DrawCircle(r, headTrans, color);

        Vector2f stemBottom = headCenter + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
        Vector2f stemTop = headCenter + rotateVec(Vector2f{r * 0.85f * scale, -stemH});
        context.Render.DrawLine(stemBottom, stemTop, color, lineThick);

        Vector2f flagMid = stemTop + rotateVec(Vector2f{6.0f * scale, 5.0f * scale});
        Vector2f flagTip = stemTop + rotateVec(Vector2f{4.0f * scale, 12.0f * scale});
        context.Render.DrawLine(stemTop, flagMid, color, lineThick);
        context.Render.DrawLine(flagMid, flagTip, color, lineThick * 0.85f);
    }
    else
    {
        float noteDist = 16.0f * scale;
        Vector2f head1 = pos + rotateVec(Vector2f{-noteDist * 0.5f, 2.0f * scale});
        Vector2f head2 = pos + rotateVec(Vector2f{noteDist * 0.5f, -2.0f * scale});

        Transform2D t1{head1, rotAngle - 0.35f, Vector2f{1.3f * scale, 0.9f * scale}};
        Transform2D t2{head2, rotAngle - 0.35f, Vector2f{1.3f * scale, 0.9f * scale}};
        context.Render.DrawCircle(r, t1, color);
        context.Render.DrawCircle(r, t2, color);

        Vector2f stem1Bottom = head1 + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
        Vector2f stem1Top = head1 + rotateVec(Vector2f{r * 0.85f * scale, -stemH});
        Vector2f stem2Bottom = head2 + rotateVec(Vector2f{r * 0.85f * scale, 0.0f});
        Vector2f stem2Top = head2 + rotateVec(Vector2f{r * 0.85f * scale, -stemH});

        context.Render.DrawLine(stem1Bottom, stem1Top, color, lineThick);
        context.Render.DrawLine(stem2Bottom, stem2Top, color, lineThick);
        context.Render.DrawLine(stem1Top, stem2Top, color, lineThick * 1.8f);
    }
}
}

void MenuScene::RenderDiscoBallPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime)
{
    (void)center;
    (void)size;

    // Disco dancefloor animated neon floor grid
    for (float gx = -540.0f; gx <= 540.0f; gx += 90.0f)
    {
        float hue = std::fmod(std::abs(gx) * 0.0015f + animTime * 0.3f, 1.0f);
        Color lineCol = DiscoRainbowColor(hue, 65);
        context.Render.DrawLine(Vector2f{gx, -240.0f}, Vector2f{gx, 240.0f}, lineCol, 1.0f);
    }
    for (float gy = -210.0f; gy <= 210.0f; gy += 70.0f)
    {
        float hue = std::fmod(std::abs(gy) * 0.002f + animTime * 0.35f, 1.0f);
        Color lineCol = DiscoRainbowColor(hue, 65);
        context.Render.DrawLine(Vector2f{-560.0f, gy}, Vector2f{560.0f, gy}, lineCol, 1.0f);
    }

    // Wandering yellow musical note particles in background
    constexpr int kPreviewNotes = 8;
    for (int ni = 0; ni < kPreviewNotes; ++ni)
    {
        float offset = static_cast<float>(ni) * 1.35f;
        float nx = std::sin(animTime * 0.8f + offset) * 460.0f + std::cos(animTime * 1.3f + offset * 2.0f) * 60.0f;
        float ny = -180.0f + std::fmod(animTime * 45.0f + offset * 70.0f, 400.0f);
        float rot = std::sin(animTime * 1.5f + offset) * 0.35f;
        float scale = 0.85f + 0.25f * std::sin(animTime * 2.0f + offset);
        uint8_t a = static_cast<uint8_t>(160 + 70 * std::sin(animTime * 3.0f + offset));

        // Warm golden halo
        DrawMusicNote(context, Vector2f{nx, ny}, scale * 1.3f, rot, Color{255, 200, 30, static_cast<uint8_t>(a * 0.45f)}, (ni % 2 == 0));
        // Vibrant yellow
        DrawMusicNote(context, Vector2f{nx, ny}, scale, rot, Color{255, 240, 50, a}, (ni % 2 == 0));
    }

    // Bricks formation with rainbow disco cycling colors
    const float brickW = 90.0f;
    const float brickH = 28.0f;
    for (int row = 0; row < 2; ++row)
    {
        float by = (row == 0) ? -130.0f : -85.0f;
        for (int col = 0; col < 8; ++col)
        {
            float bx = -420.0f + static_cast<float>(col) * 120.0f;
            float brickHue = std::fmod(animTime * 0.5f + static_cast<float>(col + row * 4) * 0.1f, 1.0f);
            Color bCol = DiscoRainbowColor(brickHue, 220);
            context.Render.DrawRectangle(brickW, brickH, Transform2D{Vector2f{bx, by}}, bCol);
            context.Render.DrawRectangleOutline(brickW, brickH, Transform2D{Vector2f{bx, by}}, Colors::White, 1.5f);
        }
    }

    // Floating rainbow Big Balls orbiting in circle
    int bigBallCount = 2 + (level - 1); // 2 at L1, up to 6 at L5
    float orbitRadius = 160.0f;
    Vector2f orbitCenter{0.0f, -40.0f};

    for (int b = 0; b < bigBallCount; ++b)
    {
        float orbAng = static_cast<float>(b) * (6.2831853f / static_cast<float>(bigBallCount)) + animTime * 1.8f;
        Vector2f ballPos = orbitCenter + Vector2f{std::cos(orbAng) * orbitRadius * 1.4f, std::sin(orbAng) * (orbitRadius * 0.6f)};

        float ballHue = std::fmod(animTime * 0.8f + static_cast<float>(b) / static_cast<float>(bigBallCount), 1.0f);
        Color ballCol = DiscoRainbowColor(ballHue, 255);
        Color haloCol = DiscoRainbowColor(ballHue, 90);

        context.Render.DrawCircle(32.0f, Transform2D{ballPos}, haloCol);
        context.Render.DrawCircle(20.0f, Transform2D{ballPos}, ballCol);
        context.Render.DrawCircle(8.0f, Transform2D{ballPos}, Colors::White);
        context.Render.DrawCircleOutline(20.0f, Transform2D{ballPos}, Colors::White, 1.5f);
    }

    // Paddle at bottom
    const Vector2f paddlePos{0.0f, 175.0f};
    context.Render.DrawRectangle(140.0f, 26.0f, Transform2D{paddlePos}, Color{45, 20, 50, 255});
    context.Render.DrawRectangleOutline(140.0f, 26.0f, Transform2D{paddlePos}, DiscoRainbowColor(std::fmod(animTime * 0.6f, 1.0f)), 2.0f);

    // Paddle Acoustic Soundwaves (repeating pulses)
    float waveCycle = std::fmod(animTime, 1.8f);
    float maxReachFraction = 0.5f + static_cast<float>(level - 1) * 0.1f; // 50% to 90%
    float maxWaveHeight = 350.0f * maxReachFraction;

    int waveCount = (level >= 4) ? 3 : 1;
    for (int w = 0; w < waveCount; ++w)
    {
        float wOffset = static_cast<float>(w) * 0.35f;
        float localCycle = waveCycle - wOffset;
        if (localCycle < 0.0f) localCycle += 1.8f;

        float waveProg = localCycle / 1.8f;
        float waveH = waveProg * maxWaveHeight;
        float waveY = paddlePos.Y - waveH;
        float waveW = 80.0f + waveProg * 360.0f;

        uint8_t waveAlpha = static_cast<uint8_t>((1.0f - waveProg) * 230.0f);
        Color waveCol = DiscoRainbowColor(std::fmod(animTime * 1.2f + static_cast<float>(w) * 0.3f, 1.0f), waveAlpha);

        context.Render.DrawLine(Vector2f{-waveW * 0.5f, waveY}, Vector2f{waveW * 0.5f, waveY}, waveCol, 4.0f);
        context.Render.DrawCircleOutline(waveW * 0.5f, Transform2D{Vector2f{0.0f, paddlePos.Y}}, waveCol, 2.5f);
    }

    // Level 5: Boule à Facettes (Sleek Mirror Disco Ball)
    if (level >= 5)
    {
        Vector2f mirrorPos{0.0f, -40.0f + std::sin(animTime * 2.5f) * 30.0f};
        float mirrorR = 26.0f;

        for (int ray = 0; ray < 12; ++ray)
        {
            float rAng = static_cast<float>(ray) * (6.2831853f / 12.0f) + animTime * 1.5f;
            Vector2f rayEnd = mirrorPos + Vector2f{std::cos(rAng) * 450.0f, std::sin(rAng) * 300.0f};
            Color rayCol = DiscoRainbowColor(std::fmod(static_cast<float>(ray) / 12.0f + animTime * 0.4f, 1.0f), 70);
            context.Render.DrawLine(mirrorPos, rayEnd, rayCol, 2.5f);
        }

        context.Render.DrawCircle(mirrorR, Transform2D{mirrorPos}, Color{210, 220, 240, 240});
        context.Render.DrawCircleOutline(mirrorR, Transform2D{mirrorPos}, Colors::White, 2.0f);

        for (int fx = -2; fx <= 2; ++fx)
        {
            for (int fy = -2; fy <= 2; ++fy)
            {
                if (fx * fx + fy * fy <= 4)
                {
                    Vector2f tilePos = mirrorPos + Vector2f{static_cast<float>(fx) * 8.0f, static_cast<float>(fy) * 8.0f};
                    float tileHue = std::fmod(animTime * 2.0f + static_cast<float>(fx + fy) * 0.15f, 1.0f);
                    Color tileCol = DiscoRainbowColor(tileHue, 220);
                    context.Render.DrawRectangle(6.0f, 6.0f, Transform2D{tilePos}, tileCol);
                    context.Render.DrawRectangleOutline(6.0f, 6.0f, Transform2D{tilePos}, Colors::White, 1.0f);
                }
            }
        }
    }

    // Dynamic HUD Overlay
    context.Render.DrawText("[ DISCO FEVER ]  MK-" + std::to_string(level), m_fontId, 24.0f, Transform2D{Vector2f{-540.0f, -240.0f}}, Color{255, 120, 210, 255});
    context.Render.DrawText("GROOVY DISCO SHOW & ACOUSTIC SHOCKWAVES", m_fontId, 16.0f, Transform2D{Vector2f{-540.0f, -212.0f}}, Color{255, 180, 230, 220});

    std::string waveStr = "SHOCKWAVE REACH: " + std::to_string(40 + level * 10) + "% (" + (level >= 3 ? "2 DMG" : "1 DMG") + ")";
    context.Render.DrawText(waveStr, m_fontId, 20.0f, Transform2D{Vector2f{240.0f, -240.0f}}, Color{255, 220, 100, 240});

    std::string bbStr = "BIG BALLS: " + std::to_string(bigBallCount) + (level >= 5 ? " + MIRROR BALL" : "");
    context.Render.DrawText(bbStr, m_fontId, 20.0f, Transform2D{Vector2f{-540.0f, 220.0f}}, Color{255, 140, 200, 240});
    context.Render.DrawText("DISCO AUDIO TRACK: 124 BPM", m_fontId, 16.0f, Transform2D{Vector2f{250.0f, 220.0f}}, Color{180, 220, 255, 200});
}

void MenuScene::RenderPowerUpRainPreview(GameContext& context, int level, const Vector2f& center, const Vector2f& size, float animTime)
{
    (void)center;
    (void)size;

    for (float gy = -210.0f; gy <= 210.0f; gy += 70.0f)
    {
        context.Render.DrawLine(Vector2f{-560.0f, gy}, Vector2f{560.0f, gy}, Color{20, 60, 45, 45}, 1.0f);
    }

    const Vector2f paddlePos{0.0f, 175.0f};
    context.Render.DrawRectangle(140.0f, 26.0f, Transform2D{paddlePos}, Color{25, 55, 45, 255});
    context.Render.DrawRectangleOutline(140.0f, 26.0f, Transform2D{paddlePos}, Colors::Gold, 2.0f);

    float pulse = 0.5f + 0.5f * std::sin(animTime * 6.0f);
    context.Render.DrawCircleOutline(85.0f + pulse * 15.0f, Transform2D{paddlePos}, Color{255, 215, 60, static_cast<uint8_t>(100.0f + pulse * 90.0f)}, 1.5f);

    struct DropDef
    {
        float x;
        float speed;
        float tOffset;
        Color col;
        int tier;
    };

    const DropDef drops[12] = {
        {-420.0f, 180.0f, 0.2f, Colors::Red, std::clamp(level, 1, 5)},
        {-340.0f, 220.0f, 0.9f, Colors::Cyan, std::clamp(level - 1, 1, 4)},
        {-260.0f, 195.0f, 1.6f, Colors::Gold, std::clamp(level, 1, 5)},
        {-180.0f, 240.0f, 0.4f, Color{180, 100, 255, 255}, std::clamp(level + 1, 1, 5)},
        {-100.0f, 210.0f, 1.2f, Colors::Green, std::clamp(level, 1, 5)},
        {-20.0f,  250.0f, 0.1f, Color{255, 140, 40, 255}, std::clamp(level, 1, 5)},
        {60.0f,   200.0f, 1.8f, Colors::Blue, std::clamp(level - 1, 1, 4)},
        {140.0f,  230.0f, 0.7f, Colors::Magenta, std::clamp(level, 1, 5)},
        {220.0f,  190.0f, 1.4f, Colors::Red, std::clamp(level, 1, 5)},
        {300.0f,  240.0f, 0.5f, Colors::Cyan, std::clamp(level + 1, 1, 5)},
        {380.0f,  215.0f, 1.1f, Colors::Gold, std::clamp(level, 1, 5)},
        {450.0f,  200.0f, 1.9f, Colors::Green, std::clamp(level, 1, 5)}
    };

    int activeCount = 5 + level * 2;
    for (int i = 0; i < std::min(activeCount, 12); ++i)
    {
        const auto& d = drops[i];
        float fallY = -230.0f + std::fmod((animTime + d.tOffset) * d.speed, 420.0f);
        Vector2f dropPos{d.x, fallY};

        context.Render.DrawLine(dropPos, Vector2f{dropPos.X, dropPos.Y - 26.0f}, Color{d.col.r, d.col.g, d.col.b, 110}, 2.5f);

        context.Render.DrawRectangle(32.0f, 16.0f, Transform2D{dropPos}, Color{30, 35, 45, 250});
        context.Render.DrawRectangleOutline(32.0f, 16.0f, Transform2D{dropPos}, d.col, 2.0f);
        context.Render.DrawCircle(6.0f, Transform2D{dropPos}, d.col);

        for (int p = 0; p < d.tier; ++p)
        {
            float px = dropPos.X - 8.0f + static_cast<float>(p) * 4.0f;
            context.Render.DrawRectangle(2.0f, 3.0f, Transform2D{Vector2f{px, dropPos.Y + 11.0f}}, Colors::Gold);
        }
    }

    // Cyber Cascade Shader Overlay in Workshop Preview
    if (m_luckyShaderId != 0 && context.Rules.GetRule(Rule::Graphics::EnableShader))
    {
        Vector2f simSize{1160.0f, 540.0f};
        Color luckyTint{0, 240, 255, 175};
        context.Render.DrawScannerEffect(simSize.X, simSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, luckyTint, animTime, m_luckyShaderId);
    }

    // Floating Cyber Question Mark particles in preview
    for (int q = 0; q < 8; ++q)
    {
        float qSeed = static_cast<float>(q) * 1.35f;
        float qx = -480.0f + std::fmod(qSeed * 180.0f + animTime * 45.0f, 960.0f);
        float qy = 190.0f - std::fmod(animTime * (48.0f + static_cast<float>(q * 8)) + qSeed * 95.0f, 420.0f);
        float qRot = std::sin(animTime * 1.5f + qSeed) * 0.35f;
        float pulseA = 0.5f + 0.5f * std::sin(animTime * 3.0f + qSeed);
        uint8_t qa = static_cast<uint8_t>(140.0f + pulseA * 95.0f);

        Color qCol = (q % 3 == 0) ? Color{0, 240, 255, qa} : ((q % 3 == 1) ? Color{255, 20, 147, qa} : Color{255, 200, 30, qa});
        Color qGlow = (q % 3 == 1) ? Color{0, 240, 255, static_cast<uint8_t>(qa * 0.50f)} : Color{255, 20, 147, static_cast<uint8_t>(qa * 0.50f)};

        Transform2D qTrans{Vector2f{qx, qy}, qRot, Vector2f{1.0f, 1.0f}};
        context.Render.DrawTextCentered("?", m_fontId, 36.0f, Transform2D{Vector2f{qx + 1.5f, qy + 1.5f}, qRot, Vector2f{1.15f, 1.15f}}, qGlow);
        context.Render.DrawTextCentered("?", m_fontId, 32.0f, qTrans, qCol);
    }

    // Dynamic HUD Overlay
    context.Render.DrawText("[ POWER-UP RAIN ]  MK-" + std::to_string(level), m_fontId, 24.0f, Transform2D{Vector2f{-540.0f, -240.0f}}, Colors::Gold);
    context.Render.DrawText("ORBITAL CASCADE OF ENHANCEMENT CAPSULES", m_fontId, 16.0f, Transform2D{Vector2f{-540.0f, -212.0f}}, Color{240, 220, 140, 220});

    std::string dropStr;
    switch (level)
    {
    case 1: dropStr = "DROP: 5 - 12 (LVL 1-3)"; break;
    case 2: dropStr = "DROP: 10 - 24 (LVL 1-3)"; break;
    case 3: dropStr = "DROP: 15 - 36 (LVL 1-4)"; break;
    case 4: dropStr = "DROP: 20 - 48 (LVL 2-5)"; break;
    case 5: dropStr = "DROP: 25 - 60 (LVL 2-5)"; break;
    default: dropStr = "DROP: CASCADE ACTIVE"; break;
    }
    context.Render.DrawText(dropStr, m_fontId, 20.0f, Transform2D{Vector2f{240.0f, -240.0f}}, Colors::Gold);

    std::string tempoStr = "BURST DURATION: 2.8s | MULTI-TIER SPAWN";
    context.Render.DrawText(tempoStr, m_fontId, 20.0f, Transform2D{Vector2f{-540.0f, 220.0f}}, Color{140, 230, 180, 240});
    context.Render.DrawText("AUTONOMOUS PARACHUTE: ENGAGED", m_fontId, 16.0f, Transform2D{Vector2f{220.0f, 220.0f}}, Color{160, 200, 180, 200});
}

