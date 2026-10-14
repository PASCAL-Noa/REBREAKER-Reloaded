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
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TweenComponent.h"
#include "Math/EasingFunctions.h"
#include "Core/PlayerPrefs.h"
#include "Core/GameRules.h"
#include "AudioMixer.h"
#include "Graphics/Renderer.h"
#include "Events/CheatSubmitEvent.h"
#include "PowerUps/PowerUpConfig.h"
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
    // 1. Escape key navigation for submenus / modals
    if (m_context.Input.IsKeyPress(KeyCode::Escape))
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

        if (m_context.Input.IsKeyPress(KeyCode::Left) && m_levelSelectPage > 0)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            m_levelSelectPage--;
            RebuildLevelSelectCards();
        }
        else if (m_context.Input.IsKeyPress(KeyCode::Right) && m_levelSelectPage < totalPages - 1)
        {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            m_levelSelectPage++;
            RebuildLevelSelectCards();
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
                    m_scene.ResumeGame();
                    return;
                }
            }
        }

        // Hover tracking
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

        for (int slot = 0; slot < levelsOnThisPage; ++slot)
        {
            const float cX = startX + static_cast<float>(slot) * stepX;
            const float cY = kInGameCardCenterY;

            if (centeredMouse.X >= cX - kInGameCardW * 0.5f && centeredMouse.X <= cX + kInGameCardW * 0.5f &&
                centeredMouse.Y >= cY - kInGameCardH * 0.5f && centeredMouse.Y <= cY + kInGameCardH * 0.5f)
            {
                m_hoveredLevelCard = startLevelIndex + slot;
                if (m_context.Input.IsMouseButtonPress(MouseButton::Left))
                {
                    m_selectedLevelCard = m_hoveredLevelCard;
                }
                break;
            }
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
    m_enteredPlayerName = "";

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
            "FINAL SCORE: " + std::to_string(finalScore) + "  |  STAGE REACHED: " + std::to_string(finalLevel);
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = "ENTER YOUR PSEUDO [PRESS ENTER OR SAVE]:";
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = "";
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    RefreshLeaderboardUI();
}

void GameUIController::SetupVictory(uint32_t finalScore)
{
    m_enteredPlayerName = "";

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
            "VICTORY SCORE: " + std::to_string(finalScore) + "  |  ALL 5 STAGES CLEARED!";
    }

    if (m_gameOverStatusText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_gameOverStatusText))
    {
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = "CHAMPION! ENTER YOUR PSEUDO:";
    }

    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_nameInputEntity))
    {
        m_registry.GetComponent<RectTransform>(m_nameInputEntity).IsActive = true;
    }
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        auto& input = m_registry.GetComponent<TextInputComponent>(m_nameInputEntity);
        input.Text = "";
        input.Placeholder = "ENTER PSEUDO";
        input.IsFocused = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<RectTransform>(m_submitNameBtn))
    {
        m_registry.GetComponent<RectTransform>(m_submitNameBtn).IsActive = true;
    }
    if (m_submitNameBtn != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_submitNameBtn))
    {
        m_registry.GetComponent<TextComponent>(m_submitNameBtn).Text = "SAVE";
    }

    if (m_victoryStatsText != NULL_ENTITY && m_registry.HasComponent<TextComponent>(m_victoryStatsText))
    {
        m_registry.GetComponent<TextComponent>(m_victoryStatsText).Text =
            "FINAL SCORE: " + std::to_string(finalScore);
    }

    RefreshLeaderboardUI();
}

bool GameUIController::IsTypingName() const
{
    if (m_nameInputEntity != NULL_ENTITY && m_registry.HasComponent<TextInputComponent>(m_nameInputEntity))
    {
        return m_registry.GetComponent<TextInputComponent>(m_nameInputEntity).IsFocused;
    }
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
        m_registry.GetComponent<TextComponent>(m_gameOverStatusText).Text = rankMsg;
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

void GameUIController::SpawnComboText(const Vector2f& worldPos, int comboMultiplier, const Camera2D& camera)
{
    if (m_textFeedback && m_uiCanvas != NULL_ENTITY)
    {
        m_textFeedback->SpawnComboText(m_uiCanvas, worldPos, comboMultiplier, camera);
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
            auto& settingsCanvas = m_registry.GetComponent<CanvasComponent>(m_settingsLayoutCanvas);
            settingsCanvas.IsEnabled = true;
        },
        .Position = {0, 70.0f},
        .FontId = m_fontId
    });

    // Quit Button
    UIFactory::CreateButton(m_registry, m_pauseCanvas, ButtonDescriptor{
        .Text = "QUIT",
        .OnClick = [this]() {
            m_context.Audio.StopMusic();
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {0, 150.0f},
        .FontId = m_fontId
    });

    // Dark overlay background to dim the active gameplay
    UIFactory::CreatePanel(m_registry, m_pauseCanvas, PanelDescriptor{
        .Position = {0, 0},
        .Size = {m_context.Render.GetLogicalViewSize().X * 2.0f, m_context.Render.GetLogicalViewSize().Y * 2.0f},
        .Tint = Color{0, 0, 0, 200},
        .AnchorPoint = Anchor::Center
    });
}

void GameUIController::UpdateVolumeBars(const std::vector<Entity>& bars, float volume)
{
    for (size_t i = 0; i < bars.size(); ++i)
    {
        if (m_registry.HasComponent<PanelComponent>(bars[i]))
        {
            auto& panel = m_registry.GetComponent<PanelComponent>(bars[i]);
            if ((i * 10.0f) < volume)
                panel.Tint.a = 255;
            else
                panel.Tint.a = 50;
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
        .Tint = Color{0, 0, 0, 200},
        .AnchorPoint = Anchor::Center
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

    UIFactory::CreateText(m_registry, m_inputsCanvas, TextDescriptor{
        .Text = "INPUTS",
        .Position = {rightPanelX, -viewY * 0.2f},
        .FontId = m_fontId,
        .FontSize = 40.0f
    });

    UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "Left: Q",
        .Position = {rightPanelX, -viewY * 0.1f},
        .FontId = m_fontId
    });

    UIFactory::CreateButton(m_registry, m_inputsCanvas, ButtonDescriptor{
        .Text = "Right: D",
        .Position = {rightPanelX, viewY * 0.1f - 200.f},
        .FontId = m_fontId
    });

    UIFactory::CreatePanel(m_registry, m_inputsCanvas, PanelDescriptor{
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth, viewY},
        .Tint = Colors::Transparent
    });
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

    UIFactory::CreateButton(m_registry, m_gamerulesCanvas, ButtonDescriptor{
        .Text = "SELECT LEVEL",
        .OnClick = [this]() {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            OpenLevelSelectModal(m_settingsLayoutCanvas);
        },
        .Position = {rightPanelX, 0.0f},
        .Size = {rightPanelWidth * 0.6f, 56.0f},
        .DefaultColor = Color{35, 75, 110, 255},
        .HoverColor = Color{50, 110, 160, 255},
        .PressedColor = Color{25, 50, 75, 255},
        .FontId = m_fontId,
        .FontSize = 28.0f
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
        m_registry.GetComponent<CanvasComponent>(targetCanvas).IsEnabled = false;
        m_activeTabCanvas = NULL_ENTITY;
        return;
    }

    if (m_activeTabCanvas != NULL_ENTITY) {
        m_registry.GetComponent<CanvasComponent>(m_activeTabCanvas).IsEnabled = false;
    }

    m_activeTabCanvas = targetCanvas;
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

void GameUIController::CreatePowerUpTesterUI()
{
    m_powerUpTesterCanvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_powerUpTesterCanvas, CanvasComponent{.IsEnabled = m_powerUpTesterActive});

    // Dark semi-transparent panel on right side of the screen
    UIFactory::CreatePanel(m_registry, m_powerUpTesterCanvas, PanelDescriptor{
        .Position = {-155.0f, 0.0f},
        .Size = {290.0f, 790.0f},
        .Tint = Color{15, 20, 30, 220},
        .AnchorPoint = Anchor::MiddleRight
    });

    // Title
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "POWER-UP TESTER",
        .Position = {-155.0f, -330.0f},
        .FontId = m_fontId,
        .FontSize = 32.0f,
        .Tint = Colors::Yellow,
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Helper hint text
    UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "[TAB] to toggle",
        .Position = {-155.0f, -295.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{180, 180, 180, 255},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

    // Active power-up indicators
    m_powerUpStatusText = UIFactory::CreateText(m_registry, m_powerUpTesterCanvas, TextDescriptor{
        .Text = "",
        .Position = {-155.0f, -265.0f},
        .FontId = m_fontId,
        .FontSize = 18.0f,
        .Tint = Color{255, 230, 100, 255},
        .AnchorPoint = Anchor::MiddleRight,
        .TextCenter = true
    });

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
        {"4. Laser Paddle", PowerUpType::LaserPaddle, Color{160, 120, 20, 255}, Color{220, 170, 30, 255}},
        {"5. Tempo Ball", PowerUpType::TempoBall, Color{110, 40, 160, 255}, Color{150, 60, 210, 255}},
        {"6. Extra Life", PowerUpType::ExtraLife, Color{160, 50, 110, 255}, Color{210, 70, 150, 255}},
        {"7. Big Ball", PowerUpType::BigBall, Color{180, 80, 15, 255}, Color{230, 110, 25, 255}},
        {"8. Fire Ball", PowerUpType::FireBall, Color{190, 40, 10, 255}, Color{240, 80, 15, 255}}
    };

    float startY = -240.0f;
    float stepY = 35.0f;

    for (size_t i = 0; i < 8; ++i)
    {
        PowerUpType pType = buttons[i].type;
        UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
            .Text = buttons[i].text,
            .OnClick = [this, pType]() { m_scene.ApplyPowerUp(pType); },
            .Position = {-155.0f, startY + i * stepY},
            .Size = {260.0f, 32.0f},
            .TextOffset = {0.0f, -8.0f},
            .DefaultColor = buttons[i].normalColor,
            .HoverColor = buttons[i].hoverColor,
            .PressedColor = Color{20, 20, 20, 255},
            .TextColor = Colors::White,
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .AnchorPoint = Anchor::MiddleRight
        });
    }

    float actionY = startY + 8 * stepY + 6.0f;
    float actionStepY = 35.0f;

    // Drop capsule button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop Capsule [P]",
        .OnClick = [this]() {
            m_scene.SpawnPowerUpAtPaddle();
        },
        .Position = {-155.0f, actionY},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Drop all 8 capsules button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Drop All 8 [O]",
        .OnClick = [this]() { m_scene.SpawnAllPowerUps(); },
        .Position = {-155.0f, actionY + actionStepY},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{60, 60, 80, 255},
        .HoverColor = Color{90, 90, 120, 255},
        .PressedColor = Color{30, 30, 40, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Reset paddle / ball button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Reset Ball [R]",
        .OnClick = [this]() { m_scene.ResetBallAndPaddle(true); },
        .Position = {-155.0f, actionY + actionStepY * 2},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Respawn bricks button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Refill Bricks [B]",
        .OnClick = [this]() { m_scene.RespawnBricks(); },
        .Position = {-155.0f, actionY + actionStepY * 3},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{70, 70, 70, 255},
        .HoverColor = Color{100, 100, 100, 255},
        .PressedColor = Color{30, 30, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Level +1 / Cycle button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Level +1 / Cycle [U]",
        .OnClick = [this]() {
            int curLvl = PowerUpManager::Get().GetLevel(PowerUpType::MultiBall);
            int nextLvl = (curLvl % 5) + 1;
            PowerUpManager::Get().SetLevel(PowerUpType::MultiBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ExpandPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ShrinkPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::LaserPaddle, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::TempoBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::ExtraLife, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::BigBall, nextLvl);
            PowerUpManager::Get().SetLevel(PowerUpType::FireBall, nextLvl);
        },
        .Position = {-155.0f, actionY + actionStepY * 4},
        .Size = {260.0f, 34.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{110, 70, 25, 255},
        .HoverColor = Color{150, 100, 35, 255},
        .PressedColor = Color{40, 25, 10, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .AnchorPoint = Anchor::MiddleRight
    });

    // Next Level button
    UIFactory::CreateButton(m_registry, m_powerUpTesterCanvas, ButtonDescriptor{
        .Text = "Next Level [N]",
        .OnClick = [this]() { m_scene.AdvanceToNextLevel(); },
        .Position = {-155.0f, actionY + actionStepY * 5},
        .Size = {260.0f, 32.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{50, 80, 120, 255},
        .HoverColor = Color{70, 110, 160, 255},
        .PressedColor = Color{30, 50, 80, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 20.0f,
        .AnchorPoint = Anchor::MiddleRight
    });
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
            m_scene.OnReplayRequested();
        },
        .Position = {-260.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{65, 65, 90, 255},
        .HoverColor = Color{95, 95, 130, 255},
        .PressedColor = Color{35, 35, 50, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Level Select Button
    UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "LEVEL SELECT",
        .OnClick = [this]() {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            OpenLevelSelectModal(m_gameOverCanvas);
        },
        .Position = {0.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{35, 65, 90, 255},
        .HoverColor = Color{50, 95, 135, 255},
        .PressedColor = Color{25, 45, 65, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    // Main Menu Button
    UIFactory::CreateButton(m_registry, m_gameOverCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            m_context.Audio.StopMusic();
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {260.0f, 310.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
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
                SubmitHighScore(input.Text, m_scene.GetScoreManager().GetScore(), m_scene.GetCurrentLevel());
            }
        },
        .Position = {170.0f, 195.0f},
        .Size = {140.0f, 54.0f},
        .TextOffset = {0.0f, -8.0f},
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
            SubmitHighScore(name, m_scene.GetScoreManager().GetScore(), m_scene.GetCurrentLevel());
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
        .TextOffset = {0.0f, -8.0f},
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
        .Position = {-260.0f, 175.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{50, 110, 160, 255},
        .HoverColor = Color{75, 145, 205, 255},
        .PressedColor = Color{35, 75, 110, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "LEVEL SELECT",
        .OnClick = [this]() {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            OpenLevelSelectModal(m_victoryCanvas);
        },
        .Position = {0.0f, 175.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = Color{35, 65, 90, 255},
        .HoverColor = Color{50, 95, 135, 255},
        .PressedColor = Color{25, 45, 65, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 26.0f,
        .AnchorPoint = Anchor::Center
    });

    UIFactory::CreateButton(m_registry, m_victoryCanvas, ButtonDescriptor{
        .Text = "MAIN MENU [ESC]",
        .OnClick = [this]() {
            m_context.Audio.StopMusic();
            m_context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {260.0f, 175.0f},
        .Size = {240.0f, 56.0f},
        .TextOffset = {0.0f, -8.0f},
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

    // Dark semi-transparent background overlay over the active game field
    const Vector2f viewSize = m_context.Render.GetLogicalViewSize();
    UIFactory::CreatePanel(m_registry, m_levelSelectCanvas, PanelDescriptor{
        .Position = {0.0f, 0.0f},
        .Size = {viewSize.X * 2.0f, viewSize.Y * 2.0f},
        .Tint = Color{0, 0, 0, 220},
        .AnchorPoint = Anchor::Center
    });

    // Header Title
    UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
        .Text = "LEVEL SELECT",
        .Position = {0.0f, -360.0f},
        .FontId = m_fontId,
        .FontSize = 64.0f,
        .Tint = Colors::Gold
    });

    // Accent line
    UIFactory::CreatePanel(m_registry, m_levelSelectCanvas, PanelDescriptor{
        .Position = {0.0f, -315.0f},
        .Size = {640.0f, 3.0f},
        .Tint = Color{60, 200, 255, 200}
    });

    // Subtitle
    UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
        .Text = "SELECT AN UNLOCKED STAGE  |  PRESS [1-5] OR CLICK CARD",
        .Position = {0.0f, -280.0f},
        .FontId = m_fontId,
        .FontSize = 24.0f,
        .Tint = Color{190, 215, 240, 230}
    });
}

void GameUIController::OpenLevelSelectModal(Entity sourceCanvas)
{
    m_levelSelectSourceCanvas = sourceCanvas;
    if (sourceCanvas != NULL_ENTITY && m_registry.HasComponent<CanvasComponent>(sourceCanvas))
    {
        m_registry.GetComponent<CanvasComponent>(sourceCanvas).IsEnabled = false;
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

        const Vector2f cardCenter = { startX + static_cast<float>(slot) * stepX, kInGameCardCenterY };

        // Card Base Panel
        const Color bgTint = isCleared
            ? Color{16, 26, 42, 230}
            : (isUnlocked ? Color{14, 22, 34, 230} : Color{12, 14, 20, 210});

        Entity p = UIFactory::CreatePanel(m_registry, m_levelSelectCanvas, PanelDescriptor{
            .Position = cardCenter,
            .Size = {kInGameCardW, kInGameCardH},
            .Tint = bgTint
        });
        m_levelSelectEntities.push_back(p);

        // Stage Title
        const Color stageColor = isUnlocked ? Colors::Gold : Color{110, 115, 130, 255};
        std::string stageNumStr = (levelNum < 10) ? ("STAGE 0" + std::to_string(levelNum)) : ("STAGE " + std::to_string(levelNum));
        Entity t1 = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
            .Text = stageNumStr,
            .Position = {cardCenter.X, cardCenter.Y - 175.0f},
            .FontId = m_fontId,
            .FontSize = 32.0f,
            .Tint = stageColor
        });
        m_levelSelectEntities.push_back(t1);

        // Status Badge
        if (!isUnlocked)
        {
            Entity tLock = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
                .Text = "[ LOCKED ]",
                .Position = {cardCenter.X, cardCenter.Y - 140.0f},
                .FontId = m_fontId,
                .FontSize = 24.0f,
                .Tint = Color{220, 65, 75, 255}
            });
            m_levelSelectEntities.push_back(tLock);
        }
        else if (isCleared)
        {
            Entity tClear = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
                .Text = "[ CLEARED ]",
                .Position = {cardCenter.X, cardCenter.Y - 140.0f},
                .FontId = m_fontId,
                .FontSize = 24.0f,
                .Tint = Color{65, 225, 120, 255}
            });
            m_levelSelectEntities.push_back(tClear);
        }
        else
        {
            Entity tUn = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
                .Text = "[ UNLOCKED ]",
                .Position = {cardCenter.X, cardCenter.Y - 140.0f},
                .FontId = m_fontId,
                .FontSize = 24.0f,
                .Tint = Color{75, 180, 255, 255}
            });
            m_levelSelectEntities.push_back(tUn);
        }

        // High score
        Entity tBest = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
            .Text = "BEST SCORE",
            .Position = {cardCenter.X, cardCenter.Y + 30.0f},
            .FontId = m_fontId,
            .FontSize = 20.0f,
            .Tint = Color{150, 160, 185, 220}
        });
        m_levelSelectEntities.push_back(tBest);

        const std::string scoreStr = (highScore > 0) ? std::to_string(highScore) : "---";
        const Color scoreColor = (highScore > 0)
            ? Colors::White
            : (isUnlocked ? Color{130, 140, 160, 200} : Color{80, 85, 95, 180});

        Entity tScore = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
            .Text = scoreStr,
            .Position = {cardCenter.X, cardCenter.Y + 68.0f},
            .FontId = m_fontId,
            .FontSize = 30.0f,
            .Tint = scoreColor
        });
        m_levelSelectEntities.push_back(tScore);

        // Action Button
        if (isUnlocked)
        {
            Entity btn = UIFactory::CreateButton(m_registry, m_levelSelectCanvas, ButtonDescriptor{
                .Text = "PLAY [" + std::to_string(levelNum) + "]",
                .OnClick = [this, levelIndex]() {
                    if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                    CloseLevelSelectModal();
                    m_scene.LoadLevel(levelIndex, /*preserveStats=*/false);
                    m_scene.ResumeGame();
                },
                .Position = {cardCenter.X, cardCenter.Y + 155.0f},
                .Size = {200.0f, 48.0f},
                .DefaultColor = Color{35, 90, 55, 255},
                .HoverColor = Color{50, 140, 85, 255},
                .PressedColor = Color{20, 60, 35, 255},
                .TextColor = Colors::White,
                .FontId = m_fontId,
                .FontSize = 26.0f
            });
            m_levelSelectEntities.push_back(btn);
        }
        else
        {
            Entity btn = UIFactory::CreateButton(m_registry, m_levelSelectCanvas, ButtonDescriptor{
                .Text = "LOCKED",
                .OnClick = nullptr,
                .Position = {cardCenter.X, cardCenter.Y + 155.0f},
                .Size = {200.0f, 48.0f},
                .DefaultColor = Color{35, 38, 48, 160},
                .HoverColor = Color{35, 38, 48, 160},
                .PressedColor = Color{35, 38, 48, 160},
                .TextColor = Color{90, 95, 110, 255},
                .FontId = m_fontId,
                .FontSize = 24.0f
            });
            m_levelSelectEntities.push_back(btn);
        }
    }

    // Navigation Arrows
    constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
    const bool canGoPrev = m_levelSelectPage > 0;
    const bool canGoNext = m_levelSelectPage < (totalPages - 1);

    Entity leftBtn = UIFactory::CreateButton(m_registry, m_levelSelectCanvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [this, canGoPrev]() {
            if (canGoPrev)
            {
                if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                m_levelSelectPage--;
                RebuildLevelSelectCards();
            }
        },
        .Position = {-arrowX, kInGameCardCenterY},
        .Size = {54.0f, 74.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = canGoPrev ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120},
        .HoverColor   = canGoPrev ? Color{60, 110, 170, 255} : Color{25, 28, 36, 120},
        .PressedColor = canGoPrev ? Color{25, 45, 75, 255} : Color{25, 28, 36, 120},
        .TextColor    = canGoPrev ? Colors::White : Color{75, 80, 95, 160},
        .FontId = m_fontId,
        .FontSize = 46.0f
    });
    m_levelSelectEntities.push_back(leftBtn);

    Entity rightBtn = UIFactory::CreateButton(m_registry, m_levelSelectCanvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [this, canGoNext]() {
            if (canGoNext)
            {
                if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
                m_levelSelectPage++;
                RebuildLevelSelectCards();
            }
        },
        .Position = {arrowX, kInGameCardCenterY},
        .Size = {54.0f, 74.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = canGoNext ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120},
        .HoverColor   = canGoNext ? Color{60, 110, 170, 255} : Color{25, 28, 36, 120},
        .PressedColor = canGoNext ? Color{25, 45, 75, 255} : Color{25, 28, 36, 120},
        .TextColor    = canGoNext ? Colors::White : Color{75, 80, 95, 160},
        .FontId = m_fontId,
        .FontSize = 46.0f
    });
    m_levelSelectEntities.push_back(rightBtn);

    if (totalPages > 1)
    {
        std::string pageStr = "PAGE " + std::to_string(m_levelSelectPage + 1) + " / " + std::to_string(totalPages);
        Entity pageTxt = UIFactory::CreateText(m_registry, m_levelSelectCanvas, TextDescriptor{
            .Text = pageStr,
            .Position = {0.0f, 255.0f},
            .FontId = m_fontId,
            .FontSize = 24.0f,
            .Tint = Color{180, 200, 230, 220}
        });
        m_levelSelectEntities.push_back(pageTxt);
    }

    // Back Button
    Entity backBtn = UIFactory::CreateButton(m_registry, m_levelSelectCanvas, ButtonDescriptor{
        .Text = "<- BACK [ESC]",
        .OnClick = [this]() {
            if (m_bounceSfxId != 0) m_context.Audio.PlaySfx(m_bounceSfxId, 80.0f);
            CloseLevelSelectModal();
        },
        .Position = {0.0f, 320.0f},
        .Size = {300.0f, 52.0f},
        .DefaultColor = Color{95, 35, 42, 255},
        .HoverColor = Color{135, 50, 60, 255},
        .PressedColor = Color{65, 25, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f
    });
    m_levelSelectEntities.push_back(backBtn);
}

void GameUIController::OnRender(GameContext& context)
{
    if (m_levelSelectCanvas == NULL_ENTITY || !m_registry.HasComponent<CanvasComponent>(m_levelSelectCanvas) ||
        !m_registry.GetComponent<CanvasComponent>(m_levelSelectCanvas).IsEnabled)
    {
        return;
    }

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    auto& levelManager = m_scene.GetLevelManager();
    const int totalLevels = levelManager.GetLevelCount();
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
        const bool isSelected = (levelIndex == m_selectedLevelCard);
        const bool isHovered = (levelIndex == m_hoveredLevelCard);

        const float cX = screenCenterX + startX + static_cast<float>(slot) * stepX;
        const float cY = screenCenterY + kInGameCardCenterY;

        // 1. Hover & Selection Outline Feedbacks
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

        // 2. Thumbnail Frame Outline
        const float thumbBoxX = cX;
        const float thumbBoxY = cY - 50.0f;
        const Color frameColor = isSelected ? Color{255, 215, 0, 200} : (isUnlocked ? Color{70, 140, 210, 160} : Color{50, 55, 70, 120});
        context.Render.DrawRectangleOutline(210.0f, 96.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}}, frameColor, 1.5f);

        // 3. Level Pattern Miniature (Brick layout preview)
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

        // 4. Locked overlay over preview if locked
        if (!isUnlocked)
        {
            context.Render.DrawRectangle(206.0f, 92.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}}, Color{0, 0, 0, 160});
            context.Render.DrawText("LOCKED", m_fontId, 22.0f, Transform2D{Vector2f{thumbBoxX - 35.0f, thumbBoxY - 10.0f}}, Color{200, 60, 70, 220});
        }
    }
}
