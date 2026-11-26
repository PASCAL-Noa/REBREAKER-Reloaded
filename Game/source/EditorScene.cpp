#include "Scenes/EditorScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/RobostockScene.h"
#include "Editor/CustomLevelLibrary.h"
#include "Factories/BrickFactory.h"
#include "Resources/ResourceManager.h"
#include "Core/GameContext.h"
#include "Core/GameRules.h"
#include "Core/PlayerPrefs.h"
#include "Core/ProfileManager.h"
#include "Core/SceneManager.h"
#include "ECS/Systems/DynamicEventSystem.h"
#include "ECS/Systems/PaddleSystem.h"
#include "ECS/Systems/BallSystem.h"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/BoxCollider.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/LightSourceComponent.h"
#include "TweenEffects/TweenEffects.h"
#include "AudioMixer.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <sstream>

namespace
{
    constexpr float kEventQueueListStartOffsetY = 382.0f;
    constexpr int   kEventQueueMaxVisible = 5;

    std::string FormatOneDecimal(float val)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", val);
        return std::string(buf);
    }

    std::string FormatTwoDecimals(float val)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", val);
        return std::string(buf);
    }

    // Curated swatches for element tints, lantern colors, and environment lighting
    constexpr Color kAccentSwatches[8] = {
        Color{80, 220, 255, 255},   // Cyan
        Color{70, 235, 130, 255},   // Emerald
        Color{255, 215, 70, 255},   // Gold
        Color{255, 115, 55, 255},   // Coral
        Color{255, 65, 215, 255},   // Magenta
        Color{165, 95, 255, 255},   // Violet
        Color{235, 245, 255, 255},  // Ice White
        Color{255, 60, 75, 255}     // Crimson
    };

    constexpr Color kLanternSwatches[8] = {
        Color{255, 235, 130, 255},  // Warm Amber
        Color{100, 235, 255, 255},  // Cyan Glow
        Color{110, 255, 165, 255},  // Mint Glow
        Color{255, 110, 225, 255},  // Neon Pink
        Color{255, 95, 95, 255},    // Red Alert
        Color{180, 125, 255, 255},  // Plasma Violet
        Color{245, 250, 255, 255},  // Daylight White
        Color{255, 160, 70, 255}    // Sunset Orange
    };

    constexpr Color kBgDarkSwatches[8] = {
        Color{10, 14, 26, 255},     // Deep Space
        Color{18, 10, 32, 255},     // Cyber Violet
        Color{28, 12, 10, 255},     // Solar Ember
        Color{8, 22, 16, 255},      // Toxic Dark
        Color{6, 7, 12, 255},       // Void Black
        Color{10, 24, 38, 255},     // Arctic Navy
        Color{30, 8, 14, 255},      // Crimson Dark
        Color{22, 18, 10, 255}      // Cathedral Bronze
    };

    constexpr Color kBgSecondarySwatches[8] = {
        Color{22, 34, 62, 255},
        Color{42, 18, 72, 255},
        Color{68, 26, 18, 255},
        Color{18, 52, 34, 255},
        Color{22, 16, 40, 255},
        Color{24, 56, 86, 255},
        Color{74, 18, 28, 255},
        Color{58, 44, 22, 255}
    };

    constexpr Color kAmbientSwatches[8] = {
        Color{55, 95, 165, 255},
        Color{135, 65, 205, 255},
        Color{190, 95, 50, 255},
        Color{50, 175, 95, 255},
        Color{80, 85, 130, 255},
        Color{95, 185, 225, 255},
        Color{195, 55, 75, 255},
        Color{215, 175, 85, 255}
    };

    bool ColorsApproxEqual(Color a, Color b)
    {
        return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= 6 &&
               std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= 6 &&
               std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= 6;
    }

    constexpr float kLeftPanelWidth = 286.0f;
    constexpr float kRightPanelWidth = 410.0f;
    constexpr float kToolbarHeight = 74.0f;

    constexpr EditorTileType kPaletteTiles[9] = {
        EditorTileType::Light,
        EditorTileType::Medium,
        EditorTileType::Hard,
        EditorTileType::Special,
        EditorTileType::Event,
        EditorTileType::Metal,
        EditorTileType::Bomb,
        EditorTileType::Turret,
        EditorTileType::Lantern
    };
}

#include <filesystem>

EditorScene::EditorScene()
    : GameScene()
{
    m_audioEnabled = true;
    m_musicEnabled = true;
    m_renderGameUI = false; // Enabled dynamically during playtest
    m_suppressCountdown = true; // Never show or play countdown in Editor or Editor Playtest
}

EditorScene::EditorScene(std::string editLevelId, bool returnToRobostock)
    : GameScene()
    , m_editingLevelId(std::move(editLevelId))
    , m_returnToRobostock(returnToRobostock)
{
    m_audioEnabled = true;
    m_musicEnabled = true;
    m_renderGameUI = false;
    m_suppressCountdown = true;
}

void EditorScene::ShowToast(const std::string& message, Color color, float duration)
{
    m_toastMessage = message;
    m_toastColor = color;
    m_toastTimer = duration;
}

void EditorScene::SyncEditorThemeMusic(GameContext& context)
{
    const auto& thm = m_levelData.GetTheme();
    const auto& track = thm.MusicTrack;
    if (track.empty()) return;

    // Super Mario Maker style: softer, quieter, low-pass filtered music in Edit Mode,
    // seamlessly transitioning to full bandwidth & volume in Playtest Mode.
    context.Audio.SetMusicEditMode(!m_isPlaytesting, 0.32f);

    m_playlist.Clear();
    const bool muted = (!m_musicEnabled || m_environmentMusicMuted);
    if (m_isPlaytesting && !muted)
    {
        if (!thm.MusicPlaylist.empty())
        {
            for (const auto& plTrack : thm.MusicPlaylist)
            {
                if (!plTrack.empty()) m_playlist.AddTrack(context, plTrack);
            }
        }
        else
        {
            m_playlist.AddTrack(context, track);
        }
    }

    // PlayMusic reuses the live sf::Music instance and preserves its playback offset when the path matches.
    // In Edit Mode (or single-track playlists), loop the active track; in multi-track Playtest, PlaylistManager chains tracks.
    const float level = muted ? 0.0f : AudioMixer::kNominalMusicLevel;
    const bool loopStream = (!m_isPlaytesting || thm.MusicPlaylist.size() <= 1);
    context.Audio.PlayMusic(track, level, loopStream);
}

void EditorScene::OnInit(GameContext& context)
{
    // Initialize base GameScene with m_renderGameUI = true so m_uiController (Pause Menu UI) is created,
    // while m_suppressCountdown = true and m_musicEnabled = false prevent any READY audio cue or campaign music.
    m_suppressCountdown = true;
    m_renderGameUI = true;
    m_musicEnabled = false;
    GameScene::OnInit(context);
    m_renderGameUI = false;
    m_musicEnabled = true;

    // 1. Wipe campaign playlist so it cannot play campaign tracks in the editor
    m_playlist.Clear();

    // 2. Clean up any campaign bricks that GameScene::OnInit might have loaded
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    // 3. Hook up Quit callback in Pause menu to cleanly return to editor
    if (m_uiController)
    {
        m_uiController->SetDisableGameOverUI(true);
        m_uiController->SetOnQuitOverride([this]() {
            if (mp_context)
            {
                StopPlaytesting(*mp_context);
            }
        });
    }

    // 4. Enforce 1:1 camera zoom so logical coordinates cover 100% of the screen in edit mode
    auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
    camera.Zoom = 1.0f;
    camera.Position = Vector2f{0.0f, 0.0f};
    camera.Rotation = 0.0f;
    context.Render.SetCamera(camera);

    // Load assets with verified existing sound files
    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_switchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");
    m_shootSfxId = context.Resources.LoadResource("Resources/audio/sfx/powerup_laser.wav");
    m_hitSfxId = context.Resources.LoadResource("Resources/audio/sfx/ball_hit.wav");
    m_bossLaserSfxId = m_shootSfxId;

    // Reload persisted reusable custom themes
    m_themeLibrary.Load();
    m_selectedCustomThemeIdx = -1;

    // Load target Robostock level if opened for editing, otherwise generate default template
    bool loadedExisting = false;
    if (!m_editingLevelId.empty())
    {
        CustomLevelLibrary lib;
        if (lib.LoadLevelData(m_editingLevelId, m_levelData))
        {
            if (const auto* entry = lib.FindLevel(m_editingLevelId))
            {
                m_currentSavePath = entry->FilePath;
            }
            loadedExisting = true;
        }
        else
        {
            m_editingLevelId.clear();
        }
    }

    if (!loadedExisting)
    {
        m_levelData.ProceduralGenerate(36, 0.40f, 1, 1337);
    }

    // Match loaded theme to saved custom theme if applicable
    if (!m_levelData.GetTheme().ThemeId.empty())
    {
        if (auto idx = m_themeLibrary.FindThemeIndex(m_levelData.GetTheme().ThemeId); idx.has_value())
        {
            m_selectedCustomThemeIdx = static_cast<int>(*idx);
        }
    }

    SyncEditorThemeMusic(context);

    m_undoStack.clear();
    m_undoStack.push_back(m_levelData.CreateSnapshot());

    m_isPlaytesting = false;
    ResetBallAndPaddle();
}

void EditorScene::OnDestroy(GameContext& context)
{
    context.Audio.SetMusicEditMode(false, 0.15f);
    GameScene::OnDestroy(context);
}

bool EditorScene::IsGameOver() const
{
    return m_isPlaytesting && GameScene::IsGameOver();
}

bool EditorScene::IsLevelCleared() const
{
    if (!m_isPlaytesting || IsCountdownActive() || IsPaddleExploding())
    {
        return false;
    }
    if (m_brickCount > 0)
    {
        return false;
    }
    return !FindNextPlayablePage(m_playtestPageIndex + 1).has_value();
}

void EditorScene::OnGameOverEnter()
{
    if (m_isPlaytesting)
    {
        ClearActiveUltimates();
        ClearVisualMalus();
        RestoreTemporalAudio(0.05f);
        m_showDefeatModal = true;
        if (m_uiController)
        {
            m_uiController->SetDisableGameOverUI(true);
        }
    }
}

void EditorScene::OnGameOverExit()
{
    if (m_isPlaytesting && mp_context)
    {
        m_playlist.Clear();
        SyncEditorThemeMusic(*mp_context);
    }
}

void EditorScene::FullReset()
{
    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.1f);
    if (m_isPlaytesting && mp_context)
    {
        m_showDefeatModal = false;
        m_playtestElapsed = 0.0f;
        m_nextQueuedEventIdx = 0;

        ResetPaddleDurability(0.0f);
        ApplyEditorThemeToPlaytest();
        if (mp_eventSystem)
        {
            const bool manualOverride = mp_context->Rules.GetRule(Rule::Gameplay::ManualOverride);
            const bool eventsOn = manualOverride
                ? mp_context->Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled)
                : m_levelData.GetGamerules().ArenaEvents;
            mp_eventSystem->SetEnabled(eventsOn);
            mp_eventSystem->SetIntervalRange(18.0f, 30.0f);
            mp_eventSystem->ResetLifecycle();
        }

        m_playlist.Clear();
        m_playtestPageIndex = FindNextPlayablePage(0).value_or(0);
        RebuildPlayfieldFromPage(m_playtestPageIndex, *mp_context, true);
        m_pageBannerTimer = (m_levelData.GetPageCount() > 1) ? 2.2f : 0.0f;
        m_lives = 3;
        mp_context->Events.Publish(LivesChangedEvent(m_lives));
        ResetBallAndPaddle();
        StartCountdown();
        SyncEditorThemeMusic(*mp_context);
    }
    else if (mp_eventSystem)
    {
        mp_eventSystem->ResetLifecycle();
    }
}

void EditorScene::OnVictoryEnter()
{
    if (m_isPlaytesting)
    {
        if (mp_context)
        {
            StopPlaytesting(*mp_context);
        }
        else
        {
            m_isPlaytesting = false;
            m_renderGameUI = false;
        }
        ShowToast("PLAYTEST CLEARED - ALL PAGES COMPLETE", Color{85, 245, 145, 255}, 3.2f);
    }
}

void EditorScene::OnVictoryExit()
{
    if (m_isPlaytesting && mp_context)
    {
        m_playlist.Clear();
        m_playtestPageIndex = FindNextPlayablePage(0).value_or(0);
        RebuildPlayfieldFromPage(m_playtestPageIndex, *mp_context, true);
        m_lives = 3;
        mp_context->Events.Publish(LivesChangedEvent(m_lives));
        ResetPaddleDurability(0.0f);
        ResetBallAndPaddle();
        SyncEditorThemeMusic(*mp_context);
    }
}

void EditorScene::OnReplayRequested()
{
    if (mp_context)
    {
        StartPlaytesting(*mp_context);
    }
}

void EditorScene::LoadLevel(int levelIndex, bool preserveStats)
{
    (void)levelIndex;
    (void)preserveStats;
    if (mp_context)
    {
        RebuildPlayfieldFromPage(m_playtestPageIndex, *mp_context, true);
    }
}

void EditorScene::StartLevelTransition()
{
    OnVictoryEnter();
}

void EditorScene::CompleteLevelTransition()
{
    m_levelTransitionTimer = 0.0f;
    if (mp_context)
    {
        StopPlaytesting(*mp_context);
    }
}

void EditorScene::RestorePostUltimateMusic()
{
    if (mp_context)
    {
        m_playlist.Clear();
        SyncEditorThemeMusic(*mp_context);
    }
}

void EditorScene::OnUpdate(float dt, GameContext& context)
{
    mp_context = &context;
    m_pulseTimer += dt;
    if (m_toastTimer > 0.0f)
    {
        m_toastTimer = std::max(0.0f, m_toastTimer - dt);
    }
    if (m_paintSfxCooldown > 0.0f)
    {
        m_paintSfxCooldown = std::max(0.0f, m_paintSfxCooldown - dt);
    }

    if (m_isPlaytesting)
    {
        if (m_showDefeatModal)
        {
            HandleDefeatModalInput(context);
            return;
        }

        // Keep camera zoom at 0.75 in playtest so game arena and hearts HUD fit naturally
        auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
        camera.Zoom = 0.75f;
        context.Render.SetCamera(camera);

        // Normal game loop + StateMachine (handles Esc Pause cleanly!)
        GameScene::OnUpdate(dt, context);

        // StopPlaytesting may have been triggered inside OnVictoryEnter / Quit callback
        if (!m_isPlaytesting)
        {
            return;
        }

        const int curState = mp_state_machine
            ? mp_state_machine->GetCurrentState()
            : static_cast<int>(SceneState::Playing);

        // STRICT PAUSE FREEZE: freeze turrets, missiles, timers, and stop shortcuts while paused!
        if (curState != static_cast<int>(SceneState::Playing))
        {
            return;
        }

        // [P] shortcut to stop playtesting while actively playing
        if (context.Input.IsKeyPress(KeyCode::P))
        {
            StopPlaytesting(context);
            return;
        }

        if (m_pageBannerTimer > 0.0f)
        {
            m_pageBannerTimer = std::max(0.0f, m_pageBannerTimer - dt);
        }

        // Multi-Page Sequential Progression: advance to next playable page before Victory triggers
        if (m_brickCount <= 0 && !IsCountdownActive() && !IsPaddleExploding())
        {
            auto nextIdx = FindNextPlayablePage(m_playtestPageIndex + 1);
            if (nextIdx.has_value())
            {
                m_playtestPageIndex = *nextIdx;
                RebuildPlayfieldFromPage(m_playtestPageIndex, context, true);
                m_pageBannerTimer = 2.2f;
                if (m_audioEnabled && m_levelClearSfxId != 0)
                {
                    context.Audio.PlaySfx(m_levelClearSfxId, 65.0f);
                }
            }
        }

        // Initial Event & Scheduled Event Queue Execution (triggers Arena Event + Alarm!)
        if (!IsCountdownActive() && !IsPaddleExploding() && !IsBallDying())
        {
            if (!m_initialEventTriggered)
            {
                m_initialEventTriggered = true;
                const ArenaEventType initEv = m_levelData.GetInitialEvent();
                if (initEv != ArenaEventType::None && mp_eventSystem)
                {
                    mp_eventSystem->TriggerEvent(initEv, /*skipWarning=*/true, /*forceOverride=*/false);
                }
            }

            m_playtestElapsed += dt;
            const auto& queue = m_levelData.GetEventQueue();
            if (m_nextQueuedEventIdx < queue.size() &&
                m_playtestElapsed >= queue[m_nextQueuedEventIdx].TriggerDelay &&
                mp_eventSystem &&
                !mp_eventSystem->IsEventActive() &&
                !mp_eventSystem->IsWarningActive())
            {
                mp_eventSystem->TriggerEvent(
                    LevelData::MapEditorEventToArenaEvent(queue[m_nextQueuedEventIdx].Type),
                    /*skipWarning=*/false);
                m_nextQueuedEventIdx++;
            }
        }

        if (!IsPaddleExploding() && !IsBallDying())
        {
            UpdateTurrets(dt, context);
        }

        // Check if player clicked prominent [BACK TO EDITOR] pill in bottom-left
        auto [mx, my] = context.Input.GetMousePosition();
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const float halfW = viewSize.X * 0.5f;
        const float halfH = viewSize.Y * 0.5f;
        const Vector2f worldMouse{mx - halfW, my - halfH};

        const Vector2f stopBtnPos{-halfW + 150.0f, halfH - 56.0f};
        const Vector2f btnSize{236.0f, 54.0f};
        Vector2f drawPos = stopBtnPos;
        if (context.Rules.GetRule(Rule::Graphics::EnableShader))
        {
            const float normX = stopBtnPos.X / halfW;
            const float normY = stopBtnPos.Y / halfH;
            const float offX = normY / 4.2f;
            const float offY = normX / 4.2f;
            drawPos = Vector2f{(normX + normX * (offX * offX)) * halfW,
                               (normY + normY * (offY * offY)) * halfH};
        }

        const bool hitLogical = (std::abs(worldMouse.X - stopBtnPos.X) <= btnSize.X * 0.56f &&
                                 std::abs(worldMouse.Y - stopBtnPos.Y) <= btnSize.Y * 0.62f);
        const bool hitWarped = (std::abs(worldMouse.X - drawPos.X) <= btnSize.X * 0.56f &&
                                std::abs(worldMouse.Y - drawPos.Y) <= btnSize.Y * 0.62f);
        if ((hitLogical || hitWarped) && context.Input.IsMouseButtonPress(MouseButton::Left))
        {
            StopPlaytesting(context);
            return;
        }
    }
    else
    {
        // Enforce 1.0 zoom identity in edit mode
        auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
        camera.Zoom = 1.0f;
        camera.Position = Vector2f{0.0f, 0.0f};
        camera.Rotation = 0.0f;
        context.Render.SetCamera(camera);

        if (m_loadModalOpen)
        {
            HandleLoadLevelModalInput(context);
            return;
        }

        if (m_textModalMode != EditorTextModalMode::None)
        {
            HandleTextModalInput(context);
            return;
        }

        // Shortcuts to toggle left / right panels
        if (context.Input.IsKeyPress(KeyCode::LBracket))
        {
            m_leftPanelOpen = !m_leftPanelOpen;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        }
        if (context.Input.IsKeyPress(KeyCode::RBracket))
        {
            m_rightPanelOpen = !m_rightPanelOpen;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        }

        // Edit mode input handling
        HandleEditorInput(context);
    }
}

void EditorScene::OnRender(GameContext& context)
{
    if (m_isPlaytesting)
    {
        // 1. In Playtest: GameScene renders the arena, ball, paddle, damage feedback, event toasts, and HUD
        m_renderGameUI = true;
        GameScene::OnRender(context);

        // 2. Render Turrets & Projectiles in arena space (camera zoom 0.75)
        const auto& gameCam = m_registry.GetComponent<Camera2D>(m_camera);
        context.Render.SetCamera(gameCam);
        RenderTurrets(context);

        // 3. Render prominent Back to Editor button in bottom-left when actively playing
        const int curState = mp_state_machine
            ? mp_state_machine->GetCurrentState()
            : static_cast<int>(SceneState::Playing);
        if (curState == static_cast<int>(SceneState::Playing))
        {
            RenderPlaytestStopButton(context);
        }

        if (m_showDefeatModal)
        {
            RenderDefeatModal(context);
        }
    }
    else
    {
        // Enforce 1.0 zoom identity in edit mode
        Camera2D editCam{};
        editCam.Position = Vector2f{0.0f, 0.0f};
        editCam.Zoom = 1.0f;
        editCam.Rotation = 0.0f;
        context.Render.SetCamera(editCam);

        auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
        camera.Zoom = 1.0f;
        camera.Position = Vector2f{0.0f, 0.0f};
        camera.Rotation = 0.0f;

        // Draw live background theme covering 100% of the screen
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const auto& thm = m_levelData.GetTheme();
        context.Render.DrawRectangle(viewSize.X * 1.02f, viewSize.Y * 1.02f, Transform2D{Vector2f{0.0f, 0.0f}}, thm.BgColor);

        // Draw Arena Boundaries, Blueprint Grid & Page Bar
        RenderGrid(context);

        // Draw Fullscreen Editor UI Components
        RenderToolbar(context);
        RenderPalette(context);
        RenderTabsAndPanels(context);
        RenderBottomToast(context);

        if (m_loadModalOpen)
        {
            RenderLoadLevelModal(context);
        }
        else if (m_textModalMode != EditorTextModalMode::None)
        {
            RenderTextModal(context);
        }
    }
}

void EditorScene::RenderSceneSpecificHUD(GameContext& context)
{
    if (!m_isPlaytesting) return;

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfH = viewSize.Y * 0.5f;
    const size_t totalPages = m_levelData.GetPageCount();

    if (totalPages > 1)
    {
        std::string badge = "PAGE " + std::to_string(m_playtestPageIndex + 1) + " / " + std::to_string(totalPages);
        Vector2f badgePos{0.0f, -halfH + 40.0f};
        context.Render.DrawRectangle(165.0f, 32.0f, Transform2D{badgePos}, Color{16, 24, 38, 210});
        context.Render.DrawRectangleOutline(165.0f, 32.0f, Transform2D{badgePos}, Color{80, 220, 255, 220}, 1.5f);
        context.Render.DrawTextCentered(badge, m_fontId, 18.0f, Transform2D{badgePos}, Color{200, 245, 255, 255});
    }

    if (m_pageBannerTimer > 0.0f)
    {
        const float alphaF = std::min(1.0f, m_pageBannerTimer / 0.6f);
        const uint8_t a = static_cast<uint8_t>(255.0f * alphaF);
        std::string banner = "WAVE / PAGE " + std::to_string(m_playtestPageIndex + 1) + " / " + std::to_string(totalPages);
        Vector2f bPos{0.0f, -halfH + 100.0f};
        context.Render.DrawTextCentered(banner, m_fontId, 30.0f, Transform2D{bPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, a});
        context.Render.DrawTextCentered(banner, m_fontId, 30.0f, Transform2D{bPos}, Color{80, 240, 255, a});
    }
}

void EditorScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, damage);
}

void EditorScene::OnPaddleShattered(GameContext& /*context*/)
{
    ResetTurretsAndProjectiles();
}

void EditorScene::OnLifeLost(GameContext& /*context*/)
{
    ResetTurretsAndProjectiles();
}

void EditorScene::ApplyEditorThemeToPlaytest()
{
    const auto& thm = m_levelData.GetTheme();
    ApplyCustomLighting(
        LevelData::BuildLevelTheme(thm),
        thm.PaddleConeColor,
        thm.PaddleConeAngle,
        thm.PaddleLightRange,
        thm.PaddleLightIntensity);
}

void EditorScene::StartPlaytesting(GameContext& context)
{
    m_isPlaytesting = true;
    m_showDefeatModal = false;
    m_renderGameUI = true;
    m_suppressCountdown = true;
    m_playtestElapsed = 0.0f;
    m_nextQueuedEventIdx = 0;
    m_initialEventTriggered = false;

    ResetPaddleDurability(0.0f);

    // Synchronize GameRules & DynamicEventSystem
    const bool manualOverride = context.Rules.GetRule(Rule::Gameplay::ManualOverride);
    const auto& rules = m_levelData.GetGamerules();
    if (!manualOverride)
    {
        context.Rules.SetRule(Rule::Gameplay::HazardZonesEnabled, rules.HazardZones);
        context.Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, rules.ArenaEvents);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, rules.InfiniteLives);
    }

    // 1. Abort any lingering ultimate abilities & visual debuffs
    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);
    ApplyEditorThemeToPlaytest();

    if (mp_eventSystem)
    {
        const bool eventsOn = manualOverride
            ? context.Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled)
            : rules.ArenaEvents;
        mp_eventSystem->SetEnabled(eventsOn);
        mp_eventSystem->SetIntervalRange(18.0f, 30.0f);
        mp_eventSystem->ResetLifecycle();
    }

    // 2. Ensure campaign playlist is completely empty before configuring level playlist
    m_playlist.Clear();

    // 3. Build the ECS bricks starting from the first playable page in order
    m_playtestPageIndex = FindNextPlayablePage(0).value_or(0);
    RebuildPlayfieldFromPage(m_playtestPageIndex, context, true);
    m_pageBannerTimer = (m_levelData.GetPageCount() > 1) ? 2.2f : 0.0f;

    m_lives = 3;
    context.Events.Publish(LivesChangedEvent(m_lives));
    ResetBallAndPaddle();

    // 4. Enter playing state immediately (countdown is suppressed in Editor Playtest)
    StartCountdown();

    // 5. Seamlessly un-muffle active editor theme music and queue level playlist tracks
    SyncEditorThemeMusic(context);

    auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
    camera.Zoom = 0.75f;
    context.Render.SetCamera(camera);

    if (m_uiController)
    {
        m_uiController->SetDisableGameOverUI(true);
        m_uiController->SetOnQuitOverride([this]() {
            if (mp_context)
            {
                StopPlaytesting(*mp_context);
            }
        });
        m_uiController->ShowState(GameUIState::Playing);
    }

    if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
}

void EditorScene::StopPlaytesting(GameContext& context)
{
    m_isPlaytesting = false;
    m_showDefeatModal = false;
    m_renderGameUI = false;
    m_pageBannerTimer = 0.0f;

    ResetPaddleDurability(0.0f);

    // 1. Immediately abort and clear any active ultimates
    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);

    // 2. Clear campaign playlist and keep/restore editor theme music without restarting it
    m_playlist.Clear();
    SyncEditorThemeMusic(context);

    // 3. Clear projectiles & simulation bullets
    m_turretBullets.clear();
    m_turretBricks.clear();

    // 4. Destroy simulation bricks to free entities and memory
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    // 5. Reset ball and paddle
    ResetBallAndPaddle();

    // 6. Reset state machine cleanly without triggering GameOverExit or VictoryExit
    m_countdownActive = false;
    m_countdownTimer = 0.0f;
    m_countdownGoTimer = 0.0f;
    if (mp_state_machine)
    {
        mp_state_machine->SetState(static_cast<int>(SceneState::Playing));
    }

    // 7. Restore camera zoom to 1.0 for the fullscreen editor
    auto& camera = m_registry.GetComponent<Camera2D>(m_camera);
    camera.Zoom = 1.0f;
    camera.Position = Vector2f{0.0f, 0.0f};
    camera.Rotation = 0.0f;
    context.Render.SetCamera(camera);

    if (m_uiController)
    {
        m_uiController->SetDisableGameOverUI(true);
        m_uiController->ShowState(GameUIState::Playing);
    }

    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
}

void EditorScene::PushUndo()
{
    if (m_undoStack.size() >= 50)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.push_back(m_levelData.CreateSnapshot());
}

void EditorScene::Undo()
{
    if (m_undoStack.size() > 1)
    {
        m_undoStack.pop_back();
        m_levelData.RestoreSnapshot(m_undoStack.back());
        m_hasSelectedCell = false;
        ShowToast("Action undone", Color{140, 200, 255, 255}, 1.4f);
        if (mp_context && m_switchSfxId != 0)
        {
            mp_context->Audio.PlaySfx(m_switchSfxId, 14.0f, 0.92f);
        }
    }
}

void EditorScene::SetTool(EditorTool tool)
{
    m_tool = tool;
}

void EditorScene::SetSelectedTile(EditorTileType tile)
{
    m_selectedTile = tile;
    m_tool = EditorTool::Paint;
    m_hasSelectedCell = false;
}

void EditorScene::ExitEditor(GameContext& context)
{
    context.Audio.SetMusicEditMode(false, 0.15f);
    if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
    if (m_returnToRobostock)
    {
        context.Scenes.LoadSceneWithTransition<RobostockScene>(
            SceneTransitionConfig{.Type = SceneTransitionType::StaticSnow, .Duration = 0.55f},
            m_editingLevelId);
    }
    else
    {
        MenuScene::SetReturnChannelType(ChannelType::LevelMaker);
        MenuScene::SetReturnWithDezoom(true);
        context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
    }
}

void EditorScene::OpenSaveAsModal()
{
    CustomLevelLibrary lib;
    m_textModalBadge = lib.PeekNextLevelId();
    if (!m_levelData.GetLevelName().empty() && m_editingLevelId.empty())
    {
        m_textModalBuffer = m_levelData.GetLevelName();
    }
    else
    {
        m_textModalBuffer = "LEVEL " + std::to_string(lib.GetLevels().size() + 1);
    }
    m_textModalMode = EditorTextModalMode::SaveLevelAs;
}

void EditorScene::OpenThemeModal(EditorTextModalMode mode)
{
    m_textModalMode = mode;
    if (mode == EditorTextModalMode::NewTheme)
    {
        m_textModalBadge = m_themeLibrary.PeekNextThemeId();
        m_textModalBuffer = "THEME " + std::to_string(m_themeLibrary.GetThemes().size() + 1);
    }
    else if (mode == EditorTextModalMode::RenameTheme)
    {
        const auto& themes = m_themeLibrary.GetThemes();
        if (m_selectedCustomThemeIdx >= 0 && m_selectedCustomThemeIdx < static_cast<int>(themes.size()))
        {
            m_textModalBadge = themes[static_cast<size_t>(m_selectedCustomThemeIdx)].ThemeId;
            m_textModalBuffer = themes[static_cast<size_t>(m_selectedCustomThemeIdx)].Name;
        }
        else
        {
            m_textModalMode = EditorTextModalMode::None;
        }
    }
}

void EditorScene::ConfirmTextModal(GameContext& context)
{
    const EditorTextModalMode mode = m_textModalMode;
    m_textModalMode = EditorTextModalMode::None;

    if (mode == EditorTextModalMode::SaveLevelAs)
    {
        CustomLevelLibrary lib;
        const std::string savedId = lib.SaveNewLevel(m_textModalBuffer, m_levelData);
        if (!savedId.empty())
        {
            m_editingLevelId = savedId;
            if (const auto* entry = lib.FindLevel(savedId))
            {
                m_currentSavePath = entry->FilePath;
            }
            m_levelData.SaveToFile("Resources/levels/custom_level.txt");
            ShowToast("Saved " + savedId + " - " + m_levelData.GetLevelName(), Color{85, 245, 145, 255}, 2.8f);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
        }
        else
        {
            ShowToast("Save failed", Color{255, 85, 85, 255}, 2.6f);
        }
    }
    else if (mode == EditorTextModalMode::NewTheme)
    {
        const std::string newId = m_themeLibrary.CreateTheme(m_textModalBuffer, m_levelData.GetTheme());
        if (!newId.empty())
        {
            if (auto idx = m_themeLibrary.FindThemeIndex(newId); idx.has_value())
            {
                m_selectedCustomThemeIdx = static_cast<int>(*idx);
                m_levelData.SetTheme(m_themeLibrary.GetThemes()[*idx]);
            }
            ShowToast("Theme saved: " + m_levelData.GetTheme().Name, Color{85, 245, 145, 255}, 2.4f);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
        }
    }
    else if (mode == EditorTextModalMode::RenameTheme)
    {
        const auto& themes = m_themeLibrary.GetThemes();
        if (m_selectedCustomThemeIdx >= 0 && m_selectedCustomThemeIdx < static_cast<int>(themes.size()))
        {
            const std::string id = themes[static_cast<size_t>(m_selectedCustomThemeIdx)].ThemeId;
            if (m_themeLibrary.RenameTheme(id, m_textModalBuffer))
            {
                if (const auto* updated = m_themeLibrary.FindTheme(id))
                {
                    m_levelData.GetTheme().Name = updated->Name;
                }
                ShowToast("Theme renamed: " + m_levelData.GetTheme().Name, Color{110, 225, 255, 255}, 2.2f);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            }
        }
    }
}

void EditorScene::HandleTextModalInput(GameContext& context)
{
    const std::string& typed = context.Input.GetEnteredText();
    for (char ch : typed)
    {
        if (ch >= 32 && ch < 127 && ch != '|' && m_textModalBuffer.size() < 20)
        {
            char upper = (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
            m_textModalBuffer.push_back(upper);
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Backspace) && !m_textModalBuffer.empty())
    {
        m_textModalBuffer.pop_back();
    }

    if (context.Input.IsKeyPress(KeyCode::Enter))
    {
        ConfirmTextModal(context);
        return;
    }

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        m_textModalMode = EditorTextModalMode::None;
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        return;
    }

    if (context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        auto [mx, my] = context.Input.GetMousePosition();
        const Vector2f worldMouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

        const Vector2f okPos{-115.0f, 82.0f};
        const Vector2f cancelPos{115.0f, 82.0f};
        const Vector2f btnSz{195.0f, 48.0f};

        if (std::abs(worldMouse.X - okPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - okPos.Y) <= btnSz.Y * 0.5f)
        {
            ConfirmTextModal(context);
            return;
        }
        if (std::abs(worldMouse.X - cancelPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - cancelPos.Y) <= btnSz.Y * 0.5f)
        {
            m_textModalMode = EditorTextModalMode::None;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }
}

void EditorScene::RenderTextModal(GameContext& context)
{
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    context.Render.DrawRectangle(viewSize.X * 1.5f, viewSize.Y * 1.5f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{8, 10, 16, 215});

    const Vector2f modalSize{500.0f, 250.0f};
    context.Render.DrawRectangle(modalSize.X, modalSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{26, 30, 40, 255});
    context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{80, 220, 255, 255}, 2.0f);

    const char* modalTitle = "SAVE LEVEL AS";
    if (m_textModalMode == EditorTextModalMode::NewTheme) modalTitle = "NEW CUSTOM THEME";
    else if (m_textModalMode == EditorTextModalMode::RenameTheme) modalTitle = "RENAME THEME";

    // Header + ID Badge
    context.Render.DrawRectangle(modalSize.X - 4.0f, 46.0f, Transform2D{Vector2f{0.0f, -100.0f}}, Color{36, 42, 56, 255});
    context.Render.DrawText(modalTitle, m_fontId, 22.0f, Transform2D{Vector2f{-225.0f, -112.0f}}, Color{255, 215, 60, 255});

    Vector2f idBadgePos{165.0f, -100.0f};
    context.Render.DrawRectangle(120.0f, 28.0f, Transform2D{idBadgePos}, Color{18, 24, 34, 255});
    context.Render.DrawRectangleOutline(120.0f, 28.0f, Transform2D{idBadgePos}, Color{80, 220, 255, 200}, 1.5f);
    context.Render.DrawTextCentered(m_textModalBadge, m_fontId, 18.0f, Transform2D{idBadgePos}, Color{80, 235, 255, 255});

    // Input Box
    Vector2f boxPos{0.0f, -8.0f};
    context.Render.DrawRectangle(430.0f, 54.0f, Transform2D{boxPos}, Color{14, 17, 24, 255});
    context.Render.DrawRectangleOutline(430.0f, 54.0f, Transform2D{boxPos}, Color{255, 215, 60, 240}, 2.0f);

    const bool blink = (std::fmod(m_pulseTimer * 2.2f, 2.0f) < 1.0f);
    std::string shown = m_textModalBuffer + (blink ? "_" : " ");
    context.Render.DrawTextCentered(shown, m_fontId, 26.0f, Transform2D{boxPos}, Colors::White);

    // Confirm / Cancel Buttons
    const Vector2f okPos{-115.0f, 82.0f};
    const Vector2f cancelPos{115.0f, 82.0f};
    const Vector2f btnSz{195.0f, 48.0f};
    const bool okHover = (std::abs(worldMouse.X - okPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - okPos.Y) <= btnSz.Y * 0.5f);
    const bool cancelHover = (std::abs(worldMouse.X - cancelPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - cancelPos.Y) <= btnSz.Y * 0.5f);

    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{okPos}, okHover ? Color{60, 175, 95, 255} : Color{45, 140, 75, 255});
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{okPos}, okHover ? Colors::White : Color{95, 200, 125, 255}, 1.5f);
    context.Render.DrawTextCentered("CONFIRM", m_fontId, 21.0f, Transform2D{okPos}, Colors::White);

    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{cancelPos}, cancelHover ? Color{75, 50, 60, 255} : Color{48, 52, 64, 255});
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{cancelPos}, cancelHover ? Colors::White : Color{95, 102, 120, 255}, 1.5f);
    context.Render.DrawTextCentered("CANCEL", m_fontId, 21.0f, Transform2D{cancelPos}, Color{225, 230, 240, 255});
}

void EditorScene::OpenLoadLevelModal(GameContext& context)
{
    RefreshLoadModalEntries();
    m_loadModalOpen = true;
    m_loadModalScroll = 0;
    m_loadModalSelected = 0;
    if (!m_editingLevelId.empty())
    {
        for (size_t i = 0; i < m_loadableLevels.size(); ++i)
        {
            if (m_loadableLevels[i].Id == m_editingLevelId)
            {
                m_loadModalSelected = static_cast<int>(i);
                if (m_loadModalSelected >= 6)
                {
                    m_loadModalScroll = m_loadModalSelected - 5;
                }
                break;
            }
        }
    }
    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
}

void EditorScene::RefreshLoadModalEntries()
{
    m_loadableLevels.clear();

    // Only list levels registered in the Strix Vault (CustomLevelLibrary)
    CustomLevelLibrary lib;
    for (const auto& entry : lib.GetLevels())
    {
        if (std::filesystem::exists(entry.FilePath))
        {
            m_loadableLevels.push_back(LoadableLevelItem{
                .Id = entry.Id,
                .Name = entry.Name,
                .FilePath = entry.FilePath,
                .PageCount = std::max(1, entry.PageCount),
                .BrickCount = std::max(0, entry.BrickCount),
                .IsCustomVault = true
            });
        }
    }
}

void EditorScene::ConfirmLoadLevelSelection(GameContext& context)
{
    if (m_loadableLevels.empty() || m_loadModalSelected < 0 ||
        m_loadModalSelected >= static_cast<int>(m_loadableLevels.size()))
    {
        m_loadModalOpen = false;
        return;
    }

    const LoadableLevelItem chosen = m_loadableLevels[static_cast<size_t>(m_loadModalSelected)];
    CustomLevelLibrary lib;
    LevelData loaded;
    const bool ok = chosen.IsCustomVault && lib.LoadLevelData(chosen.Id, loaded);

    if (ok)
    {
        PushUndo();
        m_levelData = std::move(loaded);
        m_editingLevelId = chosen.Id;
        m_currentSavePath = chosen.FilePath;

        m_selectedCustomThemeIdx = -1;
        if (!m_levelData.GetTheme().ThemeId.empty())
        {
            if (auto idx = m_themeLibrary.FindThemeIndex(m_levelData.GetTheme().ThemeId); idx.has_value())
            {
                m_selectedCustomThemeIdx = static_cast<int>(*idx);
            }
        }

        m_hasSelectedCell = false;
        m_loadModalOpen = false;
        SyncEditorThemeMusic(context);
        ShowToast("Loaded " + chosen.Id + " - " + chosen.Name, Color{85, 245, 145, 255}, 2.4f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
    }
    else
    {
        RefreshLoadModalEntries();
        m_loadModalSelected = m_loadableLevels.empty()
            ? 0
            : std::clamp(m_loadModalSelected, 0, static_cast<int>(m_loadableLevels.size()) - 1);
        ShowToast("Level not found in Strix Vault", Color{255, 85, 95, 255}, 2.4f);
    }
}

void EditorScene::HandleLoadLevelModalInput(GameContext& context)
{
    constexpr int kMaxVisibleRows = 6;
    const int total = static_cast<int>(m_loadableLevels.size());
    const int maxScroll = std::max(0, total - kMaxVisibleRows);

    const float wheel = context.Input.GetMouseWheelDelta();
    if (std::abs(wheel) > 0.01f)
    {
        m_loadModalScroll = std::clamp(m_loadModalScroll - (wheel > 0.0f ? 1 : -1), 0, maxScroll);
    }

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        m_loadModalOpen = false;
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
        return;
    }

    if (total > 0)
    {
        if (context.Input.IsKeyPress(KeyCode::Up) || context.Input.IsKeyPress(KeyCode::W))
        {
            m_loadModalSelected = (m_loadModalSelected - 1 + total) % total;
            if (m_loadModalSelected < m_loadModalScroll) m_loadModalScroll = m_loadModalSelected;
            if (m_loadModalSelected >= m_loadModalScroll + kMaxVisibleRows) m_loadModalScroll = m_loadModalSelected - kMaxVisibleRows + 1;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 13.0f);
        }
        else if (context.Input.IsKeyPress(KeyCode::Down) || context.Input.IsKeyPress(KeyCode::S))
        {
            m_loadModalSelected = (m_loadModalSelected + 1) % total;
            if (m_loadModalSelected >= m_loadModalScroll + kMaxVisibleRows) m_loadModalScroll = m_loadModalSelected - kMaxVisibleRows + 1;
            if (m_loadModalSelected < m_loadModalScroll) m_loadModalScroll = m_loadModalSelected;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 13.0f);
        }
        else if (context.Input.IsKeyPress(KeyCode::Enter))
        {
            ConfirmLoadLevelSelection(context);
            return;
        }
    }

    if (context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        auto [mx, my] = context.Input.GetMousePosition();
        const Vector2f worldMouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

        const float listTopY = -126.0f;
        const float rowH = 46.0f;
        const float rowW = 556.0f;
        for (int vis = 0; vis < kMaxVisibleRows; ++vis)
        {
            const int idx = m_loadModalScroll + vis;
            if (idx >= total) break;
            const float ry = listTopY + static_cast<float>(vis) * rowH;
            if (std::abs(worldMouse.X) <= rowW * 0.5f && std::abs(worldMouse.Y - ry) <= (rowH - 4.0f) * 0.5f)
            {
                if (m_loadModalSelected == idx)
                {
                    ConfirmLoadLevelSelection(context);
                    return;
                }
                m_loadModalSelected = idx;
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                return;
            }
        }

        const Vector2f loadBtnPos{-125.0f, 178.0f};
        const Vector2f cancelBtnPos{125.0f, 178.0f};
        const Vector2f btnSz{210.0f, 48.0f};
        if (total > 0 && std::abs(worldMouse.X - loadBtnPos.X) <= btnSz.X * 0.5f &&
            std::abs(worldMouse.Y - loadBtnPos.Y) <= btnSz.Y * 0.5f)
        {
            ConfirmLoadLevelSelection(context);
            return;
        }
        if (std::abs(worldMouse.X - cancelBtnPos.X) <= btnSz.X * 0.5f &&
            std::abs(worldMouse.Y - cancelBtnPos.Y) <= btnSz.Y * 0.5f)
        {
            m_loadModalOpen = false;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }
}

void EditorScene::RenderLoadLevelModal(GameContext& context)
{
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    context.Render.DrawRectangle(viewSize.X * 1.5f, viewSize.Y * 1.5f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{8, 10, 16, 220});

    const Vector2f modalSize{620.0f, 440.0f};
    context.Render.DrawRectangle(modalSize.X, modalSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{24, 28, 38, 255});
    context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{80, 220, 255, 255}, 2.0f);

    // Header
    const float headerY = -modalSize.Y * 0.5f + 25.0f;
    context.Render.DrawRectangle(modalSize.X - 4.0f, 48.0f, Transform2D{Vector2f{0.0f, headerY}}, Color{34, 40, 54, 255});
    DrawIconFolderOpen(context, Vector2f{-275.0f, headerY}, 22.0f, Color{255, 215, 65, 255});
    context.Render.DrawText("LOAD LEVEL FROM STRIX VAULT", m_fontId, 22.0f, Transform2D{Vector2f{-252.0f, headerY - 12.0f}}, Color{255, 215, 65, 255});

    const std::string countTag = std::to_string(m_loadableLevels.size()) + " IN VAULT";
    const Vector2f badgePos{225.0f, headerY};
    context.Render.DrawRectangle(130.0f, 28.0f, Transform2D{badgePos}, Color{18, 24, 34, 255});
    context.Render.DrawRectangleOutline(130.0f, 28.0f, Transform2D{badgePos}, Color{80, 220, 255, 200}, 1.3f);
    context.Render.DrawTextCentered(countTag, m_fontId, 16.0f, Transform2D{badgePos}, Color{130, 235, 255, 255});

    constexpr int kMaxVisibleRows = 6;
    const int total = static_cast<int>(m_loadableLevels.size());
    const float listTopY = -126.0f;
    const float rowH = 46.0f;
    const float rowW = 556.0f;

    if (total == 0)
    {
        context.Render.DrawTextCentered("No levels found in Strix Vault.", m_fontId, 22.0f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{160, 175, 200, 220});
    }
    else
    {
        for (int vis = 0; vis < kMaxVisibleRows; ++vis)
        {
            const int idx = m_loadModalScroll + vis;
            if (idx >= total) break;

            const auto& item = m_loadableLevels[static_cast<size_t>(idx)];
            const float ry = listTopY + static_cast<float>(vis) * rowH;
            const Vector2f rPos{0.0f, ry};
            const bool isSel = (idx == m_loadModalSelected);
            const bool isHov = (std::abs(worldMouse.X) <= rowW * 0.5f && std::abs(worldMouse.Y - ry) <= (rowH - 4.0f) * 0.5f);

            const Color bg = isSel
                ? Color{42, 62, 92, 255}
                : (isHov ? Color{34, 42, 58, 255} : Color{20, 24, 34, 255});
            const Color border = isSel
                ? Color{255, 215, 65, 255}
                : (isHov ? Color{110, 185, 245, 230} : Color{48, 56, 74, 210});
            context.Render.DrawRectangle(rowW, rowH - 5.0f, Transform2D{rPos}, bg);
            context.Render.DrawRectangleOutline(rowW, rowH - 5.0f, Transform2D{rPos}, border, isSel ? 2.0f : 1.2f);

            // ID pill on left
            const Vector2f idPos{-rowW * 0.5f + 58.0f, ry};
            const Color idAccent = Color{80, 225, 255, 255};
            context.Render.DrawRectangle(96.0f, 26.0f, Transform2D{idPos}, Color{14, 18, 26, 245});
            context.Render.DrawRectangleOutline(96.0f, 26.0f, Transform2D{idPos}, idAccent, 1.2f);
            context.Render.DrawTextCentered(item.Id, m_fontId, 15.0f, Transform2D{idPos}, idAccent);

            // Level Name
            context.Render.DrawText(item.Name, m_fontId, 19.0f, Transform2D{Vector2f{-rowW * 0.5f + 120.0f, ry - 10.0f}},
                                    isSel ? Color{255, 235, 120, 255} : Colors::White);

            // Metadata on right (Pages & Bricks)
            const std::string meta = std::to_string(item.PageCount) + (item.PageCount == 1 ? " PAGE  |  " : " PAGES  |  ")
                                   + std::to_string(item.BrickCount) + " BRICKS";
            context.Render.DrawTextCentered(meta, m_fontId, 15.0f, Transform2D{Vector2f{rowW * 0.5f - 95.0f, ry}}, Color{165, 190, 220, 235});
        }
    }

    // Load & Cancel Buttons
    const Vector2f loadBtnPos{-125.0f, 178.0f};
    const Vector2f cancelBtnPos{125.0f, 178.0f};
    const Vector2f btnSz{210.0f, 48.0f};
    const bool loadHov = total > 0 && (std::abs(worldMouse.X - loadBtnPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - loadBtnPos.Y) <= btnSz.Y * 0.5f);
    const bool cancelHov = (std::abs(worldMouse.X - cancelBtnPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - cancelBtnPos.Y) <= btnSz.Y * 0.5f);

    const Color loadBg = (total > 0)
        ? (loadHov ? Color{60, 175, 95, 255} : Color{45, 140, 75, 255})
        : Color{32, 38, 48, 220};
    const Color loadBorder = (total > 0)
        ? (loadHov ? Colors::White : Color{95, 200, 125, 255})
        : Color{68, 76, 92, 180};
    const Color loadTextCol = (total > 0) ? Colors::White : Color{115, 125, 145, 200};
    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{loadBtnPos}, loadBg);
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{loadBtnPos}, loadBorder, 1.5f);
    context.Render.DrawTextCentered("LOAD LEVEL [ENTER]", m_fontId, 19.0f, Transform2D{loadBtnPos}, loadTextCol);

    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{cancelBtnPos}, cancelHov ? Color{75, 50, 60, 255} : Color{48, 52, 64, 255});
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{cancelBtnPos}, cancelHov ? Colors::White : Color{95, 102, 120, 255}, 1.5f);
    context.Render.DrawTextCentered("CANCEL [ESC]", m_fontId, 19.0f, Transform2D{cancelBtnPos}, Color{225, 230, 240, 255});
}

void EditorScene::RenderBottomToast(GameContext& context)
{
    if (m_toastTimer <= 0.0f || m_toastMessage.empty()) return;

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const EditorGridLayout layout = ComputeGridLayout(viewSize);
    const float cx = (layout.CanvasMinX + layout.CanvasMaxX) * 0.5f;
    const float y = layout.CanvasMaxY - layout.PageBarHeight - 24.0f;
    const float alphaF = std::min(1.0f, m_toastTimer / 0.45f);
    const uint8_t bgA = static_cast<uint8_t>(215.0f * alphaF);
    const uint8_t txtA = static_cast<uint8_t>(255.0f * alphaF);

    const float pillW = std::clamp(static_cast<float>(m_toastMessage.size()) * 10.5f + 44.0f, 180.0f, 440.0f);
    const Vector2f pos{cx, y};
    Color border = m_toastColor;
    border.a = txtA;
    context.Render.DrawRectangle(pillW, 30.0f, Transform2D{pos}, Color{16, 20, 30, bgA});
    context.Render.DrawRectangleOutline(pillW, 30.0f, Transform2D{pos}, border, 1.5f);
    context.Render.DrawTextCentered(m_toastMessage, m_fontId, 17.0f, Transform2D{pos}, Color{240, 246, 255, txtA});
}

void EditorScene::SaveLevel(GameContext& context)
{
    if (!m_editingLevelId.empty())
    {
        CustomLevelLibrary lib;
        if (lib.UpdateExistingLevel(m_editingLevelId, m_levelData))
        {
            m_levelData.SaveToFile("Resources/levels/custom_level.txt");
            ShowToast("Saved " + m_editingLevelId + " - " + m_levelData.GetLevelName(), Color{85, 245, 145, 255}, 2.4f);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            return;
        }
    }

    OpenSaveAsModal();
    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
}

void EditorScene::SaveLevelAs(GameContext& context)
{
    OpenSaveAsModal();
    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
}

void EditorScene::GenerateProceduralLevel()
{
    PushUndo();
    m_levelData.ProceduralGenerate(m_procBrickCount, m_procDifficulty, m_procSymmetry, static_cast<unsigned int>(m_pulseTimer * 1000.0f));
    m_hasSelectedCell = false;
    ShowToast("Generated " + std::to_string(m_levelData.GetBrickCount()) + " bricks", Color{255, 210, 95, 255}, 1.8f);
    if (mp_context && m_switchSfxId != 0)
    {
        mp_context->Audio.PlaySfx(m_switchSfxId, 16.0f);
    }
}

void EditorScene::ClearEventQueue()
{
    PushUndo();
    m_levelData.ClearEventQueue();
    m_eventQueueScroll = 0;
    ShowToast("Event queue cleared", Color{255, 130, 140, 255}, 1.6f);
    if (mp_context && m_switchSfxId != 0) mp_context->Audio.PlaySfx(m_switchSfxId, 16.0f);
}

void EditorScene::DeleteEvent(size_t index)
{
    PushUndo();
    m_levelData.RemoveEvent(index);
    if (mp_context && m_clickSfxId != 0) mp_context->Audio.PlaySfx(m_clickSfxId, 16.0f);
}

EditorScene::EditorGridLayout EditorScene::ComputeGridLayout(const Vector2f& viewSize) const
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float leftW = m_leftPanelOpen ? kLeftPanelWidth : 44.0f;
    const float rightW = m_rightPanelOpen ? kRightPanelWidth : 44.0f;

    EditorGridLayout layout;
    layout.CanvasMinX = -halfW + leftW + 6.0f;
    layout.CanvasMaxX = halfW - rightW - 6.0f;
    layout.CanvasMinY = -halfH + kToolbarHeight + 6.0f;
    layout.CanvasMaxY = halfH - 6.0f;
    layout.PageBarHeight = 64.0f;

    const float headerReserve = 36.0f;
    const float bottomReserve = layout.PageBarHeight + 18.0f;
    const float sidePad = 24.0f;

    const float availW = std::max(300.0f, (layout.CanvasMaxX - layout.CanvasMinX) - sidePad * 2.0f);
    const float availH = std::max(240.0f, (layout.CanvasMaxY - layout.CanvasMinY) - headerReserve - bottomReserve);

    const float padXRatio = 0.08f;
    const float padYRatio = 0.24f;

    layout.BrickW = std::min(112.0f, availW / (static_cast<float>(LevelData::GRID_COLS) + static_cast<float>(LevelData::GRID_COLS - 1) * padXRatio));
    layout.BrickH = std::min(56.0f, availH / (static_cast<float>(LevelData::GRID_ROWS) + static_cast<float>(LevelData::GRID_ROWS - 1) * padYRatio));
    layout.PadX = layout.BrickW * padXRatio;
    layout.PadY = layout.BrickH * padYRatio;
    layout.StepX = layout.BrickW + layout.PadX;
    layout.StepY = layout.BrickH + layout.PadY;

    const float totalGridW = static_cast<float>(LevelData::GRID_COLS) * layout.BrickW + static_cast<float>(LevelData::GRID_COLS - 1) * layout.PadX;
    const float totalGridH = static_cast<float>(LevelData::GRID_ROWS) * layout.BrickH + static_cast<float>(LevelData::GRID_ROWS - 1) * layout.PadY;

    const float canvasMidX = (layout.CanvasMinX + layout.CanvasMaxX) * 0.5f;
    const float usableTopY = layout.CanvasMinY + headerReserve;
    const float usableBotY = layout.CanvasMaxY - bottomReserve;
    const float usableMidY = (usableTopY + usableBotY) * 0.5f;

    layout.GridMinX = canvasMidX - totalGridW * 0.5f;
    layout.GridMinY = usableMidY - totalGridH * 0.5f;

    return layout;
}

std::optional<size_t> EditorScene::FindNextPlayablePage(size_t startIndex) const
{
    for (size_t i = startIndex; i < m_levelData.GetPageCount(); ++i)
    {
        if (m_levelData.GetBreakableBrickCount(i) > 0)
        {
            return i;
        }
    }
    return std::nullopt;
}

void EditorScene::RebuildPlayfieldFromGrid(GameContext& context)
{
    RebuildPlayfieldFromPage(m_levelData.GetActivePageIndex(), context, true);
}

void EditorScene::RebuildPlayfieldFromPage(size_t pageIdx, GameContext& context, bool withDescent)
{
    (void)context;
    // Clean existing simulation bricks
    m_registry.View<BrickComponent>([this](const Entity e, BrickComponent&) {
        m_registry.DestroyEntityDeferred(e);
    });
    m_registry.ProcessDeferredCommands();

    m_turretBricks.clear();
    m_turretBullets.clear();

    m_brickCount = 0;
    for (int r = 0; r < LevelData::GRID_ROWS; ++r)
    {
        for (int c = 0; c < LevelData::GRID_COLS; ++c)
        {
            EditorCellData cell = m_levelData.GetCell(c, r, pageIdx);
            if (cell.Type == EditorTileType::Empty) continue;

            Vector2f pos = m_levelData.GetCellCenter(c, r);
            BrickType bType = BrickType::Light;
            bool isSpecial = false;

            switch (cell.Type)
            {
            case EditorTileType::Light:   bType = BrickType::Light; break;
            case EditorTileType::Medium:  bType = BrickType::Medium; break;
            case EditorTileType::Hard:    bType = BrickType::Hard; break;
            case EditorTileType::Special: bType = BrickType::Special; isSpecial = true; break;
            case EditorTileType::Event:   bType = BrickType::Event; break;
            case EditorTileType::Metal:   bType = BrickType::Metal; break;
            case EditorTileType::Bomb:    bType = BrickType::Bomb; break;
            case EditorTileType::Turret:  bType = BrickType::Hard; break;
            case EditorTileType::Lantern: bType = BrickType::Lantern; break;
            default: break;
            }

            Entity bEntity = BrickFactory::Create(m_registry, pos.X, pos.Y, bType, isSpecial, m_brickTexId, 0, cell.HitPoints);

            if (m_registry.HasComponent<BrickComponent>(bEntity))
            {
                auto& bComp = m_registry.GetComponent<BrickComponent>(bEntity);
                if (cell.Type == EditorTileType::Special)
                {
                    if (!cell.RandomPowerUp)
                    {
                        bComp.HasForcedPowerUp = true;
                        bComp.ForcedPowerUp = cell.SpecificPowerUp;
                    }
                    bComp.RegenEnabled = cell.PowerUpRegenEnabled;
                    bComp.RegenCooldown = cell.PowerUpRegenCooldown;
                }
                else if (cell.Type == EditorTileType::Event)
                {
                    bComp.IsEventBrick = true;
                    bComp.RandomEvent = cell.RandomEvent;
                    bComp.TriggeredEvent = cell.SpecificEvent;
                    bComp.EventTriggerOnHit = cell.EventTriggerOnHit;
                    bComp.EventAlreadyTriggered = false;
                }
                else if (cell.Type == EditorTileType::Bomb)
                {
                    bComp.BombBlastTier = cell.BombBlastTier;
                }
                else if (cell.Type == EditorTileType::Metal)
                {
                    bComp.MetalBounceBoost = cell.MetalBounceBoost;
                }
            }

            // Custom appearance / lantern light customization
            if ((cell.Type == EditorTileType::Light || cell.Type == EditorTileType::Medium || cell.Type == EditorTileType::Hard) &&
                cell.UseCustomColor && m_registry.HasComponent<SpriteComponent>(bEntity))
            {
                m_registry.GetComponent<SpriteComponent>(bEntity).Tint = cell.CustomColor;
            }
            else if (cell.Type == EditorTileType::Lantern)
            {
                if (m_registry.HasComponent<SpriteComponent>(bEntity))
                {
                    m_registry.GetComponent<SpriteComponent>(bEntity).Tint = cell.LanternLightColor;
                }
                if (m_registry.HasComponent<LightSourceComponent>(bEntity))
                {
                    auto& light = m_registry.GetComponent<LightSourceComponent>(bEntity);
                    light.LightColor = cell.LanternLightColor;
                    light.Radius = cell.LanternLightRadius;
                    light.Intensity = cell.LanternLightIntensity;
                }
            }
            else if (cell.Type == EditorTileType::Event && m_registry.HasComponent<SpriteComponent>(bEntity))
            {
                m_registry.GetComponent<SpriteComponent>(bEntity).Tint = LevelData::GetDisplayColorForCell(cell);
            }

            if (bType != BrickType::Metal && bType != BrickType::Special)
            {
                m_brickCount++;
            }

            if (cell.Type == EditorTileType::Turret)
            {
                if (m_registry.HasComponent<SpriteComponent>(bEntity))
                {
                    m_registry.GetComponent<SpriteComponent>(bEntity).Tint =
                        (cell.HitPoints > 3) ? BrickFactory::GetColorForHitPoints(cell.HitPoints) : Color{255, 45, 55, 255};
                }
                m_turretBricks.push_back(TurretBrick{
                    .EntityId = bEntity,
                    .ShootTimer = cell.TurretInitialDelay + static_cast<float>(m_turretBricks.size()) * 0.35f,
                    .Cooldown = cell.TurretCooldown,
                    .ProjectileSpeed = cell.TurretProjectileSpeed,
                    .Damage = cell.TurretDamage,
                    .Aimed = cell.TurretAimed,
                    .BaseHP = cell.HitPoints
                });
            }

            if (withDescent)
            {
                BeginBrickDescent(bEntity, pos.Y, 340.0f, 360.0f);
            }
        }
    }
}

EditorCellData EditorScene::GetInspectorTargetData() const
{
    if (m_hasSelectedCell)
    {
        EditorCellData cell = m_levelData.GetCell(m_selectedCol, m_selectedRow);
        if (cell.Type != EditorTileType::Empty)
        {
            return cell;
        }
    }
    EditorTileType paletteType = (m_selectedTile != EditorTileType::Empty) ? m_selectedTile : EditorTileType::Light;
    return m_levelData.GetPalettePreset(paletteType);
}

void EditorScene::ApplyInspectorTargetData(const EditorCellData& updated)
{
    EditorCellData clamped = updated;
    clamped.ClampValues();

    // Always persist modifications into the palette preset for that brick type
    m_levelData.SetPalettePreset(clamped.Type, clamped);

    // If an active non-empty cell is selected and matches the type, update it too (undoable)
    if (m_hasSelectedCell)
    {
        EditorCellData current = m_levelData.GetCell(m_selectedCol, m_selectedRow);
        if (current.Type == clamped.Type && current != clamped)
        {
            PushUndo();
            m_levelData.SetCell(m_selectedCol, m_selectedRow, clamped);
        }
    }
}

void EditorScene::HandleEditorInput(GameContext& context)
{
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const Vector2f worldMouse{mx - halfW, my - halfH};

    const bool leftClick = context.Input.IsMouseButtonPress(MouseButton::Left);
    const bool leftDown = context.Input.IsMouseButtonDown(MouseButton::Left);
    const bool rightDown = context.Input.IsMouseButtonDown(MouseButton::Right);
    const bool middleClick = context.Input.IsMouseButtonPress(MouseButton::Middle);
    const bool shiftDown = context.Input.IsKeyDown(KeyCode::LShift) || context.Input.IsKeyDown(KeyCode::RShift);

    const bool ctrlDown = context.Input.IsKeyDown(KeyCode::LControl) || context.Input.IsKeyDown(KeyCode::RControl);

    // Ctrl + Z shortcut for Undo
    if (ctrlDown && context.Input.IsKeyPress(KeyCode::Z))
    {
        Undo();
        return;
    }

    // Ctrl + O shortcut for Load Level Modal
    if (ctrlDown && context.Input.IsKeyPress(KeyCode::O))
    {
        OpenLoadLevelModal(context);
        return;
    }

    // Ctrl + S shortcut for Save Level
    if (ctrlDown && context.Input.IsKeyPress(KeyCode::S))
    {
        SaveLevel(context);
        return;
    }

    // Playtest toggle shortcut [P]
    if (context.Input.IsKeyPress(KeyCode::P))
    {
        StartPlaytesting(context);
        return;
    }

    // Select tool toggle shortcut [S]
    if (!ctrlDown && context.Input.IsKeyPress(KeyCode::S))
    {
        SetTool(m_tool == EditorTool::Select ? EditorTool::Paint : EditorTool::Select);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    // Eraser toggle shortcut [G]
    if (context.Input.IsKeyPress(KeyCode::G))
    {
        SetTool(m_tool == EditorTool::Eraser ? EditorTool::Paint : EditorTool::Eraser);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    // Event brick shortcut [E]
    if (context.Input.IsKeyPress(KeyCode::E))
    {
        SetSelectedTile(EditorTileType::Event);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    // Lantern brick shortcut [L]
    if (context.Input.IsKeyPress(KeyCode::L))
    {
        SetSelectedTile(EditorTileType::Lantern);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    // Escape shortcut in edit mode: Return cleanly to Robostock or TV Menu
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        ExitEditor(context);
        return;
    }

    // Dynamic Layout Dimensions
    const float toolbarH = kToolbarHeight;
    const Vector2f toolbarPos{0.0f, -halfH + toolbarH * 0.5f};
    const Vector2f btnSize{56.0f, 50.0f};
    const float rightPanelW = m_rightPanelOpen ? kRightPanelWidth : 44.0f;
    const Vector2f rightPanelCenter{halfW - rightPanelW * 0.5f, -halfH + toolbarH + (viewSize.Y - toolbarH) * 0.5f};

    // Active EventQueue Drag-and-Drop MUST run before any panel early-return!
    if (m_isDraggingEvent)
    {
        m_dragMousePos = worldMouse;
        Vector2f trashPos{rightPanelCenter.X, halfH - 52.0f};
        Vector2f trashSize{rightPanelW - 36.0f, 58.0f};
        m_trashCanHovered = (std::abs(worldMouse.X - trashPos.X) <= trashSize.X * 0.5f &&
                             std::abs(worldMouse.Y - trashPos.Y) <= trashSize.Y * 0.5f);

        if (!context.Input.IsMouseButtonDown(MouseButton::Left))
        {
            if (m_trashCanHovered)
            {
                if (m_draggedEventIdx >= 0)
                {
                    DeleteEvent(static_cast<size_t>(m_draggedEventIdx));
                }
            }
            else
            {
                const auto& q = m_levelData.GetEventQueue();
                const float listStartY = -halfH + toolbarH + kEventQueueListStartOffsetY;
                const float itemH = 52.0f;
                int targetDropIdx = -1;

                for (int vis = 0; vis < kEventQueueMaxVisible; ++vis)
                {
                    const int actualIdx = m_eventQueueScroll + vis;
                    if (actualIdx >= static_cast<int>(q.size())) break;
                    const float ky = listStartY + static_cast<float>(vis) * itemH;
                    if (std::abs(worldMouse.X - rightPanelCenter.X) <= (rightPanelW - 36.0f) * 0.5f &&
                        std::abs(worldMouse.Y - ky) <= itemH * 0.5f)
                    {
                        targetDropIdx = actualIdx;
                        break;
                    }
                }

                if (targetDropIdx >= 0 && m_draggedEventIdx >= 0 && targetDropIdx != m_draggedEventIdx)
                {
                    PushUndo();
                    m_levelData.ReorderEvent(static_cast<size_t>(m_draggedEventIdx), static_cast<size_t>(targetDropIdx));
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                }
            }

            m_isDraggingEvent = false;
            m_draggedEventIdx = -1;
            m_trashCanHovered = false;
        }
        return;
    }

    // 1. Fullscreen Top Toolbar Interactions
    if (worldMouse.Y >= -halfH && worldMouse.Y <= -halfH + toolbarH)
    {
        Vector2f playBtnPos{-halfW + 270.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - playBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            StartPlaytesting(context);
            return;
        }
        Vector2f undoBtnPos{-halfW + 336.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - undoBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            Undo();
            return;
        }
        Vector2f selectBtnPos{-halfW + 402.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - selectBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            SetTool(m_tool == EditorTool::Select ? EditorTool::Paint : EditorTool::Select);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }
        Vector2f eraserBtnPos{-halfW + 468.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - eraserBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            SetTool(m_tool == EditorTool::Eraser ? EditorTool::Paint : EditorTool::Eraser);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return;
        }
        Vector2f saveBtnPos{-halfW + 552.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - saveBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            SaveLevel(context);
            return;
        }
        Vector2f saveAsBtnPos{-halfW + 618.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - saveAsBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            SaveLevelAs(context);
            return;
        }
        Vector2f loadBtnPos{-halfW + 684.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - loadBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            OpenLoadLevelModal(context);
            return;
        }
        Vector2f clearBtnPos{-halfW + 750.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - clearBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            PushUndo();
            m_levelData.ClearGrid();
            m_hasSelectedCell = false;
            ShowToast("Page cleared", Color{255, 115, 130, 255}, 1.6f);
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
            return;
        }
        Vector2f quitBtnPos{halfW - 55.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - quitBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            ExitEditor(context);
            return;
        }
    }

    // 2. Fullscreen Left Brick Palette Interaction (9 placeable objects, no eraser)
    const float leftPanelW = m_leftPanelOpen ? kLeftPanelWidth : 44.0f;
    const Vector2f leftTogglePos = m_leftPanelOpen ? Vector2f{-halfW + kLeftPanelWidth - 22.0f, -halfH + toolbarH + 32.0f}
                                                   : Vector2f{-halfW + 22.0f, -halfH + toolbarH + 32.0f};

    if (m_leftPanelOpen)
    {
        if (std::abs(worldMouse.X - leftTogglePos.X) <= 18.0f && std::abs(worldMouse.Y - leftTogglePos.Y) <= 18.0f && leftClick)
        {
            m_leftPanelOpen = false;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }

        if (worldMouse.X >= -halfW && worldMouse.X <= -halfW + leftPanelW)
        {
            if (leftClick)
            {
                const float palStartY = -halfH + toolbarH + 62.0f;
                const float itemH = 62.0f;
                for (int i = 0; i < 9; ++i)
                {
                    const float itemY = palStartY + static_cast<float>(i) * itemH + itemH * 0.5f;
                    if (std::abs(worldMouse.Y - itemY) <= itemH * 0.46f)
                    {
                        SetSelectedTile(kPaletteTiles[i]);
                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f, 1.0f + static_cast<float>(i) * 0.03f);
                        return;
                    }
                }
            }
            return;
        }
    }
    else
    {
        if (worldMouse.X >= -halfW && worldMouse.X <= -halfW + leftPanelW && leftClick)
        {
            m_leftPanelOpen = true;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }

    // 3. Fullscreen Right Panel Interaction
    const float tabH = 44.0f;
    const float tabY = -halfH + toolbarH + tabH * 0.5f + 10.0f;

    if (m_rightPanelOpen)
    {
        const Vector2f rightTogglePos{halfW - rightPanelW + 20.0f, tabY};
        if (std::abs(worldMouse.X - rightTogglePos.X) <= 18.0f && std::abs(worldMouse.Y - rightTogglePos.Y) <= 18.0f && leftClick)
        {
            m_rightPanelOpen = false;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }

        // 5 Segmented Tabs Click
        const float tabsStartX = halfW - rightPanelW + 42.0f;
        const float tabsAvailW = rightPanelW - 52.0f;
        const float tabW = tabsAvailW / 5.0f;

        if (worldMouse.X >= tabsStartX && worldMouse.X <= halfW - 10.0f && std::abs(worldMouse.Y - tabY) <= tabH * 0.5f && leftClick)
        {
            for (int t = 0; t < 5; ++t)
            {
                float tx = tabsStartX + static_cast<float>(t) * tabW + tabW * 0.5f;
                if (std::abs(worldMouse.X - tx) <= tabW * 0.5f)
                {
                    m_activeTab = static_cast<EditorTab>(t);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                    return;
                }
            }
        }

        if (worldMouse.X >= halfW - rightPanelW && worldMouse.X <= halfW)
        {
            const Vector2f panelCenter = rightPanelCenter;
            const float sideWidth = rightPanelW;
            switch (m_activeTab)
            {
            case EditorTab::Inspector:
            {
                if (HandleInspectorTabInput(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse, leftClick))
                {
                    return;
                }
                break;
            }
            case EditorTab::Procedural:
            {
                float cntY = -halfH + toolbarH + 150.0f;
                if (std::abs(worldMouse.Y - cntY) <= 28.0f && leftClick)
                {
                    if (worldMouse.X < panelCenter.X - 50.0f) m_procBrickCount = std::max(12, m_procBrickCount - 6);
                    else if (worldMouse.X > panelCenter.X + 50.0f) m_procBrickCount = std::min(84, m_procBrickCount + 6);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float diffY = -halfH + toolbarH + 230.0f;
                if (std::abs(worldMouse.Y - diffY) <= 28.0f && leftClick)
                {
                    if (worldMouse.X < panelCenter.X - 50.0f) m_procDifficulty = std::max(0.1f, m_procDifficulty - 0.15f);
                    else if (worldMouse.X > panelCenter.X + 50.0f) m_procDifficulty = std::min(1.0f, m_procDifficulty + 0.15f);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float symY = -halfH + toolbarH + 310.0f;
                if (std::abs(worldMouse.Y - symY) <= 28.0f && leftClick)
                {
                    m_procSymmetry = (m_procSymmetry + 1) % 4;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float genY = -halfH + toolbarH + 410.0f;
                if (std::abs(worldMouse.Y - genY) <= 34.0f && leftClick)
                {
                    GenerateProceduralLevel();
                    return;
                }
                break;
            }
            case EditorTab::ThemeLighting:
            {
                if (HandleThemeTabInput(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse, leftClick))
                {
                    return;
                }
                break;
            }
            case EditorTab::EventQueue:
            {
                if (HandleEventQueueTabInput(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse, leftClick))
                {
                    return;
                }
                break;
            }
            case EditorTab::Gamerules:
            {
                auto& gr = m_levelData.GetGamerules();
                float hzY = -halfH + toolbarH + 200.0f;
                if (std::abs(worldMouse.Y - hzY) <= 28.0f && leftClick)
                {
                    gr.HazardZones = !gr.HazardZones;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float arY = -halfH + toolbarH + 280.0f;
                if (std::abs(worldMouse.Y - arY) <= 28.0f && leftClick)
                {
                    gr.ArenaEvents = !gr.ArenaEvents;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float lfY = -halfH + toolbarH + 360.0f;
                if (std::abs(worldMouse.Y - lfY) <= 28.0f && leftClick)
                {
                    gr.InfiniteLives = !gr.InfiniteLives;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                break;
            }
            }
            return;
        }
    }
    else
    {
        if (worldMouse.X >= halfW - rightPanelW && worldMouse.X <= halfW && leftClick)
        {
            for (int t = 0; t < 5; ++t)
            {
                float ty = -halfH + toolbarH + 90.0f + static_cast<float>(t) * 46.0f;
                if (std::abs(worldMouse.Y - ty) <= 20.0f)
                {
                    m_activeTab = static_cast<EditorTab>(t);
                    break;
                }
            }
            m_rightPanelOpen = true;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }

    // 4. Central Canvas Page Bar Interaction
    const EditorGridLayout layout = ComputeGridLayout(viewSize);
    if (HandlePageBarInput(layout, worldMouse, leftClick, context))
    {
        return;
    }

    // 5. Interactive Dynamic Grid Painting / Erasing / Inspecting
    if (worldMouse.X >= layout.CanvasMinX && worldMouse.X <= layout.CanvasMaxX &&
        worldMouse.Y >= layout.CanvasMinY && worldMouse.Y <= layout.CanvasMaxY - layout.PageBarHeight)
    {
        const float relX = worldMouse.X - layout.GridMinX;
        const float relY = worldMouse.Y - layout.GridMinY;

        if (relX >= 0.0f && relY >= 0.0f)
        {
            const int col = static_cast<int>(relX / layout.StepX);
            const int row = static_cast<int>(relY / layout.StepY);

            if (col >= 0 && col < LevelData::GRID_COLS && row >= 0 && row < LevelData::GRID_ROWS)
            {
                const float localCellX = std::fmod(relX, layout.StepX);
                const float localCellY = std::fmod(relY, layout.StepY);

                if (localCellX <= layout.BrickW && localCellY <= layout.BrickH)
                {
                    if (middleClick || (shiftDown && leftClick))
                    {
                        m_hasSelectedCell = true;
                        m_selectedCol = col;
                        m_selectedRow = row;
                        EditorCellData cell = m_levelData.GetCell(col, row);
                        if (cell.Type != EditorTileType::Empty)
                        {
                            m_selectedTile = cell.Type;
                            m_tool = EditorTool::Paint;
                        }
                        m_activeTab = EditorTab::Inspector;
                        m_rightPanelOpen = true;
                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f, 1.08f);
                        return;
                    }

                    if (leftDown || rightDown)
                    {
                        const EditorCellData currentCell = m_levelData.GetCell(col, row);

                        if (rightDown || m_tool == EditorTool::Eraser)
                        {
                            if (currentCell.Type != EditorTileType::Empty)
                            {
                                PushUndo();
                                m_levelData.SetTile(col, row, EditorTileType::Empty);
                                if (m_hasSelectedCell && m_selectedCol == col && m_selectedRow == row)
                                {
                                    m_hasSelectedCell = false;
                                }
                                if (m_paintSfxCooldown <= 0.0f && m_switchSfxId != 0)
                                {
                                    context.Audio.PlaySfx(m_switchSfxId, 13.0f, 0.86f);
                                    m_paintSfxCooldown = 0.042f;
                                }
                            }
                        }
                        else if (m_tool == EditorTool::Select)
                        {
                            if (leftClick)
                            {
                                if (currentCell.Type != EditorTileType::Empty)
                                {
                                    m_hasSelectedCell = true;
                                    m_selectedCol = col;
                                    m_selectedRow = row;
                                    m_selectedTile = currentCell.Type;
                                    m_activeTab = EditorTab::Inspector;
                                    m_rightPanelOpen = true;
                                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f, 1.05f);
                                }
                                else
                                {
                                    m_hasSelectedCell = false;
                                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 10.0f, 0.90f);
                                }
                            }
                        }
                        else if (leftDown && m_tool == EditorTool::Paint)
                        {
                            const EditorCellData desiredCell = m_levelData.GetPalettePreset(m_selectedTile);
                            if (currentCell != desiredCell)
                            {
                                PushUndo();
                                m_levelData.SetCell(col, row, desiredCell);
                                if (m_paintSfxCooldown <= 0.0f && m_clickSfxId != 0)
                                {
                                    const float pitch = 0.98f + static_cast<float>(LevelData::GRID_ROWS - 1 - row) * 0.03f
                                                      + static_cast<float>(col % 3) * 0.015f;
                                    context.Audio.PlaySfx(m_clickSfxId, 14.0f, pitch);
                                    m_paintSfxCooldown = 0.042f;
                                }
                            }
                            else if (leftClick && (m_selectedCol != col || m_selectedRow != row))
                            {
                                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 11.0f, 1.02f);
                            }
                            m_hasSelectedCell = true;
                            m_selectedCol = col;
                            m_selectedRow = row;
                        }
                    }
                }
            }
        }
    }
}

void EditorScene::DrawTilePreview(GameContext& context, const EditorCellData& cell, const Vector2f& center, float w, float h) const
{
    const Color bCol = LevelData::GetDisplayColorForCell(cell);

    // 1. Render actual in-game brick texture (Resources/sprite/brick.png) scaled identically to RenderSystem
    bool drewSprite = false;
    if (m_brickTexId != 0)
    {
        const Vector2f texSize = context.Render.GetTextureSize(m_brickTexId);
        if (texSize.X > 0.0f && texSize.Y > 0.0f)
        {
            const Transform2D sprTransform{center, 0.0f, Vector2f{w / texSize.X, h / texSize.Y}};
            context.Render.DrawSprite(SpriteComponent{m_brickTexId, bCol}, sprTransform, BlendMode::Alpha);
            drewSprite = true;
        }
    }
    if (!drewSprite)
    {
        context.Render.DrawRectangle(w, h, Transform2D{center}, bCol);
    }

    // 2. Exact in-game brick overlays scaled proportionally from 100x30 in-game brick dimensions
    const float sx = w / 100.0f;
    const float sy = h / 30.0f;
    const float s = std::min(sx, sy);

    if (cell.Type == EditorTileType::Metal)
    {
        // Metallic double inset frame (matches GameScene::OnRender)
        context.Render.DrawRectangleOutline(w, h, Transform2D{center}, Color{225, 235, 250, 210}, -1.5f * s);
        context.Render.DrawRectangleOutline(w - 14.0f * sx, h - 10.0f * sy, Transform2D{center}, Color{110, 130, 155, 160}, -1.0f * s);
    }
    else if (cell.Type == EditorTileType::Bomb)
    {
        // Pulsing explosive orange-gold border + centered glowing core (matches GameScene::OnRender)
        const float p = 0.5f + 0.5f * std::sin(m_pulseTimer * 6.5f);
        const Color glowCol{255, static_cast<uint8_t>(140 + 80 * p), 30, static_cast<uint8_t>(180 + 75 * p)};
        context.Render.DrawRectangleOutline(w, h, Transform2D{center}, glowCol, -2.0f * s);
        context.Render.DrawCircle(5.0f * s, Transform2D{center}, Color{255, 250, 180, static_cast<uint8_t>(200 + 55 * p)});
    }
    else if (cell.Type == EditorTileType::Lantern)
    {
        // Pulsing custom-colored lantern border + centered luminous core (matches GameScene::OnRender)
        const float p = 0.5f + 0.5f * std::sin(m_pulseTimer * 4.5f);
        const Color lc = cell.LanternLightColor;
        const Color glowCol{lc.r, lc.g, lc.b, static_cast<uint8_t>(170 + 85 * p)};
        context.Render.DrawRectangleOutline(w, h, Transform2D{center}, glowCol, -2.0f * s);
        context.Render.DrawCircle(6.0f * s, Transform2D{center}, Color{255, 245, 180, static_cast<uint8_t>(200 + 55 * p)});
    }
    else if (cell.Type == EditorTileType::Event)
    {
        // Pulsing double electric frame + centered crisp white lightning bolt (matches GameScene::OnRender)
        const float p = 0.5f + 0.5f * std::sin(m_pulseTimer * 5.2f);
        const Color accent = cell.UseCustomColor
            ? cell.CustomColor
            : (cell.RandomEvent ? Color{110, 235, 255, 255} : GetArenaEventColor(cell.SpecificEvent));
        const Color borderCol{accent.r, accent.g, accent.b, static_cast<uint8_t>(175 + 80 * p)};
        context.Render.DrawRectangleOutline(w, h, Transform2D{center}, borderCol, -2.0f * s);
        context.Render.DrawRectangleOutline(w - 10.0f * sx, h - 8.0f * sy, Transform2D{center},
            Color{235, 250, 255, static_cast<uint8_t>(135 + 85 * p)}, -1.0f * s);

        const float lw = std::max(1.2f, 2.2f * s);
        const Color boltCol{255, 255, 255, static_cast<uint8_t>(215 + 40 * p)};
        context.Render.DrawLine(Vector2f{center.X + 3.0f * sx, center.Y - 9.0f * sy},
                                Vector2f{center.X - 4.0f * sx, center.Y + 0.5f * sy}, boltCol, lw);
        context.Render.DrawLine(Vector2f{center.X - 4.0f * sx, center.Y + 0.5f * sy},
                                Vector2f{center.X + 4.0f * sx, center.Y + 0.5f * sy}, boltCol, lw);
        context.Render.DrawLine(Vector2f{center.X + 4.0f * sx, center.Y + 0.5f * sy},
                                Vector2f{center.X - 3.0f * sx, center.Y + 9.0f * sy}, boltCol, lw);
    }
    else if (cell.Type == EditorTileType::Turret)
    {
        // Centered downward barrel triangle (matches StoryScene & EditorScene::RenderTurrets)
        const float triW = 14.0f * sx;
        const float triH = 12.0f * sy;
        std::vector<Vertex> iconVerts = {
            Vertex{center.X - triW * 0.5f, center.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{center.X + triW * 0.5f, center.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{center.X, center.Y + triH * 0.7f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);
    }
}

void EditorScene::RenderGrid(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const EditorGridLayout layout = ComputeGridLayout(viewSize);

    Vector2f canvasCenter{(layout.CanvasMinX + layout.CanvasMaxX) * 0.5f, (layout.CanvasMinY + layout.CanvasMaxY) * 0.5f};
    Vector2f canvasSize{layout.CanvasMaxX - layout.CanvasMinX, layout.CanvasMaxY - layout.CanvasMinY};

    // 1. Dedicated Dynamic Blueprint Canvas Plate with Live LightingMode & Environment Preview
    const EditorThemeData& thm = m_levelData.GetTheme();
    context.Render.DrawRectangle(canvasSize.X, canvasSize.Y, Transform2D{canvasCenter}, thm.BgColor);

    // Secondary background gradient strip
    const float bandH = canvasSize.Y * 0.34f;
    context.Render.DrawRectangle(canvasSize.X - 4.0f, bandH,
                                 Transform2D{Vector2f{canvasCenter.X, layout.CanvasMinY + bandH * 0.5f + 2.0f}},
                                 Color{thm.BgSecondaryColor.r, thm.BgSecondaryColor.g, thm.BgSecondaryColor.b, 85});

    // Live LightingMode darkness/atmosphere wash (mirrors Debug Panel presets)
    if (thm.Lighting == LightingMode::PitchBlack)
    {
        context.Render.DrawRectangle(canvasSize.X - 4.0f, canvasSize.Y - 4.0f,
                                     Transform2D{canvasCenter}, Color{2, 4, 10, 180});
    }
    else if (thm.Lighting == LightingMode::Atmospheric)
    {
        const uint8_t ambAlpha = static_cast<uint8_t>(std::clamp((1.0f - thm.AmbientIntensity) * 110.0f + 35.0f, 30.0f, 135.0f));
        context.Render.DrawRectangle(canvasSize.X - 4.0f, canvasSize.Y - 4.0f,
                                     Transform2D{canvasCenter},
                                     Color{static_cast<uint8_t>(thm.AmbientColor.r / 4),
                                           static_cast<uint8_t>(thm.AmbientColor.g / 4),
                                           static_cast<uint8_t>(thm.AmbientColor.b / 3),
                                           ambAlpha});
    }
    else // LightingMode::Lit
    {
        context.Render.DrawRectangle(canvasSize.X - 4.0f, canvasSize.Y - 4.0f,
                                     Transform2D{canvasCenter}, Color{220, 235, 255, 16});
    }

    // Subtle paddle light cone preview at bottom of grid area (stronger in Atmospheric / PitchBlack)
    if (thm.Lighting != LightingMode::Lit)
    {
        const float coneHalfW = std::clamp(thm.PaddleConeAngle * 2.2f, 40.0f, canvasSize.X * 0.36f);
        const float coneBaseY = layout.CanvasMaxY - layout.PageBarHeight - 6.0f;
        const float coneTopY = coneBaseY - std::clamp(thm.PaddleLightRange * 0.38f, 75.0f, 210.0f);
        const float modeBoost = (thm.Lighting == LightingMode::PitchBlack) ? 1.55f : 1.0f;
        const uint8_t coneAlpha = static_cast<uint8_t>(std::clamp(thm.PaddleLightIntensity * 30.0f * modeBoost, 16.0f, 110.0f));
        std::vector<Vertex> coneVerts = {
            Vertex{canvasCenter.X, coneBaseY, Color{thm.PaddleConeColor.r, thm.PaddleConeColor.g, thm.PaddleConeColor.b, coneAlpha}, 0.0f, 0.0f},
            Vertex{canvasCenter.X - coneHalfW, coneTopY, Color{thm.PaddleConeColor.r, thm.PaddleConeColor.g, thm.PaddleConeColor.b, 0}, 0.0f, 0.0f},
            Vertex{canvasCenter.X + coneHalfW, coneTopY, Color{thm.PaddleConeColor.r, thm.PaddleConeColor.g, thm.PaddleConeColor.b, 0}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(coneVerts, PrimitiveType::Triangles);
    }

    // Subtle vignette border preview
    if (thm.VignetteIntensity > 0.01f)
    {
        const uint8_t vigAlpha = static_cast<uint8_t>(std::clamp(thm.VignetteIntensity * 165.0f, 0.0f, 140.0f));
        const float edgeT = 18.0f;
        Color vigCol{thm.VignetteColor.r, thm.VignetteColor.g, thm.VignetteColor.b, vigAlpha};
        context.Render.DrawRectangle(canvasSize.X, edgeT, Transform2D{Vector2f{canvasCenter.X, layout.CanvasMinY + edgeT * 0.5f}}, vigCol);
        context.Render.DrawRectangle(canvasSize.X, edgeT, Transform2D{Vector2f{canvasCenter.X, layout.CanvasMaxY - layout.PageBarHeight - edgeT * 0.5f}}, vigCol);
        context.Render.DrawRectangle(edgeT, canvasSize.Y, Transform2D{Vector2f{layout.CanvasMinX + edgeT * 0.5f, canvasCenter.Y}}, vigCol);
        context.Render.DrawRectangle(edgeT, canvasSize.Y, Transform2D{Vector2f{layout.CanvasMaxX - edgeT * 0.5f, canvasCenter.Y}}, vigCol);
    }

    context.Render.DrawRectangleOutline(canvasSize.X, canvasSize.Y, Transform2D{canvasCenter}, Color{50, 95, 160, 220}, 2.0f);

    // 2. Clean Technical Header Tag at Canvas Top
    const char* toolTag = (m_tool == EditorTool::Select) ? "SELECT [S]" : ((m_tool == EditorTool::Eraser) ? "ERASER [G]" : "PAINT");
    std::string headerStr = "PAGE " + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount())
                          + "  |  " + toolTag
                          + "  |  " + GetLightingModeName(thm.Lighting);
    Color headerCol = (m_tool == EditorTool::Select) ? Color{110, 230, 255, 225} : ((m_tool == EditorTool::Eraser) ? Color{255, 145, 145, 225} : Color{165, 200, 240, 215});
    context.Render.DrawTextCentered(headerStr, m_fontId, 16.0f, Transform2D{Vector2f{canvasCenter.X, layout.CanvasMinY + 18.0f}}, headerCol);

    // 3. Localized Blueprint Grid Lines across dynamic canvas
    for (float y = layout.CanvasMinY + 6.0f; y < layout.CanvasMaxY - layout.PageBarHeight - 4.0f; y += 6.0f)
    {
        context.Render.DrawLine(Vector2f{layout.CanvasMinX + 2.0f, y}, Vector2f{layout.CanvasMaxX - 2.0f, y}, Color{0, 0, 0, 22}, 1.0f);
    }
    for (float x = layout.CanvasMinX + 16.0f; x < layout.CanvasMaxX - 10.0f; x += 32.0f)
    {
        context.Render.DrawLine(Vector2f{x, layout.CanvasMinY + 2.0f}, Vector2f{x, layout.CanvasMaxY - layout.PageBarHeight - 2.0f}, Color{255, 255, 255, 10}, 1.0f);
    }

    // 4. Technical Corner Drafting Brackets on the dynamic canvas bounds
    const float bLen = 24.0f;
    Color bracketCol{thm.PaddleConeColor.r, thm.PaddleConeColor.g, thm.PaddleConeColor.b, 200};
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMinY}, Vector2f{layout.CanvasMinX + bLen, layout.CanvasMinY}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMinY}, Vector2f{layout.CanvasMinX, layout.CanvasMinY + bLen}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMinY}, Vector2f{layout.CanvasMaxX - bLen, layout.CanvasMinY}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMinY}, Vector2f{layout.CanvasMaxX, layout.CanvasMinY + bLen}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMaxY}, Vector2f{layout.CanvasMinX + bLen, layout.CanvasMaxY}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMaxY}, Vector2f{layout.CanvasMinX, layout.CanvasMaxY - bLen}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMaxY}, Vector2f{layout.CanvasMaxX - bLen, layout.CanvasMaxY}, bracketCol, 2.2f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMaxY}, Vector2f{layout.CanvasMaxX, layout.CanvasMaxY - bLen}, bracketCol, 2.2f);

    // 4b. Soft radial lantern light halos on the canvas in Atmospheric / PitchBlack modes
    if (thm.Lighting != LightingMode::Lit)
    {
        const float glowIntensity = (thm.Lighting == LightingMode::PitchBlack) ? 0.55f : 0.30f;
        for (int r = 0; r < LevelData::GRID_ROWS; ++r)
        {
            for (int c = 0; c < LevelData::GRID_COLS; ++c)
            {
                const EditorCellData cell = m_levelData.GetCell(c, r);
                if (cell.Type == EditorTileType::Lantern)
                {
                    const Vector2f cellCenter = layout.GetCellCenter(c, r);
                    const float radius = std::clamp(cell.LanternLightRadius * 0.42f, 48.0f, 145.0f);
                    context.Render.DrawPointLight(cellCenter, radius, cell.LanternLightColor, glowIntensity * cell.LanternLightIntensity, BlendMode::Add);
                }
            }
        }
    }

    // 5. Grid Cells & Placed Bricks (100% faithful to in-game visuals)
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};
    const float selPulse = 0.5f + 0.5f * std::sin(m_pulseTimer * 6.0f);

    for (int r = 0; r < LevelData::GRID_ROWS; ++r)
    {
        for (int c = 0; c < LevelData::GRID_COLS; ++c)
        {
            const Vector2f cellCenter = layout.GetCellCenter(c, r);
            const EditorCellData cell = m_levelData.GetCell(c, r);

            const bool isHovered = (std::abs(worldMouse.X - cellCenter.X) <= layout.BrickW * 0.5f &&
                                    std::abs(worldMouse.Y - cellCenter.Y) <= layout.BrickH * 0.5f);
            const bool isSelectedCell = (m_hasSelectedCell && m_selectedCol == c && m_selectedRow == r);

            if (cell.Type == EditorTileType::Empty)
            {
                Color emptyCol = isHovered
                    ? ((m_tool == EditorTool::Select) ? Color{35, 65, 105, 180} : Color{45, 75, 120, 200})
                    : Color{16, 24, 40, 125};
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, emptyCol);
                context.Render.DrawRectangleOutline(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Color{75, 105, 150, 95}, 1.0f);
                context.Render.DrawRectangle(3.5f, 3.5f, Transform2D{cellCenter}, Color{100, 135, 185, 140});
            }
            else
            {
                DrawTilePreview(context, cell, cellCenter, layout.BrickW, layout.BrickH);
            }

            if (isSelectedCell)
            {
                const uint8_t glowAlpha = static_cast<uint8_t>(42.0f + 45.0f * selPulse);
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Color{80, 230, 255, glowAlpha});
                context.Render.DrawRectangleOutline(layout.BrickW + 6.0f, layout.BrickH + 6.0f, Transform2D{cellCenter}, Color{80, 230, 255, 255}, 3.0f);

                const float expand = 4.5f + 2.5f * selPulse;
                const float ox = layout.BrickW * 0.5f + expand;
                const float oy = layout.BrickH * 0.5f + expand;
                const float cLen = 10.0f;
                const Color goldCol{255, 225, 85, 255};
                context.Render.DrawLine(Vector2f{cellCenter.X - ox, cellCenter.Y - oy}, Vector2f{cellCenter.X - ox + cLen, cellCenter.Y - oy}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X - ox, cellCenter.Y - oy}, Vector2f{cellCenter.X - ox, cellCenter.Y - oy + cLen}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X + ox, cellCenter.Y - oy}, Vector2f{cellCenter.X + ox - cLen, cellCenter.Y - oy}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X + ox, cellCenter.Y - oy}, Vector2f{cellCenter.X + ox, cellCenter.Y - oy + cLen}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X - ox, cellCenter.Y + oy}, Vector2f{cellCenter.X - ox + cLen, cellCenter.Y + oy}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X - ox, cellCenter.Y + oy}, Vector2f{cellCenter.X - ox, cellCenter.Y + oy - cLen}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X + ox, cellCenter.Y + oy}, Vector2f{cellCenter.X + ox - cLen, cellCenter.Y + oy}, goldCol, 2.5f);
                context.Render.DrawLine(Vector2f{cellCenter.X + ox, cellCenter.Y + oy}, Vector2f{cellCenter.X + ox, cellCenter.Y + oy - cLen}, goldCol, 2.5f);
            }
            if (isHovered)
            {
                if (m_tool == EditorTool::Select && cell.Type != EditorTileType::Empty)
                {
                    context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Color{80, 230, 255, 50});
                }
                Color hoverBorder = (m_tool == EditorTool::Eraser)
                    ? Color{255, 60, 70, 255}
                    : ((m_tool == EditorTool::Select) ? Color{80, 230, 255, 255} : Color{100, 255, 140, 255});
                context.Render.DrawRectangleOutline(layout.BrickW + 3.0f, layout.BrickH + 3.0f, Transform2D{cellCenter}, hoverBorder, 2.0f);
            }
        }
    }

    // 6. Multi-Page Bar at bottom of central canvas
    RenderPageBar(context, layout, worldMouse);
}

void EditorScene::RenderPageBar(GameContext& context, const EditorGridLayout& layout, const Vector2f& worldMouse)
{
    const float barY = layout.CanvasMaxY - layout.PageBarHeight * 0.5f - 4.0f;
    const float barW = (layout.CanvasMaxX - layout.CanvasMinX) - 16.0f;
    const float barX = (layout.CanvasMinX + layout.CanvasMaxX) * 0.5f;

    context.Render.DrawRectangle(barW, layout.PageBarHeight - 8.0f, Transform2D{Vector2f{barX, barY}}, Color{20, 24, 34, 245});
    context.Render.DrawRectangleOutline(barW, layout.PageBarHeight - 8.0f, Transform2D{Vector2f{barX, barY}}, Color{55, 80, 125, 200}, 1.5f);

    const size_t pageCount = m_levelData.GetPageCount();
    const size_t activeIdx = m_levelData.GetActivePageIndex();
    const float btnH = 38.0f;

    auto drawBtn = [&](const Vector2f& pos, float w, const std::string& txt, Color baseBg, Color borderCol, Color txtCol) {
        const bool hov = (std::abs(worldMouse.X - pos.X) <= w * 0.5f && std::abs(worldMouse.Y - pos.Y) <= btnH * 0.5f);
        Color bg = hov ? Color{
            static_cast<uint8_t>(std::min(255, baseBg.r + 25)),
            static_cast<uint8_t>(std::min(255, baseBg.g + 25)),
            static_cast<uint8_t>(std::min(255, baseBg.b + 25)),
            baseBg.a
        } : baseBg;
        context.Render.DrawRectangle(w, btnH, Transform2D{pos}, bg);
        context.Render.DrawRectangleOutline(w, btnH, Transform2D{pos}, hov ? Colors::White : borderCol, hov ? 2.0f : 1.3f);
        context.Render.DrawTextCentered(txt, m_fontId, 17.0f, Transform2D{pos}, txtCol);
    };

    // Right-aligned page action buttons
    const float rightEdge = layout.CanvasMaxX - 18.0f;
    const float gap = 6.0f;
    const float wMoveR = 46.0f, wMoveL = 46.0f, wDel = 80.0f, wDup = 60.0f, wAdd = 94.0f;

    float cx = rightEdge - wMoveR * 0.5f;
    drawBtn(Vector2f{cx, barY}, wMoveR, ">>", Color{32, 40, 56, 255}, Color{75, 100, 145, 220}, (activeIdx + 1 < pageCount) ? Colors::White : Color{90, 100, 115, 140});
    cx -= (wMoveR * 0.5f + gap + wMoveL * 0.5f);
    drawBtn(Vector2f{cx, barY}, wMoveL, "<<", Color{32, 40, 56, 255}, Color{75, 100, 145, 220}, (activeIdx > 0) ? Colors::White : Color{90, 100, 115, 140});
    cx -= (wMoveL * 0.5f + gap + wDel * 0.5f);
    drawBtn(Vector2f{cx, barY}, wDel, "- DEL", (pageCount > 1) ? Color{68, 28, 36, 255} : Color{28, 30, 38, 200}, (pageCount > 1) ? Color{210, 75, 90, 230} : Color{50, 54, 65, 150}, (pageCount > 1) ? Color{255, 195, 200, 255} : Color{95, 100, 115, 140});
    cx -= (wDel * 0.5f + gap + wDup * 0.5f);
    drawBtn(Vector2f{cx, barY}, wDup, "DUP", Color{32, 52, 82, 255}, Color{80, 155, 235, 230}, Color{195, 230, 255, 255});
    cx -= (wDup * 0.5f + gap + wAdd * 0.5f);
    drawBtn(Vector2f{cx, barY}, wAdd, "+ PAGE", Color{32, 68, 48, 255}, Color{85, 205, 125, 230}, Color{195, 255, 210, 255});

    // Left-aligned page switcher pills
    const float leftStart = layout.CanvasMinX + 20.0f;
    const float pillsAvailMaxX = cx - wAdd * 0.5f - 12.0f;
    const float arrowW = 34.0f;
    const float pillW = 46.0f;

    float lx = leftStart + arrowW * 0.5f;
    drawBtn(Vector2f{lx, barY}, arrowW, "<", Color{32, 38, 52, 255}, Color{75, 95, 130, 220}, Colors::White);
    lx += arrowW * 0.5f + gap;

    const int maxVisiblePills = std::max(1, static_cast<int>((pillsAvailMaxX - (lx + arrowW + gap)) / (pillW + gap)));
    size_t windowStart = 0;
    if (pageCount > static_cast<size_t>(maxVisiblePills))
    {
        const size_t halfWin = static_cast<size_t>(maxVisiblePills) / 2;
        if (activeIdx > halfWin)
        {
            windowStart = std::min(activeIdx - halfWin, pageCount - static_cast<size_t>(maxVisiblePills));
        }
    }
    const size_t windowEnd = std::min(pageCount, windowStart + static_cast<size_t>(maxVisiblePills));

    for (size_t p = windowStart; p < windowEnd; ++p)
    {
        const float px = lx + pillW * 0.5f;
        const bool isAct = (p == activeIdx);
        drawBtn(Vector2f{px, barY}, pillW, std::to_string(p + 1),
                isAct ? Color{48, 76, 120, 255} : Color{26, 31, 43, 255},
                isAct ? Color{255, 215, 65, 255} : Color{58, 70, 95, 210},
                isAct ? Color{255, 225, 85, 255} : Color{190, 205, 225, 230});
        lx += pillW + gap;
    }

    drawBtn(Vector2f{lx + arrowW * 0.5f, barY}, arrowW, ">", Color{32, 38, 52, 255}, Color{75, 95, 130, 220}, Colors::White);
}

bool EditorScene::HandlePageBarInput(const EditorGridLayout& layout, const Vector2f& worldMouse, bool leftClick, GameContext& context)
{
    const float barY = layout.CanvasMaxY - layout.PageBarHeight * 0.5f - 4.0f;
    if (std::abs(worldMouse.Y - barY) > layout.PageBarHeight * 0.5f ||
        worldMouse.X < layout.CanvasMinX || worldMouse.X > layout.CanvasMaxX)
    {
        return false;
    }

    if (!leftClick) return true;

    const size_t pageCount = m_levelData.GetPageCount();
    const size_t activeIdx = m_levelData.GetActivePageIndex();
    const float btnH = 38.0f;
    auto hitBtn = [&](const Vector2f& pos, float w) {
        return std::abs(worldMouse.X - pos.X) <= w * 0.5f && std::abs(worldMouse.Y - pos.Y) <= btnH * 0.5f;
    };

    const float rightEdge = layout.CanvasMaxX - 18.0f;
    const float gap = 6.0f;
    const float wMoveR = 46.0f, wMoveL = 46.0f, wDel = 80.0f, wDup = 60.0f, wAdd = 94.0f;

    float cx = rightEdge - wMoveR * 0.5f;
    if (hitBtn(Vector2f{cx, barY}, wMoveR))
    {
        if (m_levelData.MoveActivePageRight())
        {
            PushUndo();
            m_hasSelectedCell = false;
            ShowToast("Page moved right (" + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount()) + ")", Color{120, 220, 255, 255}, 1.6f);
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        }
        return true;
    }

    cx -= (wMoveR * 0.5f + gap + wMoveL * 0.5f);
    if (hitBtn(Vector2f{cx, barY}, wMoveL))
    {
        if (m_levelData.MoveActivePageLeft())
        {
            PushUndo();
            m_hasSelectedCell = false;
            ShowToast("Page moved left (" + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount()) + ")", Color{120, 220, 255, 255}, 1.6f);
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        }
        return true;
    }

    cx -= (wMoveL * 0.5f + gap + wDel * 0.5f);
    if (hitBtn(Vector2f{cx, barY}, wDel))
    {
        if (m_levelData.RemoveActivePage())
        {
            PushUndo();
            m_hasSelectedCell = false;
            ShowToast("Page deleted", Color{255, 130, 140, 255}, 1.6f);
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        }
        return true;
    }

    cx -= (wDel * 0.5f + gap + wDup * 0.5f);
    if (hitBtn(Vector2f{cx, barY}, wDup))
    {
        PushUndo();
        m_levelData.AddPage(true);
        m_hasSelectedCell = false;
        ShowToast("Page duplicated (" + std::to_string(m_levelData.GetPageCount()) + " pages)", Color{120, 220, 255, 255}, 1.6f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return true;
    }

    cx -= (wDup * 0.5f + gap + wAdd * 0.5f);
    if (hitBtn(Vector2f{cx, barY}, wAdd))
    {
        PushUndo();
        m_levelData.AddPage(false);
        m_hasSelectedCell = false;
        ShowToast("New page added (" + std::to_string(m_levelData.GetPageCount()) + " pages)", Color{110, 235, 165, 255}, 1.6f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return true;
    }

    const float leftStart = layout.CanvasMinX + 20.0f;
    const float pillsAvailMaxX = cx - wAdd * 0.5f - 12.0f;
    const float arrowW = 34.0f;
    const float pillW = 46.0f;

    float lx = leftStart + arrowW * 0.5f;
    if (hitBtn(Vector2f{lx, barY}, arrowW))
    {
        if (activeIdx > 0)
        {
            m_levelData.SetActivePageIndex(activeIdx - 1);
            m_hasSelectedCell = false;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        }
        return true;
    }
    lx += arrowW * 0.5f + gap;

    const int maxVisiblePills = std::max(1, static_cast<int>((pillsAvailMaxX - (lx + arrowW + gap)) / (pillW + gap)));
    size_t windowStart = 0;
    if (pageCount > static_cast<size_t>(maxVisiblePills))
    {
        const size_t halfWin = static_cast<size_t>(maxVisiblePills) / 2;
        if (activeIdx > halfWin)
        {
            windowStart = std::min(activeIdx - halfWin, pageCount - static_cast<size_t>(maxVisiblePills));
        }
    }
    const size_t windowEnd = std::min(pageCount, windowStart + static_cast<size_t>(maxVisiblePills));

    for (size_t p = windowStart; p < windowEnd; ++p)
    {
        const float px = lx + pillW * 0.5f;
        if (hitBtn(Vector2f{px, barY}, pillW))
        {
            m_levelData.SetActivePageIndex(p);
            m_hasSelectedCell = false;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        lx += pillW + gap;
    }

    if (hitBtn(Vector2f{lx + arrowW * 0.5f, barY}, arrowW))
    {
        if (activeIdx + 1 < pageCount)
        {
            m_levelData.SetActivePageIndex(activeIdx + 1);
            m_hasSelectedCell = false;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        }
        return true;
    }

    return true;
}

void EditorScene::RenderDefeatModal(GameContext& context)
{
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    context.Render.DrawRectangle(viewSize.X * 1.5f, viewSize.Y * 1.5f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{10, 12, 16, 210});

    const Vector2f modalSize{640.0f, 320.0f};
    const Vector2f modalPos{0.0f, 0.0f};

    context.Render.DrawRectangle(modalSize.X, modalSize.Y, Transform2D{modalPos}, Color{30, 31, 34, 255});
    context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{modalPos}, Color{78, 82, 92, 255}, 2.0f);

    const float headerH = 52.0f;
    const Vector2f headerPos{0.0f, modalPos.Y - modalSize.Y * 0.5f + headerH * 0.5f};
    context.Render.DrawRectangle(modalSize.X - 4.0f, headerH, Transform2D{headerPos}, Color{43, 45, 48, 255});
    context.Render.DrawLine(Vector2f{-modalSize.X * 0.5f, headerPos.Y + headerH * 0.5f},
                            Vector2f{modalSize.X * 0.5f, headerPos.Y + headerH * 0.5f},
                            Color{60, 64, 72, 255}, 1.5f);

    DrawIconStop(context, Vector2f{-220.0f, headerPos.Y}, 20.0f, Color{219, 88, 96, 255});
    context.Render.DrawTextCentered("PLAYTEST ENDED - LEVEL FAILED", m_fontId, 22.0f, Transform2D{Vector2f{16.0f, headerPos.Y}}, Color{235, 238, 245, 255});

    context.Render.DrawTextCentered("All lives were lost during the playtest.", m_fontId, 21.0f, Transform2D{Vector2f{0.0f, modalPos.Y - 35.0f}}, Color{210, 215, 225, 255});
    context.Render.DrawTextCentered("What would you like to do?", m_fontId, 19.0f, Transform2D{Vector2f{0.0f, modalPos.Y}}, Color{160, 165, 178, 255});

    const Vector2f btn1Pos{-150.0f, modalPos.Y + 75.0f};
    const Vector2f btn2Pos{150.0f, modalPos.Y + 75.0f};
    const Vector2f btnSize{270.0f, 54.0f};

    bool btn1Hover = (std::abs(worldMouse.X - btn1Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn1Pos.Y) <= btnSize.Y * 0.5f);
    bool btn2Hover = (std::abs(worldMouse.X - btn2Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn2Pos.Y) <= btnSize.Y * 0.5f);

    Color btn1Bg = btn1Hover ? Color{65, 175, 95, 255} : Color{50, 145, 75, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btn1Pos}, btn1Bg);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btn1Pos}, btn1Hover ? Colors::White : Color{89, 168, 105, 255}, 1.5f);
    DrawIconPlay(context, Vector2f{btn1Pos.X - 95.0f, btn1Pos.Y}, 20.0f, Colors::White);
    context.Render.DrawTextCentered("RETRY [R / SPACE]", m_fontId, 18.0f, Transform2D{Vector2f{btn1Pos.X + 15.0f, btn1Pos.Y}}, Colors::White);

    Color btn2Bg = btn2Hover ? Color{60, 65, 76, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btn2Pos}, btn2Bg);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btn2Pos}, btn2Hover ? Colors::White : Color{90, 95, 108, 255}, 1.5f);
    DrawIconUndo(context, Vector2f{btn2Pos.X - 95.0f, btn2Pos.Y}, 20.0f, Color{88, 157, 246, 255});
    context.Render.DrawTextCentered("BACK TO EDITOR [ESC]", m_fontId, 18.0f, Transform2D{Vector2f{btn2Pos.X + 15.0f, btn2Pos.Y}}, Color{230, 235, 245, 255});
}

void EditorScene::HandleDefeatModalInput(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    const Vector2f modalPos{0.0f, 0.0f};
    const Vector2f btn1Pos{-150.0f, modalPos.Y + 75.0f};
    const Vector2f btn2Pos{150.0f, modalPos.Y + 75.0f};
    const Vector2f btnSize{270.0f, 54.0f};

    if (context.Input.IsMouseButtonPress(MouseButton::Left))
    {
        if (std::abs(worldMouse.X - btn1Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn1Pos.Y) <= btnSize.Y * 0.5f)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            m_showDefeatModal = false;
            StartPlaytesting(context);
            return;
        }

        if (std::abs(worldMouse.X - btn2Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn2Pos.Y) <= btnSize.Y * 0.5f)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            m_showDefeatModal = false;
            StopPlaytesting(context);
            return;
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsKeyPress(KeyCode::R))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        m_showDefeatModal = false;
        StartPlaytesting(context);
    }
    else if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        m_showDefeatModal = false;
        StopPlaytesting(context);
    }
}

// Vector Icon Drawing Helpers (JetBrains New UI inspired)
void EditorScene::DrawIconPlay(GameContext& context, const Vector2f& pos, float size, Color color)
{
    float h = size * 0.48f;
    float w = size * 0.40f;
    // JetBrains Run triangle: sharp right-facing equilateral triangle
    std::vector<Vertex> verts = {
        Vertex{pos.X - w, pos.Y - h, color, 0.0f, 0.0f},
        Vertex{pos.X + w + 1.0f, pos.Y, color, 0.0f, 0.0f},
        Vertex{pos.X - w, pos.Y + h, color, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(verts, PrimitiveType::Triangles);
}

void EditorScene::DrawIconStop(GameContext& context, const Vector2f& pos, float size, Color color)
{
    // JetBrains Stop square: clean flat square
    float s = size * 0.72f;
    context.Render.DrawRectangle(s, s, Transform2D{pos}, color);
    context.Render.DrawRectangleOutline(s, s, Transform2D{pos}, Color{255, 255, 255, 180}, 1.5f);
}

void EditorScene::DrawIconUndo(GameContext& context, const Vector2f& pos, float size, Color color)
{
    // JetBrains Rollback / Undo: 270 deg circular arc with arrow tip at top
    float r = size * 0.38f;
    Vector2f center = pos + Vector2f{1.0f, 2.0f};
    const int segments = 16;
    const float startAngle = 0.55f;
    const float endAngle = 4.80f;

    for (int i = 0; i < segments; ++i)
    {
        float a1 = startAngle + (endAngle - startAngle) * (static_cast<float>(i) / segments);
        float a2 = startAngle + (endAngle - startAngle) * (static_cast<float>(i + 1) / segments);
        Vector2f p1{center.X + r * std::cos(a1), center.Y + r * std::sin(a1)};
        Vector2f p2{center.X + r * std::cos(a2), center.Y + r * std::sin(a2)};
        context.Render.DrawLine(p1, p2, color, 2.5f);
    }

    // Arrow tip pointing down-left at the start of the arc
    Vector2f tip{center.X + r * std::cos(startAngle), center.Y + r * std::sin(startAngle)};
    std::vector<Vertex> arrowVerts = {
        Vertex{tip.X - 1.0f, tip.Y - 6.0f, color, 0.0f, 0.0f},
        Vertex{tip.X - 8.0f, tip.Y + 1.0f, color, 0.0f, 0.0f},
        Vertex{tip.X + 2.0f, tip.Y + 2.0f, color, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(arrowVerts, PrimitiveType::Triangles);
}

void EditorScene::DrawIconSelect(GameContext& context, const Vector2f& pos, float size, Color color)
{
    const float s = size * 0.44f;
    // Crisp pointer arrow (top-left to bottom-right)
    const Vector2f tip{pos.X - s * 0.65f, pos.Y - s * 0.80f};
    std::vector<Vertex> cursorVerts = {
        Vertex{tip.X, tip.Y, Colors::White, 0.0f, 0.0f},
        Vertex{tip.X + s * 1.15f, tip.Y + s * 0.52f, color, 0.0f, 0.0f},
        Vertex{tip.X + s * 0.35f, tip.Y + s * 1.25f, color, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(cursorVerts, PrimitiveType::Triangles);
    context.Render.DrawLine(tip + Vector2f{s * 0.55f, s * 0.70f}, tip + Vector2f{s * 1.05f, s * 1.25f}, Colors::White, 2.5f);

    // Corner selection marquee accents around bottom-right
    const Color accentCol{255, 225, 85, 255};
    const Vector2f br{pos.X + s * 0.85f, pos.Y + s * 0.85f};
    context.Render.DrawLine(br, br - Vector2f{s * 0.50f, 0.0f}, accentCol, 2.0f);
    context.Render.DrawLine(br, br - Vector2f{0.0f, s * 0.50f}, accentCol, 2.0f);
}

void EditorScene::DrawIconEraser(GameContext& context, const Vector2f& pos, float size, Color color)
{
    (void)color;
    // JetBrains eraser: slanted dual-tone rubber block (-25 deg tilt)
    float w = size * 0.85f;
    float h = size * 0.50f;
    float rot = -0.44f;

    // Pink / coral bottom half (#ED6C63)
    context.Render.DrawRectangle(w * 0.5f, h, Transform2D{pos - Vector2f{w * 0.22f, 0.0f}, rot}, Color{237, 108, 99, 255});
    // Crisp light top half (#DFE1E5)
    context.Render.DrawRectangle(w * 0.5f, h, Transform2D{pos + Vector2f{w * 0.22f, 0.0f}, rot}, Color{223, 225, 229, 255});
    // Fine outline & center band
    context.Render.DrawRectangleOutline(w, h, Transform2D{pos, rot}, Color{50, 53, 58, 255}, 1.5f);
    context.Render.DrawLine(pos + Vector2f{-2.0f, -h * 0.45f}, pos + Vector2f{2.0f, h * 0.45f}, Color{50, 53, 58, 255}, 1.5f);
}

void EditorScene::DrawIconFloppy(GameContext& context, const Vector2f& pos, float size, Color color)
{
    (void)color;
    // JetBrains diskette: Slate blue body (#4A88C7)
    float s = size * 0.80f;
    context.Render.DrawRectangle(s, s, Transform2D{pos}, Color{74, 136, 199, 255});
    context.Render.DrawRectangleOutline(s, s, Transform2D{pos}, Color{35, 65, 100, 255}, 1.5f);

    // Top metal slider (#CED0D6)
    float sliderW = s * 0.62f;
    float sliderH = s * 0.38f;
    Vector2f sliderPos{pos.X, pos.Y - s * 0.28f};
    context.Render.DrawRectangle(sliderW, sliderH, Transform2D{sliderPos}, Color{206, 210, 216, 255});
    // Dark read notch in slider
    context.Render.DrawRectangle(3.0f, sliderH * 0.55f, Transform2D{Vector2f{sliderPos.X - sliderW * 0.22f, sliderPos.Y}}, Color{40, 42, 48, 255});

    // Lower label (#F0F3F6)
    float labelW = s * 0.72f;
    float labelH = s * 0.38f;
    Vector2f labelPos{pos.X, pos.Y + s * 0.26f};
    context.Render.DrawRectangle(labelW, labelH, Transform2D{labelPos}, Color{240, 243, 246, 255});
    // Label lines
    context.Render.DrawLine(Vector2f{labelPos.X - labelW * 0.35f, labelPos.Y - 3.0f},
                            Vector2f{labelPos.X + labelW * 0.35f, labelPos.Y - 3.0f},
                            Color{150, 160, 175, 255}, 1.5f);
    context.Render.DrawLine(Vector2f{labelPos.X - labelW * 0.35f, labelPos.Y + 3.0f},
                            Vector2f{labelPos.X + labelW * 0.35f, labelPos.Y + 3.0f},
                            Color{150, 160, 175, 255}, 1.5f);
}

void EditorScene::DrawIconFloppyPlus(GameContext& context, const Vector2f& pos, float size, Color color)
{
    DrawIconFloppy(context, pos, size, color);
    // Green plus emblem in bottom-right corner (#59A869)
    Vector2f badgePos{pos.X + size * 0.36f, pos.Y + size * 0.32f};
    context.Render.DrawCircle(7.0f, Transform2D{badgePos}, Color{89, 168, 105, 255});
    context.Render.DrawCircleOutline(7.0f, Transform2D{badgePos}, Colors::White, 1.2f);
    context.Render.DrawLine(badgePos - Vector2f{4.0f, 0.0f}, badgePos + Vector2f{4.0f, 0.0f}, Colors::White, 2.0f);
    context.Render.DrawLine(badgePos - Vector2f{0.0f, 4.0f}, badgePos + Vector2f{0.0f, 4.0f}, Colors::White, 2.0f);
}

void EditorScene::DrawIconFolderOpen(GameContext& context, const Vector2f& pos, float size, Color color)
{
    (void)color;
    const float w = size * 0.88f;
    const float h = size * 0.66f;

    // Back folder tab (top-left, warm gold #D9A343)
    const Vector2f tabPos{pos.X - w * 0.22f, pos.Y - h * 0.42f};
    context.Render.DrawRectangle(w * 0.44f, h * 0.34f, Transform2D{tabPos}, Color{217, 163, 67, 255});
    context.Render.DrawRectangleOutline(w * 0.44f, h * 0.34f, Transform2D{tabPos}, Color{125, 85, 25, 255}, 1.2f);

    // Back folder body
    const Vector2f backPos{pos.X, pos.Y - h * 0.04f};
    context.Render.DrawRectangle(w * 0.92f, h * 0.72f, Transform2D{backPos}, Color{217, 163, 67, 255});

    // Crisp white document sheet peeking inside
    const Vector2f docPos{pos.X + 1.0f, pos.Y - h * 0.14f};
    context.Render.DrawRectangle(w * 0.72f, h * 0.46f, Transform2D{docPos}, Color{240, 244, 250, 255});

    // Open front folder flap (bright amber #F2C55C)
    const Vector2f frontPos{pos.X + 1.0f, pos.Y + h * 0.14f};
    context.Render.DrawRectangle(w, h * 0.64f, Transform2D{frontPos}, Color{242, 197, 92, 255});
    context.Render.DrawRectangleOutline(w, h * 0.64f, Transform2D{frontPos}, Color{135, 92, 28, 255}, 1.4f);
}

void EditorScene::DrawIconTrash(GameContext& context, const Vector2f& pos, float size, Color color)
{
    float w = size * 0.58f;
    float h = size * 0.65f;
    Vector2f binPos{pos.X, pos.Y + 3.0f};

    // Wastebasket body (#DB5860)
    context.Render.DrawRectangle(w, h, Transform2D{binPos}, color);
    context.Render.DrawRectangleOutline(w, h, Transform2D{binPos}, Color{160, 40, 50, 255}, 1.5f);

    // 2 vertical slots/stripes
    context.Render.DrawLine(Vector2f{binPos.X - w * 0.22f, binPos.Y - h * 0.32f},
                            Vector2f{binPos.X - w * 0.22f, binPos.Y + h * 0.32f},
                            Colors::White, 1.5f);
    context.Render.DrawLine(Vector2f{binPos.X + w * 0.22f, binPos.Y - h * 0.32f},
                            Vector2f{binPos.X + w * 0.22f, binPos.Y + h * 0.32f},
                            Colors::White, 1.5f);

    // Separated tilted lid (-14 degrees)
    float lidRot = -0.24f;
    Vector2f lidPos{pos.X, binPos.Y - h * 0.5f - 3.0f};
    context.Render.DrawRectangle(w * 1.35f, 4.0f, Transform2D{lidPos, lidRot}, Colors::White);
    // Lid handle
    context.Render.DrawRectangle(w * 0.40f, 3.0f, Transform2D{lidPos + Vector2f{0.0f, -4.0f}, lidRot}, Colors::White);
}

void EditorScene::DrawIconExit(GameContext& context, const Vector2f& pos, float size, Color color)
{
    float s = size * 0.72f;
    // JetBrains Door Frame bracket (#CED0D6)
    Vector2f frameCenter{pos.X - 4.0f, pos.Y};
    context.Render.DrawLine(Vector2f{frameCenter.X + s * 0.25f, frameCenter.Y - s * 0.5f},
                            Vector2f{frameCenter.X - s * 0.30f, frameCenter.Y - s * 0.5f},
                            color, 2.0f);
    context.Render.DrawLine(Vector2f{frameCenter.X - s * 0.30f, frameCenter.Y - s * 0.5f},
                            Vector2f{frameCenter.X - s * 0.30f, frameCenter.Y + s * 0.5f},
                            color, 2.0f);
    context.Render.DrawLine(Vector2f{frameCenter.X - s * 0.30f, frameCenter.Y + s * 0.5f},
                            Vector2f{frameCenter.X + s * 0.25f, frameCenter.Y + s * 0.5f},
                            color, 2.0f);

    // Cyan/blue exit arrow pointing out to the right
    Vector2f arrStart{pos.X - 5.0f, pos.Y};
    Vector2f arrEnd{pos.X + s * 0.60f, pos.Y};
    Color arrCol{88, 157, 246, 255};
    context.Render.DrawLine(arrStart, arrEnd, arrCol, 2.5f);

    std::vector<Vertex> arrTip = {
        Vertex{arrEnd.X - 6.0f, arrEnd.Y - 5.0f, arrCol, 0.0f, 0.0f},
        Vertex{arrEnd.X + 1.0f, arrEnd.Y, arrCol, 0.0f, 0.0f},
        Vertex{arrEnd.X - 6.0f, arrEnd.Y + 5.0f, arrCol, 0.0f, 0.0f}
    };
    context.Render.DrawVertices(arrTip, PrimitiveType::Triangles);
}

void EditorScene::DrawChevron(GameContext& context, const Vector2f& pos, float size, bool pointsRight, Color color)
{
    float h = size * 0.45f;
    float w = size * 0.30f;
    float dir = pointsRight ? 1.0f : -1.0f;
    Vector2f top{pos.X - w * dir, pos.Y - h};
    Vector2f mid{pos.X + w * dir, pos.Y};
    Vector2f bot{pos.X - w * dir, pos.Y + h};
    context.Render.DrawLine(top, mid, color, 2.5f);
    context.Render.DrawLine(mid, bot, color, 2.5f);
}

Color EditorScene::GetBrickColor(EditorTileType tile) const
{
    switch (tile)
    {
    case EditorTileType::Light:   return Color{60, 220, 90, 255};
    case EditorTileType::Medium:  return Color{255, 220, 40, 255};
    case EditorTileType::Hard:    return Color{240, 50, 60, 255};
    case EditorTileType::Special: return Color{255, 40, 240, 255};
    case EditorTileType::Event:   return Color{45, 220, 235, 255};
    case EditorTileType::Metal:   return Color{180, 195, 210, 255};
    case EditorTileType::Bomb:    return Color{255, 85, 25, 255};
    case EditorTileType::Turret:  return Color{225, 40, 75, 255};
    case EditorTileType::Lantern: return Color{255, 235, 120, 255};
    default:                      return Color{100, 110, 130, 255};
    }
}

void EditorScene::RenderPlaytestStopButton(GameContext& context)
{
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    const Vector2f stopBtnPos{-halfW + 150.0f, halfH - 56.0f};
    const Vector2f btnSize{236.0f, 54.0f};

    Vector2f drawPos = stopBtnPos;
    if (context.Rules.GetRule(Rule::Graphics::EnableShader))
    {
        const float normX = stopBtnPos.X / halfW;
        const float normY = stopBtnPos.Y / halfH;
        const float offX = normY / 4.2f;
        const float offY = normX / 4.2f;
        const float crtNormX = normX + normX * (offX * offX);
        const float crtNormY = normY + normY * (offY * offY);
        drawPos = Vector2f{crtNormX * halfW, crtNormY * halfH};
    }

    const bool isHovered = (std::abs(worldMouse.X - stopBtnPos.X) <= btnSize.X * 0.56f &&
                            std::abs(worldMouse.Y - stopBtnPos.Y) <= btnSize.Y * 0.62f) ||
                           (std::abs(worldMouse.X - drawPos.X) <= btnSize.X * 0.56f &&
                            std::abs(worldMouse.Y - drawPos.Y) <= btnSize.Y * 0.62f);

    const float pulse = 0.5f + 0.5f * std::sin(m_pulseTimer * 5.0f);
    const uint8_t haloAlpha = static_cast<uint8_t>(isHovered ? 130 : (55 + 45 * pulse));

    // High-visibility outer glow & drop shadow
    context.Render.DrawRectangle(btnSize.X + 10.0f, btnSize.Y + 10.0f, Transform2D{drawPos}, Color{255, 75, 85, haloAlpha});
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{drawPos + Vector2f{4.0f, 4.0f}}, Color{0, 0, 0, 210});

    // High-contrast crimson-to-slate pill body
    const Color bg = isHovered ? Color{235, 52, 68, 255} : Color{185, 32, 48, 250};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{drawPos}, bg);

    // Left icon badge plate
    const float iconBoxW = 48.0f;
    const Vector2f iconBoxPos{drawPos.X - btnSize.X * 0.5f + iconBoxW * 0.5f + 3.0f, drawPos.Y};
    context.Render.DrawRectangle(iconBoxW, btnSize.Y - 6.0f, Transform2D{iconBoxPos},
        isHovered ? Color{255, 85, 95, 255} : Color{220, 48, 64, 255});
    DrawIconStop(context, iconBoxPos, 22.0f, Colors::White);

    // Top specular highlight line & double high-contrast border
    context.Render.DrawLine(Vector2f{drawPos.X - btnSize.X * 0.5f + 2.0f, drawPos.Y - btnSize.Y * 0.5f + 2.0f},
                            Vector2f{drawPos.X + btnSize.X * 0.5f - 2.0f, drawPos.Y - btnSize.Y * 0.5f + 2.0f},
                            Color{255, 210, 140, 235}, 2.0f);

    const Color borderCol = isHovered
        ? Colors::White
        : Color{255, static_cast<uint8_t>(185 + 55 * pulse), 95, 255};
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{drawPos}, borderCol, isHovered ? 3.0f : 2.2f);

    // Always-visible bold readable text + hotkey hint
    const Vector2f labelPos{drawPos.X + 24.0f, drawPos.Y - 7.0f};
    const Vector2f subPos{drawPos.X + 24.0f, drawPos.Y + 12.0f};
    context.Render.DrawTextCentered("BACK TO EDITOR", m_fontId, 21.0f, Transform2D{labelPos + Vector2f{1.5f, 1.5f}}, Color{0, 0, 0, 220});
    context.Render.DrawTextCentered("BACK TO EDITOR", m_fontId, 21.0f, Transform2D{labelPos}, Colors::White);
    context.Render.DrawTextCentered("[P] STOP  |  [ESC] PAUSE", m_fontId, 14.0f, Transform2D{subPos}, Color{255, 235, 155, 245});
}

void EditorScene::RenderToolbar(GameContext& context)
{
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = kToolbarHeight;
    const Vector2f barPos{0.0f, -halfH + toolbarH * 0.5f};

    context.Render.DrawRectangle(viewSize.X * 1.02f, toolbarH, Transform2D{barPos}, Color{30, 31, 34, 255});
    context.Render.DrawLine(Vector2f{-halfW, barPos.Y + toolbarH * 0.5f},
                            Vector2f{halfW, barPos.Y + toolbarH * 0.5f},
                            Color{60, 64, 72, 255}, 1.5f);

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    // Left: Rebreaker Maker logo
    Vector2f logoPos{-halfW + 120.0f, barPos.Y};
    context.Render.DrawTextCentered("REBREAKER", m_fontId, 22.0f, Transform2D{logoPos}, Color{255, 215, 60, 255});
    context.Render.DrawLine(Vector2f{-halfW + 220.0f, barPos.Y - 20.0f},
                            Vector2f{-halfW + 220.0f, barPos.Y + 20.0f},
                            Color{65, 70, 80, 255}, 1.5f);

    const Vector2f btnSize{56.0f, 50.0f};
    std::string hoveredTooltip;
    float hoveredBtnX = 0.0f;

    // 1. Play / Stop Button
    Vector2f playPos{-halfW + 270.0f, barPos.Y};
    bool playHover = (std::abs(worldMouse.X - playPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - playPos.Y) <= btnSize.Y * 0.5f);

    if (m_isPlaytesting)
    {
        uint8_t pulse = static_cast<uint8_t>(200 + 55 * std::sin(m_pulseTimer * 8.0f));
        Color cutCol = playHover ? Color{255, 55, 75, 255} : Color{219, 88, 96, pulse};
        context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{playPos}, cutCol);
        context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{playPos}, Colors::White, 2.0f);
        DrawIconStop(context, playPos, 22.0f, Colors::White);
        if (playHover) { hoveredTooltip = "Stop playtest [P]"; hoveredBtnX = playPos.X; }
    }
    else
    {
        Color playCol = playHover ? Color{65, 175, 95, 255} : Color{50, 145, 75, 255};
        context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{playPos}, playCol);
        context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{playPos}, playHover ? Colors::White : Color{89, 168, 105, 255}, 1.5f);
        DrawIconPlay(context, playPos, 22.0f, Colors::White);
        if (playHover) { hoveredTooltip = "Start playtest [P]"; hoveredBtnX = playPos.X; }
    }

    if (m_isPlaytesting)
    {
        Vector2f testBadgePos{0.0f, barPos.Y};
        context.Render.DrawTextCentered("PLAYTEST ACTIVE", m_fontId, 21.0f, Transform2D{testBadgePos}, Color{80, 220, 255, 255});
        return;
    }

    // 2. Undo Button
    Vector2f undoPos{-halfW + 336.0f, barPos.Y};
    bool undoHover = (std::abs(worldMouse.X - undoPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - undoPos.Y) <= btnSize.Y * 0.5f);
    Color undoCol = undoHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{undoPos}, undoCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{undoPos}, undoHover ? Colors::White : Color{68, 73, 85, 255}, 1.5f);
    DrawIconUndo(context, undoPos, 24.0f, Color{88, 157, 246, 255});
    if (undoHover) { hoveredTooltip = "Undo [Ctrl+Z]"; hoveredBtnX = undoPos.X; }

    // 3. Select / Inspect Tool Button
    Vector2f selectPos{-halfW + 402.0f, barPos.Y};
    bool selectHover = (std::abs(worldMouse.X - selectPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - selectPos.Y) <= btnSize.Y * 0.5f);
    bool isSelectActive = (m_tool == EditorTool::Select);
    Color selectCol = isSelectActive ? Color{35, 62, 85, 255} : (selectHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255});
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{selectPos}, selectCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{selectPos}, isSelectActive ? Color{80, 230, 255, 255} : (selectHover ? Colors::White : Color{68, 73, 85, 255}), isSelectActive ? 2.5f : 1.5f);
    DrawIconSelect(context, selectPos, 24.0f, isSelectActive ? Color{80, 230, 255, 255} : Color{160, 220, 255, 255});
    if (selectHover) { hoveredTooltip = "Select / Inspect [S]"; hoveredBtnX = selectPos.X; }

    // 4. Eraser Button
    Vector2f eraserPos{-halfW + 468.0f, barPos.Y};
    bool eraserHover = (std::abs(worldMouse.X - eraserPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - eraserPos.Y) <= btnSize.Y * 0.5f);
    bool isEraserActive = (m_tool == EditorTool::Eraser);
    Color eraserCol = isEraserActive ? Color{70, 45, 55, 255} : (eraserHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255});
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{eraserPos}, eraserCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{eraserPos}, isEraserActive ? Color{237, 108, 99, 255} : (eraserHover ? Colors::White : Color{68, 73, 85, 255}), isEraserActive ? 2.5f : 1.5f);
    DrawIconEraser(context, eraserPos, 24.0f, Colors::White);
    if (eraserHover) { hoveredTooltip = "Eraser [G]"; hoveredBtnX = eraserPos.X; }

    context.Render.DrawLine(Vector2f{-halfW + 510.0f, barPos.Y - 20.0f},
                            Vector2f{-halfW + 510.0f, barPos.Y + 20.0f},
                            Color{65, 70, 80, 255}, 1.5f);

    // 5. Save Button
    Vector2f savePos{-halfW + 552.0f, barPos.Y};
    bool saveHover = (std::abs(worldMouse.X - savePos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - savePos.Y) <= btnSize.Y * 0.5f);
    Color saveCol = saveHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{savePos}, saveCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{savePos}, saveHover ? Colors::White : Color{68, 73, 85, 255}, 1.5f);
    DrawIconFloppy(context, savePos, 24.0f, Colors::White);
    if (saveHover) { hoveredTooltip = "Save level [Ctrl+S]"; hoveredBtnX = savePos.X; }

    // 6. Save As Button
    Vector2f saveAsPos{-halfW + 618.0f, barPos.Y};
    bool saveAsHover = (std::abs(worldMouse.X - saveAsPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - saveAsPos.Y) <= btnSize.Y * 0.5f);
    Color saveAsCol = saveAsHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{saveAsPos}, saveAsCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{saveAsPos}, saveAsHover ? Colors::White : Color{68, 73, 85, 255}, 1.5f);
    DrawIconFloppyPlus(context, saveAsPos, 24.0f, Colors::White);
    if (saveAsHover) { hoveredTooltip = "Save level as..."; hoveredBtnX = saveAsPos.X; }

    // 7. Load Level Button
    Vector2f loadPos{-halfW + 684.0f, barPos.Y};
    bool loadHover = (std::abs(worldMouse.X - loadPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - loadPos.Y) <= btnSize.Y * 0.5f);
    Color loadCol = loadHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{loadPos}, loadCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{loadPos}, loadHover ? Color{255, 215, 65, 255} : Color{68, 73, 85, 255}, 1.5f);
    DrawIconFolderOpen(context, loadPos, 24.0f, Color{242, 197, 92, 255});
    if (loadHover) { hoveredTooltip = "Load level [Ctrl+O]"; hoveredBtnX = loadPos.X; }

    // 8. Clear Page Button
    Vector2f clearPos{-halfW + 750.0f, barPos.Y};
    bool clearHover = (std::abs(worldMouse.X - clearPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - clearPos.Y) <= btnSize.Y * 0.5f);
    Color clearCol = clearHover ? Color{80, 35, 45, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{clearPos}, clearCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{clearPos}, clearHover ? Color{219, 88, 96, 255} : Color{68, 73, 85, 255}, 1.5f);
    DrawIconTrash(context, clearPos, 24.0f, Color{219, 88, 96, 255});
    if (clearHover) { hoveredTooltip = "Clear current page"; hoveredBtnX = clearPos.X; }

    context.Render.DrawLine(Vector2f{-halfW + 792.0f, barPos.Y - 20.0f},
                            Vector2f{-halfW + 792.0f, barPos.Y + 20.0f},
                            Color{65, 70, 80, 255}, 1.5f);

    // Active Level ID + Name badge (when saved in Robostock)
    if (!m_editingLevelId.empty())
    {
        Vector2f lvlBadgePos{-halfW + 965.0f, barPos.Y};
        std::string badgeTxt = m_editingLevelId + "  " + m_levelData.GetLevelName();
        context.Render.DrawRectangle(310.0f, 38.0f, Transform2D{lvlBadgePos}, Color{20, 26, 38, 255});
        context.Render.DrawRectangleOutline(310.0f, 38.0f, Transform2D{lvlBadgePos}, Color{80, 220, 255, 190}, 1.5f);
        context.Render.DrawTextCentered(badgeTxt, m_fontId, 18.0f, Transform2D{lvlBadgePos}, Color{190, 240, 255, 255});
    }

    // 9. Exit Button (Far Right)
    Vector2f quitPos{halfW - 55.0f, barPos.Y};
    bool quitHover = (std::abs(worldMouse.X - quitPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - quitPos.Y) <= btnSize.Y * 0.5f);
    Color quitCol = quitHover ? Color{65, 40, 48, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{quitPos}, quitCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{quitPos}, quitHover ? Color{219, 88, 96, 255} : Color{68, 73, 85, 255}, 1.5f);
    DrawIconExit(context, quitPos, 22.0f, Color{206, 210, 216, 255});
    if (quitHover) { hoveredTooltip = "Exit editor [Esc]"; hoveredBtnX = quitPos.X; }

    // Floating hover tooltip directly below hovered toolbar button (no permanent top-right clutter)
    if (!hoveredTooltip.empty())
    {
        const float tipW = std::max(150.0f, static_cast<float>(hoveredTooltip.size()) * 10.5f + 26.0f);
        const float tipX = std::clamp(hoveredBtnX, -halfW + tipW * 0.5f + 10.0f, halfW - tipW * 0.5f - 10.0f);
        const Vector2f tipPos{tipX, -halfH + toolbarH + 22.0f};
        context.Render.DrawRectangle(tipW, 30.0f, Transform2D{tipPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 145});
        context.Render.DrawRectangle(tipW, 30.0f, Transform2D{tipPos}, Color{26, 30, 38, 245});
        context.Render.DrawRectangleOutline(tipW, 30.0f, Transform2D{tipPos}, Color{85, 95, 118, 230}, 1.2f);
        context.Render.DrawTextCentered(hoveredTooltip, m_fontId, 16.0f, Transform2D{tipPos}, Color{235, 240, 248, 255});
    }
}

void EditorScene::RenderPalette(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = kToolbarHeight;
    const float palH = viewSize.Y - toolbarH;

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    if (m_leftPanelOpen)
    {
        const float sideWidth = kLeftPanelWidth;
        Vector2f palCenter{-halfW + sideWidth * 0.5f, -halfH + toolbarH + palH * 0.5f};
        Vector2f palSize{sideWidth, palH};

        context.Render.DrawRectangle(palSize.X, palSize.Y, Transform2D{palCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{-halfW + sideWidth, -halfH + toolbarH},
                                Vector2f{-halfW + sideWidth, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        // Clean Header Title
        Vector2f titlePos{-halfW + 18.0f, -halfH + toolbarH + 22.0f};
        context.Render.DrawText("PALETTE", m_fontId, 20.0f, Transform2D{titlePos}, Color{225, 232, 245, 245});

        // Header Collapse Button
        Vector2f togglePos{-halfW + sideWidth - 22.0f, -halfH + toolbarH + 32.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 15.0f, false /* left */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        context.Render.DrawLine(Vector2f{-halfW + 14.0f, -halfH + toolbarH + 54.0f},
                                Vector2f{-halfW + sideWidth - 14.0f, -halfH + toolbarH + 54.0f},
                                Color{48, 54, 68, 220}, 1.0f);

        const float palStartY = -halfH + toolbarH + 62.0f;
        const float itemH = 62.0f;
        const float cardW = sideWidth - 24.0f;

        // Display ONLY visual preview + object name (no eraser, no descriptions or extra text)
        for (int i = 0; i < 9; ++i)
        {
            const EditorTileType tileType = kPaletteTiles[i];
            const float itemY = palStartY + static_cast<float>(i) * itemH + itemH * 0.5f;
            const Vector2f itemPos{palCenter.X, itemY};
            const bool isHover = (std::abs(worldMouse.X - itemPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - itemPos.Y) <= itemH * 0.45f);
            const bool isSelected = (m_tool == EditorTool::Paint && m_selectedTile == tileType);

            Color bg = isSelected ? Color{38, 52, 76, 255} : (isHover ? Color{30, 36, 50, 255} : Color{25, 29, 39, 255});
            context.Render.DrawRectangle(cardW, itemH - 6.0f, Transform2D{itemPos}, bg);
            context.Render.DrawRectangleOutline(cardW, itemH - 6.0f, Transform2D{itemPos},
                isSelected ? Color{255, 215, 60, 255} : (isHover ? Color{110, 140, 180, 210} : Color{46, 52, 68, 190}),
                isSelected ? 2.0f : 1.0f);

            const EditorCellData preset = m_levelData.GetPalettePreset(tileType);
            const Vector2f iconPos{itemPos.X - cardW * 0.34f, itemPos.Y};
            DrawTilePreview(context, preset, iconPos, 54.0f, 24.0f);

            const Vector2f namePos{itemPos.X - cardW * 0.14f, itemPos.Y - 10.0f};
            context.Render.DrawText(LevelData::GetTileDisplayName(tileType), m_fontId, 19.0f,
                                    Transform2D{namePos}, isSelected ? Color{255, 225, 85, 255} : Color{230, 236, 245, 255});
        }
    }
    else
    {
        const float railW = 44.0f;
        Vector2f railCenter{-halfW + railW * 0.5f, -halfH + toolbarH + palH * 0.5f};
        context.Render.DrawRectangle(railW, palH, Transform2D{railCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{-halfW + railW, -halfH + toolbarH},
                                Vector2f{-halfW + railW, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        Vector2f togglePos{-halfW + railW * 0.5f, -halfH + toolbarH + 32.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 15.0f, true /* right */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        Vector2f previewPos{-halfW + railW * 0.5f, -halfH + toolbarH + 90.0f};
        if (m_tool == EditorTool::Eraser)
        {
            DrawIconEraser(context, previewPos, 24.0f, Colors::White);
        }
        else if (m_tool == EditorTool::Select)
        {
            DrawIconSelect(context, previewPos, 22.0f, Color{80, 230, 255, 255});
        }
        else
        {
            DrawTilePreview(context, m_levelData.GetPalettePreset(m_selectedTile), previewPos, 30.0f, 16.0f);
        }
    }
}

void EditorScene::RenderTabsAndPanels(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = kToolbarHeight;
    const float panelH = viewSize.Y - toolbarH;

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    if (m_rightPanelOpen)
    {
        const float sideWidth = kRightPanelWidth;
        Vector2f panelCenter{halfW - sideWidth * 0.5f, -halfH + toolbarH + panelH * 0.5f};
        Vector2f panelSize{sideWidth, panelH};

        context.Render.DrawRectangle(panelSize.X, panelSize.Y, Transform2D{panelCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{halfW - sideWidth, -halfH + toolbarH},
                                Vector2f{halfW - sideWidth, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        const float tabH = 44.0f;
        const float tabY = -halfH + toolbarH + tabH * 0.5f + 10.0f;

        Vector2f togglePos{halfW - sideWidth + 20.0f, tabY};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 15.0f, true /* right */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        const char* tabNames[5] = {"INSP", "GEN", "ENV", "EVENTS", "RULES"};
        const float tabsStartX = halfW - sideWidth + 42.0f;
        const float tabsAvailW = sideWidth - 52.0f;
        const float tabW = tabsAvailW / 5.0f;

        for (int t = 0; t < 5; ++t)
        {
            float tx = tabsStartX + static_cast<float>(t) * tabW + tabW * 0.5f;
            Vector2f tPos{tx, tabY};
            bool isCurTab = (static_cast<int>(m_activeTab) == t);
            bool isHover = (std::abs(worldMouse.X - tx) <= tabW * 0.5f && std::abs(worldMouse.Y - tabY) <= tabH * 0.5f);

            Color tBg = isCurTab ? Color{42, 54, 76, 255} : (isHover ? Color{32, 38, 52, 255} : Color{25, 29, 39, 255});
            context.Render.DrawRectangle(tabW - 4.0f, tabH - 4.0f, Transform2D{tPos}, tBg);
            context.Render.DrawRectangleOutline(tabW - 4.0f, tabH - 4.0f, Transform2D{tPos}, isCurTab ? Color{255, 215, 60, 255} : Color{50, 56, 72, 200}, isCurTab ? 2.0f : 1.0f);
            context.Render.DrawTextCentered(tabNames[t], m_fontId, 16.0f, Transform2D{tPos}, isCurTab ? Color{255, 220, 75, 255} : Color{190, 205, 225, 220});
        }

        const float cardW = sideWidth - 36.0f;

        switch (m_activeTab)
        {
        case EditorTab::Inspector:
        {
            RenderInspectorTab(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse);
            break;
        }
        case EditorTab::Procedural:
        {
            Vector2f titlePos{panelCenter.X, -halfH + toolbarH + 92.0f};
            context.Render.DrawTextCentered("PROCEDURAL GENERATOR", m_fontId, 22.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

            // 1. Target Brick Count (Y = 150)
            Vector2f cntPos{panelCenter.X, -halfH + toolbarH + 150.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{cntPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{cntPos}, Color{55, 65, 88, 200}, 1.5f);
            context.Render.DrawTextCentered("[-] TARGET BRICKS: " + std::to_string(m_procBrickCount) + " [+]", m_fontId, 20.0f, Transform2D{cntPos}, Colors::White);

            // 2. Density (Y = 230)
            Vector2f diffPos{panelCenter.X, -halfH + toolbarH + 230.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{diffPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{diffPos}, Color{55, 65, 88, 200}, 1.5f);
            int diffPct = static_cast<int>(m_procDifficulty * 100.0f);
            context.Render.DrawTextCentered("[-] DENSITY: " + std::to_string(diffPct) + "% [+]", m_fontId, 20.0f, Transform2D{diffPos}, Colors::White);

            // 3. Symmetry Mode (Y = 310)
            const char* symNames[4] = {"NONE", "HORIZONTAL", "VERTICAL", "DUAL (4X)"};
            Vector2f symPos{panelCenter.X, -halfH + toolbarH + 310.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{symPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{symPos}, Color{55, 65, 88, 200}, 1.5f);
            context.Render.DrawTextCentered("SYMMETRY: " + std::string(symNames[m_procSymmetry]), m_fontId, 20.0f, Transform2D{symPos}, Color{255, 195, 60, 255});

            // 4. Generate Button (Y = 410)
            Vector2f genPos{panelCenter.X, -halfH + toolbarH + 410.0f};
            bool genHover = (std::abs(worldMouse.X - genPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - genPos.Y) <= 34.0f);
            context.Render.DrawRectangle(cardW, 64.0f, Transform2D{genPos}, genHover ? Color{255, 175, 40, 255} : Color{215, 140, 30, 255});
            context.Render.DrawRectangleOutline(cardW, 64.0f, Transform2D{genPos}, Colors::White, 2.0f);
            context.Render.DrawTextCentered("GENERATE LEVEL", m_fontId, 22.0f, Transform2D{genPos}, Colors::White);
            break;
        }
        case EditorTab::ThemeLighting:
        {
            RenderThemeTab(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse);
            break;
        }
        case EditorTab::EventQueue:
        {
            RenderEventQueueTab(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse);
            break;
        }
        case EditorTab::Gamerules:
        {
            Vector2f titlePos{panelCenter.X, -halfH + toolbarH + 92.0f};
            context.Render.DrawTextCentered("GAME RULES", m_fontId, 22.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

            const bool manualOverride = context.Rules.GetRule(Rule::Gameplay::ManualOverride);
            Vector2f modePos{panelCenter.X, -halfH + toolbarH + 136.0f};
            if (manualOverride)
            {
                context.Render.DrawTextCentered("MODE: MANUAL (SETTINGS)", m_fontId, 18.0f, Transform2D{modePos}, Color{255, 160, 60, 255});
            }
            else
            {
                context.Render.DrawTextCentered("MODE: LEVEL RULES", m_fontId, 18.0f, Transform2D{modePos}, Color{80, 240, 140, 255});
            }

            const auto& gr = m_levelData.GetGamerules();

            // Y = 200
            Vector2f hzPos{panelCenter.X, -halfH + toolbarH + 200.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{hzPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{hzPos}, gr.HazardZones ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("HAZARD ZONES: ") + (gr.HazardZones ? "[ON]" : "[OFF]"), m_fontId, 20.0f, Transform2D{hzPos}, Colors::White);

            // Y = 280
            Vector2f arPos{panelCenter.X, -halfH + toolbarH + 280.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{arPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{arPos}, gr.ArenaEvents ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("ARENA EVENTS: ") + (gr.ArenaEvents ? "[ON]" : "[OFF]"), m_fontId, 20.0f, Transform2D{arPos}, Colors::White);

            // Y = 360
            Vector2f lfPos{panelCenter.X, -halfH + toolbarH + 360.0f};
            context.Render.DrawRectangle(cardW, 56.0f, Transform2D{lfPos}, Color{28, 32, 44, 255});
            context.Render.DrawRectangleOutline(cardW, 56.0f, Transform2D{lfPos}, gr.InfiniteLives ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("LIVES: ") + (gr.InfiniteLives ? "INFINITE" : "STANDARD (3)"), m_fontId, 20.0f, Transform2D{lfPos}, Colors::White);
            break;
        }
        }
    }
    else
    {
        const float railW = 44.0f;
        Vector2f railCenter{halfW - railW * 0.5f, -halfH + toolbarH + panelH * 0.5f};
        context.Render.DrawRectangle(railW, panelH, Transform2D{railCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{halfW - railW, -halfH + toolbarH},
                                Vector2f{halfW - railW, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        Vector2f togglePos{halfW - railW * 0.5f, -halfH + toolbarH + 32.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(28.0f, 28.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 15.0f, false /* left */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        const char* tabIcons[5] = {"I", "G", "E", "Q", "R"};
        for (int t = 0; t < 5; ++t)
        {
            Vector2f tPos{halfW - railW * 0.5f, -halfH + toolbarH + 90.0f + static_cast<float>(t) * 46.0f};
            bool isCur = (static_cast<int>(m_activeTab) == t);
            bool isHover = (std::abs(worldMouse.X - tPos.X) <= 16.0f && std::abs(worldMouse.Y - tPos.Y) <= 16.0f);

            Color bg = isCur ? Color{42, 54, 76, 255} : (isHover ? Color{32, 38, 52, 255} : Color{25, 29, 39, 255});
            context.Render.DrawRectangle(32.0f, 32.0f, Transform2D{tPos}, bg);
            context.Render.DrawRectangleOutline(32.0f, 32.0f, Transform2D{tPos}, isCur ? Color{255, 215, 60, 255} : (isHover ? Colors::White : Color{50, 56, 72, 200}), isCur ? 2.0f : 1.0f);
            context.Render.DrawTextCentered(tabIcons[t], m_fontId, 18.0f, Transform2D{tPos}, isCur ? Color{255, 220, 75, 255} : Color{190, 205, 225, 220});
        }
    }
}

void EditorScene::RenderInspectorTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse)
{
    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 32.0f;
    const float leftX = panelCenter.X - cardW * 0.5f;
    const float rightX = panelCenter.X + cardW * 0.5f;
    const float ctrlW = 196.0f;
    const float ctrlCenterX = rightX - ctrlW * 0.5f;
    const EditorCellData target = GetInspectorTargetData();
    const bool editingCell = (m_hasSelectedCell && m_levelData.GetCell(m_selectedCol, m_selectedRow).Type != EditorTileType::Empty);

    // Unity-style object header: visual icon + object name ONLY (no generic title or subtitle)
    const Vector2f headerPos{panelCenter.X, top + 92.0f};
    context.Render.DrawRectangle(cardW, 48.0f, Transform2D{headerPos}, Color{30, 35, 48, 255});
    context.Render.DrawRectangleOutline(cardW, 48.0f, Transform2D{headerPos}, Color{62, 74, 98, 220}, 1.3f);
    DrawTilePreview(context, target, Vector2f{leftX + 34.0f, headerPos.Y}, 48.0f, 22.0f);
    context.Render.DrawText(LevelData::GetTileDisplayName(target.Type), m_fontId, 20.0f,
                            Transform2D{Vector2f{leftX + 70.0f, headerPos.Y - 10.0f}}, Colors::White);
    if (editingCell)
    {
        const std::string coordTag = "[" + std::to_string(m_selectedCol) + "," + std::to_string(m_selectedRow) + "]";
        context.Render.DrawTextCentered(coordTag, m_fontId, 14.0f,
                                        Transform2D{Vector2f{rightX - 28.0f, headerPos.Y}}, Color{130, 175, 220, 210});
    }

    auto drawSection = [&](float y, const char* title) {
        context.Render.DrawRectangle(cardW, 24.0f, Transform2D{Vector2f{panelCenter.X, y}}, Color{26, 30, 41, 255});
        context.Render.DrawLine(Vector2f{leftX, y + 12.0f}, Vector2f{rightX, y + 12.0f}, Color{52, 60, 78, 220}, 1.0f);
        context.Render.DrawText(title, m_fontId, 14.0f, Transform2D{Vector2f{leftX + 6.0f, y - 7.0f}}, Color{150, 185, 230, 235});
    };

    auto drawFieldButton = [&](float y, const char* label, const std::string& value, Color borderCol, Color valCol) {
        context.Render.DrawText(label, m_fontId, 16.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, Color{200, 210, 228, 240});
        const Vector2f boxPos{ctrlCenterX, y};
        const bool hov = (std::abs(worldMouse.X - boxPos.X) <= ctrlW * 0.5f && std::abs(worldMouse.Y - boxPos.Y) <= 16.0f);
        context.Render.DrawRectangle(ctrlW, 32.0f, Transform2D{boxPos}, hov ? Color{40, 48, 66, 255} : Color{28, 33, 45, 255});
        context.Render.DrawRectangleOutline(ctrlW, 32.0f, Transform2D{boxPos}, hov ? Colors::White : borderCol, hov ? 1.6f : 1.1f);
        context.Render.DrawTextCentered(value, m_fontId, 16.0f, Transform2D{boxPos}, valCol);
    };

    auto drawStepper = [&](float y, const char* label, const std::string& value, Color valCol = Colors::White) {
        context.Render.DrawText(label, m_fontId, 16.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, Color{200, 210, 228, 240});
        const Vector2f boxPos{ctrlCenterX, y};
        const bool hov = (std::abs(worldMouse.X - boxPos.X) <= ctrlW * 0.5f && std::abs(worldMouse.Y - boxPos.Y) <= 16.0f);
        context.Render.DrawRectangle(ctrlW, 32.0f, Transform2D{boxPos}, Color{26, 30, 42, 255});
        context.Render.DrawRectangleOutline(ctrlW, 32.0f, Transform2D{boxPos}, hov ? Color{130, 165, 220, 230} : Color{56, 65, 86, 210}, 1.1f);

        const Vector2f minusPos{ctrlCenterX - ctrlW * 0.5f + 16.0f, y};
        const Vector2f plusPos{ctrlCenterX + ctrlW * 0.5f - 16.0f, y};
        const bool minHov = (std::abs(worldMouse.X - minusPos.X) <= 15.0f && std::abs(worldMouse.Y - y) <= 15.0f);
        const bool plsHov = (std::abs(worldMouse.X - plusPos.X) <= 15.0f && std::abs(worldMouse.Y - y) <= 15.0f);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{minusPos}, minHov ? Color{55, 68, 92, 255} : Color{36, 42, 56, 255});
        context.Render.DrawTextCentered("-", m_fontId, 18.0f, Transform2D{minusPos}, Colors::White);
        context.Render.DrawRectangle(28.0f, 28.0f, Transform2D{plusPos}, plsHov ? Color{55, 68, 92, 255} : Color{36, 42, 56, 255});
        context.Render.DrawTextCentered("+", m_fontId, 18.0f, Transform2D{plusPos}, Colors::White);
        context.Render.DrawTextCentered(value, m_fontId, 16.0f, Transform2D{boxPos}, valCol);
    };

    auto drawSwatches = [&](float y, const char* label, const Color (&palette)[8], Color currentColor, bool dimmed) {
        context.Render.DrawText(label, m_fontId, 16.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, dimmed ? Color{120, 130, 148, 170} : Color{200, 210, 228, 240});
        const float swSize = 21.0f;
        const float swGap = 4.0f;
        const float startX = ctrlCenterX - ctrlW * 0.5f + swSize * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            const Vector2f sPos{startX + static_cast<float>(i) * (swSize + swGap), y};
            const bool selected = !dimmed && ColorsApproxEqual(currentColor, palette[i]);
            const bool hov = !dimmed && (std::abs(worldMouse.X - sPos.X) <= swSize * 0.5f && std::abs(worldMouse.Y - sPos.Y) <= swSize * 0.5f);
            Color c = palette[i];
            if (dimmed) c.a = 90;
            context.Render.DrawRectangle(swSize, swSize, Transform2D{sPos}, c);
            context.Render.DrawRectangleOutline(swSize, swSize, Transform2D{sPos},
                selected ? Colors::White : (hov ? Color{255, 225, 95, 255} : Color{20, 24, 34, 220}),
                selected ? 2.4f : 1.0f);
        }
    };

    auto drawHitPointStrip = [&](float y, int maxHp) {
        if (maxHp == 9)
        {
            const float btnW = 36.0f;
            const float btnH = 32.0f;
            const float gap = 4.0f;
            const float totalW = 9.0f * btnW + 8.0f * gap;
            const float startX = panelCenter.X - totalW * 0.5f + btnW * 0.5f;
            for (int hp = 1; hp <= 9; ++hp)
            {
                const Vector2f bPos{startX + static_cast<float>(hp - 1) * (btnW + gap), y};
                const bool isCur = (target.HitPoints == hp);
                const bool isHov = (std::abs(worldMouse.X - bPos.X) <= btnW * 0.5f && std::abs(worldMouse.Y - bPos.Y) <= btnH * 0.5f);
                context.Render.DrawRectangle(btnW, btnH, Transform2D{bPos}, LevelData::GetColorForHitPoints(hp));
                context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{bPos},
                    isCur ? Colors::White : (isHov ? Color{255, 230, 120, 255} : Color{20, 24, 34, 220}),
                    isCur ? 2.6f : 1.1f);
                context.Render.DrawTextCentered(std::to_string(hp), m_fontId, 18.0f, Transform2D{bPos + Vector2f{1.0f, 1.0f}}, Color{0, 0, 0, 190});
                context.Render.DrawTextCentered(std::to_string(hp), m_fontId, 18.0f, Transform2D{bPos}, Color{15, 18, 25, 255});
            }
        }
        else
        {
            const float btnW = 60.0f;
            const float btnH = 32.0f;
            const float gap = 8.0f;
            const float startX = ctrlCenterX - ctrlW * 0.5f + btnW * 0.5f;
            context.Render.DrawText("Hit Points", m_fontId, 16.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, Color{200, 210, 228, 240});
            for (int hp = 1; hp <= maxHp; ++hp)
            {
                const Vector2f bPos{startX + static_cast<float>(hp - 1) * (btnW + gap), y};
                const bool isCur = (target.HitPoints == hp);
                const bool isHov = (std::abs(worldMouse.X - bPos.X) <= btnW * 0.5f && std::abs(worldMouse.Y - bPos.Y) <= btnH * 0.5f);
                context.Render.DrawRectangle(btnW, btnH, Transform2D{bPos}, isCur ? Color{55, 76, 112, 255} : (isHov ? Color{40, 48, 66, 255} : Color{28, 33, 45, 255}));
                context.Render.DrawRectangleOutline(btnW, btnH, Transform2D{bPos}, isCur ? Color{255, 215, 65, 255} : Color{60, 70, 92, 210}, isCur ? 2.0f : 1.1f);
                context.Render.DrawTextCentered(std::to_string(hp) + " HP", m_fontId, 15.0f, Transform2D{bPos}, isCur ? Color{255, 225, 90, 255} : Colors::White);
            }
        }
    };

    switch (target.Type)
    {
    case EditorTileType::Light:
    case EditorTileType::Medium:
    case EditorTileType::Hard:
        drawSection(top + 136.0f, "DURABILITY (1 - 9 HP)");
        drawHitPointStrip(top + 172.0f, 9);
        drawSection(top + 220.0f, "APPEARANCE");
        drawFieldButton(top + 258.0f, "Color Mode", target.UseCustomColor ? "Custom Tint" : "HP Auto",
                        target.UseCustomColor ? Color{95, 215, 255, 220} : Color{65, 78, 102, 210},
                        target.UseCustomColor ? Color{140, 235, 255, 255} : Colors::White);
        drawSwatches(top + 304.0f, "Tint", kAccentSwatches, target.CustomColor, !target.UseCustomColor);
        break;

    case EditorTileType::Special:
        drawSection(top + 136.0f, "DURABILITY (1 - 9 HP)");
        drawHitPointStrip(top + 172.0f, 9);
        drawSection(top + 220.0f, "PAYLOAD");
        drawFieldButton(top + 258.0f, "Selection", target.RandomPowerUp ? "Random" : "Fixed",
                        target.RandomPowerUp ? Color{220, 95, 255, 220} : Color{90, 225, 165, 220},
                        target.RandomPowerUp ? Color{240, 175, 255, 255} : Color{165, 255, 210, 255});
        drawFieldButton(top + 302.0f, "Power-Up", "< " + LevelData::GetPowerUpDisplayName(target.SpecificPowerUp) + " >",
                        Color{75, 95, 130, 210}, target.RandomPowerUp ? Color{145, 155, 175, 200} : Color{255, 225, 95, 255});
        drawSection(top + 350.0f, "RESPAWN");
        drawFieldButton(top + 388.0f, "Auto Respawn", target.PowerUpRegenEnabled ? "ON" : "OFF",
                        target.PowerUpRegenEnabled ? Color{85, 225, 140, 210} : Color{215, 75, 90, 210},
                        target.PowerUpRegenEnabled ? Color{165, 255, 200, 255} : Color{255, 165, 175, 255});
        drawStepper(top + 432.0f, "Cooldown", std::to_string(static_cast<int>(target.PowerUpRegenCooldown)) + "s",
                    target.PowerUpRegenEnabled ? Colors::White : Color{130, 140, 160, 180});
        break;

    case EditorTileType::Event:
    {
        drawSection(top + 136.0f, "DURABILITY (1 - 9 HP)");
        drawHitPointStrip(top + 172.0f, 9);
        drawSection(top + 220.0f, "EVENT");
        drawFieldButton(top + 258.0f, "Selection", target.RandomEvent ? "Random" : "Fixed",
                        target.RandomEvent ? Color{65, 225, 240, 220} : Color{255, 200, 65, 220},
                        target.RandomEvent ? Color{155, 245, 255, 255} : Color{255, 230, 120, 255});
        const auto* desc = FindArenaEventDescriptor(target.SpecificEvent);
        const std::string evName = desc ? desc->DisplayName : "Meteor Shower";
        drawFieldButton(top + 302.0f, "Event Type", "< " + evName + " >",
                        Color{75, 95, 130, 210}, target.RandomEvent ? Color{145, 155, 175, 200} : GetArenaEventColor(target.SpecificEvent));
        drawFieldButton(top + 346.0f, "Trigger", target.EventTriggerOnHit ? "On Hit" : "On Break",
                        Color{80, 155, 235, 215}, Color{190, 230, 255, 255});
        break;
    }

    case EditorTileType::Metal:
        drawSection(top + 136.0f, "PHYSICS");
        drawStepper(top + 174.0f, "Bounce Boost", FormatOneDecimal(target.MetalBounceBoost) + "x", Color{175, 225, 255, 255});
        break;

    case EditorTileType::Bomb:
        drawSection(top + 136.0f, "DURABILITY");
        drawHitPointStrip(top + 174.0f, 3);
        drawSection(top + 224.0f, "EXPLOSION");
        drawFieldButton(top + 262.0f, "Blast Radius", std::string("< ") + LevelData::GetBombBlastLabel(target.BombBlastTier) + " >",
                        Color{255, 135, 65, 220}, Color{255, 195, 115, 255});
        break;

    case EditorTileType::Turret:
        drawSection(top + 136.0f, "DURABILITY (1 - 9 HP)");
        drawHitPointStrip(top + 172.0f, 9);
        drawSection(top + 220.0f, "COMBAT");
        drawStepper(top + 258.0f, "Fire Rate", FormatOneDecimal(target.TurretCooldown) + "s");
        drawStepper(top + 302.0f, "First Shot", FormatOneDecimal(target.TurretInitialDelay) + "s");
        drawStepper(top + 346.0f, "Shot Speed", std::to_string(static_cast<int>(target.TurretProjectileSpeed)) + " px/s");
        drawStepper(top + 390.0f, "Hull Damage", std::to_string(static_cast<int>(target.TurretDamage)));
        drawFieldButton(top + 434.0f, "Targeting", target.TurretAimed ? "Aimed" : "Straight",
                        target.TurretAimed ? Color{255, 95, 105, 220} : Color{75, 105, 145, 210},
                        target.TurretAimed ? Color{255, 180, 185, 255} : Color{195, 225, 255, 255});
        break;

    case EditorTileType::Lantern:
        drawSection(top + 136.0f, "DURABILITY (1 - 9 HP)");
        drawHitPointStrip(top + 172.0f, 9);
        drawSection(top + 220.0f, "LIGHT SOURCE");
        drawSwatches(top + 258.0f, "Light Color", kLanternSwatches, target.LanternLightColor, false);
        drawStepper(top + 304.0f, "Radius", std::to_string(static_cast<int>(target.LanternLightRadius)) + " px");
        drawStepper(top + 348.0f, "Intensity", FormatTwoDecimals(target.LanternLightIntensity) + "x");
        break;

    default:
        break;
    }

    // Compact Reset Defaults Button at bottom
    const Vector2f resetPos{panelCenter.X, halfH - 46.0f};
    const bool resetHov = (std::abs(worldMouse.X - resetPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - resetPos.Y) <= 20.0f);
    context.Render.DrawRectangle(cardW, 40.0f, Transform2D{resetPos}, resetHov ? Color{68, 34, 44, 255} : Color{36, 26, 34, 255});
    context.Render.DrawRectangleOutline(cardW, 40.0f, Transform2D{resetPos}, resetHov ? Color{255, 120, 135, 255} : Color{155, 70, 82, 210}, 1.3f);
    context.Render.DrawTextCentered("RESET DEFAULTS", m_fontId, 16.0f, Transform2D{resetPos}, resetHov ? Colors::White : Color{240, 185, 192, 240});
}

bool EditorScene::HandleInspectorTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick)
{
    if (!leftClick) return false;

    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 32.0f;
    const float rightX = panelCenter.X + cardW * 0.5f;
    const float ctrlW = 196.0f;
    const float ctrlCenterX = rightX - ctrlW * 0.5f;
    EditorCellData target = GetInspectorTargetData();

    auto hitCtrl = [&](float y) {
        return std::abs(worldMouse.X - ctrlCenterX) <= ctrlW * 0.5f && std::abs(worldMouse.Y - y) <= 16.0f;
    };

    auto hitStepperDelta = [&](float y, int& outDir) {
        if (!hitCtrl(y)) return false;
        outDir = (worldMouse.X < ctrlCenterX) ? -1 : 1;
        return true;
    };

    auto hitSwatches = [&](float y, const Color (&palette)[8], Color& outColor) {
        const float swSize = 21.0f;
        const float swGap = 4.0f;
        const float startX = ctrlCenterX - ctrlW * 0.5f + swSize * 0.5f;
        if (std::abs(worldMouse.Y - y) > swSize * 0.55f) return false;
        for (int i = 0; i < 8; ++i)
        {
            const float sx = startX + static_cast<float>(i) * (swSize + swGap);
            if (std::abs(worldMouse.X - sx) <= swSize * 0.5f)
            {
                outColor = palette[i];
                return true;
            }
        }
        return false;
    };

    auto handleHpStrip9 = [&](float y) -> bool {
        const float btnW = 36.0f;
        const float btnH = 32.0f;
        const float gap = 4.0f;
        const float totalW = 9.0f * btnW + 8.0f * gap;
        const float startX = panelCenter.X - totalW * 0.5f + btnW * 0.5f;
        if (std::abs(worldMouse.Y - y) > btnH * 0.55f) return false;
        for (int hp = 1; hp <= 9; ++hp)
        {
            const float bx = startX + static_cast<float>(hp - 1) * (btnW + gap);
            if (std::abs(worldMouse.X - bx) <= btnW * 0.5f)
            {
                target.HitPoints = hp;
                ApplyInspectorTargetData(target);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                return true;
            }
        }
        return false;
    };

    int dir = 0;
    switch (target.Type)
    {
    case EditorTileType::Light:
    case EditorTileType::Medium:
    case EditorTileType::Hard:
        if (handleHpStrip9(top + 172.0f)) return true;
        if (hitCtrl(top + 258.0f))
        {
            target.UseCustomColor = !target.UseCustomColor;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitSwatches(top + 304.0f, kAccentSwatches, target.CustomColor))
        {
            target.UseCustomColor = true;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    case EditorTileType::Special:
        if (handleHpStrip9(top + 172.0f)) return true;
        if (hitCtrl(top + 258.0f))
        {
            target.RandomPowerUp = !target.RandomPowerUp;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitCtrl(top + 302.0f))
        {
            const auto& list = LevelData::GetSelectablePowerUps();
            if (!list.empty())
            {
                size_t curIdx = 0;
                for (size_t i = 0; i < list.size(); ++i)
                {
                    if (list[i] == target.SpecificPowerUp) { curIdx = i; break; }
                }
                curIdx = (worldMouse.X < ctrlCenterX)
                    ? ((curIdx == 0) ? (list.size() - 1) : (curIdx - 1))
                    : ((curIdx + 1) % list.size());
                target.RandomPowerUp = false;
                target.SpecificPowerUp = list[curIdx];
                ApplyInspectorTargetData(target);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            }
            return true;
        }
        if (hitCtrl(top + 388.0f))
        {
            target.PowerUpRegenEnabled = !target.PowerUpRegenEnabled;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 432.0f, dir))
        {
            target.PowerUpRegenCooldown = std::clamp(target.PowerUpRegenCooldown + static_cast<float>(dir) * 5.0f, 10.0f, 60.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    case EditorTileType::Event:
        if (handleHpStrip9(top + 172.0f)) return true;
        if (hitCtrl(top + 258.0f))
        {
            target.RandomEvent = !target.RandomEvent;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitCtrl(top + 302.0f))
        {
            const auto& catalog = GetArenaEventCatalog();
            if (!catalog.empty())
            {
                size_t curIdx = 0;
                for (size_t i = 0; i < catalog.size(); ++i)
                {
                    if (catalog[i].Type == target.SpecificEvent) { curIdx = i; break; }
                }
                curIdx = (worldMouse.X < ctrlCenterX)
                    ? ((curIdx == 0) ? (catalog.size() - 1) : (curIdx - 1))
                    : ((curIdx + 1) % catalog.size());
                target.RandomEvent = false;
                target.SpecificEvent = catalog[curIdx].Type;
                ApplyInspectorTargetData(target);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            }
            return true;
        }
        if (hitCtrl(top + 346.0f))
        {
            target.EventTriggerOnHit = !target.EventTriggerOnHit;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    case EditorTileType::Metal:
        if (hitStepperDelta(top + 174.0f, dir))
        {
            target.MetalBounceBoost = std::clamp(target.MetalBounceBoost + static_cast<float>(dir) * 0.1f, 1.0f, 1.4f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    case EditorTileType::Bomb:
    {
        const float btnW = 60.0f;
        const float btnH = 32.0f;
        const float gap = 8.0f;
        const float startX = ctrlCenterX - ctrlW * 0.5f + btnW * 0.5f;
        if (std::abs(worldMouse.Y - (top + 174.0f)) <= btnH * 0.55f)
        {
            for (int hp = 1; hp <= 3; ++hp)
            {
                const float bx = startX + static_cast<float>(hp - 1) * (btnW + gap);
                if (std::abs(worldMouse.X - bx) <= btnW * 0.5f)
                {
                    target.HitPoints = hp;
                    ApplyInspectorTargetData(target);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                    return true;
                }
            }
        }
        if (hitStepperDelta(top + 262.0f, dir))
        {
            target.BombBlastTier = (target.BombBlastTier + (dir > 0 ? 1 : 2)) % 3;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;
    }

    case EditorTileType::Turret:
        if (handleHpStrip9(top + 172.0f)) return true;
        if (hitStepperDelta(top + 258.0f, dir))
        {
            target.TurretCooldown = std::clamp(target.TurretCooldown + static_cast<float>(dir) * 0.2f, 0.5f, 10.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 302.0f, dir))
        {
            target.TurretInitialDelay = std::clamp(target.TurretInitialDelay + static_cast<float>(dir) * 0.5f, 0.5f, 12.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 346.0f, dir))
        {
            target.TurretProjectileSpeed = std::clamp(target.TurretProjectileSpeed + static_cast<float>(dir) * 20.0f, 100.0f, 450.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 390.0f, dir))
        {
            target.TurretDamage = std::clamp(target.TurretDamage + static_cast<float>(dir) * 5.0f, 15.0f, 50.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitCtrl(top + 434.0f))
        {
            target.TurretAimed = !target.TurretAimed;
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    case EditorTileType::Lantern:
        if (handleHpStrip9(top + 172.0f)) return true;
        if (hitSwatches(top + 258.0f, kLanternSwatches, target.LanternLightColor))
        {
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 304.0f, dir))
        {
            target.LanternLightRadius = std::clamp(target.LanternLightRadius + static_cast<float>(dir) * 20.0f, 120.0f, 420.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        if (hitStepperDelta(top + 348.0f, dir))
        {
            target.LanternLightIntensity = std::clamp(target.LanternLightIntensity + static_cast<float>(dir) * 0.15f, 0.5f, 2.5f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
        break;

    default:
        break;
    }

    // Reset Defaults Button
    const Vector2f resetPos{panelCenter.X, halfH - 46.0f};
    if (std::abs(worldMouse.X - resetPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - resetPos.Y) <= 20.0f)
    {
        EditorCellData def = EditorCellData::CreateDefaultForType(target.Type);
        ApplyInspectorTargetData(def);
        ShowToast("Defaults restored", Color{140, 210, 255, 255}, 1.4f);
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        return true;
    }

    return false;
}

void EditorScene::RenderThemeTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse)
{
    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 32.0f;
    const float leftX = panelCenter.X - cardW * 0.5f;
    const float rightX = panelCenter.X + cardW * 0.5f;
    const float ctrlW = 196.0f;
    const float ctrlCenterX = rightX - ctrlW * 0.5f;
    const EditorThemeData& thm = m_levelData.GetTheme();

    auto drawSection = [&](float y, const char* title) {
        context.Render.DrawRectangle(cardW, 22.0f, Transform2D{Vector2f{panelCenter.X, y}}, Color{26, 30, 41, 255});
        context.Render.DrawLine(Vector2f{leftX, y + 11.0f}, Vector2f{rightX, y + 11.0f}, Color{52, 60, 78, 220}, 1.0f);
        context.Render.DrawText(title, m_fontId, 13.5f, Transform2D{Vector2f{leftX + 6.0f, y - 7.0f}}, Color{150, 185, 230, 235});
    };

    auto drawFullSelector = [&](float y, const std::string& text, Color accentCol) {
        const Vector2f pos{panelCenter.X, y};
        const bool hov = (std::abs(worldMouse.X - pos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - y) <= 16.0f);
        context.Render.DrawRectangle(cardW, 32.0f, Transform2D{pos}, hov ? Color{38, 46, 64, 255} : Color{28, 33, 45, 255});
        context.Render.DrawRectangleOutline(cardW, 32.0f, Transform2D{pos}, hov ? Colors::White : Color{58, 68, 90, 210}, 1.2f);
        context.Render.DrawTextCentered(text, m_fontId, 16.0f, Transform2D{pos}, accentCol);
    };

    auto drawSwatches = [&](float y, const char* label, const Color (&palette)[8], Color currentColor) {
        context.Render.DrawText(label, m_fontId, 15.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, Color{200, 210, 228, 240});
        const float swSize = 21.0f;
        const float swGap = 4.0f;
        const float startX = ctrlCenterX - ctrlW * 0.5f + swSize * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            const Vector2f sPos{startX + static_cast<float>(i) * (swSize + swGap), y};
            const bool selected = ColorsApproxEqual(currentColor, palette[i]);
            const bool hov = (std::abs(worldMouse.X - sPos.X) <= swSize * 0.5f && std::abs(worldMouse.Y - sPos.Y) <= swSize * 0.5f);
            context.Render.DrawRectangle(swSize, swSize, Transform2D{sPos}, palette[i]);
            context.Render.DrawRectangleOutline(swSize, swSize, Transform2D{sPos},
                selected ? Colors::White : (hov ? Color{255, 225, 95, 255} : Color{20, 24, 34, 220}),
                selected ? 2.4f : 1.0f);
        }
    };

    auto drawStepper = [&](float y, const char* label, const std::string& value) {
        context.Render.DrawText(label, m_fontId, 15.0f, Transform2D{Vector2f{leftX + 6.0f, y - 8.0f}}, Color{200, 210, 228, 240});
        const Vector2f boxPos{ctrlCenterX, y};
        const bool hov = (std::abs(worldMouse.X - boxPos.X) <= ctrlW * 0.5f && std::abs(worldMouse.Y - y) <= 15.0f);
        context.Render.DrawRectangle(ctrlW, 30.0f, Transform2D{boxPos}, Color{26, 30, 42, 255});
        context.Render.DrawRectangleOutline(ctrlW, 30.0f, Transform2D{boxPos}, hov ? Color{130, 165, 220, 230} : Color{56, 65, 86, 210}, 1.1f);
        const Vector2f minusPos{ctrlCenterX - ctrlW * 0.5f + 15.0f, y};
        const Vector2f plusPos{ctrlCenterX + ctrlW * 0.5f - 15.0f, y};
        context.Render.DrawRectangle(26.0f, 26.0f, Transform2D{minusPos}, Color{36, 42, 56, 255});
        context.Render.DrawTextCentered("-", m_fontId, 17.0f, Transform2D{minusPos}, Colors::White);
        context.Render.DrawRectangle(26.0f, 26.0f, Transform2D{plusPos}, Color{36, 42, 56, 255});
        context.Render.DrawTextCentered("+", m_fontId, 17.0f, Transform2D{plusPos}, Colors::White);
        context.Render.DrawTextCentered(value, m_fontId, 15.0f, Transform2D{boxPos}, Colors::White);
    };

    // 0. Lighting Mode Presets (reuses the 3 Debug Panel lighting presets directly)
    drawSection(top + 84.0f, "LIGHTING MODE");
    const LightingMode lightingModes[3] = {
        LightingMode::Lit,
        LightingMode::Atmospheric,
        LightingMode::PitchBlack
    };
    const char* lightingLabels[3] = {"LIT", "ATMOSPHERIC", "PITCH BLACK"};
    const Color lightingAccents[3] = {
        Color{255, 225, 115, 255},
        Color{95, 210, 255, 255},
        Color{190, 120, 255, 255}
    };
    const float modeGap = 6.0f;
    const float modeW = (cardW - modeGap * 2.0f) / 3.0f;
    const float modeY = top + 118.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float mx = leftX + modeW * 0.5f + static_cast<float>(i) * (modeW + modeGap);
        const Vector2f mPos{mx, modeY};
        const bool selected = (thm.Lighting == lightingModes[i]);
        const bool hov = (std::abs(worldMouse.X - mx) <= modeW * 0.5f && std::abs(worldMouse.Y - modeY) <= 17.0f);
        const Color bg = selected
            ? Color{40, 56, 84, 255}
            : (hov ? Color{36, 44, 62, 255} : Color{25, 29, 40, 255});
        context.Render.DrawRectangle(modeW, 34.0f, Transform2D{mPos}, bg);
        if (selected)
        {
            context.Render.DrawRectangle(modeW - 4.0f, 3.0f, Transform2D{Vector2f{mx, modeY + 14.5f}}, lightingAccents[i]);
        }
        context.Render.DrawRectangleOutline(modeW, 34.0f, Transform2D{mPos},
            selected ? lightingAccents[i] : (hov ? Colors::White : Color{56, 66, 88, 210}),
            selected ? 2.0f : 1.1f);
        context.Render.DrawTextCentered(lightingLabels[i], m_fontId, 13.5f, Transform2D{mPos},
            selected ? lightingAccents[i] : (hov ? Colors::White : Color{185, 198, 220, 230}));
    }

    // 1. Built-in Environment Presets
    drawSection(top + 160.0f, "ENVIRONMENT PRESETS");
    const std::string presetLabel = (m_selectedCustomThemeIdx >= 0)
        ? ("< Preset: " + LevelData::CreatePresetTheme(thm.Preset).Name + " >")
        : ("< " + thm.Name + " >");
    drawFullSelector(top + 192.0f, presetLabel, Color{255, 215, 75, 255});

    // 2. Custom Reusable Themes (Create / Save / Rename / Delete)
    drawSection(top + 232.0f, "CUSTOM THEMES");
    const auto& customThemes = m_themeLibrary.GetThemes();
    std::string customLabel;
    Color customCol{165, 185, 210, 225};
    if (customThemes.empty())
    {
        customLabel = "No custom themes saved";
    }
    else if (m_selectedCustomThemeIdx >= 0 && m_selectedCustomThemeIdx < static_cast<int>(customThemes.size()))
    {
        customLabel = "< " + customThemes[m_selectedCustomThemeIdx].Name + " >";
        customCol = Color{110, 235, 255, 255};
    }
    else
    {
        customLabel = "< Select Custom Theme (" + std::to_string(customThemes.size()) + ") >";
        customCol = Color{175, 215, 245, 240};
    }
    drawFullSelector(top + 264.0f, customLabel, customCol);

    const float actGap = 6.0f;
    const float actW = (cardW - actGap * 3.0f) / 4.0f;
    const float actY = top + 302.0f;
    const bool hasCustomSel = (m_selectedCustomThemeIdx >= 0 && m_selectedCustomThemeIdx < static_cast<int>(customThemes.size()));
    const char* actLabels[4] = {"NEW", "SAVE", "RENAME", "DELETE"};
    const bool actEnabled[4] = {true, true, hasCustomSel, hasCustomSel};
    const Color actBorders[4] = {
        Color{85, 205, 130, 220},
        Color{80, 175, 245, 220},
        Color{230, 185, 75, 220},
        Color{220, 75, 90, 220}
    };
    for (int i = 0; i < 4; ++i)
    {
        const float ax = leftX + actW * 0.5f + static_cast<float>(i) * (actW + actGap);
        const Vector2f aPos{ax, actY};
        const bool hov = actEnabled[i] && (std::abs(worldMouse.X - ax) <= actW * 0.5f && std::abs(worldMouse.Y - actY) <= 15.0f);
        context.Render.DrawRectangle(actW, 30.0f, Transform2D{aPos},
            !actEnabled[i] ? Color{24, 27, 35, 200} : (hov ? Color{44, 54, 72, 255} : Color{30, 35, 48, 255}));
        context.Render.DrawRectangleOutline(actW, 30.0f, Transform2D{aPos},
            !actEnabled[i] ? Color{46, 52, 66, 150} : (hov ? Colors::White : actBorders[i]), 1.2f);
        context.Render.DrawTextCentered(actLabels[i], m_fontId, 14.0f, Transform2D{aPos},
            actEnabled[i] ? Colors::White : Color{95, 102, 118, 160});
    }

    // 3. Expanded Lighting Controls
    drawSection(top + 342.0f, "LIGHTING & ATMOSPHERE");
    drawSwatches(top + 374.0f, "Background", kBgDarkSwatches, thm.BgColor);
    drawSwatches(top + 410.0f, "Secondary Bg", kBgSecondarySwatches, thm.BgSecondaryColor);
    drawSwatches(top + 446.0f, "Ambient Tint", kAmbientSwatches, thm.AmbientColor);
    drawStepper(top + 482.0f, "Ambient", std::to_string(static_cast<int>(std::round(thm.AmbientIntensity * 100.0f))) + "%");
    drawSwatches(top + 518.0f, "Paddle Light", kAccentSwatches, thm.PaddleConeColor);
    drawStepper(top + 554.0f, "Cone Angle", std::to_string(static_cast<int>(std::round(thm.PaddleConeAngle))) + " deg");
    drawStepper(top + 590.0f, "Light Power", FormatTwoDecimals(thm.PaddleLightIntensity) + "x");
    drawStepper(top + 626.0f, "Light Range", std::to_string(static_cast<int>(std::round(thm.PaddleLightRange))) + " px");
    drawStepper(top + 662.0f, "Vignette", std::to_string(static_cast<int>(std::round(thm.VignetteIntensity * 100.0f))) + "%");

    // 4. Audio Track, Level Music Playlist & Background Music Mute Toggle
    drawSection(top + 702.0f, "AUDIO & PLAYLIST");
    context.Render.DrawText("Primary Track", m_fontId, 15.0f, Transform2D{Vector2f{leftX + 6.0f, top + 726.0f}}, Color{200, 210, 228, 240});
    const Vector2f musPos{ctrlCenterX, top + 734.0f};
    const bool musHov = (std::abs(worldMouse.X - musPos.X) <= ctrlW * 0.5f && std::abs(worldMouse.Y - musPos.Y) <= 15.0f);
    context.Render.DrawRectangle(ctrlW, 30.0f, Transform2D{musPos}, musHov ? Color{38, 48, 66, 255} : Color{28, 33, 45, 255});
    context.Render.DrawRectangleOutline(ctrlW, 30.0f, Transform2D{musPos}, musHov ? Colors::White : Color{75, 110, 165, 210}, 1.2f);
    context.Render.DrawTextCentered("< " + LevelData::GetMusicTrackLabel(thm.MusicTrack) + " >", m_fontId, 15.0f, Transform2D{musPos}, Color{170, 235, 255, 255});

    context.Render.DrawText("Background Music", m_fontId, 15.0f, Transform2D{Vector2f{leftX + 6.0f, top + 762.0f}}, Color{200, 210, 228, 240});
    const Vector2f mutePos{ctrlCenterX, top + 770.0f};
    const bool muteHov = (std::abs(worldMouse.X - mutePos.X) <= ctrlW * 0.5f && std::abs(worldMouse.Y - mutePos.Y) <= 15.0f);
    const Color muteBg = m_environmentMusicMuted
        ? (muteHov ? Color{70, 32, 38, 255} : Color{54, 24, 30, 255})
        : (muteHov ? Color{26, 62, 52, 255} : Color{20, 48, 40, 255});
    const Color muteBorder = m_environmentMusicMuted ? Color{240, 105, 115, 225} : Color{95, 230, 160, 225};
    const Color muteText = m_environmentMusicMuted ? Color{255, 145, 155, 255} : Color{135, 245, 185, 255};
    context.Render.DrawRectangle(ctrlW, 30.0f, Transform2D{mutePos}, muteBg);
    context.Render.DrawRectangleOutline(ctrlW, 30.0f, Transform2D{mutePos}, muteHov ? Colors::White : muteBorder, 1.3f);
    context.Render.DrawTextCentered(m_environmentMusicMuted ? "MUTED" : "ON", m_fontId, 16.0f, Transform2D{mutePos}, muteText);

    // 5. Multi-Track Level Playlist Queue (01 | 02 | 03 | 04 | ALL)
    const size_t qCount = std::max<size_t>(1, thm.MusicPlaylist.size());
    const std::string plLabel = "Playlist (" + std::to_string(qCount) + (qCount == 1 ? " trk)" : " trks)");
    context.Render.DrawText(plLabel, m_fontId, 15.0f, Transform2D{Vector2f{leftX + 6.0f, top + 798.0f}}, Color{200, 210, 228, 240});

    const auto& allTracks = LevelData::GetAvailableMusicTracks();
    const float plY = top + 806.0f;
    const float chipGap = 4.0f;
    const float chipW = (ctrlW - chipGap * 4.0f) / 5.0f;
    const float chipStartX = ctrlCenterX - ctrlW * 0.5f + chipW * 0.5f;
    const char* chipNames[5] = {"01", "02", "03", "04", "ALL"};

    for (int i = 0; i < 5; ++i)
    {
        const float cx = chipStartX + static_cast<float>(i) * (chipW + chipGap);
        const Vector2f cPos{cx, plY};
        const bool hov = (std::abs(worldMouse.X - cx) <= chipW * 0.5f && std::abs(worldMouse.Y - plY) <= 14.0f);
        bool active = false;
        bool isPrimary = false;
        if (i < 4 && static_cast<size_t>(i) < allTracks.size())
        {
            active = thm.IsTrackInPlaylist(allTracks[static_cast<size_t>(i)]);
            isPrimary = (thm.MusicTrack == allTracks[static_cast<size_t>(i)]);
        }
        else if (i == 4)
        {
            active = (thm.MusicPlaylist.size() >= allTracks.size());
        }

        const Color bg = active
            ? (isPrimary ? Color{48, 76, 120, 255} : Color{32, 66, 88, 255})
            : (hov ? Color{36, 42, 56, 255} : Color{24, 28, 38, 255});
        const Color border = isPrimary
            ? Color{255, 215, 65, 255}
            : (active ? Color{85, 225, 255, 235} : (hov ? Colors::White : Color{56, 65, 86, 200}));
        const Color txtCol = isPrimary
            ? Color{255, 230, 95, 255}
            : (active ? Color{155, 245, 255, 255} : Color{150, 165, 188, 210});

        context.Render.DrawRectangle(chipW, 28.0f, Transform2D{cPos}, bg);
        context.Render.DrawRectangleOutline(chipW, 28.0f, Transform2D{cPos}, border, (isPrimary || active) ? 1.8f : 1.1f);
        context.Render.DrawTextCentered(chipNames[i], m_fontId, 14.0f, Transform2D{cPos}, txtCol);
    }
}

bool EditorScene::HandleThemeTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick)
{
    if (!leftClick) return false;

    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 32.0f;
    const float leftX = panelCenter.X - cardW * 0.5f;
    const float rightX = panelCenter.X + cardW * 0.5f;
    const float ctrlW = 196.0f;
    const float ctrlCenterX = rightX - ctrlW * 0.5f;

    auto playThemeMusic = [&]() {
        SyncEditorThemeMusic(context);
    };

    // 0. Lighting Mode Selector (LIT | ATMOSPHERIC | PITCH BLACK at Y = top + 118)
    const float modeGap = 6.0f;
    const float modeW = (cardW - modeGap * 2.0f) / 3.0f;
    const float modeY = top + 118.0f;
    if (std::abs(worldMouse.Y - modeY) <= 17.0f)
    {
        const LightingMode lightingModes[3] = {
            LightingMode::Lit,
            LightingMode::Atmospheric,
            LightingMode::PitchBlack
        };
        for (int i = 0; i < 3; ++i)
        {
            const float mx = leftX + modeW * 0.5f + static_cast<float>(i) * (modeW + modeGap);
            if (std::abs(worldMouse.X - mx) <= modeW * 0.5f)
            {
                if (m_levelData.GetLightingMode() != lightingModes[i])
                {
                    PushUndo();
                    m_levelData.ApplyLightingMode(lightingModes[i]);
                    ShowToast(std::string("Lighting: ") + GetLightingModeName(lightingModes[i]), Color{110, 230, 255, 255}, 1.5f);
                }
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                return true;
            }
        }
    }

    // 1. Built-in Environment Preset Selector (Y = top + 192)
    if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 192.0f)) <= 16.0f)
    {
        const int count = static_cast<int>(EditorThemePreset::Count);
        int p = static_cast<int>(m_levelData.GetTheme().Preset);
        p = (worldMouse.X < panelCenter.X) ? ((p - 1 + count) % count) : ((p + 1) % count);
        PushUndo();
        m_levelData.ApplyThemePreset(static_cast<EditorThemePreset>(p));
        m_selectedCustomThemeIdx = -1;
        playThemeMusic();
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        return true;
    }

    // 2. Custom Theme Selector (Y = top + 264)
    const auto& customThemes = m_themeLibrary.GetThemes();
    if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 264.0f)) <= 16.0f)
    {
        if (!customThemes.empty())
        {
            const int n = static_cast<int>(customThemes.size());
            if (m_selectedCustomThemeIdx < 0 || m_selectedCustomThemeIdx >= n)
            {
                m_selectedCustomThemeIdx = (worldMouse.X < panelCenter.X) ? (n - 1) : 0;
            }
            else
            {
                m_selectedCustomThemeIdx = (worldMouse.X < panelCenter.X)
                    ? ((m_selectedCustomThemeIdx - 1 + n) % n)
                    : ((m_selectedCustomThemeIdx + 1) % n);
            }
            PushUndo();
            m_levelData.SetTheme(customThemes[m_selectedCustomThemeIdx]);
            playThemeMusic();
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        }
        return true;
    }

    // 3. Custom Theme Action Buttons (NEW / SAVE / RENAME / DELETE at Y = top + 302)
    const float actGap = 6.0f;
    const float actW = (cardW - actGap * 3.0f) / 4.0f;
    const float actY = top + 302.0f;
    if (std::abs(worldMouse.Y - actY) <= 15.0f)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float ax = leftX + actW * 0.5f + static_cast<float>(i) * (actW + actGap);
            if (std::abs(worldMouse.X - ax) <= actW * 0.5f)
            {
                const bool hasCustomSel = (m_selectedCustomThemeIdx >= 0 && m_selectedCustomThemeIdx < static_cast<int>(customThemes.size()));
                if (i == 0) // NEW
                {
                    OpenThemeModal(EditorTextModalMode::NewTheme);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                }
                else if (i == 1) // SAVE
                {
                    if (hasCustomSel)
                    {
                        const std::string id = customThemes[m_selectedCustomThemeIdx].ThemeId;
                        EditorThemeData updated = m_levelData.GetTheme();
                        updated.ThemeId = id;
                        updated.Name = customThemes[m_selectedCustomThemeIdx].Name;
                        if (m_themeLibrary.UpdateTheme(id, updated))
                        {
                            m_levelData.SetTheme(updated);
                            ShowToast("Theme updated: " + updated.Name, Color{110, 235, 165, 255}, 1.8f);
                            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                        }
                    }
                    else
                    {
                        OpenThemeModal(EditorTextModalMode::NewTheme);
                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                    }
                }
                else if (i == 2 && hasCustomSel) // RENAME
                {
                    OpenThemeModal(EditorTextModalMode::RenameTheme);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                }
                else if (i == 3 && hasCustomSel) // DELETE
                {
                    const std::string id = customThemes[m_selectedCustomThemeIdx].ThemeId;
                    const std::string name = customThemes[m_selectedCustomThemeIdx].Name;
                    if (m_themeLibrary.DeleteTheme(id))
                    {
                        m_selectedCustomThemeIdx = -1;
                        m_levelData.GetTheme().ThemeId.clear();
                        ShowToast("Theme deleted: " + name, Color{255, 130, 140, 255}, 1.8f);
                        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
                    }
                }
                return true;
            }
        }
    }

    auto& thm = m_levelData.GetTheme();

    auto hitSwatches = [&](float y, const Color (&palette)[8], Color& outColor) {
        const float swSize = 21.0f;
        const float swGap = 4.0f;
        const float startX = ctrlCenterX - ctrlW * 0.5f + swSize * 0.5f;
        if (std::abs(worldMouse.Y - y) > swSize * 0.55f) return false;
        for (int i = 0; i < 8; ++i)
        {
            const float sx = startX + static_cast<float>(i) * (swSize + swGap);
            if (std::abs(worldMouse.X - sx) <= swSize * 0.5f)
            {
                outColor = palette[i];
                return true;
            }
        }
        return false;
    };

    auto hitStepperDelta = [&](float y, int& outDir) {
        if (std::abs(worldMouse.X - ctrlCenterX) > ctrlW * 0.5f || std::abs(worldMouse.Y - y) > 15.0f) return false;
        outDir = (worldMouse.X < ctrlCenterX) ? -1 : 1;
        return true;
    };

    if (hitSwatches(top + 374.0f, kBgDarkSwatches, thm.BgColor))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitSwatches(top + 410.0f, kBgSecondarySwatches, thm.BgSecondaryColor))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitSwatches(top + 446.0f, kAmbientSwatches, thm.AmbientColor))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }

    int dir = 0;
    if (hitStepperDelta(top + 482.0f, dir))
    {
        thm.AmbientIntensity = std::clamp(thm.AmbientIntensity + static_cast<float>(dir) * 0.05f, 0.10f, 1.00f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitSwatches(top + 518.0f, kAccentSwatches, thm.PaddleConeColor))
    {
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitStepperDelta(top + 554.0f, dir))
    {
        thm.PaddleConeAngle = std::clamp(thm.PaddleConeAngle + static_cast<float>(dir) * 5.0f, 20.0f, 110.0f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitStepperDelta(top + 590.0f, dir))
    {
        thm.PaddleLightIntensity = std::clamp(thm.PaddleLightIntensity + static_cast<float>(dir) * 0.10f, 0.40f, 2.40f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitStepperDelta(top + 626.0f, dir))
    {
        thm.PaddleLightRange = std::clamp(thm.PaddleLightRange + static_cast<float>(dir) * 25.0f, 220.0f, 650.0f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }
    if (hitStepperDelta(top + 662.0f, dir))
    {
        thm.VignetteIntensity = std::clamp(thm.VignetteIntensity + static_cast<float>(dir) * 0.05f, 0.0f, 0.75f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
        return true;
    }

    // 4. Primary Music Track Selector (Y = top + 734)
    if (hitStepperDelta(top + 734.0f, dir))
    {
        const auto& tracks = LevelData::GetAvailableMusicTracks();
        if (!tracks.empty())
        {
            size_t idx = 0;
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                if (tracks[i] == thm.MusicTrack) { idx = i; break; }
            }
            idx = (dir < 0)
                ? ((idx == 0) ? (tracks.size() - 1) : (idx - 1))
                : ((idx + 1) % tracks.size());
            PushUndo();
            const bool wasSingleTrack = (thm.MusicPlaylist.size() <= 1);
            thm.MusicTrack = tracks[idx];
            if (wasSingleTrack)
            {
                thm.SetSingleTrack(tracks[idx]);
            }
            else
            {
                thm.ClampValues(); // Keeps MusicTrack at front of MusicPlaylist
            }
            playThemeMusic();
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        }
        return true;
    }

    // 5. Background Music Mute Toggle (Y = top + 770) — affects only level background music, never SFX/UI
    if (std::abs(worldMouse.X - ctrlCenterX) <= ctrlW * 0.5f && std::abs(worldMouse.Y - (top + 770.0f)) <= 15.0f)
    {
        m_environmentMusicMuted = !m_environmentMusicMuted;
        SyncEditorThemeMusic(context);
        ShowToast(m_environmentMusicMuted ? "Background Music: Muted" : "Background Music: Active",
                  m_environmentMusicMuted ? Color{255, 145, 155, 255} : Color{110, 235, 165, 255},
                  1.5f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        return true;
    }

    // 6. Multi-Track Level Playlist Chips (01 | 02 | 03 | 04 | ALL at Y = top + 806)
    const float plY = top + 806.0f;
    if (std::abs(worldMouse.Y - plY) <= 15.0f)
    {
        const auto& allTracks = LevelData::GetAvailableMusicTracks();
        const float chipGap = 4.0f;
        const float chipW = (ctrlW - chipGap * 4.0f) / 5.0f;
        const float chipStartX = ctrlCenterX - ctrlW * 0.5f + chipW * 0.5f;

        for (int i = 0; i < 5; ++i)
        {
            const float cx = chipStartX + static_cast<float>(i) * (chipW + chipGap);
            if (std::abs(worldMouse.X - cx) <= chipW * 0.5f)
            {
                PushUndo();
                if (i < 4 && static_cast<size_t>(i) < allTracks.size())
                {
                    thm.TogglePlaylistTrack(allTracks[static_cast<size_t>(i)]);
                }
                else if (i == 4)
                {
                    if (thm.MusicPlaylist.size() >= allTracks.size())
                    {
                        thm.SetSingleTrack(thm.MusicTrack);
                    }
                    else
                    {
                        thm.EnableAllPlaylistTracks(allTracks);
                    }
                }
                playThemeMusic();
                ShowToast("Level Playlist: " + std::to_string(thm.MusicPlaylist.size()) +
                          (thm.MusicPlaylist.size() == 1 ? " track queued" : " tracks queued"),
                          Color{110, 230, 255, 255}, 1.5f);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                return true;
            }
        }
    }

    return false;
}

void EditorScene::RenderEventQueueTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse)
{
    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 36.0f;
    const float leftX = panelCenter.X - cardW * 0.5f;

    // 0. Initial Event Selector (triggers immediately on level start)
    context.Render.DrawText("INITIAL EVENT", m_fontId, 18.0f, Transform2D{Vector2f{leftX, top + 74.0f}}, Color{110, 230, 255, 255});
    const ArenaEventType initEv = m_levelData.GetInitialEvent();
    const bool hasInitEv = (initEv != ArenaEventType::None);
    const ArenaEventDescriptor* initDesc = FindArenaEventDescriptor(initEv);
    const Color initAccent = (hasInitEv && initDesc) ? initDesc->AccentColor : Color{140, 155, 180, 220};
    const std::string initLabel = (hasInitEv && initDesc) ? ("< " + std::string(initDesc->DisplayName) + " >") : "< NONE >";

    const Vector2f initPos{panelCenter.X, top + 114.0f};
    const bool initHov = (std::abs(worldMouse.X - initPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - initPos.Y) <= 18.0f);
    context.Render.DrawRectangle(cardW, 36.0f, Transform2D{initPos},
        initHov ? Color{38, 48, 68, 255} : (hasInitEv ? Color{30, 38, 54, 255} : Color{24, 28, 38, 255}));
    if (hasInitEv)
    {
        context.Render.DrawRectangle(6.0f, 32.0f, Transform2D{Vector2f{leftX + 4.0f, initPos.Y}}, initAccent);
    }
    context.Render.DrawRectangleOutline(cardW, 36.0f, Transform2D{initPos},
        initHov ? Colors::White : (hasInitEv ? initAccent : Color{56, 66, 88, 210}),
        (initHov || hasInitEv) ? 1.8f : 1.2f);
    context.Render.DrawTextCentered(initLabel, m_fontId, 17.0f, Transform2D{initPos},
        hasInitEv ? initAccent : Color{180, 195, 215, 225});

    // 1. Header + Count Badge for Scheduled Event Queue
    const auto& q = m_levelData.GetEventQueue();
    context.Render.DrawText("EVENT QUEUE", m_fontId, 20.0f, Transform2D{Vector2f{leftX, top + 150.0f}}, Color{255, 215, 60, 255});

    Vector2f cntBadge{panelCenter.X + cardW * 0.5f - 28.0f, top + 162.0f};
    context.Render.DrawRectangle(54.0f, 26.0f, Transform2D{cntBadge}, Color{28, 36, 52, 255});
    context.Render.DrawRectangleOutline(54.0f, 26.0f, Transform2D{cntBadge}, Color{80, 220, 255, 200}, 1.2f);
    context.Render.DrawTextCentered(std::to_string(q.size()), m_fontId, 18.0f, Transform2D{cntBadge}, Color{170, 235, 255, 255});

    // 2. 2x3 Graphical Event Palette Chips
    struct EventChipDesc
    {
        EditorEventType Type;
        const char* ShortLabel;
        Color Accent;
    };
    const EventChipDesc chips[6] = {
        {EditorEventType::HazardSurge,      "METEORS",  Color{255, 75, 75, 255}},
        {EditorEventType::LaserSweep,       "GRAVITY",  Color{255, 160, 40, 255}},
        {EditorEventType::DarknessEclipse,  "ECLIPSE",  Color{150, 75, 255, 255}},
        {EditorEventType::TurretOvercharge, "LOCKDOWN", Color{255, 45, 95, 255}},
        {EditorEventType::BossAlert,        "QUAKE",    Color{255, 215, 0, 255}},
        {EditorEventType::SpeedBoost,       "TEMPO",    Color{65, 225, 255, 255}}
    };

    const float chipGap = 8.0f;
    const float chipW = (cardW - chipGap * 2.0f) / 3.0f;
    const float chipH = 42.0f;
    const float startX = leftX + chipW * 0.5f;

    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        const Vector2f cPos{startX + static_cast<float>(col) * (chipW + chipGap), top + 210.0f + static_cast<float>(row) * 50.0f};
        const bool hover = (std::abs(worldMouse.X - cPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - cPos.Y) <= chipH * 0.5f);

        context.Render.DrawRectangle(chipW, chipH, Transform2D{cPos}, hover ? Color{40, 48, 66, 255} : Color{28, 32, 44, 255});
        context.Render.DrawRectangle(5.0f, chipH - 4.0f, Transform2D{Vector2f{cPos.X - chipW * 0.5f + 3.5f, cPos.Y}}, chips[i].Accent);
        context.Render.DrawRectangleOutline(chipW, chipH, Transform2D{cPos}, hover ? chips[i].Accent : Color{55, 65, 86, 210}, hover ? 2.0f : 1.2f);
        context.Render.DrawTextCentered(std::string("+ ") + chips[i].ShortLabel, m_fontId, 16.0f, Transform2D{Vector2f{cPos.X + 3.0f, cPos.Y}}, Colors::White);
    }

    // 3. 3 Compact Sort Pills (A-Z | Z-A | SHUFFLE)
    const char* sortLabels[3] = {"SORT A-Z", "SORT Z-A", "SHUFFLE"};
    const float sortY = top + 318.0f;
    const float sortH = 32.0f;
    for (int s = 0; s < 3; ++s)
    {
        const Vector2f sPos{startX + static_cast<float>(s) * (chipW + chipGap), sortY};
        const bool hover = (std::abs(worldMouse.X - sPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - sPos.Y) <= sortH * 0.5f);
        const bool active = (static_cast<int>(m_levelData.GetEventFilter()) == s);

        context.Render.DrawRectangle(chipW, sortH, Transform2D{sPos}, hover ? Color{44, 54, 74, 255} : (active ? Color{34, 44, 62, 255} : Color{24, 28, 38, 255}));
        context.Render.DrawRectangleOutline(chipW, sortH, Transform2D{sPos}, hover ? Color{255, 215, 60, 255} : Color{52, 60, 78, 200}, 1.2f);
        context.Render.DrawTextCentered(sortLabels[s], m_fontId, 15.0f, Transform2D{sPos}, hover ? Color{255, 225, 90, 255} : Color{185, 205, 225, 240});
    }

    // 4. Queue Cards (up to kEventQueueMaxVisible visible, scrollable)
    const int maxVisible = kEventQueueMaxVisible;
    const int maxScroll = std::max(0, static_cast<int>(q.size()) - maxVisible);
    const int clampedScroll = std::clamp(m_eventQueueScroll, 0, maxScroll);
    const float listStartY = top + kEventQueueListStartOffsetY;
    const float itemH = 52.0f;

    if (q.empty())
    {
        Vector2f emptyBox{panelCenter.X, listStartY + 80.0f};
        context.Render.DrawRectangle(cardW, 100.0f, Transform2D{emptyBox}, Color{20, 24, 34, 200});
        context.Render.DrawRectangleOutline(cardW, 100.0f, Transform2D{emptyBox}, Color{48, 56, 74, 180}, 1.2f);
        context.Render.DrawTextCentered("NO EVENTS QUEUED", m_fontId, 20.0f, Transform2D{emptyBox}, Color{135, 155, 180, 190});
    }
    else
    {
        for (int vis = 0; vis < maxVisible; ++vis)
        {
            const int idx = clampedScroll + vis;
            if (idx >= static_cast<int>(q.size())) break;

            const float iy = listStartY + static_cast<float>(vis) * itemH;
            const Vector2f iPos{panelCenter.X, iy};
            const bool isDragged = (m_isDraggingEvent && m_draggedEventIdx == idx);
            const bool rowHover = (std::abs(worldMouse.X - iPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - iy) <= 22.0f);

            if (m_isDraggingEvent && rowHover)
            {
                context.Render.DrawLine(Vector2f{panelCenter.X - cardW * 0.5f, iy - itemH * 0.5f},
                                        Vector2f{panelCenter.X + cardW * 0.5f, iy - itemH * 0.5f},
                                        Color{255, 220, 60, 255}, 3.0f);
            }

            if (isDragged)
            {
                context.Render.DrawRectangle(cardW, 44.0f, Transform2D{iPos}, Color{30, 38, 54, 110});
                context.Render.DrawRectangleOutline(cardW, 44.0f, Transform2D{iPos}, Color{80, 105, 150, 140}, 1.2f);
                continue;
            }

            context.Render.DrawRectangle(cardW, 44.0f, Transform2D{iPos}, rowHover ? Color{34, 40, 56, 255} : Color{27, 31, 43, 255});
            context.Render.DrawRectangle(6.0f, 40.0f, Transform2D{Vector2f{iPos.X - cardW * 0.5f + 4.0f, iy}}, q[idx].ThemeColor);
            context.Render.DrawRectangleOutline(cardW, 44.0f, Transform2D{iPos}, rowHover ? q[idx].ThemeColor : Color{52, 60, 80, 220}, rowHover ? 1.8f : 1.2f);

            // Drag grip dots (2x3)
            for (int gy = -1; gy <= 1; ++gy)
            {
                for (int gx = 0; gx < 2; ++gx)
                {
                    context.Render.DrawRectangle(3.0f, 3.0f,
                        Transform2D{Vector2f{iPos.X - cardW * 0.5f + 16.0f + gx * 5.0f, iy + gy * 6.0f}},
                        Color{130, 145, 170, 200});
                }
            }

            // Index + Name
            std::string title = std::to_string(idx + 1) + ". " + q[idx].Name;
            context.Render.DrawText(title, m_fontId, 18.0f, Transform2D{Vector2f{iPos.X - cardW * 0.5f + 30.0f, iy - 10.0f}}, Colors::White);

            // Delay Stepper: [-] 15s [+]
            const Vector2f minusPos{iPos.X + cardW * 0.5f - 122.0f, iy};
            const Vector2f timePos{iPos.X + cardW * 0.5f - 82.0f, iy};
            const Vector2f plusPos{iPos.X + cardW * 0.5f - 44.0f, iy};
            const Vector2f delPos{iPos.X + cardW * 0.5f - 16.0f, iy};

            const bool minHov = (std::abs(worldMouse.X - minusPos.X) <= 13.0f && std::abs(worldMouse.Y - iy) <= 14.0f);
            const bool plsHov = (std::abs(worldMouse.X - plusPos.X) <= 13.0f && std::abs(worldMouse.Y - iy) <= 14.0f);
            const bool delHov = (std::abs(worldMouse.X - delPos.X) <= 13.0f && std::abs(worldMouse.Y - iy) <= 14.0f);

            context.Render.DrawRectangle(24.0f, 26.0f, Transform2D{minusPos}, minHov ? Color{55, 68, 92, 255} : Color{38, 44, 58, 255});
            context.Render.DrawTextCentered("-", m_fontId, 18.0f, Transform2D{minusPos}, Colors::White);

            context.Render.DrawTextCentered(std::to_string(static_cast<int>(q[idx].TriggerDelay)) + "s", m_fontId, 17.0f, Transform2D{timePos}, Color{255, 220, 90, 255});

            context.Render.DrawRectangle(24.0f, 26.0f, Transform2D{plusPos}, plsHov ? Color{55, 68, 92, 255} : Color{38, 44, 58, 255});
            context.Render.DrawTextCentered("+", m_fontId, 18.0f, Transform2D{plusPos}, Colors::White);

            context.Render.DrawRectangle(24.0f, 26.0f, Transform2D{delPos}, delHov ? Color{210, 50, 65, 255} : Color{65, 32, 42, 255});
            context.Render.DrawTextCentered("X", m_fontId, 16.0f, Transform2D{delPos}, delHov ? Colors::White : Color{255, 130, 140, 255});
        }

        if (maxScroll > 0)
        {
            std::string scrollHint = std::to_string(clampedScroll + 1) + "-" + std::to_string(std::min(static_cast<int>(q.size()), clampedScroll + maxVisible)) + " / " + std::to_string(q.size());
            context.Render.DrawTextCentered(scrollHint, m_fontId, 15.0f, Transform2D{Vector2f{panelCenter.X, listStartY + maxVisible * itemH - 12.0f}}, Color{150, 175, 205, 210});
        }
    }

    // Bottom Trash / Clear Zone
    Vector2f trashPos{panelCenter.X, halfH - 52.0f};
    Vector2f trashSize{cardW, 58.0f};
    m_trashCanHovered = (std::abs(worldMouse.X - trashPos.X) <= trashSize.X * 0.5f &&
                         std::abs(worldMouse.Y - trashPos.Y) <= trashSize.Y * 0.5f);
    RenderEventTrashBin(context, trashPos, trashSize);

    // Floating Drag Proxy
    if (m_isDraggingEvent && m_draggedEventIdx >= 0 && m_draggedEventIdx < static_cast<int>(q.size()))
    {
        context.Render.DrawRectangle(250.0f, 44.0f, Transform2D{m_dragMousePos}, Color{36, 48, 72, 245});
        context.Render.DrawRectangle(6.0f, 40.0f, Transform2D{Vector2f{m_dragMousePos.X - 121.0f, m_dragMousePos.Y}}, q[m_draggedEventIdx].ThemeColor);
        context.Render.DrawRectangleOutline(250.0f, 44.0f, Transform2D{m_dragMousePos}, Color{255, 215, 60, 255}, 2.2f);
        context.Render.DrawTextCentered(q[m_draggedEventIdx].Name, m_fontId, 19.0f, Transform2D{m_dragMousePos}, Colors::White);
    }
}

bool EditorScene::HandleEventQueueTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick)
{
    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 36.0f;
    const auto& q = m_levelData.GetEventQueue();

    // Mouse wheel scroll support for EventQueue
    const float wheel = context.Input.GetMouseWheelDelta();
    if (std::abs(wheel) > 0.01f)
    {
        const int maxScroll = std::max(0, static_cast<int>(q.size()) - kEventQueueMaxVisible);
        m_eventQueueScroll = std::clamp(m_eventQueueScroll - (wheel > 0.0f ? 1 : -1), 0, maxScroll);
    }

    if (!leftClick) return false;

    // 0. Initial Event Selector (Y = top + 114)
    if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 114.0f)) <= 18.0f)
    {
        PushUndo();
        const int dir = (worldMouse.X < panelCenter.X) ? -1 : 1;
        m_levelData.CycleInitialEvent(dir);
        const ArenaEventType ev = m_levelData.GetInitialEvent();
        const ArenaEventDescriptor* evDesc = FindArenaEventDescriptor(ev);
        ShowToast((ev == ArenaEventType::None || !evDesc)
            ? "Initial Event: None"
            : ("Initial Event: " + std::string(evDesc->DisplayName)),
            ev == ArenaEventType::None ? Color{175, 195, 220, 255} : GetArenaEventColor(ev),
            1.6f);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
        return true;
    }

    const EditorEventType chipTypes[6] = {
        EditorEventType::HazardSurge,
        EditorEventType::LaserSweep,
        EditorEventType::DarknessEclipse,
        EditorEventType::TurretOvercharge,
        EditorEventType::BossAlert,
        EditorEventType::SpeedBoost
    };

    const float chipGap = 8.0f;
    const float chipW = (cardW - chipGap * 2.0f) / 3.0f;
    const float chipH = 42.0f;
    const float startX = panelCenter.X - cardW * 0.5f + chipW * 0.5f;

    // 1. Add Event Chips
    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        const Vector2f cPos{startX + static_cast<float>(col) * (chipW + chipGap), top + 210.0f + static_cast<float>(row) * 50.0f};
        if (std::abs(worldMouse.X - cPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - cPos.Y) <= chipH * 0.5f)
        {
            PushUndo();
            m_levelData.AddEvent(chipTypes[i]);
            const int maxScroll = std::max(0, static_cast<int>(m_levelData.GetEventQueue().size()) - kEventQueueMaxVisible);
            m_eventQueueScroll = maxScroll;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
    }

    // 2. Sort Pills
    const float sortY = top + 318.0f;
    for (int s = 0; s < 3; ++s)
    {
        const Vector2f sPos{startX + static_cast<float>(s) * (chipW + chipGap), sortY};
        if (std::abs(worldMouse.X - sPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - sPos.Y) <= 16.0f)
        {
            PushUndo();
            m_levelData.ApplyEventFilter(static_cast<EventSortFilter>(s));
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
    }

    // 3. Queue Cards: [-], [+], [X], or start Drag
    const int maxScroll = std::max(0, static_cast<int>(q.size()) - kEventQueueMaxVisible);
    m_eventQueueScroll = std::clamp(m_eventQueueScroll, 0, maxScroll);
    const float listStartY = top + kEventQueueListStartOffsetY;
    const float itemH = 52.0f;

    for (int vis = 0; vis < kEventQueueMaxVisible; ++vis)
    {
        const int idx = m_eventQueueScroll + vis;
        if (idx >= static_cast<int>(q.size())) break;

        const float iy = listStartY + static_cast<float>(vis) * itemH;
        const Vector2f iPos{panelCenter.X, iy};
        if (std::abs(worldMouse.X - iPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - iy) <= 22.0f)
        {
            const Vector2f minusPos{iPos.X + cardW * 0.5f - 122.0f, iy};
            const Vector2f plusPos{iPos.X + cardW * 0.5f - 44.0f, iy};
            const Vector2f delPos{iPos.X + cardW * 0.5f - 16.0f, iy};

            if (std::abs(worldMouse.X - delPos.X) <= 13.0f)
            {
                DeleteEvent(static_cast<size_t>(idx));
                return true;
            }
            if (std::abs(worldMouse.X - minusPos.X) <= 13.0f)
            {
                PushUndo();
                m_levelData.AdjustEventDelay(static_cast<size_t>(idx), -3.0f);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                return true;
            }
            if (std::abs(worldMouse.X - plusPos.X) <= 13.0f)
            {
                PushUndo();
                m_levelData.AdjustEventDelay(static_cast<size_t>(idx), 3.0f);
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                return true;
            }

            m_isDraggingEvent = true;
            m_draggedEventIdx = idx;
            m_dragMousePos = worldMouse;
            return true;
        }
    }

    // 4. Bottom Clear Button
    Vector2f trashPos{panelCenter.X, halfH - 52.0f};
    Vector2f trashSize{cardW, 58.0f};
    if (std::abs(worldMouse.X - trashPos.X) <= trashSize.X * 0.5f &&
        std::abs(worldMouse.Y - trashPos.Y) <= trashSize.Y * 0.5f)
    {
        if (!q.empty())
        {
            ClearEventQueue();
        }
        return true;
    }

    return false;
}

void EditorScene::RenderEventTrashBin(GameContext& context, const Vector2f& trashPos, const Vector2f& trashSize)
{
    const bool isHovered = m_trashCanHovered;
    const Color trashBg = (m_isDraggingEvent && isHovered)
        ? Color{205, 35, 50, 255}
        : (isHovered ? Color{95, 30, 42, 255} : Color{42, 22, 30, 255});
    const Color trashOutline = isHovered ? Color{255, 75, 85, 255} : Color{165, 55, 70, 200};

    context.Render.DrawRectangle(trashSize.X, trashSize.Y, Transform2D{trashPos}, trashBg);
    context.Render.DrawRectangleOutline(trashSize.X, trashSize.Y, Transform2D{trashPos}, trashOutline, isHovered ? 2.2f : 1.4f);

    DrawIconTrash(context, Vector2f{trashPos.X - trashSize.X * 0.34f, trashPos.Y}, 24.0f, isHovered ? Colors::White : Color{255, 110, 125, 255});
    const char* trashText = m_isDraggingEvent
        ? (isHovered ? "RELEASE TO DELETE" : "DROP HERE TO DELETE")
        : "CLEAR QUEUE";
    context.Render.DrawTextCentered(trashText, m_fontId, 19.0f, Transform2D{Vector2f{trashPos.X + 18.0f, trashPos.Y}}, isHovered ? Colors::White : Color{245, 190, 200, 240});
}

void EditorScene::ResetTurretsAndProjectiles()
{
    m_turretBullets.clear();

    for (size_t i = 0; i < m_turretBricks.size(); ++i)
    {
        auto& tb = m_turretBricks[i];
        tb.ShootTimer = std::max(1.2f, tb.Cooldown * 0.65f) + 0.45f * static_cast<float>(i % 4);
        if (m_registry.IsAlive(tb.EntityId) && m_registry.HasComponent<SpriteComponent>(tb.EntityId))
        {
            const Color baseCol = (tb.BaseHP > 3)
                ? BrickFactory::GetColorForHitPoints(tb.BaseHP)
                : Color{255, 45, 55, 255};
            m_registry.GetComponent<SpriteComponent>(tb.EntityId).Tint = baseCol;
        }
    }
}

void EditorScene::UpdateTurrets(float dt, GameContext& context)
{
    // Match Campaign (StoryScene): do NOT advance turrets or projectiles during countdown, death, or paddle respawn
    if (IsCountdownActive() || IsPaddleExploding() || IsBallDying())
    {
        return;
    }

    const bool hasPaddle = m_registry.IsAlive(m_paddle) && !IsPaddleExploding() &&
                           m_registry.HasComponent<Transform2D>(m_paddle) &&
                           m_registry.HasComponent<BoxCollider>(m_paddle);

    Vector2f paddlePos{0.0f, 350.0f};
    Vector2f halfSize{60.0f, 10.0f};
    if (hasPaddle)
    {
        const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
        const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
        paddlePos = pTrans.Position;
        halfSize = pCol.GetEffectiveSize(pTrans.Scale) * 0.5f;
    }

    // Pause turret charging & firing while ball is waiting on the paddle for launch
    if (!IsBallAttached())
    {
        for (auto it = m_turretBricks.begin(); it != m_turretBricks.end(); )
        {
            if (!m_registry.IsAlive(it->EntityId) || !m_registry.HasComponent<BrickComponent>(it->EntityId) ||
                m_registry.GetComponent<BrickComponent>(it->EntityId).HitPoints == 0)
            {
                it = m_turretBricks.erase(it);
                continue;
            }

            const auto& brick = m_registry.GetComponent<BrickComponent>(it->EntityId);
            if (brick.IsDescending)
            {
                ++it;
                continue;
            }

            it->ShootTimer -= dt;

            // Visual warning charge in the last 0.45s before firing
            if (it->ShootTimer <= 0.45f && m_registry.HasComponent<Transform2D>(it->EntityId))
            {
                const float pulse = 0.5f + 0.5f * std::sin(it->ShootTimer * 30.0f);
                if (m_registry.HasComponent<SpriteComponent>(it->EntityId))
                {
                    m_registry.GetComponent<SpriteComponent>(it->EntityId).Tint = Color{
                        255,
                        static_cast<uint8_t>(45 + 140 * pulse),
                        static_cast<uint8_t>(55 + 140 * pulse),
                        255
                    };
                }
            }

            // Fire downward (or aimed) triangle missile
            if (it->ShootTimer <= 0.0f)
            {
                it->ShootTimer = it->Cooldown;

                if (m_registry.HasComponent<Transform2D>(it->EntityId))
                {
                    const auto& bTrans = m_registry.GetComponent<Transform2D>(it->EntityId);
                    Vector2f spawnPos{bTrans.Position.X, bTrans.Position.Y + 18.0f};

                    TriangleProjectile proj;
                    proj.Position = spawnPos;
                    proj.Size = 16.0f;
                    proj.Damage = it->Damage;

                    if (it->Aimed && hasPaddle)
                    {
                        Vector2f dir = paddlePos - spawnPos;
                        dir.Y = std::max(80.0f, dir.Y);
                        const float len = std::sqrt(dir.X * dir.X + dir.Y * dir.Y);
                        if (len > 0.001f)
                        {
                            proj.Velocity = Vector2f{(dir.X / len) * it->ProjectileSpeed,
                                                     (dir.Y / len) * it->ProjectileSpeed};
                        }
                        else
                        {
                            proj.Velocity = Vector2f{0.0f, it->ProjectileSpeed};
                        }
                    }
                    else
                    {
                        proj.Velocity = Vector2f{0.0f, it->ProjectileSpeed};
                    }

                    m_turretBullets.push_back(proj);

                    SpawnExplosionParticles(spawnPos, Color{255, 80, 40, 255}, 12);
                    if (m_audioEnabled && m_bossLaserSfxId != 0)
                    {
                        context.Audio.PlaySfx(m_bossLaserSfxId, 55.0f, 1.45f);
                    }
                }

                if (m_registry.HasComponent<SpriteComponent>(it->EntityId))
                {
                    const Color baseCol = (it->BaseHP > 3)
                        ? BrickFactory::GetColorForHitPoints(it->BaseHP)
                        : Color{255, 45, 55, 255};
                    m_registry.GetComponent<SpriteComponent>(it->EntityId).Tint = baseCol;
                }
            }
            ++it;
        }
    }

    // Update Projectiles (unified GameScene paddle durability & feedback)
    if (m_turretBullets.empty()) return;

    for (auto it = m_turretBullets.begin(); it != m_turretBullets.end(); )
    {
        it->Position.X += it->Velocity.X * dt;
        it->Position.Y += it->Velocity.Y * dt;

        if (it->Position.Y > 450.0f || std::abs(it->Position.X) > 760.0f)
        {
            it = m_turretBullets.erase(it);
            continue;
        }

        if (hasPaddle)
        {
            const float projW = it->Size * 0.75f;
            const float projH = it->Size * 0.85f;

            const bool hitX = (it->Position.X + projW >= paddlePos.X - halfSize.X) &&
                              (it->Position.X - projW <= paddlePos.X + halfSize.X);
            const bool hitY = (it->Position.Y + projH >= paddlePos.Y - halfSize.Y) &&
                              (it->Position.Y - projH <= paddlePos.Y + halfSize.Y + 6.0f);

            if (hitX && hitY)
            {
                const Vector2f impactPos = it->Position;
                const float dmg = it->Damage;
                it = m_turretBullets.erase(it);

                DamagePaddle(dmg, impactPos, context);
                // OnPaddleShattered clears m_turretBullets; stop iterating immediately!
                if (IsPaddleExploding())
                {
                    return;
                }
                continue;
            }
        }

        ++it;
    }
}

void EditorScene::RenderTurrets(GameContext& context)
{
    // 1. Turret brick downward barrel & 0.45s muzzle warning pulse
    for (const auto& tb : m_turretBricks)
    {
        if (!m_registry.IsAlive(tb.EntityId) || !m_registry.HasComponent<Transform2D>(tb.EntityId))
            continue;
        if (m_registry.HasComponent<BrickComponent>(tb.EntityId) && m_registry.GetComponent<BrickComponent>(tb.EntityId).IsDescending)
            continue;

        const Vector2f pos = m_registry.GetComponent<Transform2D>(tb.EntityId).Position;

        const float triW = 14.0f;
        const float triH = 12.0f;
        std::vector<Vertex> iconVerts = {
            Vertex{pos.X - triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X + triW * 0.5f, pos.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + triH * 0.7f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);

        if (tb.ShootTimer <= 0.45f)
        {
            const float chargeRatio = 1.0f - (std::max(0.0f, tb.ShootTimer) / 0.45f);
            const float muzzleRadius = 8.0f + 6.0f * chargeRatio;
            context.Render.DrawCircle(muzzleRadius, Transform2D{Vector2f{pos.X, pos.Y + 14.0f}},
                                      Color{255, 60, 40, static_cast<uint8_t>(200 * chargeRatio)});
        }
    }

    // 2. Glowing red-to-gold triangle projectiles + outline + point light
    for (const auto& proj : m_turretBullets)
    {
        const Vector2f pos = proj.Position;
        const float w = proj.Size * 0.75f;
        const float h = proj.Size * 0.85f;

        std::vector<Vertex> verts = {
            Vertex{pos.X - w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X + w, pos.Y - h * 0.6f, Color{255, 50, 40, 245}, 0.0f, 0.0f},
            Vertex{pos.X, pos.Y + h * 0.8f, Color{255, 220, 80, 255}, 0.0f, 0.0f}
        };
        context.Render.DrawVertices(verts, PrimitiveType::Triangles);

        Color outlineCol{255, 240, 160, 255};
        context.Render.DrawLine(Vector2f{pos.X - w, pos.Y - h * 0.6f}, Vector2f{pos.X + w, pos.Y - h * 0.6f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X + w, pos.Y - h * 0.6f}, Vector2f{pos.X, pos.Y + h * 0.8f}, outlineCol, 2.0f);
        context.Render.DrawLine(Vector2f{pos.X, pos.Y + h * 0.8f}, Vector2f{pos.X - w, pos.Y - h * 0.6f}, outlineCol, 2.0f);

        context.Render.DrawPointLight(pos, 45.0f, Color{255, 60, 40, 200}, 0.8f, BlendMode::Add);
    }
}

