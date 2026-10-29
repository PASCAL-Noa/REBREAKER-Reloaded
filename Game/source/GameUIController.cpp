#include "UI/GameUIController.h"
#include "Scenes/GameScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/LevelSelectScene.h"
#include "Core/SceneManager.h"
#include "UI/UIFactory.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/TextInputComponent.h"
#include "ECS/Components/UI/RectTransform.h"
#include "ECS/Systems/UISystem.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "Math/EasingFunctions.h"
#include "Core/PlayerPrefs.h"
#include "Core/InputRemapManager.h"
#include "Core/GameRules.h"
#include "AudioMixer.h"
#include "Graphics/Renderer.h"
#include "Events/CheatSubmitEvent.h"
#include "PowerUps/PowerUpConfig.h"
#include "Core/ProfileManager.h"
#include <algorithm>
#include <cmath>

GameUIController::GameUIController(Registry& registry, GameContext& context, GameScene& scene)
    : m_registry(registry),
      m_context(context),
      m_scene(scene)
{
}

void GameUIController::OnInit(uint32_t fontId, uint32_t heartTexId, uint32_t fireTexId, uint32_t bounceSfxId, uint32_t scoreRecordedSfxId)
{
    m_fontId = fontId;
    m_heartTexId = heartTexId;
    m_fireTexId = fireTexId;
    m_bounceSfxId = bounceSfxId;
    m_scoreRecordedSfxId = scoreRecordedSfxId;

    CreateHUD();
    m_textFeedback = std::make_unique<TextFeedback>(m_registry, m_context, m_fontId, m_fireTexId);
    m_textFeedback->OnInit(m_uiCanvas);

    CreatePauseMenu();
    CreateLevelSelectModal();
    CreateAudioTab();
    CreateRenderTab();
    CreateInputsTab();
    CreateGamerulesTab();
    CreateCheatsTab();
    CreateSettingsLayout();
    CreatePowerUpTesterUI();
    CreateGameOverMenu();
    CreateLevelClearMenu();
    CreateVictoryMenu();

    SetupSubscriptions();
}

void GameUIController::SetupSubscriptions()
{
    m_stateSub = m_context.Events.SubscribeScoped<GameStateChangedEvent>([this](const GameStateChangedEvent& e) {
        ShowState(e.State);
    });

    m_scoreSub = m_context.Events.SubscribeScoped<ScoreChangedEvent>([this](const ScoreChangedEvent& e) {
        if (m_scoreTextEntity != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_scoreTextEntity) && m_registry.HasComponent<RectTransform>(m_scoreTextEntity))
        {
            auto& textComp = m_registry.GetComponent<TextComponent>(m_scoreTextEntity);
            auto& textRect = m_registry.GetComponent<RectTransform>(m_scoreTextEntity);
            textComp.Text = "Score : " + std::to_string(e.Score);

            Vector2f textSize = m_context.Render.GetTextSize(textComp.Text, textComp.FontId, textComp.FontSize);
            if (m_textFeedback)
            {
                m_textFeedback->SetFlamePosition(textRect, textSize.X + 40.0f, 50.0f);
            }
        }
    });

    m_livesSub = m_context.Events.SubscribeScoped<LivesChangedEvent>([this](const LivesChangedEvent& e) {
        for (size_t i = 0; i < m_heartEntities.size(); ++i)
        {
            SetHeartActive(static_cast<int>(i), static_cast<int>(i) < e.Lives);
        }
    });
}

void GameUIController::ShowState(GameUIState state)
{
    m_currentState = state;

    bool isSettingsOpen = false;
    if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas))
    {
        isSettingsOpen = m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled;
    }

    bool isLevelSelectOpen = false;
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas))
    {
        isLevelSelectOpen = m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled;
    }

    if (m_pauseCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_pauseCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_pauseCanvas).IsEnabled = (state == GameUIState::Paused && !isSettingsOpen && !isLevelSelectOpen);
    }

    if (m_uiCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_uiCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_uiCanvas).IsEnabled =
            (state == GameUIState::Playing || state == GameUIState::Countdown || (state == GameUIState::Paused && !isSettingsOpen && !isLevelSelectOpen));
    }

    if (state != GameUIState::Paused)
    {
        if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled = false;
            ClearLevelSelectCards();
        }

        if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas))
        {
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
            if (m_activeTabCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_activeTabCanvas))
            {
                m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
                m_activeTabCanvas = NULL_ENTITY;
            }
        }
    }

    if (m_cheatsTabBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = m_context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    if (m_powerUpTesterCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_powerUpTesterCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_powerUpTesterCanvas).IsEnabled = (m_powerUpTesterActive && state == GameUIState::Playing);
    }

    if (m_gameOverCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_gameOverCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_gameOverCanvas).IsEnabled = (state == GameUIState::GameOver || state == GameUIState::Victory);
    }

    if (m_levelClearCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelClearCanvas))
    {
        bool isLevelClear = (state == GameUIState::LevelTransition);
        m_registry.GetComponent<CanvasComponent>(m_levelClearCanvas).IsEnabled = isLevelClear;
        if (isLevelClear)
        {
            if (m_levelClearTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearTitleText))
            {
                m_registry.GetComponent<TextComponent>(m_levelClearTitleText).Text =
                    "STAGE " + std::to_string(m_scene.GetCurrentLevel()) + " CLEARED!";
            }
            if (m_levelClearStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearStatsText))
            {
                m_registry.GetComponent<TextComponent>(m_levelClearStatsText).Text =
                    "Accumulated Score: " + std::to_string(m_scene.GetScoreManager().GetScore()) +
                    "  |  Lives: " + std::to_string(m_scene.GetLives());
            }
        }
    }

    if (m_victoryCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_victoryCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_victoryCanvas).IsEnabled = false;
    }
}

void GameUIController::OnUpdate(float dt)
{
    // 0. Rebinding input capture
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
            KeyCode pressed = m_context.Input.GetLastKeyPressed();
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
                if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                UpdateInputButtons();
                return;
            }
        }
        else
        {
            // Gamepad capture (ESC cancels; Back button 6 cancels after debounce)
            if (m_context.Input.IsKeyPress(KeyCode::Escape) || (m_rebindTimer < 4.85f && m_context.Input.IsJoystickButtonPress(0, 6) && m_rebindAction != InputAction::Pause))
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
                int joyInput = m_context.Input.GetAnyGamepadInputTriggered(0);
                if (joyInput >= 0)
                {
                    InputRemapManager::Get().SetGamepadBinding(m_rebindAction, joyInput);
                    InputRemapManager::Get().SaveToPrefs();

                    m_isRebinding = false;
                    m_rebindAction = InputAction::Count;
                    UISystem::SetNavigationEnabled(true);
                    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    UpdateInputButtons();
                    return;
                }
            }
        }

        // Live countdown text update
        UpdateInputButtons();
        return;
    }

    // 1. Escape or Gamepad B navigation for submenus / modals
    if (m_context.Input.IsMenuCancelTriggered())
    {
        if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas) &&
            m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            CloseLevelSelectModal();
            return;
        }
        if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas) &&
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled)
        {
            CloseSettingsLayout();
            return;
        }
    }

    // 2. Active Level Select modal updates
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas) &&
        m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled)
    {
        m_levelSelectAnimTime += dt;

        auto& levelManager = m_scene.GetLevelManager();
        const int totalLevels = levelManager.GetLevelCount();
        constexpr int kInGameCardsPerPage = 5;
        constexpr float kInGameCardW = 250.0f;
        constexpr float kInGameCardH = 430.0f;
        constexpr float kInGameCardGap = 35.0f;
        constexpr float kInGameCardCenterY = 0.0f;
        const int totalPages = std::max(1, (totalLevels + kInGameCardsPerPage - 1) / kInGameCardsPerPage);

        // Page flip
        if ((m_context.Input.IsKeyPress(KeyCode::Left) || m_context.Input.IsJoystickButtonPress(0, 4) || m_context.Input.IsJoystickButtonPress(0, 100)) && m_levelSelectPage > 0)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            m_levelSelectPage--;
            m_selectedLevelCard = m_levelSelectPage * kInGameCardsPerPage;
        }
        else if ((m_context.Input.IsKeyPress(KeyCode::Right) || m_context.Input.IsJoystickButtonPress(0, 5) || m_context.Input.IsJoystickButtonPress(0, 101)) && m_levelSelectPage < totalPages - 1)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            m_levelSelectPage++;
            m_selectedLevelCard = m_levelSelectPage * kInGameCardsPerPage;
        }

        const int startLevelIndex = m_levelSelectPage * kInGameCardsPerPage;
        for (int slot = 0; slot < 5; ++slot)
        {
            KeyCode key = static_cast<KeyCode>(static_cast<int>(KeyCode::Num1) + slot);
            KeyCode numpadKey = static_cast<KeyCode>(static_cast<int>(KeyCode::Numpad1) + slot);
            if (m_context.Input.IsKeyPress(key) || m_context.Input.IsKeyPress(numpadKey))
            {
                int lvlIdx = startLevelIndex + slot;
                if (lvlIdx < totalLevels && levelManager.IsLevelUnlocked(lvlIdx + 1))
                {
                    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    CloseLevelSelectModal();
                    m_scene.LoadLevel(lvlIdx, /*preserveStats=*/false);
                    m_scene.StartCountdown();
                    return;
                }
            }
        }

        // Enter / Space / Gamepad A to launch selected
        if (m_context.Input.IsKeyPress(KeyCode::Enter) || m_context.Input.IsKeyPress(KeyCode::Space) || m_context.Input.IsJoystickButtonPress(0, 0))
        {
            int lvlIdx = m_selectedLevelCard;
            if (lvlIdx >= 0 && lvlIdx < totalLevels && levelManager.IsLevelUnlocked(lvlIdx + 1))
            {
                if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                CloseLevelSelectModal();
                m_scene.LoadLevel(lvlIdx, /*preserveStats=*/false);
                m_scene.StartCountdown();
                return;
            }
        }

        // Hover tracking and mouse click
        auto [mouseX, mouseY] = m_context.Input.GetMousePosition();
        const Vector2f viewSize = m_context.Render.GetLogicalViewSize();
        const float screenCenterX = viewSize.X * 0.5f;
        const float screenCenterY = viewSize.Y * 0.5f;
        const Vector2f centeredMouse{ mouseX - screenCenterX, mouseY - screenCenterY };

        constexpr float totalSpan = static_cast<float>(kInGameCardsPerPage) * kInGameCardW + static_cast<float>(kInGameCardsPerPage - 1) * kInGameCardGap;
        constexpr float startX = -(totalSpan - kInGameCardW) * 0.5f;
        constexpr float stepX = kInGameCardW + kInGameCardGap;

        m_hoveredLevelCard = -1;
        const int levelsOnThisPage = std::clamp(totalLevels - startLevelIndex, 0, kInGameCardsPerPage);
        const bool mouseClicked = m_context.Input.IsMouseButtonPress(MouseButton::Left);

        for (int slot = 0; slot < levelsOnThisPage; ++slot)
        {
            const float cX = startX + static_cast<float>(slot) * stepX;
            const float cY = kInGameCardCenterY;

            if (centeredMouse.X >= cX - kInGameCardW * 0.5f && centeredMouse.X <= cX + kInGameCardW * 0.5f &&
                centeredMouse.Y >= cY - kInGameCardH * 0.5f && centeredMouse.Y <= cY + kInGameCardH * 0.5f)
            {
                m_hoveredLevelCard = startLevelIndex + slot;
                if (mouseClicked)
                {
                    m_selectedLevelCard = m_hoveredLevelCard;
                    int lvlIdx = m_selectedLevelCard;
                    if (lvlIdx < totalLevels && levelManager.IsLevelUnlocked(lvlIdx + 1))
                    {
                        if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                        CloseLevelSelectModal();
                        m_scene.LoadLevel(lvlIdx, /*preserveStats=*/false);
                        m_scene.StartCountdown();
                        return;
                    }
                }
                break;
            }
        }

        // Navigation arrows click
        if (totalPages > 1 && mouseClicked)
        {
            constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
            if (centeredMouse.X >= -arrowX - 27.0f && centeredMouse.X <= -arrowX + 27.0f &&
                centeredMouse.Y >= kInGameCardCenterY - 37.0f && centeredMouse.Y <= kInGameCardCenterY + 37.0f)
            {
                if (m_levelSelectPage > 0)
                {
                    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    m_levelSelectPage--;
                    m_selectedLevelCard = m_levelSelectPage * kInGameCardsPerPage;
                }
            }
            else if (centeredMouse.X >= arrowX - 27.0f && centeredMouse.X <= arrowX + 27.0f &&
                     centeredMouse.Y >= kInGameCardCenterY - 37.0f && centeredMouse.Y <= kInGameCardCenterY + 37.0f)
            {
                if (m_levelSelectPage < totalPages - 1)
                {
                    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    m_levelSelectPage++;
                    m_selectedLevelCard = m_levelSelectPage * kInGameCardsPerPage;
                }
            }
        }

        // Back button click
        constexpr float backY = 330.0f;
        constexpr float backW = 300.0f;
        constexpr float backH = 52.0f;
        if (mouseClicked &&
            centeredMouse.X >= -backW * 0.5f && centeredMouse.X <= backW * 0.5f &&
            centeredMouse.Y >= backY - backH * 0.5f && centeredMouse.Y <= backY + backH * 0.5f)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            CloseLevelSelectModal();
            return;
        }
    }

    if (m_cheatsTabBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = m_context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    if (m_currentState == GameUIState::LevelTransition)
    {
        if (m_levelClearTimerText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_levelClearTimerText))
        {
            int remainingSecs = static_cast<int>(std::ceil(std::max(0.0f, m_scene.GetTransitionTimer())));
            m_registry.GetComponent<TextComponent>(m_levelClearTimerText).Text =
                "NEXT STAGE IN " + std::to_string(remainingSecs) + "s (OR PRESS SPACE)";
        }
    }

    if (m_currentState == GameUIState::Playing && m_textFeedback)
    {
        m_textFeedback->OnUpdate(dt, m_scene.GetScoreManager().GetComboMultiplier(), m_scene.GetScoreManager().GetScore());
    }
}

void GameUIController::TriggerLifeLostAnimation(int currentLives)
{
    if (currentLives >= 0 && static_cast<size_t>(currentLives) < m_heartEntities.size())
    {
        Entity actualHeart = m_heartEntities[currentLives];

        if (m_registry.HasComponent<RectTransform>(actualHeart) &&
            m_registry.HasComponent<RectTransform>(m_explodingHeart) &&
            m_registry.HasComponent<SpriteComponent>(m_explodingHeart) &&
            m_registry.HasComponent<TweenComponent>(m_explodingHeart))
        {
            auto& actualTransform = m_registry.GetComponent<RectTransform>(actualHeart);
            auto& t = m_registry.GetComponent<RectTransform>(m_explodingHeart);
            auto& s = m_registry.GetComponent<SpriteComponent>(m_explodingHeart);
            auto& tweenComp = m_registry.GetComponent<TweenComponent>(m_explodingHeart);

            t.Position = actualTransform.Position;
            t.AnchorPoint = actualTransform.AnchorPoint;
            t.Parent = actualTransform.Parent;
            t.Size = actualTransform.Size;
            s.Tint = Colors::White;

            TweenConfig<float> config;
            config.Start = 0.0f;
            config.End = 1.0f;
            config.Duration = 0.5f;
            config.Ease = EasingFunctions::EasingType::EaseOutQuad;

            Entity heartEntity = m_explodingHeart;
            Vector2f baseSize = t.Size;

            config.Setter = [this, heartEntity, baseSize](float val) {
                if (m_registry.HasComponent<RectTransform>(heartEntity) && m_registry.HasComponent<SpriteComponent>(heartEntity)) {
                    auto& transform = m_registry.GetComponent<RectTransform>(heartEntity);
                    auto& sprite = m_registry.GetComponent<SpriteComponent>(heartEntity);

                    transform.Size = Vector2f{baseSize.X + baseSize.X * 1.5f * val, baseSize.Y + baseSize.Y * 1.5f * val};
                    sprite.Tint.a = static_cast<uint8_t>(255.0f * (1.0f - val));
                }
            };

            tweenComp.AddTween(config);
            actualTransform.IsActive = false;
        }
    }
}

void GameUIController::SetHeartActive(int index, bool active)
{
    if (index >= 0 && static_cast<size_t>(index) < m_heartEntities.size())
    {
        Entity heart = m_heartEntities[index];
        if (m_registry.HasComponent<RectTransform>(heart))
        {
            m_registry.GetComponent<RectTransform>(heart).IsActive = active;
        }
    }
}

void GameUIController::SetupGameOver(uint32_t finalScore, int finalLevel)
{
    std::string defaultName = ProfileManager::Get().GetUsername();
    m_enteredPlayerName = defaultName;

    uint32_t earnedCredits = ProfileManager::ConvertScoreToCredits(finalScore);
    int currentBank = PowerUpManager::Get().GetCoins();

    if (m_gameOverTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverTitleText))
    {
        auto& titleComp = m_registry.GetComponent<TextComponent>(m_gameOverTitleText);
        titleComp.Text = "GAME OVER";
        titleComp.Tint = Color{240, 50, 50, 255};
    }

    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_replayBtn))
    {
        m_registry.GetComponent<TextComponent>(m_replayBtn).Text = "REPLAY [SPACE]";
    }

    if (m_gameOverScoreText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverScoreText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverScoreText).Text =
            "FINAL SCORE: " + std::to_string(finalScore) + "  |  +" + std::to_string(earnedCredits) +
            " CR (BANK: " + std::to_string(currentBank) + " CR)  |  STAGE: " + std::to_string(finalLevel);
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        auto& statusComp = m_registry.GetComponent<TextComponent>(m_gameOverStatusText);
        statusComp.Text = "PLAYER: " + defaultName + " (CLICK PSEUDO TO EDIT)";
        statusComp.Tint = Color{120, 230, 255, 255};
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = defaultName;
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = false;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    if (m_gameOverLevelSelectBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_gameOverLevelSelectBtn))
    {
        m_registry.GetComponent<RectTransform>(m_gameOverLevelSelectBtn).IsActive = true;
    }
    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_replayBtn))
    {
        m_registry.GetComponent<RectTransform>(m_replayBtn).Position = {-260.0f, 310.0f};
    }
    if (m_gameOverMainMenuBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_gameOverMainMenuBtn))
    {
        m_registry.GetComponent<RectTransform>(m_gameOverMainMenuBtn).Position = {260.0f, 310.0f};
    }

    if (!defaultName.empty())
    {
        SubmitHighScore(defaultName, finalScore, finalLevel);
    }
    else
    {
        RefreshLeaderboardUI();
    }
}

void GameUIController::SetupVictory(uint32_t finalScore)
{
    std::string defaultName = ProfileManager::Get().GetUsername();
    m_enteredPlayerName = defaultName;

    uint32_t earnedCredits = ProfileManager::ConvertScoreToCredits(finalScore);
    int currentBank = PowerUpManager::Get().GetCoins();

    if (m_gameOverTitleText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverTitleText))
    {
        auto& titleComp = m_registry.GetComponent<TextComponent>(m_gameOverTitleText);
        titleComp.Text = "VICTORY !";
        titleComp.Tint = Color{255, 215, 0, 255};
    }

    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_replayBtn))
    {
        m_registry.GetComponent<TextComponent>(m_replayBtn).Text = "CONTINUE [SPACE]";
    }

    if (m_gameOverScoreText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverScoreText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverScoreText).Text =
            "VICTORY SCORE: " + std::to_string(finalScore) + "  |  +" + std::to_string(earnedCredits) +
            " CR (BANK: " + std::to_string(currentBank) + " CR)  |  ALL " + std::to_string(m_scene.GetLevelCount()) + " STAGES!";
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        auto& statusComp = m_registry.GetComponent<TextComponent>(m_gameOverStatusText);
        statusComp.Text = "CHAMPION: " + defaultName + " (CLICK PSEUDO TO EDIT)";
        statusComp.Tint = Color{255, 215, 0, 255};
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = defaultName;
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = false;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    // Hide Level Select button when completing the final level
    if (m_gameOverLevelSelectBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_gameOverLevelSelectBtn))
    {
        m_registry.GetComponent<RectTransform>(m_gameOverLevelSelectBtn).IsActive = false;
    }
    if (m_replayBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_replayBtn))
    {
        m_registry.GetComponent<RectTransform>(m_replayBtn).Position = {-160.0f, 310.0f};
    }
    if (m_gameOverMainMenuBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_gameOverMainMenuBtn))
    {
        m_registry.GetComponent<RectTransform>(m_gameOverMainMenuBtn).Position = {160.0f, 310.0f};
    }

    if (m_victoryStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_victoryStatsText))
    {
        m_registry.GetComponent<TextComponent>(m_victoryStatsText).Text =
            "FINAL SCORE: " + std::to_string(finalScore) + "  |  +" + std::to_string(earnedCredits) + " CR";
    }

    if (!defaultName.empty())
    {
        SubmitHighScore(defaultName, finalScore, m_scene.GetLevelCount());
    }
    else
    {
        RefreshLeaderboardUI();
    }
}

bool GameUIController::IsTypingName() const
{
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        return m_registry.GetComponent<TextInputComponent>(m_nameInputEntity).IsFocused;
    }
    return false;
}

void GameUIController::PromptEnterName()
{
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        m_registry.GetComponent<TextInputComponent>(m_nameInputEntity).IsFocused = true;
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        auto& statusComp = m_registry.GetComponent<TextComponent>(m_gameOverStatusText);
        statusComp.Text = "PLEASE ENTER YOUR PSEUDO BEFORE PROCEEDING!";
        statusComp.Tint = Color{255, 75, 75, 255};
    }

    if (m_bounceSfxId != 0)
    {
        m_context.Audio.PlaySfx(m_bounceSfxId, 90.0f);
    }
}

bool GameUIController::TryConfirmNameBeforeProceeding()
{
    if (!m_enteredPlayerName.empty())
    {
        return true;
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        std::string trimmed = input.Text;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        if (trimmed.find_last_not_of(" \t\n\r") != std::string::npos)
            trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (!trimmed.empty())
        {
            SubmitHighScore(trimmed, m_scene.GetScoreManager().GetScore(), m_scene.GetCurrentLevel());
            return true;
        }
    }

    PromptEnterName();
    return false;
}

void GameUIController::RefreshLeaderboardUI()
{
    const auto& entries = m_scene.GetLeaderboard().GetEntries();
    for (size_t i = 0; i < m_leaderboardRowTexts.size(); ++i)
    {
        Entity rowEntity = m_leaderboardRowTexts[i];
        if (!m_registry.IsAlive(rowEntity) || !m_registry.HasComponent<TextComponent>(rowEntity))
            continue;

        auto& textComp = m_registry.GetComponent<TextComponent>(rowEntity);
        if (i < entries.size() && entries[i].Name != "---")
        {
            std::string rankStr = "#" + std::to_string(i + 1);
            std::string nameStr = entries[i].Name;
            while (nameStr.length() < 12) nameStr += " ";
            textComp.Text = rankStr + "   " + nameStr + "   " + std::to_string(entries[i].Score) + " PTS";
        }
        else
        {
            textComp.Text = "#" + std::to_string(i + 1) + "   ---            0 PTS";
        }
    }
}

void GameUIController::SubmitHighScore(const std::string& name, uint32_t score, int levelNumber)
{
    std::string safeName = name.empty() ? "PLAYER" : name;
    ProfileManager::Get().SetUsername(safeName);
    int rank = m_scene.GetLeaderboard().AddOrUpdateEntry(m_enteredPlayerName, safeName, score, levelNumber);
    m_enteredPlayerName = safeName;

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.IsFocused = false;
        input.Text = safeName;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVED";
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        std::string rankMsg = (rank > 0 && rank <= 5) ? ("RANK #" + std::to_string(rank) + " - CONGRATULATIONS!") : "PSEUDO SAVED!";
        auto& statusComp = m_registry.GetComponent<TextComponent>(m_gameOverStatusText);
        statusComp.Text = rankMsg;
        statusComp.Tint = Color{50, 255, 120, 255};
    }

    if (m_scoreRecordedSfxId != 0)
    {
        m_context.Audio.PlaySfx(m_scoreRecordedSfxId, 100.0f);
    }

    RefreshLeaderboardUI();
}

void GameUIController::SetPowerUpTesterActive(bool active)
{
    m_powerUpTesterActive = active;
    if (m_powerUpTesterCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_powerUpTesterCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_powerUpTesterCanvas).IsEnabled = (active && m_currentState == GameUIState::Playing);
    }
}

void GameUIController::TogglePowerUpTester()
{
    SetPowerUpTesterActive(!m_powerUpTesterActive);
}

void GameUIController::UpdatePowerUpTesterStatus(const std::string& statusText)
{
    if (m_powerUpStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_powerUpStatusText))
    {
        m_registry.GetComponent<TextComponent>(m_powerUpStatusText).Text = statusText;
    }
}

void GameUIController::NavigatePowerUpTester(int delta)
{
    if (m_powerUpTesterButtons.empty()) return;
    int count = static_cast<int>(m_powerUpTesterButtons.size());
    m_powerUpTesterIndex = (m_powerUpTesterIndex + delta + count) % count;
    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 60.0f, 1.2f);
}

void GameUIController::ActivatePowerUpTesterSelected()
{
    if (m_powerUpTesterButtons.empty()) return;
    if (m_powerUpTesterIndex < 0 || m_powerUpTesterIndex >= static_cast<int>(m_powerUpTesterButtons.size())) return;
    Entity btn = m_powerUpTesterButtons[static_cast<size_t>(m_powerUpTesterIndex)];
    if (m_registry.IsAlive(btn) && m_registry.HasComponent<ButtonComponent>(btn))
    {
        auto& bc = m_registry.GetComponent<ButtonComponent>(btn);
        if (bc.OnClick)
        {
            bc.OnClick();
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 85.0f, 1.0f);
        }
    }
}

void GameUIController::SpawnComboText(const Vector2f& worldPos, int comboMultiplier, const Camera2D& camera)
{
    if (m_textFeedback && m_uiCanvas != NULL_ENTITY)
    {
        m_textFeedback->SpawnComboText(m_uiCanvas, worldPos, comboMultiplier, camera);
    }
}

void GameUIController::SpawnPowerUpText(PowerUpType type, const Vector2f& worldPos, const Camera2D& camera)
{
    if (m_textFeedback && m_uiCanvas != NULL_ENTITY)
    {
        m_textFeedback->SpawnPowerUpText(m_uiCanvas, worldPos, type, camera);
    }
}


// -------------------------------------------------------------------------------------------------
// UI Builders
// -------------------------------------------------------------------------------------------------

void GameUIController::CreateHUD()
{
    m_uiCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_uiCanvas, CanvasComponent{.IsEnabled = true});

    // Score Text
    m_scoreTextEntity = UIFactory::CreateText(m_registry, m_uiCanvas, TextDescriptor{
        .Text = "Score : 0",
        .Position = {330.0f, 130.0f},
        .FontId = m_fontId,
        .FontSize = 48.0f,
        .AnchorPoint = Anchor::TopLeft,
        .TextCenter = false
    });

    // Hearts UI
    m_heartEntities.clear();
    for (int i = 0; i < 3; ++i)
    {
        Entity heart = m_registry.CreateEntity();
        m_registry.AddComponent<RectTransform>(heart, RectTransform{
            .Position = {-380.0f - i * 100.0f, 130.0f},
            .Size = {80.0f, 80.0f},
            .AnchorPoint = Anchor::TopRight,
            .Parent = m_uiCanvas
        });
        m_registry.AddComponent<SpriteComponent>(heart, SpriteComponent{m_heartTexId, Colors::White});
        m_heartEntities.push_back(heart);
    }

    m_explodingHeart = m_registry.CreateEntity();
    m_registry.AddComponent<RectTransform>(m_explodingHeart, RectTransform{
        .Position = {0.0f, 0.0f},
        .Size = {40.0f, 40.0f},
        .AnchorPoint = Anchor::TopRight,
        .Parent = m_uiCanvas
    });
    m_registry.AddComponent<SpriteComponent>(m_explodingHeart, SpriteComponent{m_heartTexId, Colors::Transparent});
    m_registry.AddComponent<TweenComponent>(m_explodingHeart, TweenComponent{});
}

void GameUIController::CreatePauseMenu()
{
    m_pauseCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_pauseCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(m_registry, m_pauseCanvas, TextDescriptor{
        .Text = "PAUSE",
        .Position = {0, -200.0f},
        .FontId = m_fontId,
        .FontSize = 80.0f
    });

    // Continue Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "CONTINUE",
        .OnClick = [this]() {
            m_scene.ResumeGame();
        },
        .Position = {0, -90.0f},
        .FontId = m_fontId
    });

    // Level Select Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "LEVEL SELECT",
        .OnClick = [this]() {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            OpenLevelSelectModal(m_pauseCanvas);
        },
        .Position = {0, -10.0f},
        .FontId = m_fontId
    });

    // Options Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "OPTIONS",
        .OnClick = [this]() {
            auto& pauseCanvas = m_registry.GetComponent<CanvasComponent>(m_pauseCanvas);
            pauseCanvas.IsEnabled = false;
            if (m_uiCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_uiCanvas))
            {
                m_registry.GetComponent<CanvasComponent>(m_uiCanvas).IsEnabled = false;
            }
            auto& settingsCanvas = m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas);
            settingsCanvas.IsEnabled = true;
            OpenSettingsTab(m_audioCanvas);
        },
        .Position = {0, 70.0f},
        .FontId = m_fontId
    });

    // Quit Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "QUIT",
        .OnClick = [this]() {
            m_context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {0, 150.0f},
        .FontId = m_fontId
    });

    // Dark overlay background to dim the active gameplay
    UIFactory::CreatePanel(m_registry, m_pauseCanvas, PanelDescriptor{
        .Position = {0, 0},
        .Size = {m_context.Render.GetLogicalViewSize().X * 2.0f, m_context.Render.GetLogicalViewSize().Y * 2.0f},
        .Tint = Color{0, 0, 0, 160},
        .AnchorPoint = Anchor::Center,
        .ZOrder = -1
    });
}

void GameUIController::UpdateVolumeBars(const std::vector<Entity>& bars, float volume)
{
    for (size_t i = 0; i < bars.size(); ++i)
    {
        if (m_registry.HasComponent<PanelComponent>(bars[i]))
        {
            auto& panel = m_registry.GetComponent<PanelComponent>(bars[i]);
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

void GameUIController::CreateSettingsLayout()
{
    float viewX = m_context.Render.GetLogicalViewSize().X - 300;
    float viewY = m_context.Render.GetLogicalViewSize().Y - 350;

    float leftPanelWidth = viewX * 0.25f;
    float leftPanelX = -viewX * 0.375f + 200.f;

    m_settingsLayoutCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_settingsLayoutCanvas, CanvasComponent{.IsEnabled = false});

    // Title
    UIFactory::CreateText(m_registry, m_settingsLayoutCanvas, TextDescriptor{
        .Text = "SETTINGS",
        .Position = {leftPanelX, -viewY * 0.3f},
        .FontId = m_fontId,
        .FontSize = 50.0f
    });

    // 4 Tab Buttons
    float startY = -viewY * 0.15f;
    float stepY = viewY * 0.08f;

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "AUDIO",
        .OnClick = [this]() { OpenSettingsTab(m_audioCanvas); },
        .Position = {leftPanelX, startY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "RENDER",
        .OnClick = [this]() { OpenSettingsTab(m_renderCanvas); },
        .Position = {leftPanelX, startY + stepY},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "INPUTS",
        .OnClick = [this]() { OpenSettingsTab(m_inputsCanvas); },
        .Position = {leftPanelX, startY + stepY * 2.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "GAMERULES",
        .OnClick = [this]() { OpenSettingsTab(m_gamerulesCanvas); },
        .Position = {leftPanelX, startY + stepY * 3.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });

    m_cheatsTabBtn = UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "CHEATS",
        .OnClick = [this]() { OpenSettingsTab(m_cheatsCanvas); },
        .Position = {leftPanelX, startY + stepY * 4.0f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .FontId = m_fontId
    });
    if (m_registry.HasComponent<RectTransform>(m_cheatsTabBtn))
    {
        m_registry.GetComponent<RectTransform>(m_cheatsTabBtn).IsActive = m_context.Rules.GetRule(Rule::Gameplay::CheatsUnlocked);
    }

    // Back Button
    UIFactory::CreateButton(m_registry, m_settingsLayoutCanvas, ButtonDescriptor{
        .Text = "BACK",
        .OnClick = [this]() {
            m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
            if (m_activeTabCanvas != NULL_ENTITY) {
                m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
                m_activeTabCanvas = NULL_ENTITY;
            }
            m_registry.GetComponent<CanvasComponent>(m_pauseCanvas).IsEnabled = true;
        },
        .Position = {leftPanelX, viewY * 0.4f},
        .Size = {leftPanelWidth * 0.8f, 60.0f},
        .DefaultColor = Colors::Crimson,
        .HoverColor = Colors::LightCoral,
        .PressedColor = Colors::DarkRed,
        .FontId = m_fontId
    });

    // Dark transparent background for settings modal
    UIFactory::CreatePanel(m_registry, m_settingsLayoutCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX + 300.0f, viewY + 350.0f},
        .Tint = Color{0, 0, 0, 160},
        .AnchorPoint = Anchor::Center,
        .ZOrder = -1
    });

    // Left Tabs Panel
    UIFactory::CreatePanel(m_registry, m_settingsLayoutCanvas, PanelDescriptor{
        .Position = {leftPanelX, 0.0f},
        .Size = {leftPanelWidth, viewY - 50.0f},
        .Tint = Colors::Transparent
    });
}

void GameUIController::CreateAudioTab()
{
    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;
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
        .OnMinus = [&]() {
            float vol = std::max(0.0f, m_context.Audio.GetSfxVolume() - 10.0f);
            m_context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            m_context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, m_context.Audio.GetSfxVolume() + 10.0f);
            m_context.Audio.SetSfxVolume(vol);
            PlayerPrefs::SetFloat("SfxVolume", vol);
            PlayerPrefs::Save();
            m_context.Audio.PlaySfx(m_bounceSfxId, 100.0f);
            UpdateVolumeBars(m_sfxVolumeBars, vol);
        },
        .BarsOut = &m_sfxVolumeBars,
        .Position = {rightPanelX * 0.5f, -viewY * 0.1f + 20.f},
        .FontId = m_fontId
    });

    UIFactory::CreateVolumeControl(m_registry, m_audioCanvas, VolumeControlDescriptor{
        .Label = "MUSIC",
        .OnMinus = [&]() {
            float vol = std::max(0.0f, m_context.Audio.GetMusicVolume() - 10.0f);
            m_context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .OnPlus = [&]() {
            float vol = std::min(100.0f, m_context.Audio.GetMusicVolume() + 10.0f);
            m_context.Audio.SetMusicVolume(vol);
            PlayerPrefs::SetFloat("MusicVolume", vol);
            PlayerPrefs::Save();
            UpdateVolumeBars(m_musicVolumeBars, vol);
        },
        .BarsOut = &m_musicVolumeBars,
        .Position = {rightPanelX * 0.5f, viewY * 0.1f - 50.f},
        .FontId = m_fontId
    });

    UpdateVolumeBars(m_sfxVolumeBars, m_context.Audio.GetSfxVolume());
    UpdateVolumeBars(m_musicVolumeBars, m_context.Audio.GetMusicVolume());

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
        .Text = formatDeviceText(m_context.Audio.GetCurrentAudioDevice()),
        .OnClick = [this, formatDeviceText]() {
            auto devices = m_context.Audio.GetAvailableAudioDevices();
            if (devices.empty()) return;

            std::string current = m_context.Audio.GetCurrentAudioDevice();
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
            if (m_context.Audio.SetAudioDevice(newDevice))
            {
                PlayerPrefs::SetString("Audio_Device", newDevice);
                PlayerPrefs::Save();
                if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
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

void GameUIController::CreateRenderTab()
{
    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;
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

    bool isFullscreen = m_context.Render.GetWindow().GetConfig().Mode == WindowMode::Fullscreen;
    std::string fsText = isFullscreen ? "FULL-SCREEN: ON" : "FULL-SCREEN: OFF";

    m_fsBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = fsText,
        .OnClick = [this]() {
            WindowConfig config = m_context.Render.GetWindow().GetConfig();
            if (config.Mode == WindowMode::Fullscreen) {
                config.Mode = WindowMode::Windowed;
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: OFF";
                }
            } else {
                config.Mode = WindowMode::Fullscreen;
                if (m_registry.HasComponent<TextComponent>(m_fsBtn)) {
                    m_registry.GetComponent<TextComponent>(m_fsBtn).Text = "FULL-SCREEN: ON";
                }
            }
            m_context.Render.GetWindow().ApplyConfig(config);
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

    std::string currentRes = std::to_string(m_context.Render.GetWindow().GetConfig().Width) + "x" + std::to_string(m_context.Render.GetWindow().GetConfig().Height);

    UIFactory::CreateDropdown(m_registry, m_renderCanvas, DropdownDescriptor{
        .DefaultText = currentRes,
        .Options = options,
        .OnSelect = [this, uniqueModes](int index, const std::string& /*text*/) {
            WindowConfig config = m_context.Render.GetWindow().GetConfig();
            config.Width = uniqueModes[index].Width;
            config.Height = uniqueModes[index].Height;
            m_context.Render.GetWindow().ApplyConfig(config);
        },
        .Position = {rightPanelX + 250.0f, -viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId,
    });

    bool isShader = m_context.Rules.GetRule(Rule::Graphics::EnableShader);
    std::string shaderText = isShader ? "SHADER: ON" : "SHADER: OFF";
    m_shaderBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = shaderText,
        .OnClick = [this]() {
            bool state = m_context.Rules.GetRule(Rule::Graphics::EnableShader);
            m_context.Rules.SetRule(Rule::Graphics::EnableShader, !state);
            if (m_registry.HasComponent<TextComponent>(m_shaderBtn)) {
                m_registry.GetComponent<TextComponent>(m_shaderBtn).Text = !state ? "SHADER: ON" : "SHADER: OFF";
            }
        },
        .Position = {rightPanelX - 250.0f, viewY * 0.1f},
        .Size = {400.0f, 60.0f},
        .FontId = m_fontId
    });

    bool isParticles = m_context.Rules.GetRule(Rule::Graphics::EnableParticles);
    std::string particlesText = isParticles ? "PARTICLES: ON" : "PARTICLES: OFF";
    m_particlesBtn = UIFactory::CreateButton(m_registry, m_renderCanvas, ButtonDescriptor{
        .Text = particlesText,
        .OnClick = [this]() {
            bool state = m_context.Rules.GetRule(Rule::Graphics::EnableParticles);
            m_context.Rules.SetRule(Rule::Graphics::EnableParticles, !state);
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

void GameUIController::CreateInputsTab()
{
    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;
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

void GameUIController::UpdateInputButtons()
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

void GameUIController::CreateGamerulesTab()
{
    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;
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

void GameUIController::CreateCheatsTab()
{
    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;
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
        .OnSubmit = [this](const std::string& code) {
            m_context.Events.Publish(CheatSubmitEvent(code));
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

void GameUIController::OpenSettingsTab(Entity targetCanvas)
{
    if (m_activeTabCanvas == targetCanvas) {
        return;
    }

    if (m_activeTabCanvas != NULL_ENTITY) {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    m_activeTabCanvas = targetCanvas;
    m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = true;

    if (targetCanvas == m_audioCanvas)
    {
        UpdateVolumeBars(m_sfxVolumeBars, m_context.Audio.GetSfxVolume());
        UpdateVolumeBars(m_musicVolumeBars, m_context.Audio.GetMusicVolume());
    }

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

void GameUIController::CreatePowerUpTesterUI()
{
    m_powerUpTesterCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_powerUpTesterCanvas, CanvasComponent{.IsEnabled = m_powerUpTesterActive});

    // Dark semi-transparent panel on right side of the screen
    UIFactory::CreatePanel(m_registry, m_powerUpTesterCanvas, PanelDescriptor{
        .Position = {-155.0f, 0.0f},
        .Size = {290.0f, 960.0f},
        .Tint = Color{15, 20, 30, 225},
        .AnchorPoint = Anchor::MiddleRight
    });

    // Title
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "POWER-UP TESTER",
        .Position = {-155.0f, -440.0f},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .Tint = Colors::Yellow,
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Helper hint text
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "[TAB]/[R3] Hide | [RS/LB-RB] Nav | [Y] Apply",
        .Position = {-155.0f, -408.0f},
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .Tint = Color{180, 210, 255, 240},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Active power-up indicators
    m_powerUpStatusText = UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "",
        .Position = {-155.0f, -380.0f},
        .FontId = m_fontId,
        .FontSize = 16.0f,
        .Tint = Color{255, 230, 100, 255},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    m_powerUpTesterButtons.clear();
    m_powerUpTesterIndex = 0;

    struct PowerUpBtnDef {
        const char* text;
        PowerUpType type;
        Color normalColor;
        Color hoverColor;
    };

    PowerUpBtnDef buttons[] = {
        {"1. Multi-Ball", PowerUpType::MultiBall, Color{0, 120, 160, 255}, Color{0, 180, 220, 255}},
        {"2. Expand Paddle", PowerUpType::ExpandPaddle, Color{30, 130, 50, 255}, Color{50, 180, 80, 255}},
        {"3. Shrink Paddle", PowerUpType::ShrinkPaddle, Color{150, 40, 40, 255}, Color{200, 60, 60, 255}},
        {"4. Storm", PowerUpType::Storm, Color{180, 140, 20, 255}, Color{240, 200, 30, 255}},
        {"5. Tempo Ball", PowerUpType::TempoBall, Color{110, 40, 160, 255}, Color{150, 60, 210, 255}},
        {"6. Extra Life", PowerUpType::ExtraLife, Color{160, 50, 110, 255}, Color{210, 70, 150, 255}},
        {"7. Big Ball", PowerUpType::BigBall, Color{180, 80, 15, 255}, Color{230, 110, 25, 255}},
        {"8. Fire Ball", PowerUpType::FireBall, Color{190, 40, 10, 255}, Color{240, 80, 15, 255}},
        {"9. Sticky Paddle", PowerUpType::StickyPaddle, Color{160, 110, 20, 255}, Color{215, 155, 30, 255}},
        {"0. Bottom Barrier", PowerUpType::BottomBarrier, Color{0, 140, 180, 255}, Color{0, 195, 240, 255}},
        {"M. Magnet Paddle", PowerUpType::MagnetPaddle, Color{140, 35, 170, 255}, Color{190, 50, 230, 255}},
        {"H. Ghost Ball", PowerUpType::GhostBall, Color{70, 140, 170, 255}, Color{110, 190, 225, 255}}
    };

    float startY = -350.0f;
    float stepY = 31.0f;

    for (size_t i = 0; i < 12; ++i)
    {
        PowerUpType pType = buttons[i].type;
        Entity b = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
            .Text = buttons[i].text,
            .OnClick = [this, pType]() { m_scene.ApplyPowerUp(pType); },
            .Position = {-155.0f, startY + i * stepY},
            .Size = {260.0f, 27.0f},
            .DefaultColor = buttons[i].normalColor,
            .HoverColor = buttons[i].hoverColor,
            .PressedColor = Color{20, 20, 20, 255},
            .TextColor = Colors::White,
            .FontId = m_fontId,
            .FontSize = 17.0f,
            .AnchorPoint = Anchor::MiddleRight,
            .IgnoreMenuNavigation = true
        });
        m_powerUpTesterButtons.push_back(b);
    }

    float actionY = startY + 12 * stepY + 8.0f;
    float actionStepY = 31.0f;

    // Drop capsule button
    Entity btnDrop = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop Capsule [P]",
        .OnClick = [this]() {
            m_scene.SpawnPowerUpAtPaddle();
        },
        .Position = {-155.0f, actionY},
        .Size = {260.0f, 27.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnDrop);

    // Drop all 12 capsules button
    Entity btnDropAll = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop All 12 [O]",
        .OnClick = [this]() { m_scene.SpawnAllPowerUps(); },
        .Position = {-155.0f, actionY + actionStepY},
        .Size = {260.0f, 27.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnDropAll);

    // Reset paddle / ball button
    Entity btnReset = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Reset Ball [R]",
        .OnClick = [this]() { m_scene.ResetBallAndPaddle(true); },
        .Position = {-155.0f, actionY + actionStepY * 2},
        .Size = {260.0f, 27.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnReset);

    // Respawn bricks button
    Entity btnRespawn = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Refill Bricks [B]",
        .OnClick = [this]() { m_scene.RespawnBricks(); },
        .Position = {-155.0f, actionY + actionStepY * 3},
        .Size = {260.0f, 27.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnRespawn);

    // Level +1 / Cycle button
    Entity btnCycle = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Level +1 / Cycle [U]",
        .OnClick = [this]() {
            int curLvl = PowerUpManager::Get().GetLevel(PowerUpType::MultiBall);
            int nextLvl = (curLvl % 5) + 1;
            PowerUpManager::Get().SetLevel(PowerUpType::MultiBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ExpandPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ShrinkPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::Storm, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::TempoBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ExtraLife, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::BigBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::FireBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::StickyPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::BottomBarrier, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::MagnetPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::GhostBall, nextLvl);
        },
        .Position = {-155.0f, actionY + actionStepY * 4},
        .Size = {260.0f, 29.0f},
        .DefaultColor = Color{110, 70, 25, 255},
        .HoverColor = Color{150, 100, 35, 255},
        .PressedColor = Color{40, 25, 10, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 17.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnCycle);

    // Next Level button
    Entity btnNext = UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Next Level [N]",
        .OnClick = [this]() { m_scene.AdvanceToNextLevel(); },
        .Position = {-155.0f, actionY + actionStepY * 5},
        .Size = {260.0f, 32.0f},
        .DefaultColor = Color{50, 80, 120, 255},
        .HoverColor = Color{70, 110, 160, 255},
        .PressedColor = Color{30, 50, 80, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight,
        .IgnoreMenuNavigation = true
    });
    m_powerUpTesterButtons.push_back(btnNext);
}

void GameUIController::CreateGameOverMenu()
{
    m_gameOverCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_gameOverCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Inputs, Texts ---

    // Replay Button
    m_replayBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "REPLAY [SPACE]",
        .OnClick = [this]() {
            if (!TryConfirmNameBeforeProceeding())
            {
                return;
            }
            m_scene.OnReplayRequested();
        },
        .Position = {-260.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .DefaultColor = Color{65, 65, 90, 255},
        .HoverColor = Color{95, 95, 130, 255},
        .PressedColor = Color{35, 35, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Level Select Button
    m_gameOverLevelSelectBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "LEVEL SELECT",
        .OnClick = [this]() {
            if (!TryConfirmNameBeforeProceeding())
            {
                return;
            }
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            OpenLevelSelectModal(m_gameOverCanvas);
        },
        .Position = {0.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .DefaultColor = Color{35, 65, 90, 255},
        .HoverColor = Color{50, 95, 135, 255},
        .PressedColor = Color{25, 45, 65, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Main Menu Button
    m_gameOverMainMenuBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            if (!TryConfirmNameBeforeProceeding())
            {
                return;
            }
            m_context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {260.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .DefaultColor = Color{95, 45, 45, 255},
        .HoverColor = Color{135, 65, 65, 255},
        .PressedColor = Color{60, 25, 25, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Submit button
    m_submitNameBtn = UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "SAVE",
        .OnClick = [this]() {
            if (m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
            {
                auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
                std::string trimmed = input.Text;
                trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
                if (trimmed.find_last_not_of(" \t\n\r") != std::string::npos)
                    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

                if (trimmed.empty())
                {
                    PromptEnterName();
                    return;
                }
                SubmitHighScore(trimmed, m_scene.GetScoreManager().GetScore(), m_scene.GetCurrentLevel());
            }
        },
        .Position = {170.0f, 195.0f},
        .Size = {140.0f, 54.0f},
        .DefaultColor = Color{40, 130, 75, 255},
        .HoverColor = Color{60, 175, 105, 255},
        .PressedColor = Color{25, 90, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Name text input field
    m_nameInputEntity = UIFactory::CreateTextInput(m_registry, m_gameOverCanvas, TextInputDescriptor{
        .Placeholder = "YOUR NAME",
        .OnSubmit = [this](const std::string& name) {
            std::string trimmed = name;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            if (trimmed.find_last_not_of(" \t\n\r") != std::string::npos)
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

            if (trimmed.empty())
            {
                PromptEnterName();
                return;
            }
            SubmitHighScore(trimmed, m_scene.GetScoreManager().GetScore(), m_scene.GetCurrentLevel());
        },
        .Position = {-100.0f, 195.0f},
        .Size = {360.0f, 54.0f},
        .DefaultColor = Color{35, 30, 45, 255},
        .FocusedColor = Color{75, 65, 95, 255},
        .TextColor = Colors::Yellow,
        .FontId = m_fontId,
        .FontSize = 30.0f,
        .AnchorPoint = Anchor::Center,
        .MaxLength = 10
    });

    // Status label
    m_gameOverStatusText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "ENTER YOUR NAME:",
        .Position = {0.0f, 135.0f},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    // 5 Rows of Leaderboard
    m_leaderboardRowTexts.clear();
    float rowStartY = -110.0f;
    float rowSpacing = 42.0f;
    for (size_t i = 0; i < 5; ++i)
    {
        Color rankColor = (i == 0) ? Color{255, 215, 0, 255}
                        : ((i == 1) ? Color{220, 225, 240, 255}
                        : ((i == 2) ? Color{215, 140, 70, 255} : Colors::White));

        Entity rowEntity = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
            .Text = "#" + std::to_string(i + 1) + "   ---            0 PTS",
            .Position = {0.0f, rowStartY + i * rowSpacing},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = rankColor,
            .AnchorPoint = Anchor::Center
        });
        m_leaderboardRowTexts.push_back(rowEntity);
    }

    // Leaderboard Header
    UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "- HALL OF FAME  (TOP 5 ARCADE LEGENDS) -",
        .Position = {0.0f, -155.0f},
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .Tint = Color{255, 215, 0, 255},
        .AnchorPoint = Anchor::Center
    });

    // Final score text
    m_gameOverScoreText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "FINAL SCORE: 0  |  STAGE REACHED: 1",
        .Position = {0.0f, -230.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Color{255, 230, 120, 255},
        .AnchorPoint = Anchor::Center
    });

    // Title: GAME OVER
    m_gameOverTitleText = UIFactory::CreateText(m_registry, m_gameOverCanvas, TextDescriptor{
        .Text = "GAME OVER",
        .Position = {0.0f, -295.0f},
        .FontId = m_fontId,
        .FontSize = 72.0f,
        .Tint = Color{240, 50, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner plates ---
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, -40.0f},
        .Size = {1020.0f, 290.0f},
        .Tint = Color{25, 20, 38, 230},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, -345.0f},
        .Size = {1060.0f, 4.0f},
        .Tint = Color{240, 50, 50, 220},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay ---
    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1120.0f, 820.0f},
        .Tint = Color{16, 12, 24, 245},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1128.0f, 828.0f},
        .Tint = Color{220, 50, 50, 180},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_gameOverCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 210},
        .AnchorPoint = Anchor::Center
    });
}

void GameUIController::CreateLevelClearMenu()
{
    m_levelClearCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_levelClearCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Texts ---

    // Continue button
    UIFactory::CreateButton(m_registry, m_levelClearCanvas, ButtonDescriptor{
        .Text = "CONTINUE [SPACE]",
        .OnClick = [this]() {
            m_scene.SetTransitionTimer(0.0f);
        },
        .Position = {0.0f, 195.0f},
        .Size = {340.0f, 64.0f},
        .DefaultColor = Color{45, 125, 70, 255},
        .HoverColor = Color{65, 170, 95, 255},
        .PressedColor = Color{30, 85, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f,
        .AnchorPoint = Anchor::Center
    });

    // Countdown / next stage status
    m_levelClearTimerText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "NEXT STAGE IN 10s (OR PRESS SPACE)",
        .Position = {0.0f, 85.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    // Stats
    m_levelClearStatsText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "Accumulated Score: 0  |  Lives: 3",
        .Position = {0.0f, -30.0f},
        .FontId = m_fontId,
        .FontSize = 36.0f,
        .Tint = Colors::White,
        .AnchorPoint = Anchor::Center
    });

    // Subtitle & congratulations
    UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "- EXCELLENT WORK -",
        .Position = {0.0f, -135.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Color{255, 220, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // Title: STAGE CLEARED!
    m_levelClearTitleText = UIFactory::CreateText(m_registry, m_levelClearCanvas, TextDescriptor{
        .Text = "STAGE CLEARED!",
        .Position = {0.0f, -200.0f},
        .FontId = m_fontId,
        .FontSize = 72.0f,
        .Tint = Color{70, 240, 110, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner Stats panel & accent line ---
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, -30.0f},
        .Size = {1000.0f, 130.0f},
        .Tint = Color{20, 48, 35, 230},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, -260.0f},
        .Size = {1060.0f, 4.0f},
        .Tint = Color{50, 220, 90, 220},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay ---
    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1120.0f, 640.0f},
        .Tint = Color{12, 28, 20, 245},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1128.0f, 648.0f},
        .Tint = Color{50, 205, 80, 180},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_levelClearCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 200},
        .AnchorPoint = Anchor::Center
    });
}

void GameUIController::CreateVictoryMenu()
{
    m_victoryCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_victoryCanvas, CanvasComponent{.IsEnabled = false});

    float viewX = m_context.Render.GetLogicalViewSize().X;
    float viewY = m_context.Render.GetLogicalViewSize().Y;

    // --- FOREGROUND: Buttons, Texts ---
    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "PLAY AGAIN [SPACE]",
        .OnClick = [this]() {
            m_scene.OnReplayRequested();
        },
        .Position = {-160.0f, 175.0f},
        .Size = {240.0f, 56.0f},
        .DefaultColor = Color{50, 110, 160, 255},
        .HoverColor = Color{75, 145, 205, 255},
        .PressedColor = Color{35, 75, 110, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            m_context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {160.0f, 175.0f},
        .Size = {240.0f, 56.0f},
        .DefaultColor = Color{80, 80, 80, 255},
        .HoverColor = Color{115, 115, 115, 255},
        .PressedColor = Color{45, 45, 45, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    m_victoryStatsText = UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "FINAL SCORE: 0",
        .Position = {0.0f, -30.0f},
        .FontId = m_fontId,
        .FontSize = 42.0f,
        .Tint = Colors::White,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "ALL STAGES COMPLETED - GAME CLEARED !",
        .Position = {0.0f, -140.0f},
        .FontId = m_fontId,
        .FontSize = 34.0f,
        .Tint = Color{120, 230, 255, 255},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateText(m_registry, m_victoryCanvas, TextDescriptor{
        .Text = "VICTORY !",
        .Position = {0.0f, -210.0f},
        .FontId = m_fontId,
        .FontSize = 80.0f,
        .Tint = Color{255, 225, 50, 255},
        .AnchorPoint = Anchor::Center
    });

    // --- MIDGROUND: Inner Stats panel & accent line ---
    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, -30.0f},
        .Size = {1000.0f, 130.0f},
        .Tint = Color{35, 30, 55, 230},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, -280.0f},
        .Size = {1080.0f, 4.0f},
        .Tint = Color{255, 215, 0, 220},
        .AnchorPoint = Anchor::Center
    });

    // --- BACKGROUND: Main card & Backdrop overlay ---
    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1140.0f, 680.0f},
        .Tint = Color{22, 18, 34, 245},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {1148.0f, 688.0f},
        .Tint = Color{255, 215, 0, 180},
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreatePanel(m_registry, m_victoryCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewX * 2.0f, viewY * 2.0f},
        .Tint = Color{0, 0, 0, 210},
        .AnchorPoint = Anchor::Center
    });
}

bool GameUIController::IsModalOrSubMenuOpen() const
{
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas) &&
        m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled)
    {
        return true;
    }
    if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas) &&
        m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled)
    {
        return true;
    }
    return false;
}

void GameUIController::CloseSettingsLayout()
{
    if (m_settingsLayoutCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_settingsLayoutCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas).IsEnabled = false;
    }
    if (m_activeTabCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_activeTabCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
        m_activeTabCanvas = NULL_ENTITY;
    }
    if (m_pauseCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_pauseCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_pauseCanvas).IsEnabled = true;
    }
    if (m_uiCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_uiCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_uiCanvas).IsEnabled = (m_currentState == GameUIState::Playing || m_currentState == GameUIState::Countdown || m_currentState == GameUIState::Paused);
    }
}

constexpr int kInGameCardsPerPage = 5;
constexpr float kInGameCardW = 250.0f;
constexpr float kInGameCardH = 430.0f;
constexpr float kInGameCardGap = 35.0f;
constexpr float kInGameCardCenterY = 0.0f;

void GameUIController::CreateLevelSelectModal()
{
    m_levelSelectCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_levelSelectCanvas, CanvasComponent{.IsEnabled = false});
}

void GameUIController::OpenLevelSelectModal(Entity sourceCanvas)
{
    m_levelSelectSourceCanvas = sourceCanvas;
    if (sourceCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(sourceCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(sourceCanvas).IsEnabled = false;
    }
    if (m_uiCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_uiCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_uiCanvas).IsEnabled = false;
    }
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled = true;
    }
    m_levelThumbnails = LoadAllLevelThumbnails(m_scene.GetLevelManager());
    m_levelSelectPage = 0;
    m_hoveredLevelCard = -1;
    m_selectedLevelCard = 0;
    RebuildLevelSelectCards();
}

void GameUIController::CloseLevelSelectModal()
{
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled = false;
    }
    ClearLevelSelectCards();
    if (m_levelSelectSourceCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectSourceCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_levelSelectSourceCanvas).IsEnabled = true;
    }
    if (m_uiCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_uiCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(m_uiCanvas).IsEnabled = (m_currentState == GameUIState::Playing || m_currentState == GameUIState::Countdown || m_currentState == GameUIState::Paused);
    }
}

void GameUIController::ClearLevelSelectCards()
{
    for (Entity e : m_levelSelectEntities)
    {
        if (m_registry.IsAlive(e))
        {
            m_registry.DestroyEntity(e);
        }
    }
    m_levelSelectEntities.clear();
}

void GameUIController::RebuildLevelSelectCards()
{
    ClearLevelSelectCards();

    auto& levelManager = m_scene.GetLevelManager();
    const int totalLevels = levelManager.GetLevelCount();
    const int totalPages = std::max(1, (totalLevels + kInGameCardsPerPage - 1) / kInGameCardsPerPage);
    if (m_levelSelectPage >= totalPages) m_levelSelectPage = totalPages - 1;
    if (m_levelSelectPage < 0) m_levelSelectPage = 0;
}

void GameUIController::OnRender(GameContext& context)
{
    if (m_levelSelectCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas) &&
        m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled)
    {
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const float screenCenterX = viewSize.X * 0.5f;
        const float screenCenterY = viewSize.Y * 0.5f;

        // --- 0. SOLID DARK BACKDROP ---
        context.Render.DrawRectangle(viewSize.X * 2.0f, viewSize.Y * 2.0f,
            Transform2D{Vector2f{screenCenterX, screenCenterY}}, Color{10, 14, 24, 245});

        // --- 1. SCREEN HEADER ---
        const float titleY = screenCenterY - 370.0f;
        context.Render.DrawTextCentered("LEVEL SELECT", m_fontId, 56.0f,
            Transform2D{Vector2f{screenCenterX, titleY}},
            Color{255, 220, 80, 255});

        // Accent line with generous gap below title
        context.Render.DrawRectangle(720.0f, 3.0f,
            Transform2D{Vector2f{screenCenterX, titleY + 52.0f}},
            Color{60, 210, 255, 230});

        // --- 2. CARDS ---
        auto& levelManager = m_scene.GetLevelManager();
        const int totalLevels = levelManager.GetLevelCount();
        const int totalPages = std::max(1, (totalLevels + kInGameCardsPerPage - 1) / kInGameCardsPerPage);
        const int startLevelIndex = m_levelSelectPage * kInGameCardsPerPage;
        const int levelsOnThisPage = std::clamp(totalLevels - startLevelIndex, 0, kInGameCardsPerPage);

        constexpr float totalSpan = static_cast<float>(kInGameCardsPerPage) * kInGameCardW + static_cast<float>(kInGameCardsPerPage - 1) * kInGameCardGap;
        constexpr float startX = -(totalSpan - kInGameCardW) * 0.5f;
        constexpr float stepX = kInGameCardW + kInGameCardGap;

        for (int slot = 0; slot < levelsOnThisPage; ++slot)
        {
            const int levelIndex = startLevelIndex + slot;
            const int levelNum = levelIndex + 1;
            const bool isUnlocked = levelManager.IsLevelUnlocked(levelNum);
            const uint32_t highScore = levelManager.GetHighScoreForLevel(levelNum);
            const bool isCleared = (levelNum <= levelManager.GetHighestClearedLevel()) || (highScore > 0);
            const bool isSelected = (levelIndex == m_selectedLevelCard);
            const bool isHovered = (levelIndex == m_hoveredLevelCard);

            const float cX = screenCenterX + startX + static_cast<float>(slot) * stepX;
            const float cY = screenCenterY + kInGameCardCenterY;

            // Card Base Panel (opaque dark background with subtle tint)
            const Color bgTint = isCleared
                ? Color{20, 32, 52, 245}
                : (isUnlocked ? Color{16, 26, 42, 245} : Color{12, 14, 20, 245});
            context.Render.DrawRectangle(kInGameCardW, kInGameCardH, Transform2D{Vector2f{cX, cY}}, bgTint);

            // Card Header Bar
            const Color headerTint = isCleared
                ? Color{32, 54, 86, 255}
                : (isUnlocked ? Color{26, 42, 68, 255} : Color{18, 22, 32, 255});
            context.Render.DrawRectangle(kInGameCardW - 8.0f, 50.0f, Transform2D{Vector2f{cX, cY - 165.0f}}, headerTint);

            // Stage Title (cleanly centered in header bar, no redundant badge)
            const std::string stageNumStr = (levelNum < 10) ? ("STAGE 0" + std::to_string(levelNum)) : ("STAGE " + std::to_string(levelNum));
            const Color stageColor = isUnlocked ? Color{255, 230, 90, 255} : Color{130, 135, 150, 255};
            context.Render.DrawTextCentered(stageNumStr, m_fontId, 32.0f,
                Transform2D{Vector2f{cX, cY - 165.0f}},
                stageColor);

            // Thumbnail Frame Background
            const float thumbBoxX = cX;
            const float thumbBoxY = cY - 50.0f;
            context.Render.DrawRectangle(214.0f, 100.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}},
                isUnlocked ? Color{6, 10, 16, 255} : Color{6, 6, 8, 255});

            // Level Pattern Miniature (Brick layout preview)
            if (static_cast<size_t>(levelIndex) < m_levelThumbnails.size())
            {
                const auto& thumb = m_levelThumbnails[static_cast<size_t>(levelIndex)];
                for (const auto& b : thumb.Bricks)
                {
                    Color brickTint = b.Tint;
                    if (!isUnlocked)
                    {
                        uint8_t gray = static_cast<uint8_t>(b.Tint.r * 0.15f + b.Tint.g * 0.15f + b.Tint.b * 0.15f);
                        brickTint = Color{gray, gray, gray, 120};
                    }
                    context.Render.DrawRectangle(b.W, b.H, Transform2D{Vector2f{thumbBoxX + b.RelX, thumbBoxY + b.RelY}}, brickTint);
                }
            }

            // Thumbnail Frame Outline
            const Color frameColor = isSelected ? Color{255, 215, 0, 220} : (isUnlocked ? Color{70, 140, 210, 180} : Color{50, 55, 70, 140});
            context.Render.DrawRectangleOutline(214.0f, 100.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}}, frameColor, 1.8f);

            // High score label
            context.Render.DrawTextCentered("BEST SCORE", m_fontId, 22.0f,
                Transform2D{Vector2f{cX, cY + 30.0f}},
                isUnlocked ? Color{190, 215, 255, 255} : Color{130, 135, 150, 255});

            // High score value
            const std::string scoreStr = (highScore > 0) ? std::to_string(highScore) : "---";
            const Color scoreColor = (highScore > 0) ? Colors::White : (isUnlocked ? Color{160, 175, 200, 255} : Color{110, 115, 130, 255});
            context.Render.DrawTextCentered(scoreStr, m_fontId, 32.0f,
                Transform2D{Vector2f{cX, cY + 68.0f}},
                scoreColor);

            // Action Button
            const float btnW = 200.0f;
            const float btnH = 48.0f;
            const float btnY = cY + 150.0f;
            if (isUnlocked)
            {
                const Color btnBg = (isSelected || isHovered) ? Color{45, 190, 115, 255} : Color{25, 135, 80, 255};
                context.Render.DrawRectangle(btnW, btnH, Transform2D{Vector2f{cX, btnY}}, btnBg);
                const std::string btnText = "PLAY [" + std::to_string(levelNum) + "]";
                context.Render.DrawTextCentered(btnText, m_fontId, 26.0f,
                    Transform2D{Vector2f{cX, btnY}},
                    Colors::White);
            }
            else
            {
                context.Render.DrawRectangle(btnW, btnH, Transform2D{Vector2f{cX, btnY}}, Color{35, 40, 50, 200});
                context.Render.DrawTextCentered("LOCKED", m_fontId, 22.0f,
                    Transform2D{Vector2f{cX, btnY}},
                    Color{120, 125, 140, 255});
            }

            // Hover & Selection Outline Feedbacks
            if (isSelected)
            {
                const float pulse = (std::sin(m_levelSelectAnimTime * 6.0f) + 1.0f) * 0.5f;
                const uint8_t alpha = static_cast<uint8_t>(200 + pulse * 55.0f);
                context.Render.DrawRectangleOutline(kInGameCardW + 6.0f, kInGameCardH + 6.0f, Transform2D{Vector2f{cX, cY}}, Color{255, 215, 0, alpha}, 3.0f);
            }
            else if (isHovered && isUnlocked)
            {
                context.Render.DrawRectangleOutline(kInGameCardW + 4.0f, kInGameCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{80, 220, 255, 230}, 2.5f);
            }
            else if (isHovered && !isUnlocked)
            {
                context.Render.DrawRectangleOutline(kInGameCardW + 4.0f, kInGameCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{220, 60, 70, 180}, 2.0f);
            }
            else
            {
                const Color borderCol = isUnlocked ? Color{60, 95, 140, 140} : Color{45, 50, 65, 120};
                context.Render.DrawRectangleOutline(kInGameCardW, kInGameCardH, Transform2D{Vector2f{cX, cY}}, borderCol, 1.5f);
            }
        }

        // --- 3. PAGE NAVIGATION ARROWS ---
        if (totalPages > 1)
        {
            constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
            const bool canGoPrev = m_levelSelectPage > 0;
            const bool canGoNext = m_levelSelectPage < (totalPages - 1);

            // Left Arrow
            const Color leftBg = canGoPrev ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120};
            context.Render.DrawRectangle(54.0f, 74.0f, Transform2D{Vector2f{screenCenterX - arrowX, screenCenterY + kInGameCardCenterY}}, leftBg);
            context.Render.DrawTextCentered("<", m_fontId, 46.0f,
                Transform2D{Vector2f{screenCenterX - arrowX, screenCenterY + kInGameCardCenterY}},
                canGoPrev ? Colors::White : Color{75, 80, 95, 160});

            // Right Arrow
            const Color rightBg = canGoNext ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120};
            context.Render.DrawRectangle(54.0f, 74.0f, Transform2D{Vector2f{screenCenterX + arrowX, screenCenterY + kInGameCardCenterY}}, rightBg);
            context.Render.DrawTextCentered(">", m_fontId, 46.0f,
                Transform2D{Vector2f{screenCenterX + arrowX, screenCenterY + kInGameCardCenterY}},
                canGoNext ? Colors::White : Color{75, 80, 95, 160});

            // Page Indicator
            const std::string pageStr = "PAGE " + std::to_string(m_levelSelectPage + 1) + " / " + std::to_string(totalPages);
            context.Render.DrawTextCentered(pageStr, m_fontId, 24.0f,
                Transform2D{Vector2f{screenCenterX, screenCenterY + 255.0f}},
                Color{180, 200, 230, 220});
        }

        // --- 4. BACK BUTTON ---
        const float backY = screenCenterY + 330.0f;
        const float backW = 300.0f;
        const float backH = 52.0f;
        context.Render.DrawRectangle(backW, backH, Transform2D{Vector2f{screenCenterX, backY}}, Color{95, 35, 42, 255});
        context.Render.DrawRectangleOutline(backW, backH, Transform2D{Vector2f{screenCenterX, backY}}, Color{180, 60, 70, 200}, 2.0f);
        context.Render.DrawTextCentered("<- BACK [ESC]", m_fontId, 28.0f,
            Transform2D{Vector2f{screenCenterX, backY}},
            Colors::White);
    }

    // 2. Power-Up Tester Gamepad Highlight Cursor
    if (m_powerUpTesterActive && m_powerUpTesterCanvas != NULL_ENTITY &&
        m_registry.HasComponent<CanvasComponent>(m_powerUpTesterCanvas) &&
        m_registry.GetComponent<CanvasComponent>(m_powerUpTesterCanvas).IsEnabled)
    {
        if (m_powerUpTesterIndex >= 0 && m_powerUpTesterIndex < static_cast<int>(m_powerUpTesterButtons.size()))
        {
            Entity btn = m_powerUpTesterButtons[static_cast<size_t>(m_powerUpTesterIndex)];
            if (m_registry.IsAlive(btn) && m_registry.HasComponent<RectTransform>(btn))
            {
                const auto& rt = m_registry.GetComponent<RectTransform>(btn);
                const Vector2f viewSize = context.Render.GetLogicalViewSize();
                const Vector2f absPos = UISystem::GetAbsolutePosition(m_registry, btn, rt, viewSize);
                const float pulse = (std::sin(m_levelSelectAnimTime * 8.0f) + 1.0f) * 0.5f;
                const uint8_t alpha = static_cast<uint8_t>(200 + pulse * 55.0f);
                context.Render.DrawRectangleOutline(rt.Size.X + 8.0f, rt.Size.Y + 8.0f, Transform2D{absPos}, Color{255, 230, 80, alpha}, 3.0f);
            }
        }
    }
}

void GameUIController::RenderCountdown(GameContext& context)
{
    const bool isCountdown = m_scene.IsCountdownActive();
    const float goTimer = m_scene.GetCountdownGoTimer();

    if (!isCountdown && goTimer <= 0.0f) return;

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    if (isCountdown)
    {
        const float timer = m_scene.GetCountdownTimer();
        const int sec = std::clamp(static_cast<int>(std::ceil(timer)), 1, 4);
        float t = 1.0f - (timer - static_cast<float>(sec - 1));
        t = std::clamp(t, 0.0f, 1.0f);

        // 1. Scale punch with damped oscillation bounce
        const float punch = 1.0f + 0.85f * std::exp(-7.0f * t) * std::cos(11.0f * t);
        const float baseFontSize = (sec == 4) ? 130.0f : 160.0f;
        const float fontSize = std::max(20.0f, baseFontSize * punch);

        // 2. Rotation wobble: alternating slight tilt decaying to zero
        const float wobbleSign = (sec % 2 == 1) ? 1.0f : -1.0f;
        const float wobbleDeg = wobbleSign * 7.5f * std::exp(-6.0f * t) * std::cos(9.0f * t);

        // 3. Alpha fade per second: holds steady then fades smoothly near end of each second
        const float alphaRatio = (t < 0.70f) ? 1.0f : std::clamp((1.0f - t) / 0.30f, 0.0f, 1.0f);
        const auto alpha = static_cast<uint8_t>(255.0f * alphaRatio);

        // Color styling per phase tick
        Color mainColor = Color{0, 240, 255, alpha};
        Color glowColor = Color{0, 110, 240, static_cast<uint8_t>(alpha * 0.5f)};
        if (sec == 3)
        {
            mainColor = Color{0, 230, 255, alpha};
            glowColor = Color{0, 100, 220, static_cast<uint8_t>(alpha * 0.5f)};
        }
        else if (sec == 2)
        {
            mainColor = Color{255, 220, 60, alpha};
            glowColor = Color{255, 130, 20, static_cast<uint8_t>(alpha * 0.5f)};
        }
        else if (sec == 1)
        {
            mainColor = Color{255, 80, 50, alpha};
            glowColor = Color{200, 20, 20, static_cast<uint8_t>(alpha * 0.5f)};
        }

        const std::string displayText = (sec == 4) ? "READY" : std::to_string(sec);

        // --- NUMERAL / READY (Centered directly in viewport) ---
        const Vector2f numPos{screenCenterX, screenCenterY};

        // Drop shadow / glow offset
        context.Render.DrawTextCentered(displayText, m_fontId, fontSize,
            Transform2D{numPos + Vector2f{4.0f, 4.0f}, wobbleDeg, Vector2f{1.0f, 1.0f}},
            glowColor);

        // Main text
        context.Render.DrawTextCentered(displayText, m_fontId, fontSize,
            Transform2D{numPos, wobbleDeg, Vector2f{1.0f, 1.0f}},
            mainColor);
    }
    else if (goTimer > 0.0f)
    {
        const float t_go = std::clamp(1.0f - (goTimer / 0.65f), 0.0f, 1.0f);
        const float punchGo = 1.0f + 1.25f * std::exp(-6.5f * t_go) * std::cos(9.0f * t_go);
        const float goFontSize = std::max(20.0f, 160.0f * punchGo);

        const float wobbleDeg = 4.0f * std::exp(-5.0f * t_go) * std::cos(8.0f * t_go);

        const float alphaRatio = (t_go < 0.40f) ? 1.0f : std::clamp((1.0f - t_go) / 0.60f, 0.0f, 1.0f);
        const auto alphaGo = static_cast<uint8_t>(255.0f * alphaRatio);

        const Color goMain{50, 255, 120, alphaGo};
        const Color goGlow{20, 180, 70, static_cast<uint8_t>(alphaGo * 0.5f)};

        const Vector2f goPos{screenCenterX, screenCenterY};

        // Shadow / Glow
        context.Render.DrawTextCentered("GO!", m_fontId, goFontSize,
            Transform2D{goPos + Vector2f{4.0f, 4.0f}, wobbleDeg, Vector2f{1.0f, 1.0f}},
            goGlow);

        // Main "GO!"
        context.Render.DrawTextCentered("GO!", m_fontId, goFontSize,
            Transform2D{goPos, wobbleDeg, Vector2f{1.0f, 1.0f}},
            goMain);
    }
}
