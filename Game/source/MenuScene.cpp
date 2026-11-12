#include "Scenes/MenuScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/PreviewScene.h"
#include <SFML/Graphics/RenderTexture.hpp>
#include "Scenes/LevelSelectScene.h"
#include "Scenes/Sandbox.h"
#include "Scenes/SandboxScene.h"
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
    m_plasmaShaderId = context.Resources.LoadResource("Resources/shaders/plasma.frag");
    m_channelSwitchSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_tvStaticSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_static.wav");
    m_tvSwitchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");
    m_tvHoverSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_hover.wav");
    m_paddleTexId = context.Resources.LoadResource("Resources/sprite/paddle.png");
    m_ballTexId = context.Resources.LoadResource("Resources/sprite/ball.png");
    m_brickTexId = context.Resources.LoadResource("Resources/sprite/brick.png");
    m_brickCrackTexId = context.Resources.LoadResource("Resources/sprite/brick_crack.png");
    m_fireTexId = context.Resources.LoadResource("Resources/sprite/fire.png");
    m_heartTexId = context.Resources.LoadResource("Resources/sprite/heart.png");

    // 2D Sprite Asset Pipeline: Workshop Power-Up Badges (#88)
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::MultiBall)]    = context.Resources.LoadResource("Resources/sprite/workshop/badge_multiball.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::ExpandPaddle)] = context.Resources.LoadResource("Resources/sprite/workshop/badge_expand.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::ShrinkPaddle)] = context.Resources.LoadResource("Resources/sprite/workshop/badge_shrink.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::Storm)]        = context.Resources.LoadResource("Resources/sprite/workshop/badge_storm.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::TempoBall)]    = context.Resources.LoadResource("Resources/sprite/workshop/badge_tempo.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::ExtraLife)]    = context.Resources.LoadResource("Resources/sprite/workshop/badge_extralife.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::BigBall)]      = context.Resources.LoadResource("Resources/sprite/workshop/badge_bigball.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::FireBall)]     = context.Resources.LoadResource("Resources/sprite/workshop/badge_fireball.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::StickyPaddle)] = context.Resources.LoadResource("Resources/sprite/workshop/badge_sticky.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::BottomBarrier)]= context.Resources.LoadResource("Resources/sprite/workshop/badge_barrier.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::MagnetPaddle)] = context.Resources.LoadResource("Resources/sprite/workshop/badge_magnet.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::GhostBall)]    = context.Resources.LoadResource("Resources/sprite/workshop/badge_ghost.png");
    m_workshopBadgeTexIds[static_cast<size_t>(PowerUpType::ItemRegen)]    = context.Resources.LoadResource("Resources/sprite/workshop/badge_regen.png");

    // Initialize persisted audio & render preferences
    float sfxVol = PlayerPrefs::GetFloat("SfxVolume", 100.0f);
    float musicVol = PlayerPrefs::GetFloat("MusicVolume", 100.0f);
    context.Audio.SetSfxVolume(sfxVol);
    context.Audio.SetMusicVolume(musicVol);

    // Play Main Menu background music smoothly in loop
    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);

    bool vsync = PlayerPrefs::GetBool("VSync", false);
    context.Render.GetWindow().SetVSync(vsync);

    m_levelManager.Initialize("Resources/levels");
    m_leaderboard.Load();

    InitRetroBackground(context);
    InitTVChannels();
    UpdateTVLayout(context);
    m_masterVolume = context.Audio.GetMasterVolume();
    m_volumeKnobAngle = (m_masterVolume / 100.0f) * 270.0f - 135.0f;

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

    // Initialize CRT TV Screen Offscreen Surface
    m_tvScreenTexture = std::make_unique<sf::RenderTexture>();
    sf::ContextSettings rtSettings;
    rtSettings.antiAliasingLevel = 4;
    (void)m_tvScreenTexture->resize(sf::Vector2u(1390, 910), rtSettings);
    m_tvScreenTexture->setSmooth(true);

    // Initialize Workshop Embedded Preview Simulation
    m_previewTexture = std::make_unique<sf::RenderTexture>();
    (void)m_previewTexture->resize(sf::Vector2u(1280, 600), rtSettings);
    m_previewTexture->setSmooth(true);

    m_ultimatePreviewTexture = std::make_unique<sf::RenderTexture>();
    (void)m_ultimatePreviewTexture->resize(sf::Vector2u(1160, 540), rtSettings);
    m_ultimatePreviewTexture->setSmooth(true);

    const UltimateAbilityType ultTypes[3] = {
        UltimateAbilityType::PlasmaOrb,
        UltimateAbilityType::DiscoBall,
        UltimateAbilityType::PowerUpRain
    };
    UltimateAbilityType equippedUlt = ProfileManager::Get().GetEquippedUltimate();
    m_lastInspectedUltimate = equippedUlt;

    for (int i = 0; i < 3; ++i)
    {
        m_ultimateCardTextures[i] = std::make_unique<sf::RenderTexture>();
        (void)m_ultimateCardTextures[i]->resize(sf::Vector2u(1160, 520), rtSettings);
        m_ultimateCardTextures[i]->setSmooth(true);

        m_ultimatePreviewScenes[i] = std::make_unique<PreviewScene>();
        m_ultimatePreviewScenes[i]->SetEmbeddedMode(true);
        m_ultimatePreviewScenes[i]->SetMusicEnabled(false);
        m_ultimatePreviewScenes[i]->SetAudioEnabled(false);
        m_ultimatePreviewScenes[i]->OnInit(context);
        m_ultimatePreviewScenes[i]->SetMusicEnabled(false);
        m_ultimatePreviewScenes[i]->SetOnResetCallback([this, i]() {
            if (i < static_cast<int>(m_ultimateCards.size()))
            {
                m_ultimateCards[i].SwitchTimer = 0.38f;
            }
            if (i < static_cast<int>(m_ultimateCards.size()) && m_ultimateCards[i].Type == m_lastInspectedUltimate && mp_context)
            {
                mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
            }
        });
        int lvl = ProfileManager::Get().GetUltimateLevel(ultTypes[i]);
        m_ultimatePreviewScenes[i]->SetPreviewUltimate(ultTypes[i], lvl);
    }

    m_previewScene = std::make_unique<PreviewScene>();
    m_previewScene->SetEmbeddedMode(true);
    m_previewScene->SetMusicEnabled(false);
    m_previewScene->SetAudioEnabled(false);
    m_previewScene->OnInit(context);
    m_previewScene->SetMusicEnabled(false);
    m_previewScene->SetAudioEnabled(false);
    m_previewScene->SetOnResetCallback([this]() {
        m_previewSwitchTimer = 0.38f;
    });
    m_lastPreviewPowerUp = m_selectedPowerUp;
    m_lastPreviewLevel = m_inspectedLevel;
    m_lastEquippedUltimate = ProfileManager::Get().GetEquippedUltimate();
    m_lastInspectedUltimate = m_lastEquippedUltimate;
    int realLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
    m_previewScene->SetPreviewPowerUp(m_selectedPowerUp, m_inspectedLevel);
    PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLvl);

    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);

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
        if (m_activeModal == m_workshopCanvas && m_workshopActiveTabCanvas == m_workshopUltimatesCanvas)
        {
            UltimateAbilityType ultToTest = m_lastInspectedUltimate;
            int ultLvl = ProfileManager::Get().GetUltimateLevel(ultToTest);
            for (const auto& card : m_ultimateCards)
            {
                if (card.Type == ultToTest)
                {
                    ultLvl = card.InspectedLevel;
                    break;
                }
            }
            PreviewScene::SetInitialUltimate(ultToTest, ultLvl);
        }
        else
        {
            PreviewScene::SetInitialPowerUp(m_selectedPowerUp);
        }
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

    // Retro CRT TV Channel Grid navigation & controls when no modal is open
    if (m_activeModal == NULL_ENTITY)
    {
        UpdateCampaignPreviewSimulation(dt);
        UpdateTVControls(dt, context);
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
    if (m_activeModal == m_workshopCanvas)
    {
        if (m_workshopActiveTabCanvas == m_workshopUpgradesCanvas)
        {
            if (m_previewScene)
            {
                if (!m_previewScene->IsAudioEnabled())
                {
                    m_previewScene->SetAudioEnabled(true);
                    m_previewScene->SetMusicEnabled(false);
                }

                int realLevel = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
                PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, m_inspectedLevel);

                m_previewScene->OnUpdate(dt, context);

                PowerUpManager::Get().SetLevelTransient(m_selectedPowerUp, realLevel);
            }
        }
        else if (m_workshopActiveTabCanvas == m_workshopUltimatesCanvas)
        {
            if (m_previewScene && m_previewScene->IsAudioEnabled())
            {
                m_previewScene->SetAudioEnabled(false);
            }

            UltimateAbilityType equippedUlt = ProfileManager::Get().GetEquippedUltimate();
            if (equippedUlt != m_lastEquippedUltimate)
            {
                m_lastEquippedUltimate = equippedUlt;
                m_lastInspectedUltimate = equippedUlt;
                if (mp_context)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                }
                for (int i = 0; i < 3; ++i)
                {
                    if (m_ultimatePreviewScenes[i])
                    {
                        m_ultimatePreviewScenes[i]->ClearActiveUltimates();
                        m_ultimatePreviewScenes[i]->ResetPreview();
                    }
                }
            }

            int activeIdx = 0;
            if (m_lastInspectedUltimate == UltimateAbilityType::DiscoBall) activeIdx = 1;
            else if (m_lastInspectedUltimate == UltimateAbilityType::PowerUpRain) activeIdx = 2;

            for (int i = 0; i < 3; ++i)
            {
                if (m_ultimatePreviewScenes[i])
                {
                    bool isActive = (i == activeIdx);
                    m_ultimatePreviewScenes[i]->SetAudioEnabled(isActive);
                    m_ultimatePreviewScenes[i]->SetMusicEnabled(isActive);
                    m_ultimatePreviewScenes[i]->OnUpdate(dt, context);
                }
            }

            // Click anywhere on an ultimate card's viewport to inspect that ultimate
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                auto [mx, my] = context.Input.GetMousePosition();
                Vector2f mPos{mx, my};
                float screenCenterX = context.Render.GetLogicalViewSize().X * 0.5f;
                float screenCenterY = context.Render.GetLogicalViewSize().Y * 0.5f;
                const float ultColX[3] = {-720.0f, 0.0f, 720.0f};

                for (int i = 0; i < 3; ++i)
                {
                    float x = screenCenterX + ultColX[i];
                    Vector2f simCenter{x, screenCenterY - 265.0f};
                    if (std::abs(mPos.X - simCenter.X) <= 290.0f && std::abs(mPos.Y - simCenter.Y) <= 130.0f)
                    {
                        if (m_lastInspectedUltimate != m_ultimateCards[i].Type)
                        {
                            m_lastInspectedUltimate = m_ultimateCards[i].Type;
                            m_ultimateCards[i].SwitchTimer = 0.38f;
                            if (m_ultimatePreviewScenes[i])
                            {
                                m_ultimatePreviewScenes[i]->ResetPreview();
                            }
                            if (m_channelSwitchSfxId != 0 && mp_context)
                            {
                                mp_context->Audio.PlaySfx(m_channelSwitchSfxId, 80.0f, 1.2f);
                            }
                            RefreshWorkshopUI();
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            if (m_previewScene && m_previewScene->IsAudioEnabled())
            {
                m_previewScene->SetAudioEnabled(false);
            }
        }
    }
    else
    {
        if (m_previewScene && m_previewScene->IsAudioEnabled())
        {
            m_previewScene->SetAudioEnabled(false);
        }
    }

    UpdateRetroBackground(dt, context);
    UpdateMusicParticles(dt);
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

    if (m_activeModal == m_workshopCanvas)
    {
        RenderWorkshopBackgroundVisuals(context);
    }

    if (m_activeModal == NULL_ENTITY)
    {
        UpdateTVLayout(context);
        RenderRetroTVChassis(context);
        RenderCRTBezelAndGlass(context);

        if (m_tvScreenTexture)
        {
            if (!m_isStandby)
            {
                m_tvScreenTexture->clear(sf::Color(10, 14, 22, 255));
                context.Render.PushRenderTarget(*m_tvScreenTexture);
                RenderTVScreenContent(context);
                context.Render.PopRenderTarget();
                m_tvScreenTexture->display();

                uint32_t crtShader = (context.Rules.GetRule(Rule::Graphics::EnableShader)) ? m_previewShaderId : 0;
                context.Render.DrawRawTexture(
                    m_tvScreenTexture->getTexture(),
                    m_tvLayout.GlassSize.X,
                    m_tvLayout.GlassSize.Y,
                    Transform2D{m_tvLayout.GlassCenter},
                    Colors::White,
                    crtShader,
                    m_animTime,
                    m_channelSwitchGlitch
                );
            }
            else
            {
                // Standby mode: Cathode ray tube is off (deep dark glass with subtle reflection)
                context.Render.DrawRectangle(m_tvLayout.GlassSize.X, m_tvLayout.GlassSize.Y, Transform2D{m_tvLayout.GlassCenter}, Color{4, 6, 10, 255});

                // Subtle pulsating amber standby prompt
                uint8_t sbPulse = static_cast<uint8_t>(100 + 75 * std::sin(m_animTime * 3.0f));
                context.Render.DrawTextCentered("STANDBY  //  PRESS ANY KEY TO WAKE", m_fontId, m_tvLayout.ToScreenDim(14.0f),
                    Transform2D{m_tvLayout.GlassCenter + Vector2f{0.0f, m_tvLayout.GlassSize.Y * 0.40f}}, Color{255, 175, 55, sbPulse});
            }
        }

        RenderMusicParticles(context);

        // Subtle diagonal glass reflection
        const auto& L = m_tvLayout;
        context.Render.DrawLine(L.ToScreenPos(Vector2f{150.0f, 120.0f}), L.ToScreenPos(Vector2f{700.0f, 120.0f}), Color{255, 255, 255, 18}, L.ToScreenDim(2.0f));
        context.Render.DrawLine(L.ToScreenPos(Vector2f{150.0f, 130.0f}), L.ToScreenPos(Vector2f{500.0f, 130.0f}), Color{255, 255, 255, 12}, L.ToScreenDim(1.5f));

        RenderTVOSD(context);
        RenderPowerOffDialog(context);
        RenderPowerOffAnimation(context);

        // Pleasant CRT aperture expansion beam & phosphor flare during LaunchingFullScreen
        if (m_zoomState == ChannelZoomState::LaunchingFullScreen && m_launchProgress > 0.60f &&
            m_selectedChannel >= 0 && m_selectedChannel < static_cast<int>(m_tvChannels.size()))
        {
            const Vector2f viewSize = context.Render.GetLogicalViewSize();
            const Vector2f viewCenter = viewSize * 0.5f;
            const float flareT = (m_launchProgress - 0.60f) / 0.40f;
            const float easeFlare = flareT * flareT;
            const auto& ch = m_tvChannels[m_selectedChannel];

            // 1. Horizontal cathode flare streak expanding across the display
            float streakH = 16.0f + 320.0f * easeFlare;
            uint8_t streakAlpha = static_cast<uint8_t>(std::min(255.0f, easeFlare * 230.0f));
            context.Render.DrawRectangle(viewSize.X * 1.3f, streakH, Transform2D{viewCenter},
                Color{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, streakAlpha});

            // 2. Searing white-hot core laser beam
            float coreH = 6.0f + 140.0f * easeFlare;
            uint8_t coreAlpha = static_cast<uint8_t>(std::min(255.0f, easeFlare * 255.0f));
            context.Render.DrawRectangle(viewSize.X * 1.3f, coreH, Transform2D{viewCenter}, Color{255, 255, 255, coreAlpha});

            // 3. Phosphor circular resonance wave
            if (flareT > 0.35f)
            {
                float waveT = (flareT - 0.35f) / 0.65f;
                float waveR = viewSize.Y * (0.15f + waveT * 0.85f);
                uint8_t waveAlpha = static_cast<uint8_t>((1.0f - waveT) * 180.0f);
                context.Render.DrawCircleOutline(waveR, Transform2D{viewCenter},
                    Color{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, waveAlpha}, 3.0f);
                context.Render.DrawCircleOutline(waveR * 0.65f, Transform2D{viewCenter},
                    Color{255, 255, 255, static_cast<uint8_t>(waveAlpha * 0.8f)}, 2.0f);
            }

            // 4. Full-screen flash transition wipe at peak
            if (flareT > 0.85f)
            {
                uint8_t flashA = static_cast<uint8_t>(((flareT - 0.85f) / 0.15f) * 255.0f);
                context.Render.DrawRectangle(viewSize.X * 1.2f, viewSize.Y * 1.2f, Transform2D{viewCenter}, Color{255, 255, 255, flashA});
            }
        }
    }

    UISystem::OnRender(m_registry, context);

    if (m_activeModal == m_workshopCanvas)
    {
        RenderWorkshopForegroundVisuals(context);
    }
}

void MenuScene::OnDestroy(GameContext& context)
{
    if (m_previewScene)
    {
        m_previewScene->OnDestroy(context);
        m_previewScene.reset();
    }
    for (int i = 0; i < 3; ++i)
    {
        if (m_ultimatePreviewScenes[i])
        {
            m_ultimatePreviewScenes[i]->OnDestroy(context);
            m_ultimatePreviewScenes[i].reset();
        }
        m_ultimateCardTextures[i].reset();
    }
    m_previewTexture.reset();
    m_ultimatePreviewTexture.reset();
    m_tvScreenTexture.reset();
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
    params.Time = m_animTime;
    params.PreviewRect[0] = 0.0f;
    params.PreviewRect[1] = 0.0f;
    params.PreviewRect[2] = 0.0f;
    params.PreviewRect[3] = 0.0f;
    params.GlitchIntensity = m_channelSwitchGlitch;
    params.ScanlineFlicker = (m_channelSwitchGlitch > 0.01f) ? 0.4f : 0.05f;
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
            m_previewScene->SetAudioEnabled(false);
            m_previewScene->ResetPreview();
        }
        for (int i = 0; i < 3; ++i)
        {
            if (m_ultimatePreviewScenes[i])
            {
                m_ultimatePreviewScenes[i]->SetMusicEnabled(false);
                m_ultimatePreviewScenes[i]->SetAudioEnabled(false);
                m_ultimatePreviewScenes[i]->ResetPreview();
            }
        }
        if (mp_context)
        {
            mp_context->Audio.StopSfx();
            mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
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
    m_descModalForUltimate = false;
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
    m_descModalForUltimate = false;
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
            bool isLit = ((static_cast<float>(i) * 10.0f) < volume);

            if (i < 5)
                panel.Tint = isLit ? Color{50, 255, 100, 255} : Color{18, 65, 30, 90};
            else if (i < 8)
                panel.Tint = isLit ? Color{255, 240, 50, 255} : Color{70, 65, 15, 90};
            else if (i == 8)
                panel.Tint = isLit ? Color{255, 140, 25, 255} : Color{75, 40, 12, 90};
            else
                panel.Tint = isLit ? Color{255, 45, 55, 255} : Color{75, 18, 18, 90};
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

void MenuScene::CreateMainMenuUI(const GameContext& /*context*/)
{
    m_mainCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_mainCanvas, CanvasComponent{.IsEnabled = false});
}

void MenuScene::InitTVChannels()
{
    m_tvChannels.clear();
    m_tvChannels.reserve(8);

    const float colX[4] = {295.0f, 630.0f, 965.0f, 1300.0f};
    const float rowY[2] = {310.0f, 535.0f};
    const Vector2f cardSize{280.0f, 185.0f};

    // 0: Campaign
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Campaign,
        .ChannelNumber = "CH 01",
        .Title = "CAMPAIGN",
        .Subtitle = "ARCADE ODYSSEY",
        .Description = "Embark on the core arcade journey across dynamic arenas with temporal events!",
        .ThemeColor = Color{80, 220, 255, 255},
        .CardBgColor = Color{18, 26, 42, 245},
        .Position = Vector2f{colX[0], rowY[0]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[0], rowY[0]},
        .BaseSize = cardSize
    });

    // 1: Level Select
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::LevelSelect,
        .ChannelNumber = "CH 02",
        .Title = "STAGE SELECT",
        .Subtitle = "SECTOR DIRECTORY",
        .Description = "Choose your arena, browse community maps, and pick custom sector missions.",
        .ThemeColor = Color{255, 175, 55, 255},
        .CardBgColor = Color{28, 24, 18, 245},
        .Position = Vector2f{colX[1], rowY[0]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[1], rowY[0]},
        .BaseSize = cardSize
    });

    // 2: Tetris Endless
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::TetrisEndless,
        .ChannelNumber = "CH 03",
        .Title = "ENDLESS",
        .Subtitle = "SURVIVAL MATRIX",
        .Description = "Test your endurance against endless falling brick matrices and accelerating physics.",
        .ThemeColor = Color{245, 65, 115, 255},
        .CardBgColor = Color{28, 16, 26, 245},
        .Position = Vector2f{colX[2], rowY[0]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[2], rowY[0]},
        .BaseSize = cardSize
    });

    // 3: Workshop
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Workshop,
        .ChannelNumber = "CH 04",
        .Title = "WORKSHOP",
        .Subtitle = "UPGRADES & TECH",
        .Description = "Upgrade paddle chassis, test devastating ultimates, and tune ball physics.",
        .ThemeColor = Color{255, 215, 60, 255},
        .CardBgColor = Color{28, 25, 16, 245},
        .Position = Vector2f{colX[3], rowY[0]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[3], rowY[0]},
        .BaseSize = cardSize
    });

    // 4: Leaderboard
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Leaderboard,
        .ChannelNumber = "CH 05",
        .Title = "LEADERBOARD",
        .Subtitle = "ARCADE RANKINGS",
        .Description = "View global arcade rankings, stage records, and personal best scores.",
        .ThemeColor = Color{165, 105, 255, 255},
        .CardBgColor = Color{24, 18, 32, 245},
        .Position = Vector2f{colX[0], rowY[1]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[0], rowY[1]},
        .BaseSize = cardSize
    });

    // 5: Achievements
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Achievements,
        .ChannelNumber = "CH 06",
        .Title = "PROFILE",
        .Subtitle = "CAREER STATS",
        .Description = "Inspect unlocked badges, career stats, prestige rank, and performance medals.",
        .ThemeColor = Color{50, 225, 140, 255},
        .CardBgColor = Color{16, 28, 22, 245},
        .Position = Vector2f{colX[1], rowY[1]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[1], rowY[1]},
        .BaseSize = cardSize
    });

    // 6: Sandbox
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Sandbox,
        .ChannelNumber = "CH 07",
        .Title = "SANDBOX ROOM",
        .Subtitle = "PHYSICS LAB",
        .Description = "Freeform physics playground to spawn bricks, test weapons, and experiment.",
        .ThemeColor = Color{60, 190, 255, 255},
        .CardBgColor = Color{16, 24, 32, 245},
        .Position = Vector2f{colX[2], rowY[1]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[2], rowY[1]},
        .BaseSize = cardSize
    });

    // 7: Settings
    m_tvChannels.push_back(TVChannel{
        .Type = ChannelType::Settings,
        .ChannelNumber = "CH 08",
        .Title = "SETTINGS",
        .Subtitle = "SYSTEM OPTIONS",
        .Description = "Configure CRT shaders, master audio mixing, input bindings, and display modes.",
        .ThemeColor = Color{180, 205, 230, 255},
        .CardBgColor = Color{20, 24, 30, 245},
        .Position = Vector2f{colX[3], rowY[1]},
        .Size = cardSize,
        .BasePosition = Vector2f{colX[3], rowY[1]},
        .BaseSize = cardSize
    });
}

void MenuScene::InitCampaignPreviewLevel()
{
    int unlocked = m_levelManager.GetUnlockedLevel();
    if (m_campaignLoadedLevel == unlocked && !m_campaignBricks.empty()) return;

    m_campaignLoadedLevel = unlocked;
    m_campaignBricks.clear();

    std::string levelPath = m_levelManager.GetLevelPath(unlocked - 1);
    std::ifstream file(levelPath);
    std::string line;
    std::vector<std::string> lines;
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
    }

    if (lines.empty())
    {
        lines = {
            "333333333333",
            "2222B22B2222",
            "1111S11S1111",
            "110011110011"
        };
    }

    int rows = static_cast<int>(lines.size());
    int maxCols = 0;
    for (const auto& l : lines) maxCols = std::max(maxCols, static_cast<int>(l.length()));
    if (maxCols == 0) maxCols = 1;

    for (int r = 0; r < rows; ++r)
    {
        const auto& l = lines[r];
        for (int c = 0; c < static_cast<int>(l.length()); ++c)
        {
            char ch = l[c];
            if (ch == '0' || ch == ' ') continue;

            Color brickCol{80, 200, 255, 240};
            if (ch == '1') brickCol = Color{75, 195, 255, 240};
            else if (ch == '2') brickCol = Color{55, 225, 120, 240};
            else if (ch == '3') brickCol = Color{255, 125, 45, 240};
            else if (ch == 'B' || ch == 'b') brickCol = Color{245, 55, 75, 240};
            else if (ch == 'S' || ch == 's') brickCol = Color{195, 85, 255, 240};
            else if (ch == 'M' || ch == 'm') brickCol = Color{175, 185, 200, 240};

            float normX = ((static_cast<float>(c) + 0.5f) / static_cast<float>(maxCols) - 0.5f) * 1.55f;
            float normY = -0.52f + (static_cast<float>(r) / static_cast<float>(rows)) * 0.44f;

            m_campaignBricks.push_back(CampaignPreviewBrick{
                .RelX = normX,
                .RelY = normY,
                .BaseColor = brickCol,
                .Alive = true
            });
        }
    }

    m_campaignPreviewTimer = 0.0f;
    m_campaignBallPos = Vector2f{0.0f, 0.35f};
    m_campaignBallVel = Vector2f{0.48f, -0.62f};
    m_campaignPaddleX = 0.0f;
}

void MenuScene::UpdateCampaignPreviewSimulation(float dt)
{
    InitCampaignPreviewLevel();

    m_campaignPreviewTimer += dt;
    if (m_campaignPreviewTimer >= 10.0f)
    {
        m_campaignPreviewTimer = 0.0f;
        for (auto& b : m_campaignBricks) b.Alive = true;
        m_campaignBallPos = Vector2f{0.0f, 0.35f};
        m_campaignBallVel = Vector2f{0.48f, -0.62f};
        m_campaignPaddleX = 0.0f;
    }

    // Ball movement
    m_campaignBallPos += m_campaignBallVel * dt;

    // Paddle tracking
    m_campaignPaddleX += (m_campaignBallPos.X - m_campaignPaddleX) * std::min(1.0f, dt * 7.5f);

    // Wall bounces
    if (m_campaignBallPos.X < -0.72f)
    {
        m_campaignBallPos.X = -0.72f;
        m_campaignBallVel.X = std::abs(m_campaignBallVel.X);
    }
    else if (m_campaignBallPos.X > 0.72f)
    {
        m_campaignBallPos.X = 0.72f;
        m_campaignBallVel.X = -std::abs(m_campaignBallVel.X);
    }

    if (m_campaignBallPos.Y < -0.62f)
    {
        m_campaignBallPos.Y = -0.62f;
        m_campaignBallVel.Y = std::abs(m_campaignBallVel.Y);
    }
    else if (m_campaignBallPos.Y > 0.42f)
    {
        // Paddle bounce
        m_campaignBallPos.Y = 0.42f;
        m_campaignBallVel.Y = -std::abs(m_campaignBallVel.Y);
        m_campaignBallVel.X = std::clamp((m_campaignBallPos.X - m_campaignPaddleX) * 2.8f, -0.75f, 0.75f);
    }

    // Brick hits
    for (auto& b : m_campaignBricks)
    {
        if (!b.Alive) continue;
        if (std::abs(m_campaignBallPos.X - b.RelX) < 0.08f && std::abs(m_campaignBallPos.Y - b.RelY) < 0.05f)
        {
            b.Alive = false;
            m_campaignBallVel.Y = -m_campaignBallVel.Y;
            break;
        }
    }
}

void MenuScene::RenderCampaignPreview(GameContext& context, const Vector2f& pos, float sc, bool isZoomed) const
{
    float arenaW = isZoomed ? (780.0f * sc) : (230.0f * sc);
    float arenaH = isZoomed ? (420.0f * sc) : (110.0f * sc);

    // Arena boundary & backdrop
    context.Render.DrawRectangle(arenaW, arenaH, Transform2D{pos}, Color{10, 14, 22, 220});
    context.Render.DrawRectangleOutline(arenaW, arenaH, Transform2D{pos}, Color{45, 75, 115, 180}, 1.5f * sc);

    // Grid lines in background
    for (int g = -2; g <= 2; ++g)
    {
        float gx = pos.X + static_cast<float>(g) * (arenaW * 0.18f);
        context.Render.DrawLine(Vector2f{gx, pos.Y - arenaH * 0.5f}, Vector2f{gx, pos.Y + arenaH * 0.5f}, Color{255, 255, 255, 10}, 1.0f * sc);
    }

    // Bricks
    float bW = isZoomed ? (48.0f * sc) : (14.0f * sc);
    float bH = isZoomed ? (16.0f * sc) : (5.5f * sc);

    for (const auto& b : m_campaignBricks)
    {
        if (!b.Alive) continue;
        Vector2f bp = pos + Vector2f{b.RelX * (arenaW * 0.45f), b.RelY * (arenaH * 0.45f)};
        context.Render.DrawRectangle(bW, bH, Transform2D{bp}, b.BaseColor);
        context.Render.DrawRectangleOutline(bW, bH, Transform2D{bp}, Colors::White, 1.0f * sc);
    }

    // Paddle
    float padW = isZoomed ? (85.0f * sc) : (32.0f * sc);
    float padH = isZoomed ? (14.0f * sc) : (5.0f * sc);
    Vector2f padPos = pos + Vector2f{m_campaignPaddleX * (arenaW * 0.45f), 0.45f * (arenaH * 0.45f)};
    context.Render.DrawRectangle(padW, padH, Transform2D{padPos}, Color{50, 225, 120, 255});
    context.Render.DrawRectangleOutline(padW, padH, Transform2D{padPos}, Colors::White, 1.2f * sc);

    // Ball
    float ballR = isZoomed ? (7.0f * sc) : (3.5f * sc);
    Vector2f ballPos = pos + Vector2f{m_campaignBallPos.X * (arenaW * 0.45f), m_campaignBallPos.Y * (arenaH * 0.45f)};
    context.Render.DrawCircle(ballR, Transform2D{ballPos}, Colors::White);
    context.Render.DrawCircleOutline(ballR * 1.5f, Transform2D{ballPos}, Color{80, 220, 255, 150}, 1.2f * sc);

    // Fading Title Banner ("un titre qui arrive avec un fondu", 10s preview)
    float titleAlpha = std::clamp(m_campaignPreviewTimer / 1.5f, 0.0f, 1.0f);
    if (m_campaignPreviewTimer > 8.5f)
    {
        titleAlpha *= std::clamp((10.0f - m_campaignPreviewTimer) / 1.5f, 0.0f, 1.0f);
    }
    uint8_t a = static_cast<uint8_t>(titleAlpha * 255.0f);

    if (a > 2)
    {
        std::string stageTitle = "STAGE " + std::to_string(m_campaignLoadedLevel);
        Vector2f titlePos = pos + Vector2f{0.0f, isZoomed ? (-arenaH * 0.38f) : (-arenaH * 0.32f)};
        float fSize = isZoomed ? (28.0f * sc) : (14.0f * sc);
        float pillW = isZoomed ? (240.0f * sc) : (110.0f * sc);
        float pillH = isZoomed ? (38.0f * sc) : (18.0f * sc);

        context.Render.DrawRectangle(pillW, pillH, Transform2D{titlePos}, Color{12, 18, 28, static_cast<uint8_t>(a * 0.85f)});
        context.Render.DrawRectangleOutline(pillW, pillH, Transform2D{titlePos}, Color{80, 220, 255, a}, 1.5f * sc);
        context.Render.DrawTextCentered(stageTitle, m_fontId, fSize, Transform2D{titlePos}, Color{240, 250, 255, a});
    }
}

void MenuScene::RenderTwoGears(GameContext& context, const Vector2f& pos, float sc) const
{
    // Gear 1 (Left, larger): R = 28 * sc, Teeth = 10, Clockwise
    const float r1 = 28.0f * sc;
    const int N1 = 10;
    const float ang1 = m_animTime * 1.5f;
    const Vector2f p1 = pos + Vector2f{-22.0f * sc, -4.0f * sc};

    // Gear 2 (Right, smaller): R = 18 * sc, Teeth = 6, Counter-clockwise
    const float r2 = 18.0f * sc;
    const int N2 = 6;
    const float ang2 = -ang1 * (r1 / r2) + (3.14159265f / static_cast<float>(N2));
    const Vector2f p2 = pos + Vector2f{24.0f * sc, 10.0f * sc};

    auto drawSingleGear = [&](const Vector2f& center, float r, int teeth, float ang, Color bodyCol, Color rimCol) {
        for (int k = 0; k < teeth; ++k)
        {
            float tAng = ang + static_cast<float>(k) * (6.2831853f / static_cast<float>(teeth));
            Vector2f tPos = center + Vector2f{std::cos(tAng) * (r * 0.95f), std::sin(tAng) * (r * 0.95f)};
            float tW = r * 0.32f;
            float tH = r * 0.34f;
            context.Render.DrawRectangle(tW, tH, Transform2D{tPos, tAng}, bodyCol);
            context.Render.DrawRectangleOutline(tW, tH, Transform2D{tPos, tAng}, rimCol, 1.2f * sc);
        }

        context.Render.DrawCircle(r * 0.85f, Transform2D{center}, bodyCol);
        context.Render.DrawCircleOutline(r * 0.85f, Transform2D{center}, rimCol, 1.8f * sc);
        context.Render.DrawCircleOutline(r * 0.55f, Transform2D{center}, Color{static_cast<uint8_t>(std::min(255, bodyCol.r + 25)), static_cast<uint8_t>(std::min(255, bodyCol.g + 25)), static_cast<uint8_t>(std::min(255, bodyCol.b + 25)), 200}, 1.2f * sc);
        context.Render.DrawCircle(r * 0.28f, Transform2D{center}, Color{16, 20, 26, 255});
        context.Render.DrawCircleOutline(r * 0.28f, Transform2D{center}, rimCol, 1.2f * sc);
        context.Render.DrawCircle(r * 0.12f, Transform2D{center}, Color{235, 240, 250, 255});
    };

    drawSingleGear(p1, r1, N1, ang1, Color{130, 142, 160, 240}, Color{210, 225, 245, 255});
    drawSingleGear(p2, r2, N2, ang2, Color{105, 115, 130, 240}, Color{185, 200, 220, 255});
}

void MenuScene::RenderWiiZoomedChannel(GameContext& context)
{
    if (m_selectedChannel < 0 || m_selectedChannel >= static_cast<int>(m_tvChannels.size())) return;
    const auto& ch = m_tvChannels[m_selectedChannel];

    float p = m_zoomProgress;
    float smoothT = p * p * (3.0f - 2.0f * p);

    // Dim cathode glass background
    uint8_t bgDimAlpha = static_cast<uint8_t>(smoothT * 220.0f);
    context.Render.DrawRectangle(1390.0f, 910.0f, Transform2D{Vector2f{695.0f, 455.0f}}, Color{6, 8, 14, bgDimAlpha});

    // Zoomed card occupies 100% of the CRT TV screen (1390 x 910)
    Vector2f targetCenter{695.0f, 455.0f};
    Vector2f targetSize{1390.0f, 910.0f};

    Vector2f startPos = ch.BasePosition - Vector2f{105.0f, 85.0f};
    Vector2f startSize = ch.BaseSize;

    Vector2f curPos = startPos + (targetCenter - startPos) * smoothT;
    Vector2f curSize = startSize + (targetSize - startSize) * smoothT;

    context.Render.DrawRectangle(curSize.X, curSize.Y, Transform2D{curPos}, ch.CardBgColor);
    Color glowCol{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, static_cast<uint8_t>(200 + 55 * std::sin(m_animTime * 5.0f))};
    context.Render.DrawRectangleOutline(curSize.X, curSize.Y, Transform2D{curPos}, glowCol, 4.0f);

    if (smoothT > 0.35f)
    {
        float contentAlpha = std::clamp((smoothT - 0.35f) / 0.65f, 0.0f, 1.0f);
        float uiFade = (m_zoomState == ChannelZoomState::LaunchingFullScreen) ? std::clamp(1.0f - m_launchProgress * 2.8f, 0.0f, 1.0f) : 1.0f;
        uint8_t cAlpha = static_cast<uint8_t>(contentAlpha * 255.0f);
        uint8_t uiAlpha = static_cast<uint8_t>(contentAlpha * uiFade * 255.0f);

        // 1. Top Channel Header Bar (Full width across top of TV screen)
        Vector2f headerPos = curPos - Vector2f{0.0f, curSize.Y * 0.5f - 40.0f};
        context.Render.DrawRectangle(curSize.X, 60.0f, Transform2D{headerPos}, Color{16, 22, 34, cAlpha});
        context.Render.DrawRectangleOutline(curSize.X, 60.0f, Transform2D{headerPos}, Color{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, static_cast<uint8_t>(cAlpha * 0.7f)}, 2.0f);
        context.Render.DrawText(ch.ChannelNumber + "  //  " + ch.Title, m_fontId, 26.0f,
                                Transform2D{headerPos - Vector2f{curSize.X * 0.5f - 40.0f, 16.0f}},
                                Color{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, cAlpha});
        context.Render.DrawText(ch.Subtitle, m_fontId, 20.0f,
                                Transform2D{headerPos + Vector2f{curSize.X * 0.5f - 320.0f, -12.0f}},
                                Color{200, 215, 235, cAlpha});

        // 2. Center Large Animated Vignette / Preview (scaled up beautifully)
        Vector2f centerVignettePos = curPos - Vector2f{0.0f, 25.0f};
        float vignetteSc = (m_zoomState == ChannelZoomState::LaunchingFullScreen)
            ? (1.85f * (1.0f + m_launchProgress * 0.35f))
            : (1.85f * smoothT);

        if (ch.Type == ChannelType::Campaign)
        {
            RenderCampaignPreview(context, centerVignettePos, vignetteSc, true);
        }
        else
        {
            RenderChannelVignette(context, ch, centerVignettePos, vignetteSc);
        }

        // 3. Channel Description Banner
        if (uiAlpha > 2)
        {
            Vector2f descPos = curPos + Vector2f{0.0f, curSize.Y * 0.5f - 145.0f};
            context.Render.DrawRectangle(1050.0f, 44.0f, Transform2D{descPos}, Color{12, 16, 26, static_cast<uint8_t>(uiAlpha * 0.85f)});
            context.Render.DrawRectangleOutline(1050.0f, 44.0f, Transform2D{descPos}, Color{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, static_cast<uint8_t>(uiAlpha * 0.6f)}, 1.5f);
            context.Render.DrawTextCentered(ch.Description, m_fontId, 19.0f, Transform2D{descPos}, Color{225, 235, 250, uiAlpha});

            // 4. Wii Channel Style Action Buttons: [ RETOUR ] and [ ENTRER ]
            Vector2f btnRetourPos = curPos + Vector2f{-240.0f, curSize.Y * 0.5f - 65.0f};
            Vector2f btnEntrerPos = curPos + Vector2f{240.0f, curSize.Y * 0.5f - 65.0f};
            Vector2f btnSize{260.0f, 62.0f};

            // RETOUR Button
            bool retourActive = (m_zoomChoice == 0);
            Color retourBg = retourActive ? Color{45, 65, 95, uiAlpha} : Color{20, 26, 38, uiAlpha};
            Color retourBorder = retourActive ? Color{90, 200, 255, uiAlpha} : Color{60, 75, 100, uiAlpha};
            context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btnRetourPos}, retourBg);
            context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btnRetourPos}, retourBorder, retourActive ? 3.5f : 1.5f);
            context.Render.DrawTextCentered("[ RETOUR ]", m_fontId, 23.0f, Transform2D{btnRetourPos}, Color{240, 245, 255, uiAlpha});

            // ENTRER Button
            bool entrerActive = (m_zoomChoice == 1);
            Color entrerBg = entrerActive ? Color{40, 110, 75, uiAlpha} : Color{20, 48, 35, uiAlpha};
            Color entrerBorder = entrerActive ? Colors::Gold : Color{70, 140, 95, uiAlpha};
            context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btnEntrerPos}, entrerBg);
            context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btnEntrerPos}, entrerBorder, entrerActive ? 3.5f : 1.5f);
            context.Render.DrawTextCentered("[ ENTRER ]", m_fontId, 23.0f, Transform2D{btnEntrerPos}, Color{255, 255, 255, uiAlpha});
        }
    }
}

void MenuScene::UpdateTVLayout(const GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    m_tvLayout.CabinetCenter = Vector2f{viewSize.X * 0.5f, viewSize.Y * 0.5f};

    // Scale base 1880x1040 design uniformly to fit viewport with retro borders
    const float maxScaleX = (viewSize.X * 0.96f) / 1880.0f;
    const float maxScaleY = (viewSize.Y * 0.94f) / 1040.0f;
    const float baseScale = std::min(maxScaleX, maxScaleY);

    if (m_zoomState == ChannelZoomState::LaunchingFullScreen)
    {
        // Smooth camera dive: TV cathode screen expands to fill 100% of the viewport
        const float targetScale = std::max(viewSize.X / 1390.0f, viewSize.Y / 910.0f) * 1.05f;
        const float diveT = m_launchProgress * m_launchProgress * (3.0f - 2.0f * m_launchProgress);
        m_tvLayout.Scale = baseScale + (targetScale - baseScale) * diveT;

        const Vector2f baseGlassCenter = m_tvLayout.CabinetCenter + (Vector2f{800.0f, 540.0f} - Vector2f{960.0f, 540.0f}) * baseScale;
        const Vector2f targetGlassCenter = Vector2f{viewSize.X * 0.5f, viewSize.Y * 0.5f};
        m_tvLayout.GlassCenter = baseGlassCenter + (targetGlassCenter - baseGlassCenter) * diveT;
        m_tvLayout.CabinetCenter = m_tvLayout.GlassCenter - (Vector2f{800.0f, 540.0f} - Vector2f{960.0f, 540.0f}) * m_tvLayout.Scale;
    }
    else
    {
        m_tvLayout.Scale = baseScale;
        m_tvLayout.GlassCenter = m_tvLayout.ToScreenPos(Vector2f{800.0f, 540.0f});
    }

    m_tvLayout.GlassSize = m_tvLayout.ToScreenSize(Vector2f{1390.0f, 910.0f});
    m_tvLayout.GlassRectUV = context.Render.WorldRectToNormalizedUV(m_tvLayout.GlassCenter, m_tvLayout.GlassSize);

    for (auto& ch : m_tvChannels)
    {
        ch.Position = m_tvLayout.ToScreenPos(ch.BasePosition);
        ch.Size = m_tvLayout.ToScreenSize(ch.BaseSize);
    }
}

void MenuScene::UpdateTVControls(float dt, GameContext& context)
{
    // Ensure layout is up-to-date with current window / logical size
    UpdateTVLayout(context);
    const auto& L = m_tvLayout;

    // 1. Power-off shutdown collapse animation in progress
    if (m_isPoweringOff)
    {
        m_powerOffTimer += dt;
        if (m_powerOffTimer >= 0.85f)
        {
            context.Render.GetWindow().Close();
        }
        return;
    }

    auto [mx, my] = context.Input.GetMousePosition();
    Vector2f mPos{mx, my};
    Vector2f baseMousePos = L.ToBasePos(mPos);

    // 0. Standby Mode (TV Cathode Screen Asleep)
    if (m_isStandby)
    {
        m_animTime += dt;
        bool wakeUp = false;
        if (context.Input.GetLastKeyPressed() != KeyCode::Count ||
            context.Input.GetAnyGamepadInputTriggered() >= 0 ||
            context.Input.IsMouseButtonPress(MouseButton::Left) ||
            context.Input.IsMouseButtonPress(MouseButton::Right) ||
            context.Input.IsMenuSelectTriggered() ||
            context.Input.IsMenuCancelTriggered())
        {
            wakeUp = true;
        }

        // Clicking the chassis power button also wakes up the TV
        float distToPowerBtn = std::sqrt((baseMousePos.X - 1690.0f) * (baseMousePos.X - 1690.0f) + (baseMousePos.Y - 220.0f) * (baseMousePos.Y - 220.0f));
        if (distToPowerBtn <= 26.0f && context.Input.IsMouseButtonPress(MouseButton::Left))
        {
            wakeUp = true;
        }

        if (wakeUp)
        {
            m_isStandby = false;
            m_channelSwitchGlitch = 0.5f;
            if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
            if (m_tvStaticSfxId != 0) context.Audio.PlaySfx(m_tvStaticSfxId, 60.0f);
            m_osdText = "POWER ON";
            m_osdTimer = 2.0f;
        }
        return;
    }

    // 2. Power-off dialog active
    if (m_showPowerOffDialog)
    {
        // Cancel via MenuCancel (Escape / Gamepad B)
        if (context.Input.IsMenuCancelTriggered())
        {
            m_showPowerOffDialog = false;
            if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
            return;
        }

        // Navigate between Cancel (0), Standby (1), and Power Off (2)
        if (context.Input.IsMenuLeftTriggered())
        {
            m_powerDialogChoice = (m_powerDialogChoice + 2) % 3;
            if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 75.0f);
        }
        if (context.Input.IsMenuRightTriggered())
        {
            m_powerDialogChoice = (m_powerDialogChoice + 1) % 3;
            if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 75.0f);
        }

        // Confirm choice
        if (context.Input.IsMenuSelectTriggered())
        {
            if (m_powerDialogChoice == 0)
            {
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
            }
            else if (m_powerDialogChoice == 1)
            {
                m_isStandby = true;
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
            }
            else if (m_powerDialogChoice == 2)
            {
                m_isPoweringOff = true;
                m_powerOffTimer = 0.0f;
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 100.0f);
            }
            return;
        }

        // Mouse hover and click on dialog buttons (base coordinates)
        // Cancel button: (605.0f, 620.0f), size (175.0f, 52.0f)
        if (std::abs(baseMousePos.X - 605.0f) <= 87.5f && std::abs(baseMousePos.Y - 620.0f) <= 26.0f)
        {
            m_powerDialogChoice = 0;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
            }
            return;
        }

        // Standby button: (800.0f, 620.0f), size (175.0f, 52.0f)
        if (std::abs(baseMousePos.X - 800.0f) <= 87.5f && std::abs(baseMousePos.Y - 620.0f) <= 26.0f)
        {
            m_powerDialogChoice = 1;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_isStandby = true;
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
            }
            return;
        }

        // Power Off button: (995.0f, 620.0f), size (175.0f, 52.0f)
        if (std::abs(baseMousePos.X - 995.0f) <= 87.5f && std::abs(baseMousePos.Y - 620.0f) <= 26.0f)
        {
            m_powerDialogChoice = 2;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_isPoweringOff = true;
                m_powerOffTimer = 0.0f;
                m_showPowerOffDialog = false;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 100.0f);
            }
            return;
        }
        return;
    }

    // 3. Normal TV operation timers
    m_animTime += dt;
    if (m_osdTimer > 0.0f) m_osdTimer -= dt;
    if (m_channelSwitchGlitch > 0.0f) m_channelSwitchGlitch = std::max(0.0f, m_channelSwitchGlitch - dt * 2.0f);
    if (m_navDebounceTimer > 0.0f) m_navDebounceTimer -= dt;

    if (m_tvChannels.empty()) return;
    if (m_selectedChannel < 0 || m_selectedChannel >= static_cast<int>(m_tvChannels.size()))
    {
        m_selectedChannel = 0;
    }

    // 4. Wii Channel Zoom State Machine
    if (m_zoomState == ChannelZoomState::LaunchingFullScreen)
    {
        m_launchTimer += dt;
        m_launchProgress = std::clamp(m_launchTimer / 0.55f, 0.0f, 1.0f);
        if (m_launchTimer >= 0.55f)
        {
            m_launchProgress = 1.0f;
            LaunchChannel(m_selectedChannel, context);
            m_zoomState = ChannelZoomState::Grid;
            m_zoomProgress = 0.0f;
            m_launchProgress = 0.0f;
            m_launchTimer = 0.0f;
        }
        return;
    }
    if (m_zoomState == ChannelZoomState::ZoomingIn)
    {
        m_zoomTimer += dt;
        m_zoomProgress = std::clamp(m_zoomTimer / 0.30f, 0.0f, 1.0f);
        if (m_zoomTimer >= 0.30f)
        {
            m_zoomProgress = 1.0f;
            m_zoomState = ChannelZoomState::ZoomedView;
        }
        return;
    }
    if (m_zoomState == ChannelZoomState::ZoomingOut)
    {
        m_zoomTimer += dt;
        m_zoomProgress = std::clamp(1.0f - (m_zoomTimer / 0.25f), 0.0f, 1.0f);
        if (m_zoomTimer >= 0.25f)
        {
            m_zoomProgress = 0.0f;
            m_zoomState = ChannelZoomState::Grid;
        }
        return;
    }
    if (m_zoomState == ChannelZoomState::ZoomedView)
    {
        // Left / Right arrows switch between RETOUR (0) and ENTRER (1)
        if (context.Input.IsMenuLeftTriggered() || context.Input.IsMenuRightTriggered())
        {
            m_zoomChoice = 1 - m_zoomChoice;
            if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 75.0f);
        }

        // Escape / Cancel returns to 4x2 grid (RETOUR)
        if (context.Input.IsMenuCancelTriggered())
        {
            m_zoomState = ChannelZoomState::ZoomingOut;
            m_zoomTimer = 0.0f;
            if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
            return;
        }

        // Enter / Select activates the selected button
        if (context.Input.IsMenuSelectTriggered())
        {
            if (m_zoomChoice == 1)
            {
                m_zoomState = ChannelZoomState::LaunchingFullScreen;
                m_launchTimer = 0.0f;
                m_launchProgress = 0.0f;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 100.0f);
                if (m_tvStaticSfxId != 0) context.Audio.PlaySfx(m_tvStaticSfxId, 75.0f);
            }
            else
            {
                m_zoomState = ChannelZoomState::ZoomingOut;
                m_zoomTimer = 0.0f;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
            }
            return;
        }

        // Mouse hover and click on RETOUR and ENTRER buttons mapped into CRT glass texture space
        Vector2f mouseOnGlass = Vector2f{695.0f, 455.0f} + (mPos - L.GlassCenter) / (L.Scale > 0.0001f ? L.Scale : 1.0f);
        Vector2f btnRetourPos{455.0f, 845.0f};
        Vector2f btnEntrerPos{935.0f, 845.0f};
        Vector2f btnHalfSize{130.0f, 31.0f};

        if (std::abs(mouseOnGlass.X - btnRetourPos.X) <= btnHalfSize.X && std::abs(mouseOnGlass.Y - btnRetourPos.Y) <= btnHalfSize.Y)
        {
            m_zoomChoice = 0;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_zoomState = ChannelZoomState::ZoomingOut;
                m_zoomTimer = 0.0f;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 80.0f);
                return;
            }
        }
        else if (std::abs(mouseOnGlass.X - btnEntrerPos.X) <= btnHalfSize.X && std::abs(mouseOnGlass.Y - btnEntrerPos.Y) <= btnHalfSize.Y)
        {
            m_zoomChoice = 1;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_zoomState = ChannelZoomState::LaunchingFullScreen;
                m_launchTimer = 0.0f;
                m_launchProgress = 0.0f;
                if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 100.0f);
                if (m_tvStaticSfxId != 0) context.Audio.PlaySfx(m_tvStaticSfxId, 75.0f);
                return;
            }
        }
        return;
    }

    // Scale tweening for channel cards
    for (size_t i = 0; i < m_tvChannels.size(); ++i)
    {
        float target = (static_cast<int>(i) == m_selectedChannel) ? 1.07f : 1.0f;
        m_tvChannels[i].CurrentScale += (target - m_tvChannels[i].CurrentScale) * std::min(1.0f, 16.0f * dt);
    }

    // 5. Physical Chassis Controls: Power Button, Volume Dial, Channel Step
    // Standby direct hotkey: V or S when on TV grid
    if (m_zoomState == ChannelZoomState::Grid && (context.Input.IsKeyPress(KeyCode::V) || context.Input.IsKeyPress(KeyCode::S)))
    {
        m_isStandby = true;
        if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
        return;
    }

    // Chassis Power Button: center (1690.0f, 220.0f), radius 26.0f in base coords
    float distToPowerBtn = std::sqrt((baseMousePos.X - 1690.0f) * (baseMousePos.X - 1690.0f) + (baseMousePos.Y - 220.0f) * (baseMousePos.Y - 220.0f));
    if (distToPowerBtn <= 26.0f && context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        m_showPowerOffDialog = true;
        m_powerDialogChoice = 1; // Default to Standby
        if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
        return;
    }

    // Volume adjustment helper
    auto adjustVolume = [this, &context](float delta) {
        m_masterVolume = std::clamp(m_masterVolume + delta, 0.0f, 100.0f);
        context.Audio.SetMasterVolume(m_masterVolume);
        m_volumeKnobAngle = (m_masterVolume / 100.0f) * 270.0f - 135.0f;
        m_osdTimer = 2.0f;
        m_osdText = "VOL  " + std::to_string(static_cast<int>(m_masterVolume)) + "%";
        if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 45.0f);
    };

    // Mouse wheel volume control
    float wheelDelta = context.Input.GetMouseWheelDelta();
    if (wheelDelta != 0.0f)
    {
        adjustVolume(wheelDelta * 5.0f);
    }

    // Volume keys: + / - / = / _
    if (context.Input.IsKeyPress(KeyCode::Add) || context.Input.IsKeyPress(KeyCode::Equal))
    {
        adjustVolume(5.0f);
    }
    if (context.Input.IsKeyPress(KeyCode::Subtract) || context.Input.IsKeyPress(KeyCode::Hyphen))
    {
        adjustVolume(-5.0f);
    }

    // Chassis Volume Dial click: center (1690.0f, 360.0f), radius 48.0f
    float distToVolDial = std::sqrt((baseMousePos.X - 1690.0f) * (baseMousePos.X - 1690.0f) + (baseMousePos.Y - 360.0f) * (baseMousePos.Y - 360.0f));
    if (distToVolDial <= 48.0f && context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        if (baseMousePos.X < 1690.0f) adjustVolume(-10.0f);
        else adjustVolume(10.0f);
    }

    // Channel cycle helper
    auto cycleChannel = [this, &context](int step) {
        int count = static_cast<int>(m_tvChannels.size());
        m_selectedChannel = (m_selectedChannel + step + count) % count;
        if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 85.0f);
    };

    // Chassis Channel Step Buttons (Fixed inverted logic: CH + advances, CH - decrements)
    // CH +: (1690.0f, 472.0f), size (130.0f, 32.0f)
    if (std::abs(baseMousePos.X - 1690.0f) <= 65.0f && std::abs(baseMousePos.Y - 472.0f) <= 16.0f && context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        cycleChannel(1);
    }
    // CH -: (1690.0f, 516.0f), size (130.0f, 32.0f)
    if (std::abs(baseMousePos.X - 1690.0f) <= 65.0f && std::abs(baseMousePos.Y - 516.0f) <= 16.0f && context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        cycleChannel(-1);
    }

    // Channel switch keys: PageUp / PageDown / [ / ]
    if (context.Input.IsKeyPress(KeyCode::PageUp) || context.Input.IsKeyPress(KeyCode::RBracket))
    {
        cycleChannel(1);
    }
    if (context.Input.IsKeyPress(KeyCode::PageDown) || context.Input.IsKeyPress(KeyCode::LBracket))
    {
        cycleChannel(-1);
    }

    // 6. Channel Card Mouse Hover & Click (Wii Channel select zoom)
    for (size_t i = 0; i < m_tvChannels.size(); ++i)
    {
        const auto& ch = m_tvChannels[i];
        if (std::abs(baseMousePos.X - ch.BasePosition.X) <= ch.BaseSize.X * 0.5f &&
            std::abs(baseMousePos.Y - ch.BasePosition.Y) <= ch.BaseSize.Y * 0.5f)
        {
            if (m_hoveredChannel != static_cast<int>(i))
            {
                m_hoveredChannel = static_cast<int>(i);
                m_selectedChannel = static_cast<int>(i);
                if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 75.0f);
            }

            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_selectedChannel = static_cast<int>(i);
                m_zoomState = ChannelZoomState::ZoomingIn;
                m_zoomTimer = 0.0f;
                m_zoomChoice = 1;
                if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 85.0f);
                return;
            }
        }
    }

    // 7. Keyboard / Gamepad 4x2 Navigation
    if (m_navDebounceTimer <= 0.0f)
    {
        int prevSelected = m_selectedChannel;
        int col = m_selectedChannel % 4;
        int row = m_selectedChannel / 4;

        if (context.Input.IsMenuLeftTriggered())
        {
            col = (col + 3) % 4;
            m_selectedChannel = row * 4 + col;
            m_navDebounceTimer = 0.20f;
        }
        else if (context.Input.IsMenuRightTriggered())
        {
            col = (col + 1) % 4;
            m_selectedChannel = row * 4 + col;
            m_navDebounceTimer = 0.20f;
        }
        else if (context.Input.IsMenuUpTriggered())
        {
            row = (row + 1) % 2;
            m_selectedChannel = row * 4 + col;
            m_navDebounceTimer = 0.20f;
        }
        else if (context.Input.IsMenuDownTriggered())
        {
            row = (row + 1) % 2;
            m_selectedChannel = row * 4 + col;
            m_navDebounceTimer = 0.20f;
        }

        if (m_selectedChannel != prevSelected)
        {
            if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 75.0f);
        }
    }

    // 8. Channel Select / Zoom confirmation: Enter, Space, Gamepad A
    if (context.Input.IsMenuSelectTriggered())
    {
        m_zoomState = ChannelZoomState::ZoomingIn;
        m_zoomTimer = 0.0f;
        m_zoomChoice = 1;
        if (m_tvHoverSfxId != 0) context.Audio.PlaySfx(m_tvHoverSfxId, 85.0f);
        return;
    }

    // 9. Escape / Gamepad B brings up Power Off dialog
    if (context.Input.IsMenuCancelTriggered())
    {
        m_showPowerOffDialog = true;
        m_powerDialogChoice = 0;
        if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 90.0f);
        return;
    }
}

void MenuScene::LaunchChannel(int channelIndex, GameContext& context)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_tvChannels.size())) return;
    const auto& ch = m_tvChannels[channelIndex];

    m_zoomState = ChannelZoomState::Grid;
    m_zoomProgress = 0.0f;
    m_launchProgress = 0.0f;
    m_launchTimer = 0.0f;

    if (m_tvSwitchSfxId != 0) context.Audio.PlaySfx(m_tvSwitchSfxId, 100.0f);
    if (m_tvStaticSfxId != 0) context.Audio.PlaySfx(m_tvStaticSfxId, 75.0f);
    m_channelSwitchGlitch = 0.45f;

    switch (ch.Type)
    {
    case ChannelType::Campaign:
        context.Scenes.LoadSceneWithTransition<GameScene>(SceneTransitionType::StaticSnow, 0.45f);
        break;
    case ChannelType::LevelSelect:
        context.Scenes.LoadSceneWithTransition<LevelSelectScene>(SceneTransitionType::StaticSnow, 0.45f);
        break;
    case ChannelType::TetrisEndless:
        context.Scenes.LoadSceneWithTransition<GameScene>(SceneTransitionType::StaticSnow, 0.45f);
        break;
    case ChannelType::Workshop:
        m_channelSwitchGlitch = 0.0f;
        OpenModal(m_workshopCanvas);
        OpenWorkshopTab(m_workshopUpgradesCanvas);
        break;
    case ChannelType::Leaderboard:
        m_channelSwitchGlitch = 0.0f;
        OpenModal(m_leaderboardCanvas);
        break;
    case ChannelType::Achievements:
        m_channelSwitchGlitch = 0.0f;
        OpenModal(m_workshopCanvas);
        OpenWorkshopTab(m_workshopProfileCanvas);
        break;
    case ChannelType::Sandbox:
        context.Scenes.LoadSceneWithTransition<SandboxScene>(SceneTransitionType::StaticSnow, 0.45f);
        break;
    case ChannelType::Settings:
        m_channelSwitchGlitch = 0.0f;
        OpenModal(m_settingsCanvas);
        OpenSettingsTab(m_audioCanvas);
        break;
    default:
        break;
    }
}

void MenuScene::RenderRetroTVChassis(GameContext& context) const
{
    const auto& L = m_tvLayout;

    // 1. Outer Dark Woodgrain Cabinet Frame
    context.Render.DrawRectangle(L.ToScreenDim(1880.0f), L.ToScreenDim(1040.0f), Transform2D{L.CabinetCenter}, Color{24, 18, 14, 255});

    // Cabinet Bevels
    context.Render.DrawRectangle(L.ToScreenDim(1880.0f), L.ToScreenDim(16.0f), Transform2D{L.ToScreenPos(Vector2f{960.0f, 28.0f})}, Color{65, 52, 42, 255});
    context.Render.DrawRectangle(L.ToScreenDim(16.0f), L.ToScreenDim(1040.0f), Transform2D{L.ToScreenPos(Vector2f{28.0f, 540.0f})}, Color{55, 44, 35, 255});
    context.Render.DrawRectangle(L.ToScreenDim(1880.0f), L.ToScreenDim(16.0f), Transform2D{L.ToScreenPos(Vector2f{960.0f, 1052.0f})}, Color{12, 9, 7, 255});
    context.Render.DrawRectangle(L.ToScreenDim(16.0f), L.ToScreenDim(1040.0f), Transform2D{L.ToScreenPos(Vector2f{1892.0f, 540.0f})}, Color{14, 11, 8, 255});

    // Corner Screws
    const Vector2f screws[4] = {
        {50.0f, 45.0f}, {1870.0f, 45.0f}, {50.0f, 1035.0f}, {1870.0f, 1035.0f}
    };
    for (const auto& sc : screws)
    {
        Vector2f scPos = L.ToScreenPos(sc);
        context.Render.DrawCircle(L.ToScreenDim(5.0f), Transform2D{scPos}, Color{160, 165, 175, 255});
        context.Render.DrawLine(scPos - Vector2f{L.ToScreenDim(3.0f), 0.0f}, scPos + Vector2f{L.ToScreenDim(3.0f), 0.0f}, Color{40, 45, 55, 255}, L.ToScreenDim(1.2f));
    }

    // 2. Right Control Panel
    const Vector2f panelCenter = L.ToScreenPos(Vector2f{1690.0f, 540.0f});
    context.Render.DrawRectangle(L.ToScreenDim(300.0f), L.ToScreenDim(960.0f), Transform2D{panelCenter}, Color{32, 36, 44, 255});
    context.Render.DrawRectangleOutline(L.ToScreenDim(300.0f), L.ToScreenDim(960.0f), Transform2D{panelCenter}, Color{55, 62, 75, 255}, L.ToScreenDim(3.0f));

    // Section Demarcations & Recessed Grooves
    const float sectionDividersY[5] = {175.0f, 285.0f, 435.0f, 560.0f, 835.0f};
    for (float dy : sectionDividersY)
    {
        Vector2f pA = L.ToScreenPos(Vector2f{1555.0f, dy});
        Vector2f pB = L.ToScreenPos(Vector2f{1825.0f, dy});
        context.Render.DrawLine(pA, pB, Color{15, 18, 24, 255}, L.ToScreenDim(2.5f));
        context.Render.DrawLine(pA + Vector2f{0.0f, L.ToScreenDim(2.0f)}, pB + Vector2f{0.0f, L.ToScreenDim(2.0f)}, Color{60, 68, 82, 180}, L.ToScreenDim(1.0f));
    }

    // Top Nameplate Badge ("REBREAKER RELOADED")
    const Vector2f badgeCenter = L.ToScreenPos(Vector2f{1690.0f, 125.0f});
    context.Render.DrawRectangle(L.ToScreenDim(246.0f), L.ToScreenDim(48.0f), Transform2D{badgeCenter}, Color{42, 34, 18, 255});
    context.Render.DrawRectangleOutline(L.ToScreenDim(246.0f), L.ToScreenDim(48.0f), Transform2D{badgeCenter}, Color{220, 180, 70, 255}, L.ToScreenDim(2.0f));
    context.Render.DrawTextCentered("REBREAKER RELOADED", m_fontId, L.ToScreenDim(17.0f), Transform2D{badgeCenter}, Colors::Gold);

    // Master Power Button & Recessed Socket
    const Vector2f pwrCenter = L.ToScreenPos(Vector2f{1690.0f, 220.0f});
    context.Render.DrawCircle(L.ToScreenDim(32.0f), Transform2D{pwrCenter}, Color{16, 18, 22, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(32.0f), Transform2D{pwrCenter}, Color{48, 54, 65, 255}, L.ToScreenDim(1.5f));
    context.Render.DrawCircle(L.ToScreenDim(24.0f), Transform2D{pwrCenter}, Color{180, 40, 45, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(24.0f), Transform2D{pwrCenter}, Color{240, 80, 85, 255}, L.ToScreenDim(2.0f));
    context.Render.DrawTextCentered("POWER", m_fontId, L.ToScreenDim(13.0f), Transform2D{pwrCenter + Vector2f{0.0f, L.ToScreenDim(38.0f)}}, Color{200, 210, 225, 255});

    // Power Indicator LED
    const Vector2f ledPos = L.ToScreenPos(Vector2f{1690.0f + 68.0f, 220.0f});
    context.Render.DrawCircle(L.ToScreenDim(8.0f), Transform2D{ledPos}, Color{20, 25, 30, 255});
    if (m_showPowerOffDialog)
    {
        uint8_t pulseRed = static_cast<uint8_t>(180 + 75 * std::sin(m_animTime * 8.0f));
        context.Render.DrawCircle(L.ToScreenDim(5.0f), Transform2D{ledPos}, Color{pulseRed, 40, 40, 255});
    }
    else if (m_isStandby)
    {
        uint8_t pulseAmber = static_cast<uint8_t>(180 + 65 * std::sin(m_animTime * 3.0f));
        context.Render.DrawCircle(L.ToScreenDim(5.0f), Transform2D{ledPos}, Color{pulseAmber, 140, 30, 255});
    }
    else
    {
        context.Render.DrawCircle(L.ToScreenDim(5.0f), Transform2D{ledPos}, Color{50, 255, 75, 255});
    }

    // Rotary Volume Dial & Basin
    const Vector2f volCenter = L.ToScreenPos(Vector2f{1690.0f, 360.0f});
    context.Render.DrawTextCentered("VOLUME", m_fontId, L.ToScreenDim(13.0f), Transform2D{volCenter - Vector2f{0.0f, L.ToScreenDim(56.0f)}}, Color{200, 210, 225, 255});
    context.Render.DrawCircle(L.ToScreenDim(48.0f), Transform2D{volCenter}, Color{18, 20, 25, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(48.0f), Transform2D{volCenter}, Color{45, 52, 62, 255}, L.ToScreenDim(1.5f));
    context.Render.DrawCircle(L.ToScreenDim(38.0f), Transform2D{volCenter}, Color{55, 62, 72, 255});
    context.Render.DrawCircle(L.ToScreenDim(22.0f), Transform2D{volCenter}, Color{35, 40, 48, 255});

    // Graduation ticks around dial
    for (int deg = -135; deg <= 135; deg += 27)
    {
        float rad = static_cast<float>(deg) * 3.14159f / 180.0f;
        Vector2f p1 = volCenter + Vector2f{std::cos(rad) * L.ToScreenDim(42.0f), std::sin(rad) * L.ToScreenDim(42.0f)};
        Vector2f p2 = volCenter + Vector2f{std::cos(rad) * L.ToScreenDim(48.0f), std::sin(rad) * L.ToScreenDim(48.0f)};
        context.Render.DrawLine(p1, p2, Color{140, 150, 165, 200}, L.ToScreenDim(1.5f));
    }

    // Rotating indicator notch
    float knotRad = m_volumeKnobAngle * 3.14159f / 180.0f;
    Vector2f notchEnd = volCenter + Vector2f{std::cos(knotRad) * L.ToScreenDim(36.0f), std::sin(knotRad) * L.ToScreenDim(36.0f)};
    context.Render.DrawLine(volCenter, notchEnd, Color{255, 160, 50, 255}, L.ToScreenDim(3.0f));

    context.Render.DrawText("MIN", m_fontId, L.ToScreenDim(11.0f), Transform2D{volCenter + Vector2f{L.ToScreenDim(-45.0f), L.ToScreenDim(46.0f)}}, Color{160, 170, 185, 200});
    context.Render.DrawText("MAX", m_fontId, L.ToScreenDim(11.0f), Transform2D{volCenter + Vector2f{L.ToScreenDim(25.0f), L.ToScreenDim(46.0f)}}, Color{160, 170, 185, 200});

    // Channel Step Buttons (Bug-free geometric arrow rendering & demarcations)
    const Vector2f chUpPos = L.ToScreenPos(Vector2f{1690.0f, 472.0f});
    const Vector2f chDnPos = L.ToScreenPos(Vector2f{1690.0f, 516.0f});

    // CH + Button
    context.Render.DrawRectangle(L.ToScreenDim(130.0f), L.ToScreenDim(32.0f), Transform2D{chUpPos}, Color{42, 48, 58, 255});
    context.Render.DrawRectangleOutline(L.ToScreenDim(130.0f), L.ToScreenDim(32.0f), Transform2D{chUpPos}, Color{70, 80, 95, 255}, L.ToScreenDim(1.5f));
    context.Render.DrawTextCentered("CH +", m_fontId, L.ToScreenDim(16.0f), Transform2D{chUpPos - Vector2f{L.ToScreenDim(14.0f), 0.0f}}, Color{210, 220, 235, 255});
    // Geometric Up-Arrow Triangle
    Vector2f triTop = chUpPos + Vector2f{L.ToScreenDim(32.0f), L.ToScreenDim(-6.0f)};
    Vector2f triLeft = chUpPos + Vector2f{L.ToScreenDim(24.0f), L.ToScreenDim(5.0f)};
    Vector2f triRight = chUpPos + Vector2f{L.ToScreenDim(40.0f), L.ToScreenDim(5.0f)};
    context.Render.DrawLine(triTop, triLeft, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));
    context.Render.DrawLine(triTop, triRight, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));
    context.Render.DrawLine(triLeft, triRight, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));

    // CH - Button
    context.Render.DrawRectangle(L.ToScreenDim(130.0f), L.ToScreenDim(32.0f), Transform2D{chDnPos}, Color{42, 48, 58, 255});
    context.Render.DrawRectangleOutline(L.ToScreenDim(130.0f), L.ToScreenDim(32.0f), Transform2D{chDnPos}, Color{70, 80, 95, 255}, L.ToScreenDim(1.5f));
    context.Render.DrawTextCentered("CH -", m_fontId, L.ToScreenDim(16.0f), Transform2D{chDnPos - Vector2f{L.ToScreenDim(14.0f), 0.0f}}, Color{210, 220, 235, 255});
    // Geometric Down-Arrow Triangle
    Vector2f triBot = chDnPos + Vector2f{L.ToScreenDim(32.0f), L.ToScreenDim(6.0f)};
    Vector2f triLeftD = chDnPos + Vector2f{L.ToScreenDim(24.0f), L.ToScreenDim(-5.0f)};
    Vector2f triRightD = chDnPos + Vector2f{L.ToScreenDim(40.0f), L.ToScreenDim(-5.0f)};
    context.Render.DrawLine(triBot, triLeftD, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));
    context.Render.DrawLine(triBot, triRightD, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));
    context.Render.DrawLine(triLeftD, triRightD, Color{220, 230, 245, 255}, L.ToScreenDim(1.8f));

    // Realistic Circular Acoustic Speaker Cone (Replacing old slats and phone jack)
    const Vector2f spkCenter = L.ToScreenPos(Vector2f{1690.0f, 695.0f});
    // Outer mounting ring with hex bolts
    context.Render.DrawCircle(L.ToScreenDim(84.0f), Transform2D{spkCenter}, Color{24, 28, 36, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(84.0f), Transform2D{spkCenter}, Color{55, 62, 75, 255}, L.ToScreenDim(2.5f));

    const float boltAngles[4] = {0.785398f, 2.35619f, 3.92699f, 5.49779f};
    for (float bAng : boltAngles)
    {
        Vector2f boltPos = spkCenter + Vector2f{std::cos(bAng) * L.ToScreenDim(76.0f), std::sin(bAng) * L.ToScreenDim(76.0f)};
        context.Render.DrawCircle(L.ToScreenDim(3.5f), Transform2D{boltPos}, Color{160, 168, 180, 255});
        context.Render.DrawCircle(L.ToScreenDim(1.5f), Transform2D{boltPos}, Color{30, 35, 42, 255});
    }

    // Flexible rubber suspension surround
    context.Render.DrawCircle(L.ToScreenDim(70.0f), Transform2D{spkCenter}, Color{16, 18, 24, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(70.0f), Transform2D{spkCenter}, Color{38, 44, 54, 255}, L.ToScreenDim(2.0f));

    // Deep acoustic cone
    context.Render.DrawCircle(L.ToScreenDim(60.0f), Transform2D{spkCenter}, Color{10, 12, 16, 255});

    // Concentric acoustic rib rings
    context.Render.DrawCircleOutline(L.ToScreenDim(48.0f), Transform2D{spkCenter}, Color{32, 38, 48, 220}, L.ToScreenDim(1.2f));
    context.Render.DrawCircleOutline(L.ToScreenDim(36.0f), Transform2D{spkCenter}, Color{36, 42, 54, 220}, L.ToScreenDim(1.2f));

    // Center metallic dome dust cap
    context.Render.DrawCircle(L.ToScreenDim(22.0f), Transform2D{spkCenter}, Color{22, 26, 34, 255});
    context.Render.DrawCircleOutline(L.ToScreenDim(22.0f), Transform2D{spkCenter}, Color{70, 80, 95, 255}, L.ToScreenDim(1.5f));
    float spkPulse = std::sin(m_animTime * 10.0f) * L.ToScreenDim(1.2f);
    context.Render.DrawCircleOutline(L.ToScreenDim(22.0f) + spkPulse, Transform2D{spkCenter}, Color{80, 160, 240, 70}, L.ToScreenDim(1.2f));

    // Game Version & Credits Badge ("NOA PASCAL")
    const Vector2f plateCenter = L.ToScreenPos(Vector2f{1690.0f, 885.0f});
    context.Render.DrawRectangle(L.ToScreenDim(230.0f), L.ToScreenDim(44.0f), Transform2D{plateCenter}, Color{22, 26, 34, 240});
    context.Render.DrawRectangleOutline(L.ToScreenDim(230.0f), L.ToScreenDim(44.0f), Transform2D{plateCenter}, Color{70, 80, 95, 200}, L.ToScreenDim(1.5f));
    context.Render.DrawTextCentered("VERSION 1.0.0", m_fontId, L.ToScreenDim(12.0f), Transform2D{plateCenter - Vector2f{0.0f, L.ToScreenDim(8.0f)}}, Color{190, 205, 225, 240});
    context.Render.DrawTextCentered("NOA PASCAL", m_fontId, L.ToScreenDim(14.0f), Transform2D{plateCenter + Vector2f{0.0f, L.ToScreenDim(9.0f)}}, Colors::Gold);
}

void MenuScene::RenderCRTBezelAndGlass(GameContext& context) const
{
    const auto& L = m_tvLayout;

    // Molded matte CRT casing bezel
    context.Render.DrawRectangle(L.ToScreenDim(1440.0f), L.ToScreenDim(960.0f), Transform2D{L.GlassCenter}, Color{18, 20, 25, 255});
    context.Render.DrawRectangleOutline(L.ToScreenDim(1440.0f), L.ToScreenDim(960.0f), Transform2D{L.GlassCenter}, Color{40, 45, 55, 255}, L.ToScreenDim(4.0f));

    // Inner Cathode Screen Glass
    context.Render.DrawRectangle(L.GlassSize.X, L.GlassSize.Y, Transform2D{L.GlassCenter}, Color{12, 16, 26, 255});

    // Dark cathodic shadows along glass borders
    context.Render.DrawRectangle(L.GlassSize.X, L.ToScreenDim(20.0f), Transform2D{L.ToScreenPos(Vector2f{800.0f, 95.0f})}, Color{5, 7, 12, 180});
    context.Render.DrawRectangle(L.GlassSize.X, L.ToScreenDim(20.0f), Transform2D{L.ToScreenPos(Vector2f{800.0f, 985.0f})}, Color{5, 7, 12, 180});
    context.Render.DrawRectangle(L.ToScreenDim(20.0f), L.GlassSize.Y, Transform2D{L.ToScreenPos(Vector2f{115.0f, 540.0f})}, Color{5, 7, 12, 180});
    context.Render.DrawRectangle(L.ToScreenDim(20.0f), L.GlassSize.Y, Transform2D{L.ToScreenPos(Vector2f{1485.0f, 540.0f})}, Color{5, 7, 12, 180});

}

void MenuScene::RenderTVScreenContent(GameContext& context)
{
    if (m_zoomState != ChannelZoomState::Grid)
    {
        RenderWiiZoomedChannel(context);
    }
    else
    {
        RenderChannelGrid(context);
    }
}

void MenuScene::RenderChannelGrid(GameContext& context)
{
    if (m_tvChannels.empty()) return;
    if (m_selectedChannel < 0 || m_selectedChannel >= static_cast<int>(m_tvChannels.size()))
    {
        m_selectedChannel = 0;
    }

    // 1. CRT Screen Header Bar (Fixed inside 1390 x 910 TV screen)
    const Vector2f headerPillPos{695.0f, 55.0f};
    context.Render.DrawRectangle(520.0f, 44.0f, Transform2D{headerPillPos}, Color{20, 28, 44, 230});
    context.Render.DrawRectangleOutline(520.0f, 44.0f, Transform2D{headerPillPos}, Color{70, 150, 240, 220}, 2.0f);
    context.Render.DrawTextCentered("REBREAKER CHANNEL MENU", m_fontId, 22.0f, Transform2D{headerPillPos}, Color{240, 250, 255, 255});

    // Left Phosphor TV channel tag
    std::string chTag = "CH 0" + std::to_string(m_selectedChannel + 1) + "  AV-1";
    context.Render.DrawText(chTag, m_fontId, 18.0f, Transform2D{Vector2f{45.0f, 45.0f}}, Color{75, 255, 120, 220});

    // Right Phosphor audio spec tag
    context.Render.DrawText("STEREO [NICAM] 60Hz", m_fontId, 16.0f, Transform2D{Vector2f{1155.0f, 45.0f}}, Color{255, 195, 70, 220});

    // 2. 8 Channel Cards (4x2 Grid)
    for (size_t i = 0; i < m_tvChannels.size(); ++i)
    {
        const auto& ch = m_tvChannels[i];
        float s = ch.CurrentScale;
        Vector2f pos = ch.BasePosition - Vector2f{105.0f, 85.0f};
        float w = ch.BaseSize.X * s;
        float h = ch.BaseSize.Y * s;

        // Drop shadow
        context.Render.DrawRectangle(w, h, Transform2D{pos + Vector2f{5.0f * s, 6.0f * s}}, Color{0, 0, 0, 130});

        // Background
        context.Render.DrawRectangle(w, h, Transform2D{pos}, ch.CardBgColor);

        // Border: pulsing neon glow when selected
        if (static_cast<int>(i) == m_selectedChannel)
        {
            uint8_t glowAlpha = static_cast<uint8_t>(180 + 75 * std::sin(m_animTime * 6.0f));
            Color glowCol{ch.ThemeColor.r, ch.ThemeColor.g, ch.ThemeColor.b, glowAlpha};
            context.Render.DrawRectangleOutline(w, h, Transform2D{pos}, glowCol, 4.0f);
        }
        else
        {
            context.Render.DrawRectangleOutline(w, h, Transform2D{pos}, Color{50, 65, 88, 180}, 2.0f);
        }

        // Top Banner inside card
        Vector2f topBannerPos = pos + Vector2f{0.0f, -h * 0.5f + 16.0f * s};
        context.Render.DrawRectangle(w - 4.0f, 30.0f * s, Transform2D{topBannerPos}, Color{26, 34, 52, 255});
        context.Render.DrawText(ch.ChannelNumber, m_fontId, 13.0f * s, Transform2D{pos + Vector2f{-w * 0.5f + 12.0f * s, -h * 0.5f + 7.0f * s}}, ch.ThemeColor);
        context.Render.DrawText(ch.Subtitle, m_fontId, 11.0f * s, Transform2D{pos + Vector2f{w * 0.5f - 110.0f * s, -h * 0.5f + 8.0f * s}}, Color{170, 185, 205, 220});

        // Bottom Title Banner
        Vector2f botBannerPos = pos + Vector2f{0.0f, h * 0.5f - 18.0f * s};
        context.Render.DrawRectangle(w - 4.0f, 32.0f * s, Transform2D{botBannerPos}, Color{12, 16, 26, 240});
        context.Render.DrawTextCentered(ch.Title, m_fontId, 17.0f * s, Transform2D{botBannerPos}, ch.ThemeColor);

        // Center Procedural Vignette
        Vector2f vignettePos = pos + Vector2f{0.0f, -4.0f * s};
        RenderChannelVignette(context, ch, vignettePos, ch.CurrentScale);
    }
}

void MenuScene::RenderChannelVignette(GameContext& context, const TVChannel& ch, const Vector2f& pos, float sc) const
{
    switch (ch.Type)
    {
    case ChannelType::Campaign:
    {
        RenderCampaignPreview(context, pos, sc, false);
        break;
    }
    case ChannelType::LevelSelect:
    {
        // Holographic Sector Grid & Pulsing Stage Nodes
        for (int l = -3; l <= 3; ++l)
        {
            float lx = pos.X + static_cast<float>(l) * 16.0f * sc;
            context.Render.DrawLine(Vector2f{lx, pos.Y - 24.0f * sc}, Vector2f{pos.X + static_cast<float>(l) * 26.0f * sc, pos.Y + 26.0f * sc},
                                    Color{255, 175, 55, 35}, 1.0f * sc);
        }

        // Connecting data highways between 4 stage nodes
        const Vector2f nodes[4] = {
            pos + Vector2f{-45.0f * sc, 10.0f * sc},
            pos + Vector2f{-15.0f * sc, -18.0f * sc},
            pos + Vector2f{18.0f * sc, 14.0f * sc},
            pos + Vector2f{48.0f * sc, -12.0f * sc}
        };

        for (int i = 0; i < 3; ++i)
        {
            context.Render.DrawLine(nodes[i], nodes[i + 1], Color{255, 180, 60, 160}, 1.5f * sc);
            float pulseT = std::fmod(m_animTime * 1.8f + static_cast<float>(i) * 0.33f, 1.0f);
            Vector2f pulsePos = nodes[i] + (nodes[i + 1] - nodes[i]) * pulseT;
            context.Render.DrawCircle(3.0f * sc, Transform2D{pulsePos}, Color{255, 235, 140, 255});
        }

        // 4 Stage Sector Nodes
        for (int i = 0; i < 4; ++i)
        {
            float pRing = 5.0f * sc + std::sin(m_animTime * 4.0f + static_cast<float>(i)) * 2.0f * sc;
            context.Render.DrawCircle(6.5f * sc, Transform2D{nodes[i]}, Color{30, 24, 16, 255});
            context.Render.DrawCircle(4.5f * sc, Transform2D{nodes[i]}, Color{255, 180, 55, 255});
            context.Render.DrawCircleOutline(pRing + 3.0f * sc, Transform2D{nodes[i]}, Color{255, 210, 80, 140}, 1.2f * sc);
        }

        // Target selector sweeping across sectors
        float scanT = std::fmod(m_animTime * 0.8f, 3.0f);
        int idxA = static_cast<int>(scanT);
        int idxB = (idxA + 1) % 4;
        float frac = scanT - static_cast<float>(idxA);
        Vector2f reticlePos = nodes[idxA] + (nodes[idxB] - nodes[idxA]) * frac;

        context.Render.DrawRectangleOutline(18.0f * sc, 18.0f * sc, Transform2D{reticlePos}, Color{255, 240, 100, 240}, 1.5f * sc);
        context.Render.DrawLine(reticlePos - Vector2f{12.0f * sc, 0.0f}, reticlePos + Vector2f{12.0f * sc, 0.0f}, Color{255, 220, 70, 200}, 1.0f * sc);
        context.Render.DrawLine(reticlePos - Vector2f{0.0f, 12.0f * sc}, reticlePos + Vector2f{0.0f, 12.0f * sc}, Color{255, 220, 70, 200}, 1.0f * sc);
        break;
    }
    case ChannelType::TetrisEndless:
    {
        // Doom / Mortal Kombat style inferno flames with flashing ENDLESS title
        // 1. Fiery inferno flames background
        const float flameBaseY = pos.Y + 36.0f * sc;
        constexpr int kFlameTongues = 11;
        for (int k = 0; k < kFlameTongues; ++k)
        {
            float fx = pos.X + (static_cast<float>(k) - 5.0f) * (12.0f * sc);
            float fHeight = (28.0f + 16.0f * std::sin(m_animTime * 6.5f + k * 1.25f) + 8.0f * std::cos(m_animTime * 9.0f + k * 2.1f)) * sc;
            float sway = std::sin(m_animTime * 7.5f + k * 1.4f) * 4.0f * sc;
            Vector2f tip{fx + sway, flameBaseY - fHeight};
            float bW = 11.0f * sc;

            // Outer crimson/blood flame tongue
            context.Render.DrawLine(Vector2f{fx - bW * 0.5f, flameBaseY}, tip, Color{195, 25, 12, 215}, 4.0f * sc);
            context.Render.DrawLine(Vector2f{fx + bW * 0.5f, flameBaseY}, tip, Color{195, 25, 12, 215}, 4.0f * sc);
            context.Render.DrawLine(Vector2f{fx, flameBaseY}, tip, Color{240, 75, 18, 230}, 6.0f * sc);

            // Inner searing yellow-orange core
            Vector2f innerTip{fx + sway * 0.55f, flameBaseY - fHeight * 0.65f};
            context.Render.DrawLine(Vector2f{fx, flameBaseY}, innerTip, Color{255, 225, 60, 245}, 3.5f * sc);
        }

        // 2. Rising glowing embers
        for (int e = 0; e < 6; ++e)
        {
            float eProgress = std::fmod(m_animTime * 1.6f + static_cast<float>(e) * 0.35f, 1.0f);
            float ex = pos.X + std::sin(static_cast<float>(e) * 2.3f + m_animTime * 3.5f) * 45.0f * sc;
            float ey = flameBaseY - eProgress * 68.0f * sc;
            uint8_t eAlpha = static_cast<uint8_t>((1.0f - eProgress) * 255.0f);
            context.Render.DrawCircle(1.8f * sc, Transform2D{Vector2f{ex, ey}}, Color{255, 205, 70, eAlpha});
        }

        // 3. Brutalist "ENDLESS" title with Doom / Mortal Kombat blinking strobe
        Vector2f titlePos = pos + Vector2f{0.0f, -8.0f * sc};

        // Flashing strobe logic: rapid pulse between searing white-gold and blood crimson
        float flashPhase = std::fmod(m_animTime * 4.5f, 1.0f);
        bool isStrobe = (flashPhase < 0.28f);

        Color titleCol = isStrobe ? Color{255, 255, 225, 255} : Color{255, 65, 25, 255};
        Color accentCol = isStrobe ? Color{255, 240, 100, 255} : Color{215, 30, 15, 230};

        // Charred drop shadow
        context.Render.DrawTextCentered("ENDLESS", m_fontId, 22.0f * sc, Transform2D{titlePos + Vector2f{2.0f * sc, 2.5f * sc}}, Color{15, 2, 4, 255});

        // Fiery top & bottom bracket lines
        context.Render.DrawLine(titlePos + Vector2f{-52.0f * sc, -14.0f * sc}, titlePos + Vector2f{52.0f * sc, -14.0f * sc}, accentCol, 2.2f * sc);
        context.Render.DrawLine(titlePos + Vector2f{-52.0f * sc, 14.0f * sc}, titlePos + Vector2f{52.0f * sc, 14.0f * sc}, accentCol, 2.2f * sc);

        // Blinking title
        context.Render.DrawTextCentered("ENDLESS", m_fontId, 22.0f * sc, Transform2D{titlePos}, titleCol);
        break;
    }
    case ChannelType::Workshop:
    {
        // 3D Rotating Golden Credit Token ("un crédit qui rotate")
        float rotAngle = m_animTime * 2.5f;
        float cosA = std::cos(rotAngle);
        float bounceY = std::sin(m_animTime * 3.2f) * 4.0f * sc;

        // Ground shadow on the pedestal below
        Vector2f shadowPos = pos + Vector2f{0.0f, 32.0f * sc};
        float shadowScale = 1.0f - (bounceY / (12.0f * sc));
        context.Render.DrawCircle(22.0f * sc * shadowScale, Transform2D{shadowPos, 0.0f, Vector2f{1.2f, 0.35f}}, Color{10, 14, 22, 160});

        Vector2f coinPos = pos + Vector2f{0.0f, -4.0f * sc + bounceY};
        const float R = 26.0f * sc;
        float w = std::max(2.5f * sc, std::abs(cosA) * R);
        float h = R;

        // 3D Coin cylindrical edge thickness when tilted
        float edgeThickness = 4.5f * sc * std::sin(rotAngle);
        if (std::abs(edgeThickness) > 0.8f * sc)
        {
            Vector2f edgePos = coinPos - Vector2f{edgeThickness * 0.5f, 0.0f};
            context.Render.DrawCircle(h, Transform2D{edgePos, 0.0f, Vector2f{w / h, 1.0f}}, Color{180, 135, 25, 255});
        }

        // Coin Face
        Color faceCol = (cosA >= 0.0f) ? Color{250, 210, 45, 255} : Color{235, 190, 35, 255};
        context.Render.DrawCircle(h, Transform2D{coinPos, 0.0f, Vector2f{w / h, 1.0f}}, faceCol);
        context.Render.DrawCircleOutline(h, Transform2D{coinPos, 0.0f, Vector2f{w / h, 1.0f}}, Color{255, 245, 175, 255}, 1.8f * sc);

        if (w > 8.0f * sc)
        {
            // Inner engraved groove
            context.Render.DrawCircleOutline(h * 0.76f, Transform2D{coinPos, 0.0f, Vector2f{w / h, 1.0f}}, Color{195, 145, 20, 220}, 1.2f * sc);

            // Sci-fi Credit Glyph "₵" (engraved C with horizontal tech bars)
            float cW = w * 0.40f;
            float cH = h * 0.40f;
            // Left vertical spine of C
            context.Render.DrawLine(coinPos + Vector2f{-cW, -cH}, coinPos + Vector2f{-cW, cH}, Color{85, 52, 12, 255}, 2.5f * sc);
            // Top and bottom arms of C
            context.Render.DrawLine(coinPos + Vector2f{-cW, -cH}, coinPos + Vector2f{cW, -cH}, Color{85, 52, 12, 255}, 2.0f * sc);
            context.Render.DrawLine(coinPos + Vector2f{-cW, cH}, coinPos + Vector2f{cW, cH}, Color{85, 52, 12, 255}, 2.0f * sc);
            // Twin horizontal credit hashes
            context.Render.DrawLine(coinPos + Vector2f{-cW * 1.35f, -cH * 0.28f}, coinPos + Vector2f{cW * 1.35f, -cH * 0.28f}, Color{255, 250, 210, 255}, 1.5f * sc);
            context.Render.DrawLine(coinPos + Vector2f{-cW * 1.35f, cH * 0.28f}, coinPos + Vector2f{cW * 1.35f, cH * 0.28f}, Color{255, 250, 210, 255}, 1.5f * sc);
        }

        // Specular gleam when coin faces forward
        if (std::abs(cosA) > 0.82f)
        {
            Vector2f glintPos = coinPos + Vector2f{w * 0.62f, -h * 0.62f};
            context.Render.DrawLine(glintPos - Vector2f{6.0f * sc, 0.0f}, glintPos + Vector2f{6.0f * sc, 0.0f}, Colors::White, 1.8f * sc);
            context.Render.DrawLine(glintPos - Vector2f{0.0f, 6.0f * sc}, glintPos + Vector2f{0.0f, 6.0f * sc}, Colors::White, 1.8f * sc);
        }
        break;
    }
    case ChannelType::Leaderboard:
    {
        // 3 Podium pedestal bars
        context.Render.DrawRectangle(26.0f * sc, 30.0f * sc, Transform2D{pos + Vector2f{-34.0f * sc, 22.0f * sc}}, Color{125, 135, 155, 240});
        context.Render.DrawRectangleOutline(26.0f * sc, 30.0f * sc, Transform2D{pos + Vector2f{-34.0f * sc, 22.0f * sc}}, Color{180, 195, 215, 255}, 1.2f * sc);
        context.Render.DrawTextCentered("2", m_fontId, 14.0f * sc, Transform2D{pos + Vector2f{-34.0f * sc, 22.0f * sc}}, Color{200, 215, 235, 255});

        context.Render.DrawRectangle(32.0f * sc, 46.0f * sc, Transform2D{pos + Vector2f{0.0f, 14.0f * sc}}, Color{215, 175, 45, 245});
        context.Render.DrawRectangleOutline(32.0f * sc, 46.0f * sc, Transform2D{pos + Vector2f{0.0f, 14.0f * sc}}, Colors::Gold, 1.5f * sc);
        context.Render.DrawTextCentered("1", m_fontId, 18.0f * sc, Transform2D{pos + Vector2f{0.0f, 14.0f * sc}}, Colors::White);

        context.Render.DrawRectangle(26.0f * sc, 20.0f * sc, Transform2D{pos + Vector2f{34.0f * sc, 27.0f * sc}}, Color{165, 100, 65, 240});
        context.Render.DrawRectangleOutline(26.0f * sc, 20.0f * sc, Transform2D{pos + Vector2f{34.0f * sc, 27.0f * sc}}, Color{215, 140, 95, 255}, 1.2f * sc);
        context.Render.DrawTextCentered("3", m_fontId, 14.0f * sc, Transform2D{pos + Vector2f{34.0f * sc, 27.0f * sc}}, Color{235, 170, 130, 255});

        // Floating Golden Crown above 1st place with light beam radiance
        float crownBob = std::sin(m_animTime * 3.0f) * 4.0f * sc;
        Vector2f cp = pos + Vector2f{0.0f, -22.0f * sc + crownBob};

        // Radiant light rays
        for (int r = 0; r < 6; ++r)
        {
            float ang = m_animTime * 1.5f + static_cast<float>(r) * 1.047197f;
            Vector2f rayEnd = cp + Vector2f{std::cos(ang) * 22.0f * sc, std::sin(ang) * 22.0f * sc};
            context.Render.DrawLine(cp, rayEnd, Color{255, 235, 120, 130}, 1.2f * sc);
        }

        // Crown geometry
        context.Render.DrawRectangle(24.0f * sc, 10.0f * sc, Transform2D{cp + Vector2f{0.0f, 4.0f * sc}}, Colors::Gold);
        context.Render.DrawCircle(3.5f * sc, Transform2D{cp + Vector2f{-10.0f * sc, -4.0f * sc}}, Colors::Gold);
        context.Render.DrawCircle(4.5f * sc, Transform2D{cp + Vector2f{0.0f, -6.0f * sc}}, Color{255, 245, 160, 255});
        context.Render.DrawCircle(3.5f * sc, Transform2D{cp + Vector2f{10.0f * sc, -4.0f * sc}}, Colors::Gold);

        // Rising golden arcade stars
        for (int s = 0; s < 3; ++s)
        {
            float starT = std::fmod(m_animTime * 1.2f + static_cast<float>(s) * 0.33f, 1.0f);
            float sy = pos.Y + 30.0f * sc - starT * 55.0f * sc;
            float sx = pos.X + (static_cast<float>(s) - 1.0f) * 32.0f * sc + std::sin(m_animTime * 4.0f + static_cast<float>(s)) * 5.0f * sc;
            uint8_t sa = static_cast<uint8_t>((1.0f - starT) * 230.0f);
            context.Render.DrawCircle(2.5f * sc, Transform2D{Vector2f{sx, sy}}, Color{255, 235, 120, sa});
        }
        break;
    }
    case ChannelType::Achievements:
    {
        // Holographic Pilot ID, Player Visor Avatar, Filling EXP Bar, Shimmering Medal
        context.Render.DrawRectangle(95.0f * sc, 58.0f * sc, Transform2D{pos}, Color{14, 26, 22, 220});
        context.Render.DrawRectangleOutline(95.0f * sc, 58.0f * sc, Transform2D{pos}, Color{50, 225, 140, 160}, 1.2f * sc);

        // Pilot Avatar Silhouette with glowing neon visor
        Vector2f avPos = pos + Vector2f{-26.0f * sc, -4.0f * sc};
        context.Render.DrawCircle(10.0f * sc, Transform2D{avPos + Vector2f{0.0f, -6.0f * sc}}, Color{35, 60, 50, 255});
        context.Render.DrawRectangle(14.0f * sc, 4.0f * sc, Transform2D{avPos + Vector2f{0.0f, -6.0f * sc}}, Color{60, 255, 180, 255});
        context.Render.DrawRectangle(22.0f * sc, 9.0f * sc, Transform2D{avPos + Vector2f{0.0f, 6.0f * sc}}, Color{30, 52, 44, 255});

        // Prestige Star Medal with rotating sheen
        Vector2f medPos = pos + Vector2f{24.0f * sc, -8.0f * sc};
        context.Render.DrawCircle(14.0f * sc, Transform2D{medPos}, Color{235, 190, 45, 255});
        context.Render.DrawCircleOutline(14.0f * sc, Transform2D{medPos}, Color{255, 240, 140, 255}, 1.5f * sc);
        float starRot = m_animTime * 1.8f;
        context.Render.DrawRectangle(10.0f * sc, 10.0f * sc, Transform2D{medPos, starRot}, Colors::White);

        // Animated EXP Progress Bar
        Vector2f barPos = pos + Vector2f{0.0f, 18.0f * sc};
        context.Render.DrawRectangle(78.0f * sc, 7.0f * sc, Transform2D{barPos}, Color{10, 18, 14, 255});
        context.Render.DrawRectangleOutline(78.0f * sc, 7.0f * sc, Transform2D{barPos}, Color{50, 225, 140, 150}, 1.0f * sc);
        float fillP = 0.5f + 0.4f * std::sin(m_animTime * 1.6f);
        float fillW = 76.0f * sc * fillP;
        float fillX = barPos.X - 38.0f * sc + fillW * 0.5f;
        context.Render.DrawRectangle(fillW, 5.0f * sc, Transform2D{Vector2f{fillX, barPos.Y}}, Color{50, 235, 140, 240});
        break;
    }
    case ChannelType::Sandbox:
    {
        // Interactive Physics Lab Preview
        context.Render.DrawRectangle(95.0f * sc, 58.0f * sc, Transform2D{pos}, Color{12, 20, 30, 220});
        context.Render.DrawRectangleOutline(95.0f * sc, 58.0f * sc, Transform2D{pos}, Color{60, 190, 255, 160}, 1.2f * sc);

        // Central gravity well
        float pulseWell = 7.0f * sc + std::sin(m_animTime * 4.0f) * 2.0f * sc;
        context.Render.DrawCircle(pulseWell, Transform2D{pos}, Color{60, 190, 255, 60});
        context.Render.DrawCircle(4.0f * sc, Transform2D{pos}, Color{100, 220, 255, 255});

        // 3 Kinetic test spheres orbiting and bouncing
        Vector2f sp1 = pos + Vector2f{std::cos(m_animTime * 3.2f) * 28.0f * sc, std::sin(m_animTime * 2.4f) * 18.0f * sc};
        Vector2f sp2 = pos + Vector2f{std::sin(m_animTime * 2.8f) * 32.0f * sc, std::cos(m_animTime * 3.6f) * 19.0f * sc};
        Vector2f sp3 = pos + Vector2f{std::cos(m_animTime * 2.1f + 2.0f) * 22.0f * sc, std::sin(m_animTime * 3.1f + 1.0f) * 20.0f * sc};

        context.Render.DrawCircle(4.0f * sc, Transform2D{sp1}, Color{80, 220, 255, 255});
        context.Render.DrawCircle(4.0f * sc, Transform2D{sp2}, Color{255, 75, 135, 255});
        context.Render.DrawCircle(4.0f * sc, Transform2D{sp3}, Color{85, 255, 130, 255});

        // Laser deflection beam bouncing between 2 test mirrors
        Vector2f m1 = pos + Vector2f{-34.0f * sc, 16.0f * sc};
        Vector2f m2 = pos + Vector2f{34.0f * sc, -16.0f * sc};
        context.Render.DrawRectangle(4.0f * sc, 12.0f * sc, Transform2D{m1, 0.7854f}, Color{200, 220, 240, 255});
        context.Render.DrawRectangle(4.0f * sc, 12.0f * sc, Transform2D{m2, 0.7854f}, Color{200, 220, 240, 255});
        context.Render.DrawLine(m1, pos, Color{255, 60, 90, 180}, 1.2f * sc);
        context.Render.DrawLine(pos, m2, Color{255, 60, 90, 180}, 1.2f * sc);
        break;
    }
    case ChannelType::Settings:
    {
        RenderTwoGears(context, pos, sc);
        break;
    }
    default:
        break;
    }
}

void MenuScene::RenderTVOSD(GameContext& context) const
{
    if (m_osdTimer <= 0.0f) return;

    const auto& L = m_tvLayout;
    const Vector2f osdCenter = L.ToScreenPos(Vector2f{800.0f, 195.0f});
    context.Render.DrawRectangle(L.ToScreenDim(440.0f), L.ToScreenDim(44.0f), Transform2D{osdCenter}, Color{0, 0, 0, 220});
    context.Render.DrawRectangleOutline(L.ToScreenDim(440.0f), L.ToScreenDim(44.0f), Transform2D{osdCenter}, Color{50, 255, 80, 230}, L.ToScreenDim(2.0f));
    context.Render.DrawTextCentered(m_osdText, m_fontId, L.ToScreenDim(20.0f), Transform2D{osdCenter}, Color{60, 255, 90, 255});

    if (m_osdText.rfind("VOL", 0) == 0)
    {
        // Segmented volume progress bar below text
        const Vector2f barCenter = L.ToScreenPos(Vector2f{800.0f, 210.0f});
        context.Render.DrawRectangle(L.ToScreenDim(380.0f), L.ToScreenDim(6.0f), Transform2D{barCenter}, Color{20, 40, 25, 255});
        float fillW = L.ToScreenDim(380.0f) * (m_masterVolume / 100.0f);
        if (fillW > 0.0f)
        {
            float fillX = barCenter.X - L.ToScreenDim(190.0f) + fillW * 0.5f;
            context.Render.DrawRectangle(fillW, L.ToScreenDim(6.0f), Transform2D{Vector2f{fillX, barCenter.Y}}, Color{60, 255, 90, 255});
        }
    }
}

void MenuScene::RenderPowerOffDialog(GameContext& context)
{
    if (!m_showPowerOffDialog) return;

    const auto& L = m_tvLayout;
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    // Full screen dim backdrop
    context.Render.DrawRectangle(viewSize.X * 2.0f, viewSize.Y * 2.0f, Transform2D{L.CabinetCenter}, Color{0, 0, 0, 190});

    const Vector2f diagCenter = L.ToScreenPos(Vector2f{800.0f, 540.0f});
    context.Render.DrawRectangle(L.ToScreenDim(660.0f), L.ToScreenDim(320.0f), Transform2D{diagCenter}, Color{12, 16, 26, 250});
    context.Render.DrawRectangleOutline(L.ToScreenDim(660.0f), L.ToScreenDim(320.0f), Transform2D{diagCenter}, Color{235, 60, 65, 240}, L.ToScreenDim(3.0f));

    // Header Red Warning Bar
    context.Render.DrawRectangle(L.ToScreenDim(654.0f), L.ToScreenDim(38.0f), Transform2D{diagCenter - Vector2f{0.0f, L.ToScreenDim(130.0f)}}, Color{180, 35, 40, 255});
    context.Render.DrawTextCentered("/// SYSTEM POWER ///", m_fontId, L.ToScreenDim(18.0f), Transform2D{diagCenter - Vector2f{0.0f, L.ToScreenDim(130.0f)}}, Colors::White);

    context.Render.DrawTextCentered("STANDBY OR POWER DOWN TELEVISION?", m_fontId, L.ToScreenDim(24.0f), Transform2D{diagCenter - Vector2f{0.0f, L.ToScreenDim(60.0f)}}, Colors::White);
    context.Render.DrawTextCentered("Choose Standby to sleep TV screen, or Power Off to exit.", m_fontId, L.ToScreenDim(16.0f), Transform2D{diagCenter - Vector2f{0.0f, L.ToScreenDim(15.0f)}}, Color{190, 205, 225, 255});

    // 3 Buttons: Cancel (0), Standby (1), Power Off (2)
    const Vector2f cancelPos = L.ToScreenPos(Vector2f{800.0f - 195.0f, 540.0f + 80.0f});
    const Vector2f standbyPos = L.ToScreenPos(Vector2f{800.0f, 540.0f + 80.0f});
    const Vector2f pwrPos = L.ToScreenPos(Vector2f{800.0f + 195.0f, 540.0f + 80.0f});
    const float btnW = L.ToScreenDim(175.0f);
    const float btnH = L.ToScreenDim(52.0f);

    // Cancel Button (0)
    if (m_powerDialogChoice == 0)
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{cancelPos}, Color{35, 50, 75, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{cancelPos}, Color{80, 180, 255, 255}, L.ToScreenDim(3.0f));
    }
    else
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{cancelPos}, Color{20, 25, 38, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{cancelPos}, Color{50, 65, 90, 180}, L.ToScreenDim(1.5f));
    }
    context.Render.DrawTextCentered("[ CANCEL ]", m_fontId, L.ToScreenDim(19.0f), Transform2D{cancelPos}, Colors::White);

    // Standby Button (1)
    if (m_powerDialogChoice == 1)
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{standbyPos}, Color{170, 100, 20, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{standbyPos}, Color{255, 185, 50, 255}, L.ToScreenDim(3.0f));
    }
    else
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{standbyPos}, Color{40, 26, 12, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{standbyPos}, Color{120, 75, 25, 180}, L.ToScreenDim(1.5f));
    }
    context.Render.DrawTextCentered("[ STANDBY ]", m_fontId, L.ToScreenDim(19.0f), Transform2D{standbyPos}, Colors::White);

    // Power Off Button (2)
    if (m_powerDialogChoice == 2)
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{pwrPos}, Color{150, 35, 40, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{pwrPos}, Color{255, 80, 85, 255}, L.ToScreenDim(3.0f));
    }
    else
    {
        context.Render.DrawRectangle(btnW, btnH, Transform2D{pwrPos}, Color{45, 18, 22, 255});
        context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{pwrPos}, Color{120, 40, 45, 180}, L.ToScreenDim(1.5f));
    }
    context.Render.DrawTextCentered("[ POWER OFF ]", m_fontId, L.ToScreenDim(19.0f), Transform2D{pwrPos}, Colors::White);
}

void MenuScene::UpdateMusicParticles(float dt)
{
    // If master volume is off or 0, or in standby, do not spawn new particles
    if (m_masterVolume > 1.0f && !m_isStandby)
    {
        m_musicParticleTimer += dt;
        // Spawn frequency: at vol 100 -> ~0.08s, at vol 50 -> ~0.26s, at vol 10 -> ~0.40s
        float spawnInterval = 0.08f + ((100.0f - m_masterVolume) / 100.0f) * 0.35f;
        if (m_musicParticleTimer >= spawnInterval && m_musicParticles.size() < 40)
        {
            m_musicParticleTimer = 0.0f;
            MusicNoteParticle p;
            // Spawn around speaker center (1690.0f, 695.0f) in TV base coords
            static uint32_t s_pSeed = 13579;
            auto pRand = []() -> float {
                s_pSeed = s_pSeed * 1664525u + 1013904223u;
                return static_cast<float>(s_pSeed % 10000) / 10000.0f;
            };

            float offsetAng = pRand() * 6.283185f;
            float offsetR = pRand() * 32.0f;
            p.BasePos = Vector2f{1690.0f + std::cos(offsetAng) * offsetR, 695.0f + std::sin(offsetAng) * offsetR};

            // Drift leftwards (into the room) and upwards
            float speedScale = 0.7f + (m_masterVolume / 100.0f) * 0.6f;
            p.Velocity = Vector2f{
                (-35.0f - pRand() * 45.0f) * speedScale,
                (-55.0f - pRand() * 50.0f) * speedScale
            };

            p.MaxLifetime = 2.0f + pRand() * 1.2f;
            p.Lifetime = 0.0f;
            p.Scale = 0.85f + pRand() * 0.45f;
            p.WobblePhase = pRand() * 6.28f;
            p.WobbleSpeed = 2.5f + pRand() * 3.0f;
            p.NoteType = (pRand() > 0.45f) ? 1 : 0;

            const Color noteCols[5] = {
                Color{100, 230, 255, 255}, // Neon cyan
                Color{255, 115, 220, 255}, // Electric magenta
                Color{255, 235, 90, 255},  // Radiant gold
                Color{180, 140, 255, 255}, // Bright purple
                Color{120, 255, 175, 255}  // Mint green
            };
            p.NoteColor = noteCols[static_cast<int>(pRand() * 5) % 5];

            m_musicParticles.push_back(p);
        }
    }

    // Update existing particles
    for (auto it = m_musicParticles.begin(); it != m_musicParticles.end();)
    {
        it->Lifetime += dt;
        if (it->Lifetime >= it->MaxLifetime)
        {
            it = m_musicParticles.erase(it);
        }
        else
        {
            it->BasePos.X += (it->Velocity.X + std::sin(it->Lifetime * it->WobbleSpeed + it->WobblePhase) * 20.0f) * dt;
            it->BasePos.Y += it->Velocity.Y * dt;
            ++it;
        }
    }
}

void MenuScene::RenderMusicParticles(GameContext& context) const
{
    if (m_musicParticles.empty()) return;

    const auto& L = m_tvLayout;
    for (const auto& p : m_musicParticles)
    {
        float progress = p.Lifetime / p.MaxLifetime;
        float alphaFactor = 1.0f;
        if (progress < 0.2f)
        {
            alphaFactor = progress / 0.2f;
        }
        else if (progress > 0.65f)
        {
            alphaFactor = (1.0f - progress) / 0.35f;
        }

        uint8_t a = static_cast<uint8_t>(std::clamp(alphaFactor * 240.0f, 0.0f, 240.0f));
        Color col = p.NoteColor;
        col.a = a;
        Color glowCol = p.NoteColor;
        glowCol.a = static_cast<uint8_t>(a * 0.35f);

        Vector2f screenPos = L.ToScreenPos(p.BasePos);
        float sc = L.Scale * p.Scale;

        if (p.NoteType == 0)
        {
            // Single musical note (♪)
            // Glowing aura behind notehead
            context.Render.DrawCircle(L.ToScreenDim(9.0f * sc), Transform2D{screenPos}, glowCol);

            // Notehead (tilted filled ellipse)
            context.Render.DrawCircle(L.ToScreenDim(5.0f * sc), Transform2D{screenPos, -0.3f, Vector2f{1.25f, 0.85f}}, col);

            // Stem (vertical line upwards from right side of notehead)
            Vector2f stemBase = screenPos + Vector2f{L.ToScreenDim(4.5f * sc), L.ToScreenDim(-1.0f * sc)};
            Vector2f stemTop = stemBase + Vector2f{0.0f, L.ToScreenDim(-17.0f * sc)};
            context.Render.DrawLine(stemBase, stemTop, col, L.ToScreenDim(2.0f * sc));

            // Curved flag at the top of stem
            Vector2f flagMid = stemTop + Vector2f{L.ToScreenDim(5.0f * sc), L.ToScreenDim(5.0f * sc)};
            Vector2f flagEnd = flagMid + Vector2f{L.ToScreenDim(2.5f * sc), L.ToScreenDim(7.0f * sc)};
            context.Render.DrawLine(stemTop, flagMid, col, L.ToScreenDim(2.0f * sc));
            context.Render.DrawLine(flagMid, flagEnd, col, L.ToScreenDim(1.5f * sc));
        }
        else
        {
            // Double beamed musical notes (♫)
            Vector2f leftHead = screenPos + Vector2f{L.ToScreenDim(-7.0f * sc), L.ToScreenDim(2.0f * sc)};
            Vector2f rightHead = screenPos + Vector2f{L.ToScreenDim(7.0f * sc), 0.0f};

            // Glow
            context.Render.DrawCircle(L.ToScreenDim(8.0f * sc), Transform2D{leftHead}, glowCol);
            context.Render.DrawCircle(L.ToScreenDim(8.0f * sc), Transform2D{rightHead}, glowCol);

            // Left notehead & right notehead
            context.Render.DrawCircle(L.ToScreenDim(4.5f * sc), Transform2D{leftHead, -0.3f, Vector2f{1.25f, 0.85f}}, col);
            context.Render.DrawCircle(L.ToScreenDim(4.5f * sc), Transform2D{rightHead, -0.3f, Vector2f{1.25f, 0.85f}}, col);

            // Stems
            Vector2f leftStemBase = leftHead + Vector2f{L.ToScreenDim(4.0f * sc), L.ToScreenDim(-1.0f * sc)};
            Vector2f leftStemTop = leftStemBase + Vector2f{0.0f, L.ToScreenDim(-16.0f * sc)};
            Vector2f rightStemBase = rightHead + Vector2f{L.ToScreenDim(4.0f * sc), L.ToScreenDim(-1.0f * sc)};
            Vector2f rightStemTop = rightStemBase + Vector2f{0.0f, L.ToScreenDim(-18.0f * sc)};

            context.Render.DrawLine(leftStemBase, leftStemTop, col, L.ToScreenDim(1.8f * sc));
            context.Render.DrawLine(rightStemBase, rightStemTop, col, L.ToScreenDim(1.8f * sc));

            // Connecting Beam between tops
            context.Render.DrawLine(leftStemTop, rightStemTop, col, L.ToScreenDim(3.5f * sc));
        }
    }
}

void MenuScene::RenderPowerOffAnimation(GameContext& context) const
{
    if (!m_isPoweringOff) return;

    const auto& L = m_tvLayout;
    const Vector2f screenCenter = L.GlassCenter;
    const float screenW = L.GlassSize.X;
    const float screenH = L.GlassSize.Y;

    if (m_powerOffTimer < 0.40f)
    {
        float p = m_powerOffTimer / 0.40f;
        float h = (1.0f - p) * screenH;
        if (h < 4.0f) h = 4.0f;

        float blackTopH = (screenH - h) * 0.5f;
        context.Render.DrawRectangle(screenW, blackTopH, Transform2D{Vector2f{screenCenter.X, screenCenter.Y - (screenH + h) * 0.25f}}, Color{0, 0, 0, 255});
        context.Render.DrawRectangle(screenW, blackTopH, Transform2D{Vector2f{screenCenter.X, screenCenter.Y + (screenH + h) * 0.25f}}, Color{0, 0, 0, 255});
        context.Render.DrawRectangle(screenW, h, Transform2D{screenCenter}, Color{235, 245, 255, 255});
    }
    else if (m_powerOffTimer < 0.65f)
    {
        float p = (m_powerOffTimer - 0.40f) / 0.25f;
        float w = (1.0f - p) * screenW;
        if (w < 8.0f) w = 8.0f;

        context.Render.DrawRectangle(screenW, screenH, Transform2D{screenCenter}, Color{0, 0, 0, 255});
        context.Render.DrawRectangle(w, 4.0f, Transform2D{screenCenter}, Color{245, 250, 255, 255});
    }
    else
    {
        float p = (m_powerOffTimer - 0.65f) / 0.20f;
        context.Render.DrawRectangle(screenW, screenH, Transform2D{screenCenter}, Color{0, 0, 0, 255});
        uint8_t a = static_cast<uint8_t>(std::clamp((1.0f - p) * 255.0f, 0.0f, 255.0f));
        context.Render.DrawCircle(L.ToScreenDim(4.0f), Transform2D{screenCenter}, Color{255, 255, 255, a});
    }
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

        if (targetCanvas == m_audioCanvas && mp_context)
        {
            UpdateVolumeBars(m_sfxVolumeBars, mp_context->Audio.GetSfxVolume());
            UpdateVolumeBars(m_musicVolumeBars, mp_context->Audio.GetMusicVolume());
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
        .Text = "LEADERBOARD",
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
        .Text = "Playground",
        .OnClick = [&context]() { context.Scenes.LoadScene<SandboxScene>(); },
        .Position = {col1X, -160.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "Physics",
        .OnClick = [&context]() { context.Scenes.LoadScene<SamplePhysics>(); },
        .Position = {col1X, -95.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "Audio & Music",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleAudio>(); },
        .Position = {col1X, -30.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "State Machine",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleStateMachine>(); },
        .Position = {col1X, 35.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    // Column 2
    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "Tween Effects",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleTween>(); },
        .Position = {col2X, -160.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "UI Showcase",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleUI>(); },
        .Position = {col2X, -95.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "Render Layer",
        .OnClick = [&context]() { context.Scenes.LoadScene<SampleRenderLayer>(); },
        .Position = {col2X, -30.0f},
        .Size = {btnW, btnH},
        .FontId = m_fontId,
        .FontSize = 28.0f
    });

    UIFactory::CreateButton(m_registry, m_debugCanvas, ButtonDescriptor{
        .Text = "Game Direct",
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

static constexpr float kWorkshopPipW = 96.0f;
static constexpr float kWorkshopPipH = 34.0f;
static constexpr float kWorkshopPipGap = 12.0f;

static constexpr float kWorkshopUltPipW = 98.0f;
static constexpr float kWorkshopUltPipH = 38.0f;
static constexpr float kWorkshopUltPipGap = 12.0f;

static Color GetPowerUpThemeColor(PowerUpType type)
{
    switch (type)
    {
    case PowerUpType::Storm:        return Color{255, 235, 40, 255}; // Electric Yellow
    case PowerUpType::FireBall:     return Color{255, 70, 20, 255};  // Inferno Red
    case PowerUpType::BigBall:      return Color{255, 120, 20, 255}; // Magma Orange
    case PowerUpType::MultiBall:    return Color{0, 220, 255, 255};  // Cyan Plasma
    case PowerUpType::StickyPaddle: return Color{245, 175, 40, 255}; // Honey Amber
    case PowerUpType::BottomBarrier:return Color{0, 240, 255, 255};  // Forcefield Cyan
    case PowerUpType::MagnetPaddle: return Color{220, 60, 255, 255}; // Polarized Violet
    case PowerUpType::GhostBall:    return Color{160, 240, 255, 255};// Spectral Ice
    case PowerUpType::TempoBall:    return Color{180, 70, 255, 255}; // Chrono Purple
    case PowerUpType::ExpandPaddle: return Color{50, 255, 80, 255};  // Emerald Green
    case PowerUpType::ShrinkPaddle: return Color{255, 60, 60, 255};  // Kinetic Red
    case PowerUpType::ExtraLife:    return Color{255, 100, 180, 255};// Cyber Pink
    case PowerUpType::ItemRegen:    return Color{32, 255, 176, 255}; // Biotech Mint
    default:                        return Color{0, 200, 255, 255};
    }
}

void MenuScene::CreateWorkshopModal(const GameContext& /*context*/)
{
    m_workshopCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_workshopCanvas, CanvasComponent{.IsEnabled = false});

    // 1. Massive Sleek Modal Backdrop Frame (2400 x 1400 px)
    UIFactory::CreatePanel(m_registry, m_workshopCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {2400.0f, 1400.0f},
        .Tint = Color{0, 0, 0, 0},
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
            .DefaultColor = Color{0, 0, 0, 0},
            .HoverColor = Color{255, 255, 255, 14},
            .PressedColor = Color{0, 0, 0, 35},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .ZOrder = 2
        });

        card.LevelText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "",
            .Position = {x, y + 108.0f},
            .FontId = m_fontId,
            .FontSize = 18.0f,
            .Tint = Colors::Gold,
            .ZOrder = 3
        });

        card.TitleText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "STORM",
            .Position = {x, y - 105.0f},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .Tint = Colors::White,
            .ZOrder = 3
        });

        card.CategoryText = NULL_ENTITY;

        card.CostText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
            .Text = "150 CR",
            .Position = {x, y + 66.0f},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .Tint = Colors::White,
            .ZOrder = 3
        });

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
        .Tint = Color{0, 0, 0, 0},
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
    constexpr float pipBtnW = kWorkshopPipW;
    constexpr float pipBtnH = kWorkshopPipH;
    constexpr float pipBtnGap = kWorkshopPipGap;
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
                m_descModalForUltimate = false;
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
        .Tint = Color{0, 0, 0, 0},
        .ZOrder = 2
    });

    m_inspectorDescText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "Description.",
        .Position = {600.0f, 65.0f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Color{225, 235, 250, 255},
        .MaxWidth = 780.0f,
        .ZOrder = 3
    });

    m_inspectorCurTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "CURRENT: ...",
        .Position = {600.0f, 125.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{100, 220, 255, 255},
        .MaxWidth = 800.0f,
        .ZOrder = 3
    });

    m_inspectorNextTierText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "VIEWING: ...",
        .Position = {600.0f, 180.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{255, 215, 80, 255},
        .MaxWidth = 800.0f,
        .ZOrder = 3
    });

    m_inspectorTierCostText = UIFactory::CreateText(m_registry, m_workshopUpgradesCanvas, TextDescriptor{
        .Text = "COST: ...",
        .Position = {600.0f, 235.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
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
    // --- TAB 2: ULTIMATES HANGAR (3 ALL-IN-ONE COLUMNS) ---
    // =========================================================================
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
        float y = -30.0f;
        int cardIdx = i;

        card.RootPanel = UIFactory::CreatePanel(m_registry, m_workshopUltimatesCanvas, PanelDescriptor{
            .Position = {x, y},
            .Size = {680.0f, 960.0f},
            .Tint = Color{0, 0, 0, 0},
            .ZOrder = 1
        });

        // 1. Title Header
        card.TitleText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = ProfileManager::GetUltimateName(card.Type),
            .Position = {x, -440.0f},
            .FontId = m_fontId,
            .FontSize = 34.0f,
            .Tint = Colors::Gold,
            .ZOrder = 3
        });

        card.CategoryText = NULL_ENTITY;

        // 2. Interactive Level Inspection Buttons (NIV. 1 to NIV. 5)
        constexpr float ultPipW = kWorkshopUltPipW;
        constexpr float ultPipH = kWorkshopUltPipH;
        constexpr float ultPipGap = kWorkshopUltPipGap;
        float startUltPipX = x - (5.0f * (ultPipW + ultPipGap) - ultPipGap) * 0.5f + (ultPipW * 0.5f);
        float ultPipY = -95.0f;

        for (int p = 0; p < 5; ++p)
        {
            float bx = startUltPipX + static_cast<float>(p) * (ultPipW + ultPipGap);
            int lvl = p + 1;
            Entity pipBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
                .Text = "LVL. " + std::to_string(lvl),
                .OnClick = [this, cardIdx, lvl, ultType = card.Type]() {
                    if (m_showDescriptionModal) return;
                    m_ultimateCards[cardIdx].InspectedLevel = lvl;
                    m_ultimateCards[cardIdx].SwitchTimer = 0.38f;
                    m_lastInspectedUltimate = ultType;
                    for (int c = 0; c < 3; ++c)
                    {
                        if (c != cardIdx && m_ultimatePreviewScenes[c])
                        {
                            m_ultimatePreviewScenes[c]->ClearActiveUltimates();
                        }
                    }
                    if (cardIdx < 3 && m_ultimatePreviewScenes[cardIdx])
                    {
                        m_ultimatePreviewScenes[cardIdx]->SetPreviewUltimate(ultType, lvl);
                    }
                    if (mp_context)
                    {
                        mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                    }
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
                .FontSize = 22.0f,
                .ZOrder = 3
            });
            card.LevelPipButtons[p] = pipBtn;
        }

        UltimateAbilityType ultType = card.Type;

        // 3. Description & Stats Toggle Button
        card.DescStatsBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "DESCRIPTION & STATS",
            .OnClick = [this, ultType]() {
                if (m_showDescriptionModal)
                {
                    CloseDescriptionModal();
                }
                else
                {
                    m_descModalForUltimate = true;
                    m_descModalUltimate = ultType;
                    OpenDescriptionModal();
                }
            },
            .Position = {x, -38.0f},
            .Size = {600.0f, 44.0f},
            .DefaultColor = Color{30, 60, 95, 240},
            .HoverColor = Color{45, 90, 145, 255},
            .PressedColor = Color{20, 45, 75, 255},
            .FontId = m_fontId,
            .FontSize = 24.0f,
            .ZOrder = 3
        });

        // 4. Stats & Description Info Box
        UIFactory::CreatePanel(m_registry, m_workshopUltimatesCanvas, PanelDescriptor{
            .Position = {x, 80.0f},
            .Size = {600.0f, 180.0f},
            .Tint = Color{24, 30, 48, 240},
            .ZOrder = 2
        });

        card.DescText = NULL_ENTITY;

        card.CurTierText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "CURRENT LEVEL: ...",
            .Position = {x, 20.0f},
            .FontId = m_fontId,
            .FontSize = 21.0f,
            .Tint = Color{100, 220, 255, 255},
            .MaxWidth = 560.0f,
            .ZOrder = 3
        });

        card.NextTierText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "VIEWING LEVEL: ...",
            .Position = {x, 75.0f},
            .FontId = m_fontId,
            .FontSize = 21.0f,
            .Tint = Colors::Gold,
            .MaxWidth = 560.0f,
            .ZOrder = 3
        });

        card.CostText = UIFactory::CreateText(m_registry, m_workshopUltimatesCanvas, TextDescriptor{
            .Text = "NEXT TIER COST: ...",
            .Position = {x, 130.0f},
            .FontId = m_fontId,
            .FontSize = 22.0f,
            .Tint = Color{80, 240, 140, 255},
            .MaxWidth = 560.0f,
            .ZOrder = 3
        });

        // 5. Big Upgrade Action Button
        card.UpgradeBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "UPGRADE",
            .OnClick = [this, cardIdx, ultType]() {
                if (m_showDescriptionModal) return;
                if (ProfileManager::Get().CanUpgradeUltimate(ultType))
                {
                    ProfileManager::Get().UpgradeUltimate(ultType);
                    int newLvl = ProfileManager::Get().GetUltimateLevel(ultType);
                    m_ultimateCards[cardIdx].InspectedLevel = newLvl;
                    m_ultimateCards[cardIdx].SwitchTimer = 0.38f;
                    m_lastInspectedUltimate = ultType;
                    if (cardIdx < 3 && m_ultimatePreviewScenes[cardIdx])
                    {
                        m_ultimatePreviewScenes[cardIdx]->SetPreviewUltimate(ultType, newLvl);
                    }
                    if (m_channelSwitchSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_channelSwitchSfxId, 85.0f, 1.25f);
                    if (m_bounceSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_bounceSfxId, 85.0f);
                    RefreshWorkshopUI();
                }
            },
            .Position = {x, 235.0f},
            .Size = {600.0f, 62.0f},
            .DefaultColor = Color{35, 145, 60, 255},
            .HoverColor = Color{50, 195, 85, 255},
            .PressedColor = Color{22, 95, 40, 255},
            .FontId = m_fontId,
            .FontSize = 28.0f,
            .ZOrder = 3
        });

        // 6. Equip Action Button
        card.EquipBtn = UIFactory::CreateButton(m_registry, m_workshopUltimatesCanvas, ButtonDescriptor{
            .Text = "EQUIP ULTIMATE",
            .OnClick = [this, ultType]() {
                if (m_showDescriptionModal) return;
                ProfileManager::Get().SetEquippedUltimate(ultType);
                m_lastInspectedUltimate = ultType;
                m_lastEquippedUltimate = ultType;
                if (mp_context)
                {
                    mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
                }
                const UltimateAbilityType allUltTypes[3] = {
                    UltimateAbilityType::PlasmaOrb,
                    UltimateAbilityType::DiscoBall,
                    UltimateAbilityType::PowerUpRain
                };
                for (int c = 0; c < 3; ++c)
                {
                    if (m_ultimatePreviewScenes[c])
                    {
                        bool isEquipped = (allUltTypes[c] == ultType);
                        m_ultimatePreviewScenes[c]->SetMusicEnabled(isEquipped);
                        m_ultimatePreviewScenes[c]->SetAudioEnabled(isEquipped);
                        m_ultimatePreviewScenes[c]->ClearActiveUltimates();
                        m_ultimatePreviewScenes[c]->ResetPreview();
                    }
                }
                if (m_clickSfxId != 0 && mp_context) mp_context->Audio.PlaySfx(m_clickSfxId, 85.0f);
                RefreshWorkshopUI();
            },
            .Position = {x, 325.0f},
            .Size = {600.0f, 62.0f},
            .DefaultColor = Color{40, 70, 130, 255},
            .HoverColor = Color{60, 105, 195, 255},
            .PressedColor = Color{25, 45, 85, 255},
            .FontId = m_fontId,
            .FontSize = 28.0f,
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
            m_previewScene->SetAudioEnabled(false);
            m_previewScene->ResetPreview();
        }
        if (mp_context)
        {
            mp_context->Audio.StopSfx();
        }
    }
    if (targetCanvas != m_workshopUltimatesCanvas)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (m_ultimatePreviewScenes[i])
            {
                m_ultimatePreviewScenes[i]->SetMusicEnabled(false);
                m_ultimatePreviewScenes[i]->SetAudioEnabled(false);
                m_ultimatePreviewScenes[i]->ClearActiveUltimates();
                m_ultimatePreviewScenes[i]->ResetPreview();
            }
        }
        if (mp_context)
        {
            mp_context->Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 100.0f, true);
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
        if (m_previewScene)
        {
            m_previewScene->SetAudioEnabled(true);
            m_previewScene->SetMusicEnabled(false);
        }
        TriggerPreviewChannelSwitch();
    }
    else if (targetCanvas == m_workshopUltimatesCanvas)
    {
        UltimateAbilityType equippedUlt = ProfileManager::Get().GetEquippedUltimate();
        for (size_t i = 0; i < m_ultimateCards.size(); ++i)
        {
            m_ultimateCards[i].SwitchTimer = 0.38f;
            if (i < 3 && m_ultimatePreviewScenes[i])
            {
                bool isEquipped = (m_ultimateCards[i].Type == equippedUlt);
                m_ultimatePreviewScenes[i]->SetMusicEnabled(isEquipped);
                m_ultimatePreviewScenes[i]->SetAudioEnabled(isEquipped);
                m_ultimatePreviewScenes[i]->SetPreviewUltimate(m_ultimateCards[i].Type, m_ultimateCards[i].InspectedLevel);
            }
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

            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].TitleText))
            {
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].TitleText).Text =
                    PowerUpManager::Get().GetName(type);
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].TitleText).Tint =
                    (type == m_selectedPowerUp) ? Colors::Gold : Colors::White;
            }

            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].CostText))
            {
                int curL = PowerUpManager::Get().GetLevel(type);
                int maxL = PowerUpManager::Get().GetMaxLevel(type);
                if (curL >= maxL)
                {
                    m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].CostText).Text = "MAXED";
                    m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].CostText).Tint = Colors::Gold;
                }
                else
                {
                    int c = PowerUpManager::Get().GetUpgradeCost(type);
                    bool canAfford = (PowerUpManager::Get().GetCoins() >= c);
                    m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].CostText).Text = std::to_string(c) + " CR";
                    m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].CostText).Tint =
                        canAfford ? Colors::White : Color{255, 185, 140, 255};
                }
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
            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].TitleText))
            {
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].TitleText).Text = "";
            }
            if (m_registry.HasComponent<TextComponent>(m_upgradeCards[slot].CostText))
            {
                m_registry.GetComponent<TextComponent>(m_upgradeCards[slot].CostText).Text = "";
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
        m_registry.GetComponent<TextComponent>(m_inspectorDescText).Tint = Color{235, 245, 255, 255};
    }

    if (m_inspectorCurTierText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_inspectorCurTierText))
    {
        m_registry.GetComponent<TextComponent>(m_inspectorCurTierText).Text =
            "CURRENT LEVEL (" + std::to_string(selCurLvl) + "): " + PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, selCurLvl);
        m_registry.GetComponent<TextComponent>(m_inspectorCurTierText).Tint = Color{80, 230, 255, 255};
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

        // Root card panel tint (kept transparent so simulation and custom background plates render)
        if (m_registry.HasComponent<PanelComponent>(card.RootPanel))
        {
            auto& panelComp = m_registry.GetComponent<PanelComponent>(card.RootPanel);
            panelComp.Tint = Color{0, 0, 0, 0};
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

uint32_t MenuScene::GetWorkshopBadgeTexture(PowerUpType type) const
{
    const auto idx = static_cast<size_t>(type);
    if (idx < sizeof(m_workshopBadgeTexIds) / sizeof(m_workshopBadgeTexIds[0]) && m_workshopBadgeTexIds[idx] != 0)
    {
        return m_workshopBadgeTexIds[idx];
    }
    return 0;
}

void MenuScene::RenderWorkshopBackgroundVisuals(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    constexpr float modalW = 2400.0f;
    constexpr float modalH = 1400.0f;
    const float halfW = modalW * 0.5f;
    const float halfH = modalH * 0.5f;

    // -------------------------------------------------------------
    // 1. Sci-Fi Workshop Deck & Holographic Blueprint Backdrop
    // -------------------------------------------------------------
    // Deep metallic workshop chassis base
    context.Render.DrawRectangle(modalW, modalH,
        Transform2D{Vector2f{screenCenterX, screenCenterY}},
        Color{12, 17, 28, 248});

    // Soft radial ambient illumination / energy fields
    // Left Hangar Bay: Soft Cyan illumination
    context.Render.DrawCircle(480.0f,
        Transform2D{Vector2f{screenCenterX - 670.0f, screenCenterY - 50.0f}},
        Color{0, 150, 255, 22});
    // Right Telemetry Bay: Soft Gold illumination
    context.Render.DrawCircle(520.0f,
        Transform2D{Vector2f{screenCenterX + 600.0f, screenCenterY - 35.0f}},
        Color{255, 185, 45, 18});

    // High-Tech Blueprint Grid inside the workshop modal
    const float gridMinX = screenCenterX - halfW + 20.0f;
    const float gridMaxX = screenCenterX + halfW - 20.0f;
    const float gridMinY = screenCenterY - halfH + 85.0f;
    const float gridMaxY = screenCenterY + halfH - 20.0f;

    // Fine grid lines (every 80px)
    for (float gx = gridMinX + 40.0f; gx <= gridMaxX; gx += 80.0f)
    {
        context.Render.DrawLine(Vector2f{gx, gridMinY}, Vector2f{gx, gridMaxY}, Color{22, 36, 62, 55}, 1.0f);
    }
    for (float gy = gridMinY + 40.0f; gy <= gridMaxY; gy += 80.0f)
    {
        context.Render.DrawLine(Vector2f{gridMinX, gy}, Vector2f{gridMaxX, gy}, Color{22, 36, 62, 55}, 1.0f);
    }

    // Prominent Blueprint Grid lines (every 240px)
    for (float gx = gridMinX + 120.0f; gx <= gridMaxX; gx += 240.0f)
    {
        context.Render.DrawLine(Vector2f{gx, gridMinY}, Vector2f{gx, gridMaxY}, Color{35, 65, 115, 80}, 1.5f);
    }
    for (float gy = gridMinY + 120.0f; gy <= gridMaxY; gy += 240.0f)
    {
        context.Render.DrawLine(Vector2f{gridMinX, gy}, Vector2f{gridMaxX, gy}, Color{35, 65, 115, 80}, 1.5f);
    }

    // Precision crosshairs (+) at major grid intersections
    constexpr float chLen = 6.0f;
    const Color crosshairCol{0, 220, 255, 75};
    for (float gx = gridMinX + 120.0f; gx <= gridMaxX; gx += 240.0f)
    {
        for (float gy = gridMinY + 120.0f; gy <= gridMaxY; gy += 240.0f)
        {
            context.Render.DrawLine(Vector2f{gx - chLen, gy}, Vector2f{gx + chLen, gy}, crosshairCol, 1.0f);
            context.Render.DrawLine(Vector2f{gx, gy - chLen}, Vector2f{gx, gy + chLen}, crosshairCol, 1.0f);
        }
    }

    // Vertical Cyber Spine / Data Conduit between Left Deck and Right Deck
    const float spineX = screenCenterX - 130.0f;
    context.Render.DrawLine(Vector2f{spineX, gridMinY + 20.0f}, Vector2f{spineX, gridMaxY - 20.0f}, Color{35, 60, 105, 160}, 2.0f);
    for (float py = gridMinY + 100.0f; py <= gridMaxY - 60.0f; py += 160.0f)
    {
        context.Render.DrawCircle(4.0f, Transform2D{Vector2f{spineX, py}}, Color{0, 220, 255, 180});
        context.Render.DrawLine(Vector2f{spineX - 12.0f, py}, Vector2f{spineX + 12.0f, py}, Color{0, 200, 255, 120}, 1.5f);
    }

    // Modal Frame Cyber Outline & Corner Brackets
    context.Render.DrawRectangleOutline(modalW, modalH,
        Transform2D{Vector2f{screenCenterX, screenCenterY}},
        Color{45, 95, 175, 230}, 3.0f);

    constexpr float cw = 40.0f;
    const Color bracketCol{255, 215, 60, 220};
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

    // -------------------------------------------------------------
    // 2. Tab 1 Backgrounds: Cards Hangar & Inspector Engineering Deck
    // -------------------------------------------------------------
    if (m_workshopActiveTabCanvas == m_workshopUpgradesCanvas)
    {
        // Dedicated Hangar Deck Plate for the 9-Card Grid
        const Vector2f hangarDeckCenter{screenCenterX - 670.0f, screenCenterY - 50.0f};
        const Vector2f hangarDeckSize{840.0f, 920.0f};
        context.Render.DrawRectangle(hangarDeckSize.X, hangarDeckSize.Y, Transform2D{hangarDeckCenter}, Color{15, 21, 35, 190});
        context.Render.DrawRectangleOutline(hangarDeckSize.X, hangarDeckSize.Y, Transform2D{hangarDeckCenter}, Color{36, 58, 96, 180}, 1.5f);

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
            Color elemCol = GetPowerUpThemeColor(type);

            int curLvl = PowerUpManager::Get().GetLevel(type);
            int maxLvl = PowerUpManager::Get().GetMaxLevel(type);
            bool isMaxed = (curLvl >= maxLvl);
            int cost = PowerUpManager::Get().GetUpgradeCost(type);
            bool canAfford = (!isMaxed && PowerUpManager::Get().GetCoins() >= cost);

            // Card Outer Body Plate (240 x 280 px)
            Color cardFill = isSelected ? Color{22, 32, 54, 255} : Color{16, 22, 38, 245};
            context.Render.DrawRectangle(240.0f, 280.0f, Transform2D{Vector2f{x, y}}, cardFill);

            // Card Header Banner Plate (Title backing)
            context.Render.DrawRectangle(228.0f, 40.0f, Transform2D{Vector2f{x, y - 105.0f}}, Color{24, 32, 52, 255});
            // Elemental Top Accent Line on the header
            context.Render.DrawLine(Vector2f{x - 114.0f, y - 125.0f}, Vector2f{x + 114.0f, y - 125.0f}, elemCol, isSelected ? 3.0f : 2.0f);

            // Central Hero Artwork Showcase
            Vector2f artCenter{x, y - 18.0f};
            uint32_t badgeTex = GetWorkshopBadgeTexture(type);

            // Radial ambient glow of the elemental color behind the hero badge
            uint8_t haloAlpha = isSelected ? 65 : 30;
            context.Render.DrawCircle(56.0f, Transform2D{artCenter}, Color{elemCol.r, elemCol.g, elemCol.b, haloAlpha});

            if (badgeTex != 0)
            {
                SpriteComponent badgeSprite{badgeTex, Colors::White};
                context.Render.DrawSprite(badgeSprite, Transform2D{artCenter, 0.0f, Vector2f{1.15f, 1.15f}});
            }

            // Card Cost Pill Badge (Centered at y + 66.0f)
            Vector2f costPillCenter{x, y + 66.0f};
            Vector2f costPillSize{154.0f, 30.0f};
            if (isMaxed)
            {
                context.Render.DrawRectangle(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Color{170, 135, 25, 220});
                context.Render.DrawRectangleOutline(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Colors::Gold, 1.5f);
            }
            else if (canAfford)
            {
                context.Render.DrawRectangle(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Color{22, 130, 58, 235});
                context.Render.DrawRectangleOutline(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Color{65, 245, 130, 255}, 1.5f);
            }
            else
            {
                context.Render.DrawRectangle(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Color{28, 34, 48, 220});
                context.Render.DrawRectangleOutline(costPillSize.X, costPillSize.Y, Transform2D{costPillCenter}, Color{55, 70, 95, 200}, 1.0f);
            }

            // Card Level Segmented Bar Tray (Centered at y + 108.0f)
            Vector2f trayCenter{x, y + 108.0f};
            Vector2f traySize{216.0f, 22.0f};
            context.Render.DrawRectangle(traySize.X, traySize.Y, Transform2D{trayCenter}, Color{10, 14, 24, 255});
            context.Render.DrawRectangleOutline(traySize.X, traySize.Y, Transform2D{trayCenter}, isSelected ? Colors::Gold : Color{40, 55, 85, 200}, 1.0f);

            // 5 Segmented Neon Tier Bars
            constexpr float barW = 36.0f;
            constexpr float barH = 12.0f;
            constexpr float barGap = 5.0f;
            float startBarX = x - (5.0f * barW + 4.0f * barGap) * 0.5f + (barW * 0.5f);

            for (int p = 0; p < 5; ++p)
            {
                Vector2f barCenter{startBarX + static_cast<float>(p) * (barW + barGap), y + 108.0f};
                bool isLit = (p < curLvl);

                if (isLit)
                {
                    Color litCol = isSelected ? Colors::Gold : elemCol;
                    context.Render.DrawRectangle(barW, barH, Transform2D{barCenter}, litCol);
                    context.Render.DrawLine(Vector2f{barCenter.X - barW * 0.5f, barCenter.Y - barH * 0.5f},
                                            Vector2f{barCenter.X + barW * 0.5f, barCenter.Y - barH * 0.5f},
                                            Colors::White, 1.5f);
                }
                else
                {
                    context.Render.DrawRectangle(barW, barH, Transform2D{barCenter}, Color{18, 24, 38, 220});
                    context.Render.DrawRectangleOutline(barW, barH, Transform2D{barCenter}, Color{35, 45, 68, 180}, 1.0f);
                }
            }

            // Unselected Card Outline
            if (!isSelected)
            {
                context.Render.DrawRectangleOutline(240.0f, 280.0f,
                    Transform2D{Vector2f{x, y}},
                    Color{42, 60, 96, 200}, 2.0f);
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

        // Right Column Inspector Visual Architecture
        const float inspCenterX = screenCenterX + 600.0f;
        const Vector2f inspDeckCenter{inspCenterX, screenCenterY - 35.0f};
        const Vector2f inspDeckSize{1040.0f, 900.0f};

        // Inspector Outer Cyber Frame with Corner Brackets
        context.Render.DrawRectangle(inspDeckSize.X, inspDeckSize.Y, Transform2D{inspDeckCenter}, Color{15, 21, 35, 190});
        context.Render.DrawRectangleOutline(inspDeckSize.X, inspDeckSize.Y, Transform2D{inspDeckCenter}, Color{45, 75, 125, 220}, 2.0f);

        constexpr float iLen = 22.0f;
        Color inspBracketCol{255, 215, 60, 200};
        context.Render.DrawLine(Vector2f{inspCenterX - 520.0f, screenCenterY - 485.0f}, Vector2f{inspCenterX - 520.0f + iLen, screenCenterY - 485.0f}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX - 520.0f, screenCenterY - 485.0f}, Vector2f{inspCenterX - 520.0f, screenCenterY - 485.0f + iLen}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX + 520.0f, screenCenterY - 485.0f}, Vector2f{inspCenterX + 520.0f - iLen, screenCenterY - 485.0f}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX + 520.0f, screenCenterY - 485.0f}, Vector2f{inspCenterX + 520.0f, screenCenterY - 485.0f + iLen}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX - 520.0f, screenCenterY + 415.0f}, Vector2f{inspCenterX - 520.0f + iLen, screenCenterY + 415.0f}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX - 520.0f, screenCenterY + 415.0f}, Vector2f{inspCenterX - 520.0f, screenCenterY + 415.0f - iLen}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX + 520.0f, screenCenterY + 415.0f}, Vector2f{inspCenterX + 520.0f - iLen, screenCenterY + 415.0f}, inspBracketCol, 2.5f);
        context.Render.DrawLine(Vector2f{inspCenterX + 520.0f, screenCenterY + 415.0f}, Vector2f{inspCenterX + 520.0f, screenCenterY + 415.0f - iLen}, inspBracketCol, 2.5f);

        // Header Plate behind Inspector Title
        Color selTheme = GetPowerUpThemeColor(m_selectedPowerUp);
        context.Render.DrawRectangle(980.0f, 54.0f, Transform2D{Vector2f{inspCenterX, screenCenterY - 440.0f}}, Color{20, 26, 44, 255});
        context.Render.DrawLine(Vector2f{inspCenterX - 490.0f, screenCenterY - 413.0f}, Vector2f{inspCenterX + 490.0f, screenCenterY - 413.0f}, selTheme, 2.0f);

        // Preview Window in Right Column (Enhanced 640x300 Widescreen CRT Monitor)
        const Vector2f monitorCenter{inspCenterX, screenCenterY - 260.0f};
        const Vector2f monitorSize{640.0f, 300.0f};
        RenderPowerUpSimulation(context, m_selectedPowerUp, monitorCenter, monitorSize);

        // Description Card Panel (Glass background with Cyan left accent bar)
        context.Render.DrawRectangle(840.0f, 65.0f, Transform2D{Vector2f{inspCenterX, screenCenterY + 65.0f}}, Color{20, 26, 42, 240});
        context.Render.DrawRectangleOutline(840.0f, 65.0f, Transform2D{Vector2f{inspCenterX, screenCenterY + 65.0f}}, Color{40, 58, 92, 180}, 1.0f);
        context.Render.DrawLine(Vector2f{inspCenterX - 420.0f, screenCenterY + 33.0f}, Vector2f{inspCenterX - 420.0f, screenCenterY + 97.0f}, Color{0, 220, 255, 255}, 4.0f);

        // Tier Specs Deck Panel (Glass background with Gold left accent bar and dividers)
        context.Render.DrawRectangle(840.0f, 150.0f, Transform2D{Vector2f{inspCenterX, screenCenterY + 185.0f}}, Color{18, 22, 36, 240});
        context.Render.DrawRectangleOutline(840.0f, 150.0f, Transform2D{Vector2f{inspCenterX, screenCenterY + 185.0f}}, Color{40, 58, 92, 180}, 1.0f);
        context.Render.DrawLine(Vector2f{inspCenterX - 420.0f, screenCenterY + 110.0f}, Vector2f{inspCenterX - 420.0f, screenCenterY + 260.0f}, Colors::Gold, 4.0f);
        context.Render.DrawLine(Vector2f{inspCenterX - 400.0f, screenCenterY + 155.0f}, Vector2f{inspCenterX + 400.0f, screenCenterY + 155.0f}, Color{35, 48, 75, 180}, 1.0f);
        context.Render.DrawLine(Vector2f{inspCenterX - 400.0f, screenCenterY + 210.0f}, Vector2f{inspCenterX + 400.0f, screenCenterY + 210.0f}, Color{35, 48, 75, 180}, 1.0f);
    }

    // -------------------------------------------------------------
    // 3. Tab 2 Backgrounds: Ultimates Ability Simulation Viewports
    // -------------------------------------------------------------
    // -------------------------------------------------------------
    // 3. Tab 2 Backgrounds: Ultimates Ability Simulation Viewports
    // -------------------------------------------------------------
    if (m_workshopActiveTabCanvas == m_workshopUltimatesCanvas)
    {
        const float ultColX[3] = {-720.0f, 0.0f, 720.0f};
        UltimateAbilityType equipped = ProfileManager::Get().GetEquippedUltimate();

        for (int i = 0; i < 3; ++i)
        {
            float x = screenCenterX + ultColX[i];
            float y = screenCenterY - 30.0f;

            UltimateAbilityType ultType = m_ultimateCards[i].Type;
            bool isEquipped = (ultType == equipped);
            bool isInspected = (ultType == m_lastInspectedUltimate);

            // Card background plate
            Color cardBg = isInspected ? Color{22, 32, 54, 255} : Color{16, 22, 36, 245};
            context.Render.DrawRectangle(680.0f, 960.0f, Transform2D{Vector2f{x, y}}, cardBg);
            Color borderCol = isEquipped ? Colors::Gold : Color{50, 80, 135, 200};
            context.Render.DrawRectangleOutline(680.0f, 960.0f, Transform2D{Vector2f{x, y}}, borderCol, isEquipped ? 3.5f : 1.5f);

            // Header Banner Plate behind Title
            context.Render.DrawRectangle(640.0f, 50.0f, Transform2D{Vector2f{x, screenCenterY - 440.0f}}, Color{22, 30, 52, 255});
            context.Render.DrawLine(Vector2f{x - 320.0f, screenCenterY - 415.0f}, Vector2f{x + 320.0f, screenCenterY - 415.0f}, borderCol, 2.0f);

            // Simulation Viewport on the card
            Vector2f simCenter{x, screenCenterY - 265.0f};
            Vector2f simSize{580.0f, 260.0f};

            RenderUltimateSimulation(context, ultType, m_ultimateCards[i].InspectedLevel, simCenter, simSize, m_ultimateCards[i].SwitchTimer);

            if (!isInspected)
            {
                // Subtle 35 alpha dimming on inactive cards to keep focus on the inspected one while keeping previews vibrant
                context.Render.DrawRectangle(simSize.X - 4.0f, simSize.Y - 4.0f, Transform2D{simCenter}, Color{0, 0, 0, 35});
            }
        }
    }
}

void MenuScene::RenderWorkshopForegroundVisuals(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    // 1. Tab 1 Foreground Reticles, Pulsing Selection Frames & CTA Glows
    if (m_workshopActiveTabCanvas == m_workshopUpgradesCanvas)
    {
        const float cardColX[3] = {-930.0f, -670.0f, -410.0f};
        const float cardRowY[3] = {-350.0f, -50.0f, 250.0f};

        for (size_t slot = 0; slot < 9; ++slot)
        {
            int pwrIdx = m_workshopScrollRow * 3 + static_cast<int>(slot);
            if (pwrIdx >= 13) continue;

            PowerUpType type = kAllWorkshopPowerUps[pwrIdx];
            if (type != m_selectedPowerUp) continue;

            float x = screenCenterX + cardColX[slot % 3];
            float y = screenCenterY + cardRowY[slot / 3];

            // Card Selection Frame with Pulsing Gold & Cyber Corner Brackets
            float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 4.0f);
            uint8_t alpha = static_cast<uint8_t>(200.0f + pulse * 55.0f);
            context.Render.DrawRectangleOutline(240.0f, 280.0f,
                Transform2D{Vector2f{x, y}},
                Color{255, 215, 60, alpha}, 3.5f);

            // Corner glowing brackets
            constexpr float cLen = 16.0f;
            Color goldColor{255, 215, 60, 255};
            context.Render.DrawLine(Vector2f{x - 120.0f, y - 140.0f}, Vector2f{x - 120.0f + cLen, y - 140.0f}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x - 120.0f, y - 140.0f}, Vector2f{x - 120.0f, y - 140.0f + cLen}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x + 120.0f, y - 140.0f}, Vector2f{x + 120.0f - cLen, y - 140.0f}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x + 120.0f, y - 140.0f}, Vector2f{x + 120.0f, y - 140.0f + cLen}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x - 120.0f, y + 140.0f}, Vector2f{x - 120.0f + cLen, y + 140.0f}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x - 120.0f, y + 140.0f}, Vector2f{x - 120.0f, y + 140.0f - cLen}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x + 120.0f, y + 140.0f}, Vector2f{x + 120.0f - cLen, y + 140.0f}, goldColor, 3.0f);
            context.Render.DrawLine(Vector2f{x + 120.0f, y + 140.0f}, Vector2f{x + 120.0f, y + 140.0f - cLen}, goldColor, 3.0f);
            break;
        }

        // Reticle on Inspected Level Button
        const float inspCenterX = screenCenterX + 600.0f;
        constexpr float pipW = kWorkshopPipW;
        constexpr float pipH = kWorkshopPipH;
        constexpr float pipGap = kWorkshopPipGap;
        float startX = inspCenterX - (5.0f * (pipW + pipGap) - pipGap) * 0.5f + (pipW * 0.5f);
        float barY = screenCenterY - 65.0f;
        float activeBx = startX + static_cast<float>(m_inspectedLevel - 1) * (pipW + pipGap);

        float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 5.0f);
        uint8_t glowAlpha = static_cast<uint8_t>(180.0f + pulse * 75.0f);
        context.Render.DrawRectangleOutline(pipW, pipH,
            Transform2D{Vector2f{activeBx, barY}}, Color{255, 215, 60, glowAlpha}, 2.5f);

        // Animated Glowing Pulse Border around Upgrade CTA Button when affordable
        int curSelLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
        int maxSelLvl = PowerUpManager::Get().GetMaxLevel(m_selectedPowerUp);
        if (curSelLvl < maxSelLvl)
        {
            int costSel = PowerUpManager::Get().GetUpgradeCost(m_selectedPowerUp);
            if (PowerUpManager::Get().GetCoins() >= costSel)
            {
                float upPulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 6.0f);
                uint8_t upAlpha = static_cast<uint8_t>(180.0f + upPulse * 75.0f);
                context.Render.DrawRectangleOutline(644.0f, 74.0f,
                    Transform2D{Vector2f{inspCenterX, screenCenterY + 345.0f}},
                    Color{80, 255, 140, upAlpha}, 3.0f);
            }
        }
    }

    // 2. Tab 2 Foreground Reticle on Inspected Pip
    if (m_workshopActiveTabCanvas == m_workshopUltimatesCanvas)
    {
        const float ultColX[3] = {-720.0f, 0.0f, 720.0f};
        constexpr float ultPipW = kWorkshopUltPipW;
        constexpr float ultPipH = kWorkshopUltPipH;
        constexpr float ultPipGap = kWorkshopUltPipGap;

        for (int i = 0; i < 3; ++i)
        {
            float x = screenCenterX + ultColX[i];
            float startUltPipX = x - (5.0f * (ultPipW + ultPipGap) - ultPipGap) * 0.5f + (ultPipW * 0.5f);
            float activePipX = startUltPipX + static_cast<float>(m_ultimateCards[i].InspectedLevel - 1) * (ultPipW + ultPipGap);
            float pipY = screenCenterY - 95.0f;

            float pulse = 0.5f + 0.5f * std::sin(m_previewAnimTime * 5.0f);
            uint8_t glowAlpha = static_cast<uint8_t>(180.0f + pulse * 75.0f);
            context.Render.DrawRectangleOutline(ultPipW, ultPipH,
                Transform2D{Vector2f{activePipX, pipY}}, Color{255, 215, 60, glowAlpha}, 2.0f);
        }
    }

    // 3. Description & Stats Modal Overlay (Universal for PowerUps and Ultimates)
    if (m_showDescriptionModal)
    {
        Vector2f modalCenter{screenCenterX, screenCenterY};
        Vector2f modalSize{1360.0f, 860.0f};

        // Fullscreen Dark Dimming Backdrop
        context.Render.DrawRectangle(viewSize.X * 2.0f, viewSize.Y * 2.0f, Transform2D{modalCenter}, Color{6, 10, 20, 245});

        // Main Dialog Solid Window Panel
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

        std::string title;
        std::string desc;
        int selCurLvl = 1;
        int selInsLvl = 1;

        if (m_descModalForUltimate)
        {
            title = ProfileManager::GetUltimateName(m_descModalUltimate);
            desc = ProfileManager::GetUltimateDescription(m_descModalUltimate);
            selCurLvl = ProfileManager::Get().GetUltimateLevel(m_descModalUltimate);
            for (const auto& uc : m_ultimateCards)
            {
                if (uc.Type == m_descModalUltimate)
                {
                    selInsLvl = uc.InspectedLevel;
                    break;
                }
            }
        }
        else
        {
            title = PowerUpManager::Get().GetName(m_selectedPowerUp);
            desc = PowerUpManager::Get().GetDescription(m_selectedPowerUp);
            selCurLvl = PowerUpManager::Get().GetLevel(m_selectedPowerUp);
            selInsLvl = m_inspectedLevel;
        }

        // Dialog Title
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
        std::string wrappedDesc = context.Render.WrapText(desc, 1200.0f, m_fontId, 23.0f);
        context.Render.DrawTextCentered(wrappedDesc, m_fontId, 23.0f, Transform2D{Vector2f{modalCenter.X, modalCenter.Y - 265.0f}}, Color{225, 235, 250, 255});

        // 5 Tier Rows for NIV. 1 to NIV. 5
        constexpr float rowCenterY[5] = {-160.0f, -75.0f, 10.0f, 95.0f, 180.0f};
        for (int lvl = 1; lvl <= 5; ++lvl)
        {
            float ry = modalCenter.Y + rowCenterY[lvl - 1];
            bool isCur = (lvl == selCurLvl);
            bool isIns = (lvl == selInsLvl);
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

            std::string tierInfo = m_descModalForUltimate
                ? ProfileManager::GetUltimateTierInfo(m_descModalUltimate, lvl)
                : PowerUpManager::Get().GetTierInfoForLevel(m_selectedPowerUp, lvl);
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

        // Render dynamic explosion / storm lightning flash impact onto the preview monitor
        const auto& postParams = m_previewScene->GetPostProcessParams();
        if (postParams.FlashIntensity > 0.001f)
        {
            Color flashCol = postParams.FlashColor;
            flashCol.a = static_cast<uint8_t>(std::clamp(postParams.FlashIntensity * 255.0f, 0.0f, 255.0f));
            context.Render.DrawRectangle(size.X - 4.0f, size.Y - 4.0f, Transform2D{center}, flashCol);
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
    (void)level;
    int idx = 0;
    if (type == UltimateAbilityType::DiscoBall) idx = 1;
    else if (type == UltimateAbilityType::PowerUpRain) idx = 2;

    bool isActive = (type == m_lastInspectedUltimate);

    // Solid dark monitor screen background
    context.Render.DrawRectangle(size.X, size.Y, Transform2D{center}, Color{10, 14, 24, 255});

    if (isActive && m_ultimatePreviewScenes[idx] && m_ultimateCardTextures[idx])
    {
        m_ultimateCardTextures[idx]->clear(sf::Color(8, 11, 18, 255));

        context.Render.PushRenderTarget(*m_ultimateCardTextures[idx]);
        m_ultimatePreviewScenes[idx]->OnRender(context);
        context.Render.PopRenderTarget();

        m_ultimateCardTextures[idx]->display();

        constexpr float kSwitchDuration = 0.38f;
        float switchProgress = std::clamp(switchTimer / kSwitchDuration, 0.0f, 1.0f);
        uint32_t previewShader = (context.Rules.GetRule(Rule::Graphics::EnableShader)) ? m_previewShaderId : 0;

        context.Render.DrawRawTexture(
            m_ultimateCardTextures[idx]->getTexture(),
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

        // Render dynamic ultimate flash impact onto the preview card
        const auto& postParams = m_ultimatePreviewScenes[idx]->GetPostProcessParams();
        if (postParams.FlashIntensity > 0.001f)
        {
            Color flashCol = postParams.FlashColor;
            flashCol.a = static_cast<uint8_t>(std::clamp(postParams.FlashIntensity * 255.0f, 0.0f, 255.0f));
            context.Render.DrawRectangle(size.X - 4.0f, size.Y - 4.0f, Transform2D{center}, flashCol);
        }
    }
    else
    {
        // Inactive card: Render retro CRT pixel static noise ("bruit de pixel") with zero simulation overhead!
        uint32_t previewShader = (context.Rules.GetRule(Rule::Graphics::EnableShader)) ? m_previewShaderId : 0;
        if (previewShader != 0 && m_ultimateCardTextures[idx])
        {
            context.Render.DrawRawTexture(
                m_ultimateCardTextures[idx]->getTexture(),
                size.X - 4.0f,
                size.Y - 4.0f,
                Transform2D{center},
                Colors::White,
                previewShader,
                m_previewAnimTime,
                1.0f
            );
        }
        else
        {
            context.Render.DrawRectangle(size.X - 4.0f, size.Y - 4.0f, Transform2D{center}, Color{10, 14, 24, 255});
            for (int p = 0; p < 8; ++p)
            {
                float py = center.Y - (size.Y * 0.4f) + static_cast<float>(p) * (size.Y * 0.1f);
                context.Render.DrawRectangle(size.X * 0.85f, 2.5f, Transform2D{Vector2f{center.X, py}}, Color{120, 160, 220, 45});
            }
        }
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

    // High-Voltage Plasma Storm Shader Overlay in Workshop Preview
    if (m_plasmaShaderId != 0 && context.Rules.GetRule(Rule::Graphics::EnableShader))
    {
        Vector2f simSize{1160.0f, 540.0f};
        Color plasmaTint{180, 80, 255, 160};
        context.Render.DrawScannerEffect(simSize.X, simSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, plasmaTint, animTime, m_plasmaShaderId);
    }

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

