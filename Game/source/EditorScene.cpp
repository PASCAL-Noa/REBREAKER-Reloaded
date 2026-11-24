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
    std::string FormatOneDecimal(float val)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", val);
        return std::string(buf);
    }

    const char* GetHitPointsColorName(int hp)
    {
        switch (std::clamp(hp, 1, 9))
        {
        case 1: return "VERT EMERAUDE";
        case 2: return "JAUNE SOLAIRE";
        case 3: return "ROUGE CRAMOISI";
        case 4: return "ORANGE VIF";
        case 5: return "CYAN ELECTRIQUE";
        case 6: return "BLEU AZUR";
        case 7: return "VIOLET PLASMA";
        case 8: return "MAGENTA NEON";
        case 9: return "OR DIAMANT";
        default: return "";
        }
    }

    const char* GetTileTypeName(EditorTileType type)
    {
        switch (type)
        {
        case EditorTileType::Light:   return "1. LIGHT";
        case EditorTileType::Medium:  return "2. MEDIUM";
        case EditorTileType::Hard:    return "3. HARD";
        case EditorTileType::Special: return "*. BONUS";
        case EditorTileType::Metal:   return "M. METAL";
        case EditorTileType::Bomb:    return "B. BOMBE";
        case EditorTileType::Turret:  return "V. TOURELLE";
        case EditorTileType::Lantern: return "L. LANTERNE";
        default:                      return "VIDE";
        }
    }
}

EditorScene::EditorScene()
    : GameScene()
{
    m_audioEnabled = true;
    m_musicEnabled = true;
    m_renderGameUI = false; // Enabled dynamically during playtest
}

EditorScene::EditorScene(std::string editLevelId, bool returnToRobostock)
    : GameScene()
    , m_editingLevelId(std::move(editLevelId))
    , m_returnToRobostock(returnToRobostock)
{
    m_audioEnabled = true;
    m_musicEnabled = true;
    m_renderGameUI = false;
}

void EditorScene::OnInit(GameContext& context)
{
    // Initialize base GameScene (creates systems, camera, and m_uiController)
    m_renderGameUI = true;
    GameScene::OnInit(context);
    m_renderGameUI = false; // Keep hidden in edit mode

    // 1. Immediately wipe the campaign playlist so it CANNOT play campaign tracks in the editor
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
    m_paddleCrackSfxId = context.Resources.LoadResource("Resources/audio/sfx/brick_hit.wav");

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
            m_statusMessage = m_editingLevelId + " // " + m_levelData.GetLevelName();
            m_statusTimer = 2.5f;
        }
        else
        {
            m_editingLevelId.clear();
        }
    }

    if (!loadedExisting)
    {
        m_levelData.ProceduralGenerate(36, 0.40f, 1, 1337);
        m_statusMessage = "REBREAKER MAKER PRET";
        m_statusTimer = 2.5f;
    }

    context.Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);

    m_undoStack.clear();
    m_undoStack.push_back(m_levelData.CreateSnapshot());

    m_isPlaytesting = false;

    // Reset ball once on scene startup
    ResetBallAndPaddle();
}

void EditorScene::OnDestroy(GameContext& context)
{
    GameScene::OnDestroy(context);
}

bool EditorScene::IsGameOver() const
{
    if (m_isPaddleDestroyed)
    {
        return m_paddleDestroyedTimer <= 0.0f;
    }
    return false;
}

bool EditorScene::IsLevelCleared() const
{
    if (!m_isPlaytesting || IsCountdownActive() || m_isPaddleDestroyed)
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
        mp_context->Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
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
        m_isPaddleDestroyed = false;
        m_paddleDestroyedTimer = 0.0f;
        m_paddleDamageFlashTimer = 0.0f;
        m_sparkEmissionTimer = 0.0f;
        m_playtestElapsed = 0.0f;
        m_nextQueuedEventIdx = 0;

        if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
        {
            m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
        }
        if (mp_paddleSystem)
        {
            mp_paddleSystem->SetInputEnabled(true);
        }

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
        mp_context->Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
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
        m_statusMessage = "VICTOIRE ! TOUTES LES PAGES COMPLETEES !";
        m_statusTimer = 4.0f;
        m_statusColor = Color{80, 255, 140, 255};
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
        ResetBallAndPaddle();
        mp_context->Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
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
        mp_context->Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
    }
}

void EditorScene::OnUpdate(float dt, GameContext& context)
{
    mp_context = &context;
    m_pulseTimer += dt;
    if (m_statusTimer > 0.0f)
    {
        m_statusTimer -= dt;
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

        // Paddle damage flash recovery
        if (m_paddleDamageFlashTimer > 0.0f)
        {
            m_paddleDamageFlashTimer -= dt;
            if (m_paddleDamageFlashTimer <= 0.0f)
            {
                m_paddleDamageFlashTimer = 0.0f;
                if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle) && !m_isPaddleDestroyed)
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
                }
            }
        }

        // Multi-Page Sequential Progression: advance to next playable page before Victory triggers
        if (m_brickCount <= 0 && !IsCountdownActive() && !m_isPaddleDestroyed)
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

        // Scheduled Event Queue Execution (triggers Arena Event + Alarm!)
        if (!IsCountdownActive() && !m_isPaddleDestroyed)
        {
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

        if (m_lives <= 0 && !m_isPaddleDestroyed)
        {
            TriggerPaddleExplosion(context);
        }

        if (m_isPaddleDestroyed)
        {
            if (m_paddleDestroyedTimer > 0.0f)
            {
                m_paddleDestroyedTimer -= dt;
                m_sparkEmissionTimer += dt;
                if (m_sparkEmissionTimer >= 0.07f)
                {
                    m_sparkEmissionTimer = 0.0f;
                    Vector2f paddlePos{0.0f, 350.0f};
                    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
                    {
                        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
                    }
                    const float offsetRnd = (static_cast<float>(rand() % 100) - 50.0f) * 1.4f;
                    Color sparkCol{200, 235, 255, 200};
                    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
                    {
                        sparkCol = m_registry.GetComponent<LightSourceComponent>(m_paddle).LightColor;
                        sparkCol.a = 200;
                    }
                    SpawnExplosionParticles(paddlePos + Vector2f{offsetRnd, 0.0f}, sparkCol, 8);
                }
            }
        }
        else
        {
            UpdateTurrets(dt, context);
        }

        // Check if player clicked [STOP] in bottom-left
        auto [mx, my] = context.Input.GetMousePosition();
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        const float halfW = viewSize.X * 0.5f;
        const float halfH = viewSize.Y * 0.5f;
        const Vector2f worldMouse{mx - halfW, my - halfH};

        const Vector2f stopBtnPos{-halfW + 80.0f, halfH - 75.0f};
        const Vector2f btnSize{60.0f, 60.0f};
        if (std::abs(worldMouse.X - stopBtnPos.X) <= btnSize.X * 0.6f && std::abs(worldMouse.Y - stopBtnPos.Y) <= btnSize.Y * 0.6f)
        {
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                StopPlaytesting(context);
                return;
            }
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

        if (m_showSaveAsModal)
        {
            HandleSaveAsModalInput(context);
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
        // 1. In Playtest: GameScene renders the arena, ball, paddle, HUD (hearts, combo, countdown, and centered pause menu)
        m_renderGameUI = true;
        GameScene::OnRender(context);

        // 2. Render Turrets, Projectiles & Paddle Fracture Overlay in arena space (camera zoom 0.75)
        const auto& gameCam = m_registry.GetComponent<Camera2D>(m_camera);
        context.Render.SetCamera(gameCam);
        RenderTurrets(context);
        RenderPaddleDurabilityOverlay(context);

        // 3. Render clean Stop button in bottom-left when actively playing
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

        // Draw background theme covering 100% of the screen
        const Vector2f viewSize = context.Render.GetLogicalViewSize();
        context.Render.DrawRectangle(viewSize.X * 1.02f, viewSize.Y * 1.02f, Transform2D{Vector2f{0.0f, 0.0f}}, m_levelData.GetTheme().BgColor);

        // Draw Arena Boundaries, Blueprint Grid & Page Bar (dynamically adapts when panels fold)
        RenderGrid(context);

        // Draw Fullscreen Editor UI Components
        RenderToolbar(context);
        RenderPalette(context);
        RenderTabsAndPanels(context);

        if (m_showSaveAsModal)
        {
            RenderSaveAsModal(context);
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
        std::string banner = "VAGUE / PAGE " + std::to_string(m_playtestPageIndex + 1) + " / " + std::to_string(totalPages);
        Vector2f bPos{0.0f, -halfH + 100.0f};
        context.Render.DrawTextCentered(banner, m_fontId, 30.0f, Transform2D{bPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, a});
        context.Render.DrawTextCentered(banner, m_fontId, 30.0f, Transform2D{bPos}, Color{80, 240, 255, a});
    }
}

void EditorScene::HandleBrickCollision(Entity entity, Entity ballEntity, bool isLaser, bool isExplosion, int damage)
{
    GameScene::HandleBrickCollision(entity, ballEntity, isLaser, isExplosion, damage);
}

void EditorScene::ApplyEditorThemeToPlaytest()
{
    const auto& thm = m_levelData.GetTheme();
    m_bgColor = thm.BgColor;
    m_currentTheme.BgBase1 = thm.BgColor;
    m_currentTheme.BgBase2 = thm.BgColor;
    const float mult = std::clamp(thm.AmbientIntensity * 2.0f, 0.25f, 1.0f);
    m_currentTheme.AmbientLight = Color{
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.r) * mult, 20.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.g) * mult, 20.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.b) * mult, 20.0f, 255.0f)),
        255
    };
    m_ambientBackground.SetTheme(m_currentTheme);

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
    {
        auto& light = m_registry.GetComponent<LightSourceComponent>(m_paddle);
        light.LightColor = thm.PaddleConeColor;
        light.ConeAngleDeg = thm.PaddleConeAngle;
    }
}

void EditorScene::StartPlaytesting(GameContext& context)
{
    m_isPlaytesting = true;
    m_showDefeatModal = false;
    m_renderGameUI = true;
    m_isPaddleDestroyed = false;
    m_paddleDestroyedTimer = 0.0f;
    m_paddleDamageFlashTimer = 0.0f;
    m_sparkEmissionTimer = 0.0f;
    m_playtestElapsed = 0.0f;
    m_nextQueuedEventIdx = 0;

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(true);
    }

    m_statusMessage = "TEST EN COURS - CLIQUEZ [STOP] POUR REVENIR";
    m_statusTimer = 2.5f;
    m_statusColor = Color{80, 220, 255, 255};

    // Synchronize GameRules & DynamicEventSystem
    const bool manualOverride = context.Rules.GetRule(Rule::Gameplay::ManualOverride);
    const auto& rules = m_levelData.GetGamerules();
    if (!manualOverride)
    {
        context.Rules.SetRule(Rule::Gameplay::HazardZonesEnabled, rules.HazardZones);
        context.Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, rules.ArenaEvents);
        context.Rules.SetRule(Rule::Gameplay::InfiniteLives, rules.InfiniteLives);
    }

    // 1. Immediately abort any lingering ultimate abilities & visual malus
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

    // 2. Ensure campaign playlist is completely empty
    m_playlist.Clear();

    // 3. Build the ECS bricks starting from the first playable page in order
    m_playtestPageIndex = FindNextPlayablePage(0).value_or(0);
    RebuildPlayfieldFromPage(m_playtestPageIndex, context, true);
    m_pageBannerTimer = (m_levelData.GetPageCount() > 1) ? 2.2f : 0.0f;

    m_lives = 3;
    context.Events.Publish(LivesChangedEvent(m_lives));
    ResetBallAndPaddle();

    // 4. Start countdown state
    StartCountdown();

    // 5. Ensure editor theme track is playing
    context.Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);

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
        m_uiController->ShowState(GameUIState::Countdown);
    }

    if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
}

void EditorScene::StopPlaytesting(GameContext& context)
{
    m_isPlaytesting = false;
    m_showDefeatModal = false;
    m_renderGameUI = false;
    m_isPaddleDestroyed = false;
    m_paddleDestroyedTimer = 0.0f;
    m_paddleDamageFlashTimer = 0.0f;
    m_sparkEmissionTimer = 0.0f;
    m_pageBannerTimer = 0.0f;

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Colors::White;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(true);
    }

    m_statusMessage = "MODE EDITION RESTAURE";
    m_statusTimer = 2.0f;
    m_statusColor = Color{100, 255, 160, 255};

    // 1. Immediately abort and clear any active ultimates (Disco, Plasma, Lucky Drop)
    ClearActiveUltimates();
    ClearVisualMalus();
    RestoreTemporalAudio(0.05f);

    // 2. Clear campaign playlist and immediately restore editor theme music
    m_playlist.Clear();
    context.Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);

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
        m_statusMessage = "ACTION ANNULEE (CTRL+Z)";
        m_statusTimer = 1.5f;
    }
}

void EditorScene::SetTool(EditorTool tool)
{
    m_tool = tool;
    if (tool == EditorTool::Eraser)
    {
        m_statusMessage = "OUTIL GOMME ACTIF";
        m_statusColor = Color{237, 108, 99, 255};
    }
    else if (tool == EditorTool::Select)
    {
        m_statusMessage = "MODE SELECTION ACTIF - CLIQUEZ UNE BRIQUE";
        m_statusColor = Color{80, 230, 255, 255};
    }
    else
    {
        m_statusMessage = "OUTIL PINCEAU ACTIF";
        m_statusColor = Color{100, 255, 160, 255};
    }
    m_statusTimer = 1.8f;
}

void EditorScene::SetSelectedTile(EditorTileType tile)
{
    m_selectedTile = tile;
    m_tool = EditorTool::Paint;
    m_hasSelectedCell = false;
}

void EditorScene::ExitEditor(GameContext& context)
{
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
    m_saveAsPreviewId = lib.PeekNextLevelId();
    if (!m_levelData.GetLevelName().empty() && m_editingLevelId.empty())
    {
        m_saveAsNameBuffer = m_levelData.GetLevelName();
    }
    else
    {
        m_saveAsNameBuffer = "NIVEAU " + std::to_string(lib.GetLevels().size() + 1);
    }
    m_showSaveAsModal = true;
}

void EditorScene::ConfirmSaveAsModal(GameContext& context)
{
    CustomLevelLibrary lib;
    const std::string savedId = lib.SaveNewLevel(m_saveAsNameBuffer, m_levelData);
    m_showSaveAsModal = false;

    if (!savedId.empty())
    {
        m_editingLevelId = savedId;
        if (const auto* entry = lib.FindLevel(savedId))
        {
            m_currentSavePath = entry->FilePath;
        }
        m_levelData.SaveToFile("Resources/levels/custom_level.txt");
        m_statusMessage = "ENREGISTRE : " + savedId + " // " + m_levelData.GetLevelName();
        m_statusTimer = 3.2f;
        m_statusColor = Color{80, 255, 140, 255};
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
    }
    else
    {
        m_statusMessage = "ERREUR SAUVEGARDE !";
        m_statusTimer = 3.0f;
        m_statusColor = Color{255, 70, 70, 255};
    }
}

void EditorScene::HandleSaveAsModalInput(GameContext& context)
{
    const std::string& typed = context.Input.GetEnteredText();
    for (char ch : typed)
    {
        if (ch >= 32 && ch < 127 && ch != '|' && m_saveAsNameBuffer.size() < 20)
        {
            char upper = (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
            m_saveAsNameBuffer.push_back(upper);
        }
    }

    if (context.Input.IsKeyPress(KeyCode::Backspace) && !m_saveAsNameBuffer.empty())
    {
        m_saveAsNameBuffer.pop_back();
    }

    if (context.Input.IsKeyPress(KeyCode::Enter))
    {
        ConfirmSaveAsModal(context);
        return;
    }

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        m_showSaveAsModal = false;
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
            ConfirmSaveAsModal(context);
            return;
        }
        if (std::abs(worldMouse.X - cancelPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - cancelPos.Y) <= btnSz.Y * 0.5f)
        {
            m_showSaveAsModal = false;
            if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
            return;
        }
    }
}

void EditorScene::RenderSaveAsModal(GameContext& context)
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

    // Header + ID Badge
    context.Render.DrawRectangle(modalSize.X - 4.0f, 46.0f, Transform2D{Vector2f{0.0f, -100.0f}}, Color{36, 42, 56, 255});
    context.Render.DrawText("ENREGISTRER SOUS", m_fontId, 22.0f, Transform2D{Vector2f{-225.0f, -112.0f}}, Color{255, 215, 60, 255});

    Vector2f idBadgePos{165.0f, -100.0f};
    context.Render.DrawRectangle(120.0f, 28.0f, Transform2D{idBadgePos}, Color{18, 24, 34, 255});
    context.Render.DrawRectangleOutline(120.0f, 28.0f, Transform2D{idBadgePos}, Color{80, 220, 255, 200}, 1.5f);
    context.Render.DrawTextCentered(m_saveAsPreviewId, m_fontId, 18.0f, Transform2D{idBadgePos}, Color{80, 235, 255, 255});

    // Input Box
    Vector2f boxPos{0.0f, -8.0f};
    context.Render.DrawRectangle(430.0f, 54.0f, Transform2D{boxPos}, Color{14, 17, 24, 255});
    context.Render.DrawRectangleOutline(430.0f, 54.0f, Transform2D{boxPos}, Color{255, 215, 60, 240}, 2.0f);

    const bool blink = (std::fmod(m_pulseTimer * 2.2f, 2.0f) < 1.0f);
    std::string shown = m_saveAsNameBuffer + (blink ? "_" : " ");
    context.Render.DrawTextCentered(shown, m_fontId, 26.0f, Transform2D{boxPos}, Colors::White);

    // Confirm / Cancel Buttons
    const Vector2f okPos{-115.0f, 82.0f};
    const Vector2f cancelPos{115.0f, 82.0f};
    const Vector2f btnSz{195.0f, 48.0f};
    const bool okHover = (std::abs(worldMouse.X - okPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - okPos.Y) <= btnSz.Y * 0.5f);
    const bool cancelHover = (std::abs(worldMouse.X - cancelPos.X) <= btnSz.X * 0.5f && std::abs(worldMouse.Y - cancelPos.Y) <= btnSz.Y * 0.5f);

    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{okPos}, okHover ? Color{60, 175, 95, 255} : Color{45, 140, 75, 255});
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{okPos}, okHover ? Colors::White : Color{95, 200, 125, 255}, 1.5f);
    context.Render.DrawTextCentered("VALIDER", m_fontId, 21.0f, Transform2D{okPos}, Colors::White);

    context.Render.DrawRectangle(btnSz.X, btnSz.Y, Transform2D{cancelPos}, cancelHover ? Color{75, 50, 60, 255} : Color{48, 52, 64, 255});
    context.Render.DrawRectangleOutline(btnSz.X, btnSz.Y, Transform2D{cancelPos}, cancelHover ? Colors::White : Color{95, 102, 120, 255}, 1.5f);
    context.Render.DrawTextCentered("ANNULER", m_fontId, 21.0f, Transform2D{cancelPos}, Color{225, 230, 240, 255});
}

void EditorScene::SaveLevel(GameContext& context)
{
    if (!m_editingLevelId.empty())
    {
        CustomLevelLibrary lib;
        if (lib.UpdateExistingLevel(m_editingLevelId, m_levelData))
        {
            m_levelData.SaveToFile("Resources/levels/custom_level.txt");
            m_statusMessage = "SAUVEGARDE : " + m_editingLevelId + " // " + m_levelData.GetLevelName();
            m_statusTimer = 3.0f;
            m_statusColor = Color{80, 255, 140, 255};
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            return;
        }
    }

    // First save of an unnamed draft: open Save As modal so it gets Name + ID in Robostock
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
    m_statusMessage = "NIVEAU GENERE (" + std::to_string(m_levelData.GetBrickCount()) + " BRIQUES)";
    m_statusTimer = 2.0f;
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
    m_statusMessage = "FILE D'EVENEMENTS VIDEE";
    m_statusTimer = 2.0f;
    if (mp_context && m_switchSfxId != 0) mp_context->Audio.PlaySfx(m_switchSfxId, 16.0f);
}

void EditorScene::DeleteEvent(size_t index)
{
    PushUndo();
    m_levelData.RemoveEvent(index);
    m_statusMessage = "EVENEMENT SUPPRIME";
    m_statusTimer = 2.0f;
    if (mp_context && m_clickSfxId != 0) mp_context->Audio.PlaySfx(m_clickSfxId, 16.0f);
}

EditorScene::EditorGridLayout EditorScene::ComputeGridLayout(const Vector2f& viewSize) const
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = 74.0f;
    const float leftW = m_leftPanelOpen ? 360.0f : 44.0f;
    const float rightW = m_rightPanelOpen ? 410.0f : 44.0f;

    EditorGridLayout layout;
    layout.CanvasMinX = -halfW + leftW + 6.0f;
    layout.CanvasMaxX = halfW - rightW - 6.0f;
    layout.CanvasMinY = -halfH + toolbarH + 6.0f;
    layout.CanvasMaxY = halfH - 6.0f;
    layout.PageBarHeight = 64.0f;

    const float headerReserve = 44.0f;
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
            case EditorTileType::Metal:   bType = BrickType::Metal; break;
            case EditorTileType::Bomb:    bType = BrickType::Bomb; break;
            case EditorTileType::Turret:  bType = BrickType::Hard; break;
            case EditorTileType::Lantern: bType = BrickType::Lantern; break;
            default: break;
            }

            Entity bEntity = BrickFactory::Create(m_registry, pos.X, pos.Y, bType, isSpecial, m_brickTexId, 0, cell.HitPoints);

            // Forced specific power-up configuration
            if (cell.Type == EditorTileType::Special && !cell.RandomPowerUp && m_registry.HasComponent<BrickComponent>(bEntity))
            {
                auto& bComp = m_registry.GetComponent<BrickComponent>(bEntity);
                bComp.HasForcedPowerUp = true;
                bComp.ForcedPowerUp = cell.SpecificPowerUp;
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

    // Ctrl + Z shortcut for Undo
    if ((context.Input.IsKeyDown(KeyCode::LControl) || context.Input.IsKeyDown(KeyCode::RControl)) && context.Input.IsKeyPress(KeyCode::Z))
    {
        Undo();
        return;
    }

    // Playtest toggle shortcut [P]
    if (context.Input.IsKeyPress(KeyCode::P))
    {
        StartPlaytesting(context);
        return;
    }

    // Select tool toggle shortcut [S]
    if (!context.Input.IsKeyDown(KeyCode::LControl) && !context.Input.IsKeyDown(KeyCode::RControl) && context.Input.IsKeyPress(KeyCode::S))
    {
        SetTool(m_tool == EditorTool::Select ? EditorTool::Paint : EditorTool::Select);
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return;
    }

    // Gomme toggle shortcut [G]
    if (context.Input.IsKeyPress(KeyCode::G))
    {
        SetTool(m_tool == EditorTool::Eraser ? EditorTool::Paint : EditorTool::Eraser);
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

    // Dynamic Layout Dimensions (harmonized toolbarH = 74.0f everywhere)
    const float toolbarH = 74.0f;
    const Vector2f toolbarPos{0.0f, -halfH + toolbarH * 0.5f};
    const Vector2f btnSize{56.0f, 50.0f};
    const float rightPanelW = m_rightPanelOpen ? 410.0f : 44.0f;
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
                const float listStartY = -halfH + toolbarH + 312.0f;
                const float itemH = 52.0f;
                int targetDropIdx = -1;

                for (int vis = 0; vis < 6; ++vis)
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
                    m_statusMessage = "ORDRE MIS A JOUR";
                    m_statusTimer = 1.5f;
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
        Vector2f gommeBtnPos{-halfW + 468.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - gommeBtnPos.X) <= btnSize.X * 0.5f && leftClick)
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
        Vector2f viderBtnPos{-halfW + 684.0f, toolbarPos.Y};
        if (std::abs(worldMouse.X - viderBtnPos.X) <= btnSize.X * 0.5f && leftClick)
        {
            PushUndo();
            m_levelData.ClearGrid();
            m_hasSelectedCell = false;
            m_statusMessage = "PAGE EFFACEE";
            m_statusTimer = 2.0f;
            m_statusColor = Color{255, 100, 120, 255};
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

    // 2. Fullscreen Left Brick Palette Interaction (Touches left edge -halfW)
    const float leftPanelW = m_leftPanelOpen ? 360.0f : 44.0f;
    const Vector2f leftTogglePos = m_leftPanelOpen ? Vector2f{-halfW + 360.0f - 22.0f, -halfH + toolbarH + 35.0f}
                                                   : Vector2f{-halfW + 22.0f, -halfH + toolbarH + 35.0f};

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
                const float palStartY = -halfH + toolbarH + 68.0f;
                const float itemH = 76.0f;
                for (int i = 0; i < 9; ++i)
                {
                    float itemY = palStartY + static_cast<float>(i) * itemH + itemH * 0.5f;
                    if (std::abs(worldMouse.Y - itemY) <= itemH * 0.45f)
                    {
                        if (i == 8)
                        {
                            SetTool(EditorTool::Eraser);
                        }
                        else
                        {
                            const EditorTileType types[] = {
                                EditorTileType::Light,
                                EditorTileType::Medium,
                                EditorTileType::Hard,
                                EditorTileType::Special,
                                EditorTileType::Metal,
                                EditorTileType::Bomb,
                                EditorTileType::Turret,
                                EditorTileType::Lantern
                            };
                            SetSelectedTile(types[i]);
                        }
                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
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

    // 3. Fullscreen Right Panel Interaction (Touches right edge +halfW)
    const float tabH = 48.0f;
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
                float cntY = -halfH + toolbarH + 160.0f;
                if (std::abs(worldMouse.Y - cntY) <= 30.0f && leftClick)
                {
                    if (worldMouse.X < panelCenter.X - 50.0f) m_procBrickCount = std::max(12, m_procBrickCount - 6);
                    else if (worldMouse.X > panelCenter.X + 50.0f) m_procBrickCount = std::min(84, m_procBrickCount + 6);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float diffY = -halfH + toolbarH + 250.0f;
                if (std::abs(worldMouse.Y - diffY) <= 30.0f && leftClick)
                {
                    if (worldMouse.X < panelCenter.X - 50.0f) m_procDifficulty = std::max(0.1f, m_procDifficulty - 0.15f);
                    else if (worldMouse.X > panelCenter.X + 50.0f) m_procDifficulty = std::min(1.0f, m_procDifficulty + 0.15f);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float symY = -halfH + toolbarH + 340.0f;
                if (std::abs(worldMouse.Y - symY) <= 30.0f && leftClick)
                {
                    m_procSymmetry = (m_procSymmetry + 1) % 4;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float genY = -halfH + toolbarH + 450.0f;
                if (std::abs(worldMouse.Y - genY) <= 36.0f && leftClick)
                {
                    GenerateProceduralLevel();
                    return;
                }
                break;
            }
            case EditorTab::ThemeLighting:
            {
                float thmY = -halfH + toolbarH + 160.0f;
                if (std::abs(worldMouse.Y - thmY) <= 30.0f && leftClick)
                {
                    if (worldMouse.X < panelCenter.X) m_levelData.PrevThemePreset();
                    else m_levelData.NextThemePreset();
                    context.Audio.PlayMusic(m_levelData.GetTheme().MusicTrack, true);
                    if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 14.0f);
                }
                float ambY = -halfH + toolbarH + 250.0f;
                if (std::abs(worldMouse.Y - ambY) <= 30.0f && leftClick)
                {
                    float& amb = m_levelData.GetTheme().AmbientIntensity;
                    if (worldMouse.X < panelCenter.X - 50.0f) amb = std::max(0.10f, amb - 0.08f);
                    else if (worldMouse.X > panelCenter.X + 50.0f) amb = std::min(0.90f, amb + 0.08f);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float coneY = -halfH + toolbarH + 340.0f;
                if (std::abs(worldMouse.Y - coneY) <= 30.0f && leftClick)
                {
                    float& angle = m_levelData.GetTheme().PaddleConeAngle;
                    if (worldMouse.X < panelCenter.X - 50.0f) angle = std::max(20.0f, angle - 10.0f);
                    else if (worldMouse.X > panelCenter.X + 50.0f) angle = std::min(90.0f, angle + 10.0f);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
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
                float hzY = -halfH + toolbarH + 210.0f;
                if (std::abs(worldMouse.Y - hzY) <= 30.0f && leftClick)
                {
                    gr.HazardZones = !gr.HazardZones;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float arY = -halfH + toolbarH + 300.0f;
                if (std::abs(worldMouse.Y - arY) <= 30.0f && leftClick)
                {
                    gr.ArenaEvents = !gr.ArenaEvents;
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 12.0f);
                }
                float lfY = -halfH + toolbarH + 390.0f;
                if (std::abs(worldMouse.Y - lfY) <= 30.0f && leftClick)
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
                        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
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
                                    m_statusMessage = "BRIQUE [" + std::to_string(col + 1) + "," + std::to_string(row + 1) + "] SELECTIONNEE";
                                    m_statusTimer = 1.8f;
                                    m_statusColor = Color{80, 230, 255, 255};
                                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
                                }
                                else
                                {
                                    m_hasSelectedCell = false;
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

void EditorScene::RenderGrid(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const EditorGridLayout layout = ComputeGridLayout(viewSize);

    Vector2f canvasCenter{(layout.CanvasMinX + layout.CanvasMaxX) * 0.5f, (layout.CanvasMinY + layout.CanvasMaxY) * 0.5f};
    Vector2f canvasSize{layout.CanvasMaxX - layout.CanvasMinX, layout.CanvasMaxY - layout.CanvasMinY};

    // 1. Dedicated Dynamic Blueprint Canvas Plate
    context.Render.DrawRectangle(canvasSize.X, canvasSize.Y, Transform2D{canvasCenter}, Color{12, 18, 30, 240});
    context.Render.DrawRectangleOutline(canvasSize.X, canvasSize.Y, Transform2D{canvasCenter}, Color{50, 95, 160, 220}, 2.0f);

    // 2. Technical Header Tag at Canvas Top
    std::string toolTag = (m_tool == EditorTool::Select) ? "OUTIL: SELECTION [S]" : ((m_tool == EditorTool::Eraser) ? "OUTIL: GOMME [G]" : "OUTIL: PINCEAU");
    std::string dimStr = "CANVA D'EDITION (" + std::to_string(static_cast<int>(canvasSize.X)) + "x" + std::to_string(static_cast<int>(canvasSize.Y)) + ")  //  "
                       + "PAGE " + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount())
                       + "  //  " + toolTag;
    Color headerCol = (m_tool == EditorTool::Select) ? Color{110, 230, 255, 220} : ((m_tool == EditorTool::Eraser) ? Color{255, 145, 145, 220} : Color{110, 165, 225, 195});
    context.Render.DrawTextCentered(dimStr, m_fontId, 16.0f, Transform2D{Vector2f{canvasCenter.X, layout.CanvasMinY + 18.0f}}, headerCol);

    // 3. Localized CRT Scanlines & Blueprint Grid Lines across dynamic canvas
    for (float y = layout.CanvasMinY + 6.0f; y < layout.CanvasMaxY - 4.0f; y += 4.0f)
    {
        context.Render.DrawLine(Vector2f{layout.CanvasMinX + 2.0f, y}, Vector2f{layout.CanvasMaxX - 2.0f, y}, Color{0, 0, 0, 35}, 1.0f);
    }
    for (float x = layout.CanvasMinX + 16.0f; x < layout.CanvasMaxX - 10.0f; x += 32.0f)
    {
        context.Render.DrawLine(Vector2f{x, layout.CanvasMinY + 2.0f}, Vector2f{x, layout.CanvasMaxY - 2.0f}, Color{25, 55, 95, 20}, 1.0f);
    }

    // 4. Technical Corner Drafting Brackets on the dynamic canvas bounds
    const float bLen = 28.0f;
    Color bracketCol{80, 220, 255, 220};
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMinY}, Vector2f{layout.CanvasMinX + bLen, layout.CanvasMinY}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMinY}, Vector2f{layout.CanvasMinX, layout.CanvasMinY + bLen}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMinY}, Vector2f{layout.CanvasMaxX - bLen, layout.CanvasMinY}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMinY}, Vector2f{layout.CanvasMaxX, layout.CanvasMinY + bLen}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMaxY}, Vector2f{layout.CanvasMinX + bLen, layout.CanvasMaxY}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMinX, layout.CanvasMaxY}, Vector2f{layout.CanvasMinX, layout.CanvasMaxY - bLen}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMaxY}, Vector2f{layout.CanvasMaxX - bLen, layout.CanvasMaxY}, bracketCol, 2.5f);
    context.Render.DrawLine(Vector2f{layout.CanvasMaxX, layout.CanvasMaxY}, Vector2f{layout.CanvasMaxX, layout.CanvasMaxY - bLen}, bracketCol, 2.5f);

    // 5. Grid Cells & Placed Bricks (Fitted dynamically inside canvas, no paddle clutter)
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
                    ? ((m_tool == EditorTool::Select) ? Color{35, 65, 105, 190} : Color{45, 75, 120, 210})
                    : Color{20, 30, 48, 140};
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, emptyCol);
                context.Render.DrawRectangleOutline(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Color{45, 70, 110, 130}, 1.0f);
                context.Render.DrawRectangle(4.0f, 4.0f, Transform2D{cellCenter}, Color{70, 105, 155, 180});
            }
            else
            {
                const Color bCol = LevelData::GetDisplayColorForCell(cell);
                std::string label;
                switch (cell.Type)
                {
                case EditorTileType::Light:
                case EditorTileType::Medium:
                case EditorTileType::Hard:
                    label = std::to_string(cell.HitPoints);
                    break;
                case EditorTileType::Special:
                    label = cell.RandomPowerUp ? "*?" : "*F";
                    break;
                case EditorTileType::Metal:
                    label = "M";
                    break;
                case EditorTileType::Bomb:
                    label = "B";
                    break;
                case EditorTileType::Turret:
                    label = "T" + std::to_string(cell.HitPoints);
                    break;
                case EditorTileType::Lantern:
                    label = (cell.HitPoints > 1) ? ("L" + std::to_string(cell.HitPoints)) : "L";
                    break;
                default:
                    break;
                }

                // 3D Drop shadow
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter + Vector2f{3.0f, 3.0f}}, Color{0, 0, 0, 160});

                // Brick body
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, bCol);

                // Top & Left 3D bevel highlight lines
                const float halfBW = layout.BrickW * 0.5f;
                const float halfBH = layout.BrickH * 0.5f;
                context.Render.DrawLine(Vector2f{cellCenter.X - halfBW + 1.0f, cellCenter.Y - halfBH + 1.0f},
                                        Vector2f{cellCenter.X + halfBW - 1.0f, cellCenter.Y - halfBH + 1.0f},
                                        Color{255, 255, 255, 120}, 2.0f);
                context.Render.DrawLine(Vector2f{cellCenter.X - halfBW + 1.0f, cellCenter.Y - halfBH + 1.0f},
                                        Vector2f{cellCenter.X - halfBW + 1.0f, cellCenter.Y + halfBH - 1.0f},
                                        Color{255, 255, 255, 80}, 2.0f);

                // Bottom 3D shadow line
                context.Render.DrawLine(Vector2f{cellCenter.X - halfBW + 1.0f, cellCenter.Y + halfBH - 1.0f},
                                        Vector2f{cellCenter.X + halfBW - 1.0f, cellCenter.Y + halfBH - 1.0f},
                                        Color{0, 0, 0, 100}, 2.0f);

                // Turret downward barrel preview on grid
                if (cell.Type == EditorTileType::Turret)
                {
                    const float triW = 10.0f;
                    const float triH = 9.0f;
                    const Vector2f barCenter = cellCenter + Vector2f{halfBW * 0.52f, 0.0f};
                    std::vector<Vertex> iconVerts = {
                        Vertex{barCenter.X - triW * 0.5f, barCenter.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
                        Vertex{barCenter.X + triW * 0.5f, barCenter.Y - triH * 0.4f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
                        Vertex{barCenter.X, barCenter.Y + triH * 0.7f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
                    };
                    context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);
                }

                // Crisp outline
                context.Render.DrawRectangleOutline(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Colors::White, isHovered ? 2.5f : 1.2f);

                if (!label.empty())
                {
                    const float labelOffsetX = (cell.Type == EditorTileType::Turret) ? -6.0f : 0.0f;
                    context.Render.DrawTextCentered(label, m_fontId, 20.0f, Transform2D{cellCenter + Vector2f{labelOffsetX + 1.0f, 0.0f}}, Color{0, 0, 0, 200});
                    context.Render.DrawTextCentered(label, m_fontId, 20.0f, Transform2D{cellCenter + Vector2f{labelOffsetX, -1.5f}}, Color{18, 22, 32, 255});
                }
            }

            if (isSelectedCell)
            {
                const uint8_t glowAlpha = static_cast<uint8_t>(42.0f + 45.0f * selPulse);
                context.Render.DrawRectangle(layout.BrickW, layout.BrickH, Transform2D{cellCenter}, Color{80, 230, 255, glowAlpha});
                context.Render.DrawRectangleOutline(layout.BrickW + 6.0f, layout.BrickH + 6.0f, Transform2D{cellCenter}, Color{80, 230, 255, 255}, 3.0f);

                // Animated gold L-shaped corner brackets around selected brick
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
    const float wMoveR = 46.0f, wMoveL = 46.0f, wDel = 92.0f, wDup = 60.0f, wAdd = 94.0f;

    float cx = rightEdge - wMoveR * 0.5f;
    drawBtn(Vector2f{cx, barY}, wMoveR, ">>", Color{32, 40, 56, 255}, Color{75, 100, 145, 220}, (activeIdx + 1 < pageCount) ? Colors::White : Color{90, 100, 115, 140});
    cx -= (wMoveR * 0.5f + gap + wMoveL * 0.5f);
    drawBtn(Vector2f{cx, barY}, wMoveL, "<<", Color{32, 40, 56, 255}, Color{75, 100, 145, 220}, (activeIdx > 0) ? Colors::White : Color{90, 100, 115, 140});
    cx -= (wMoveL * 0.5f + gap + wDel * 0.5f);
    drawBtn(Vector2f{cx, barY}, wDel, "- SUPPR", (pageCount > 1) ? Color{68, 28, 36, 255} : Color{28, 30, 38, 200}, (pageCount > 1) ? Color{210, 75, 90, 230} : Color{50, 54, 65, 150}, (pageCount > 1) ? Color{255, 195, 200, 255} : Color{95, 100, 115, 140});
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

    if (!leftClick) return true; // Consume hover/click inside bar region

    const size_t pageCount = m_levelData.GetPageCount();
    const size_t activeIdx = m_levelData.GetActivePageIndex();
    const float btnH = 38.0f;
    auto hitBtn = [&](const Vector2f& pos, float w) {
        return std::abs(worldMouse.X - pos.X) <= w * 0.5f && std::abs(worldMouse.Y - pos.Y) <= btnH * 0.5f;
    };

    const float rightEdge = layout.CanvasMaxX - 18.0f;
    const float gap = 6.0f;
    const float wMoveR = 46.0f, wMoveL = 46.0f, wDel = 92.0f, wDup = 60.0f, wAdd = 94.0f;

    float cx = rightEdge - wMoveR * 0.5f;
    if (hitBtn(Vector2f{cx, barY}, wMoveR))
    {
        if (m_levelData.MoveActivePageRight())
        {
            PushUndo();
            m_hasSelectedCell = false;
            m_statusMessage = "PAGE DEPLACEE A DROITE (" + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount()) + ")";
            m_statusTimer = 1.8f;
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
            m_statusMessage = "PAGE DEPLACEE A GAUCHE (" + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount()) + ")";
            m_statusTimer = 1.8f;
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
            m_statusMessage = "PAGE SUPPRIMEE";
            m_statusTimer = 1.8f;
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
        m_statusMessage = "PAGE DUPLIQUEE (" + std::to_string(m_levelData.GetPageCount()) + " PAGES)";
        m_statusTimer = 1.8f;
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
        return true;
    }

    cx -= (wDup * 0.5f + gap + wAdd * 0.5f);
    if (hitBtn(Vector2f{cx, barY}, wAdd))
    {
        PushUndo();
        m_levelData.AddPage(false);
        m_hasSelectedCell = false;
        m_statusMessage = "NOUVELLE PAGE AJOUTEE (" + std::to_string(m_levelData.GetPageCount()) + " PAGES)";
        m_statusTimer = 1.8f;
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
    // Force identity center-aligned UI camera (pos: 0, 0, zoom: 1.0)
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

    // Dim background
    context.Render.DrawRectangle(viewSize.X * 1.5f, viewSize.Y * 1.5f, Transform2D{Vector2f{0.0f, 0.0f}}, Color{10, 12, 16, 210});

    // JetBrains styled Dialog Window
    const Vector2f modalSize{640.0f, 320.0f};
    const Vector2f modalPos{0.0f, 0.0f};

    // Window frame
    context.Render.DrawRectangle(modalSize.X, modalSize.Y, Transform2D{modalPos}, Color{30, 31, 34, 255});
    context.Render.DrawRectangleOutline(modalSize.X, modalSize.Y, Transform2D{modalPos}, Color{78, 82, 92, 255}, 2.0f);

    // Header bar
    const float headerH = 52.0f;
    const Vector2f headerPos{0.0f, modalPos.Y - modalSize.Y * 0.5f + headerH * 0.5f};
    context.Render.DrawRectangle(modalSize.X - 4.0f, headerH, Transform2D{headerPos}, Color{43, 45, 48, 255});
    context.Render.DrawLine(Vector2f{-modalSize.X * 0.5f, headerPos.Y + headerH * 0.5f},
                            Vector2f{modalSize.X * 0.5f, headerPos.Y + headerH * 0.5f},
                            Color{60, 64, 72, 255}, 1.5f);

    // Stop icon + Title in header
    DrawIconStop(context, Vector2f{-230.0f, headerPos.Y}, 20.0f, Color{219, 88, 96, 255});
    context.Render.DrawTextCentered("TEST TERMINE - NIVEAU NON COMPLETE", m_fontId, 22.0f, Transform2D{Vector2f{20.0f, headerPos.Y}}, Color{235, 238, 245, 255});

    // Body texts
    context.Render.DrawTextCentered("Vous avez epuise toutes vos vies lors du playtest.", m_fontId, 21.0f, Transform2D{Vector2f{0.0f, modalPos.Y - 35.0f}}, Color{210, 215, 225, 255});
    context.Render.DrawTextCentered("Que souhaitez-vous faire maintenant ?", m_fontId, 19.0f, Transform2D{Vector2f{0.0f, modalPos.Y}}, Color{160, 165, 178, 255});

    // Buttons
    const Vector2f btn1Pos{-150.0f, modalPos.Y + 75.0f};
    const Vector2f btn2Pos{150.0f, modalPos.Y + 75.0f};
    const Vector2f btnSize{270.0f, 54.0f};

    bool btn1Hover = (std::abs(worldMouse.X - btn1Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn1Pos.Y) <= btnSize.Y * 0.5f);
    bool btn2Hover = (std::abs(worldMouse.X - btn2Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn2Pos.Y) <= btnSize.Y * 0.5f);

    // Button 1: REJOUER LE NIVEAU (JetBrains green button)
    Color btn1Bg = btn1Hover ? Color{65, 175, 95, 255} : Color{50, 145, 75, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btn1Pos}, btn1Bg);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btn1Pos}, btn1Hover ? Colors::White : Color{89, 168, 105, 255}, 1.5f);
    DrawIconPlay(context, Vector2f{btn1Pos.X - 95.0f, btn1Pos.Y}, 20.0f, Colors::White);
    context.Render.DrawTextCentered("REJOUER [R / ESPACE]", m_fontId, 18.0f, Transform2D{Vector2f{btn1Pos.X + 15.0f, btn1Pos.Y}}, Colors::White);

    // Button 2: RETOUR EN MODE EDIT (JetBrains neutral slate button)
    Color btn2Bg = btn2Hover ? Color{60, 65, 76, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{btn2Pos}, btn2Bg);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{btn2Pos}, btn2Hover ? Colors::White : Color{90, 95, 108, 255}, 1.5f);
    DrawIconUndo(context, Vector2f{btn2Pos.X - 95.0f, btn2Pos.Y}, 20.0f, Color{88, 157, 246, 255});
    context.Render.DrawTextCentered("RETOUR EDIT [ECHAP]", m_fontId, 18.0f, Transform2D{Vector2f{btn2Pos.X + 15.0f, btn2Pos.Y}}, Color{230, 235, 245, 255});
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
        // 1. REJOUER LE NIVEAU
        if (std::abs(worldMouse.X - btn1Pos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - btn1Pos.Y) <= btnSize.Y * 0.5f)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            m_showDefeatModal = false;
            StartPlaytesting(context);
            return;
        }

        // 2. RETOUR EDIT
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
    case EditorTileType::Light:   return Color{60, 220, 90, 255};    // Green (1 PV)
    case EditorTileType::Medium:  return Color{255, 220, 40, 255};   // Yellow (2 PV)
    case EditorTileType::Hard:    return Color{240, 50, 60, 255};    // Red (3 PV)
    case EditorTileType::Special: return Color{255, 40, 240, 255};   // Magenta / Bonus
    case EditorTileType::Metal:   return Color{180, 195, 210, 255};  // Steel Silver
    case EditorTileType::Bomb:    return Color{255, 85, 25, 255};    // Explosive Orange
    case EditorTileType::Turret:  return Color{225, 40, 75, 255};    // Tactical Crimson
    case EditorTileType::Lantern: return Color{255, 235, 120, 255};  // Warm Amber
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

    const Vector2f stopBtnPos{-halfW + 80.0f, halfH - 75.0f};
    const Vector2f btnSize{60.0f, 60.0f};
    bool isHovered = (std::abs(worldMouse.X - stopBtnPos.X) <= btnSize.X * 0.6f &&
                      std::abs(worldMouse.Y - stopBtnPos.Y) <= btnSize.Y * 0.6f);

    // Compensate for CRT curvature shader in playtest mode
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

    // Drop shadow
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{drawPos + Vector2f{3.0f, 3.0f}}, Color{0, 0, 0, 180});

    // Button body
    Color bg = isHovered ? Color{235, 50, 65, 255} : Color{195, 38, 52, 240};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{drawPos}, bg);

    // 3D Bevel highlight
    context.Render.DrawLine(Vector2f{drawPos.X - btnSize.X * 0.5f, drawPos.Y - btnSize.Y * 0.5f},
                            Vector2f{drawPos.X + btnSize.X * 0.5f, drawPos.Y - btnSize.Y * 0.5f},
                            Color{255, 140, 150, 200}, 2.0f);
    context.Render.DrawLine(Vector2f{drawPos.X - btnSize.X * 0.5f, drawPos.Y - btnSize.Y * 0.5f},
                            Vector2f{drawPos.X - btnSize.X * 0.5f, drawPos.Y + btnSize.Y * 0.5f},
                            Color{255, 140, 150, 200}, 2.0f);

    // Outer border
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{drawPos}, isHovered ? Colors::White : Color{255, 110, 125, 200}, isHovered ? 2.5f : 1.5f);

    // Stop icon inside
    DrawIconStop(context, drawPos, 22.0f, Colors::White);

    // Tooltip pill next to the button when hovered
    if (isHovered)
    {
        Vector2f tipPos{drawPos.X + 130.0f, drawPos.Y};
        context.Render.DrawRectangle(190.0f, 36.0f, Transform2D{tipPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 160});
        context.Render.DrawRectangle(190.0f, 36.0f, Transform2D{tipPos}, Color{30, 33, 40, 250});
        context.Render.DrawRectangleOutline(190.0f, 36.0f, Transform2D{tipPos}, Color{90, 95, 110, 220}, 1.5f);
        context.Render.DrawTextCentered("STOP SIMULATION [P]", m_fontId, 16.0f, Transform2D{tipPos}, Colors::White);
    }
}

void EditorScene::RenderToolbar(GameContext& context)
{
    // Force identity center-aligned UI camera (pos: 0, 0, zoom: 1.0)
    Camera2D uiCamera{};
    uiCamera.Position = Vector2f{0.0f, 0.0f};
    uiCamera.Zoom = 1.0f;
    uiCamera.Rotation = 0.0f;
    context.Render.SetCamera(uiCamera);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = 74.0f;
    const Vector2f barPos{0.0f, -halfH + toolbarH * 0.5f};

    // Fullscreen Top Toolbar (JetBrains dark theme #1E1F22)
    context.Render.DrawRectangle(viewSize.X * 1.02f, toolbarH, Transform2D{barPos}, Color{30, 31, 34, 255});
    context.Render.DrawLine(Vector2f{-halfW, barPos.Y + toolbarH * 0.5f},
                            Vector2f{halfW, barPos.Y + toolbarH * 0.5f},
                            Color{60, 64, 72, 255}, 1.5f);

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    // Left: Rebreaker Maker logo / badge
    Vector2f logoPos{-halfW + 120.0f, barPos.Y};
    context.Render.DrawTextCentered("REBREAKER", m_fontId, 22.0f, Transform2D{logoPos}, Color{255, 215, 60, 255});
    // Vertical separator
    context.Render.DrawLine(Vector2f{-halfW + 220.0f, barPos.Y - 20.0f},
                            Vector2f{-halfW + 220.0f, barPos.Y + 20.0f},
                            Color{65, 70, 80, 255}, 1.5f);

    // Button dimensions
    const Vector2f btnSize{56.0f, 50.0f};
    std::string hoveredTooltip = "";

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
        if (playHover) hoveredTooltip = "Arreter le test [Echap / Couper]";
    }
    else
    {
        Color playCol = playHover ? Color{65, 175, 95, 255} : Color{50, 145, 75, 255};
        context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{playPos}, playCol);
        context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{playPos}, playHover ? Colors::White : Color{89, 168, 105, 255}, 1.5f);
        DrawIconPlay(context, playPos, 22.0f, Colors::White);
        if (playHover) hoveredTooltip = "Lancer le test du niveau [P]";
    }

    if (m_isPlaytesting)
    {
        // When playtesting, display playtest badge & status
        Vector2f testBadgePos{0.0f, barPos.Y};
        context.Render.DrawTextCentered("TEST DU NIVEAU EN COURS", m_fontId, 22.0f, Transform2D{testBadgePos}, Color{80, 220, 255, 255});
        if (!hoveredTooltip.empty())
        {
            Vector2f tipPos{halfW - 300.0f, barPos.Y};
            context.Render.DrawTextCentered(hoveredTooltip, m_fontId, 18.0f, Transform2D{tipPos}, Color{255, 225, 100, 255});
        }
        return;
    }

    // 2. Undo Button
    Vector2f undoPos{-halfW + 336.0f, barPos.Y};
    bool undoHover = (std::abs(worldMouse.X - undoPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - undoPos.Y) <= btnSize.Y * 0.5f);
    Color undoCol = undoHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{undoPos}, undoCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{undoPos}, undoHover ? Colors::White : Color{68, 73, 85, 255}, 1.5f);
    DrawIconUndo(context, undoPos, 24.0f, Color{88, 157, 246, 255});
    if (undoHover) hoveredTooltip = "Annuler la derniere action [Ctrl+Z]";

    // 3. Select / Inspect Tool Button
    Vector2f selectPos{-halfW + 402.0f, barPos.Y};
    bool selectHover = (std::abs(worldMouse.X - selectPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - selectPos.Y) <= btnSize.Y * 0.5f);
    bool isSelectActive = (m_tool == EditorTool::Select);
    Color selectCol = isSelectActive ? Color{35, 62, 85, 255} : (selectHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255});
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{selectPos}, selectCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{selectPos}, isSelectActive ? Color{80, 230, 255, 255} : (selectHover ? Colors::White : Color{68, 73, 85, 255}), isSelectActive ? 2.5f : 1.5f);
    DrawIconSelect(context, selectPos, 24.0f, isSelectActive ? Color{80, 230, 255, 255} : Color{160, 220, 255, 255});
    if (selectHover) hoveredTooltip = "Outil Selection / Inspecter brique [S]";

    // 4. Eraser (Gomme) Button
    Vector2f gommePos{-halfW + 468.0f, barPos.Y};
    bool gommeHover = (std::abs(worldMouse.X - gommePos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - gommePos.Y) <= btnSize.Y * 0.5f);
    bool isGommeActive = (m_tool == EditorTool::Eraser);
    Color gommeCol = isGommeActive ? Color{70, 45, 55, 255} : (gommeHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255});
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{gommePos}, gommeCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{gommePos}, isGommeActive ? Color{237, 108, 99, 255} : (gommeHover ? Colors::White : Color{68, 73, 85, 255}), isGommeActive ? 2.5f : 1.5f);
    DrawIconEraser(context, gommePos, 24.0f, Colors::White);
    if (gommeHover) hoveredTooltip = "Outil Gomme / Effacer brique [G]";

    // Separator line
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
    if (saveHover) hoveredTooltip = "Sauvegarder le niveau actuel";

    // 6. Save As Button
    Vector2f saveAsPos{-halfW + 618.0f, barPos.Y};
    bool saveAsHover = (std::abs(worldMouse.X - saveAsPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - saveAsPos.Y) <= btnSize.Y * 0.5f);
    Color saveAsCol = saveAsHover ? Color{55, 60, 72, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{saveAsPos}, saveAsCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{saveAsPos}, saveAsHover ? Colors::White : Color{68, 73, 85, 255}, 1.5f);
    DrawIconFloppyPlus(context, saveAsPos, 24.0f, Colors::White);
    if (saveAsHover) hoveredTooltip = "Enregistrer sous un nouveau fichier";

    // 7. Clear Button
    Vector2f clearPos{-halfW + 684.0f, barPos.Y};
    bool clearHover = (std::abs(worldMouse.X - clearPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - clearPos.Y) <= btnSize.Y * 0.5f);
    Color clearCol = clearHover ? Color{80, 35, 45, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{clearPos}, clearCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{clearPos}, clearHover ? Color{219, 88, 96, 255} : Color{68, 73, 85, 255}, 1.5f);
    DrawIconTrash(context, clearPos, 24.0f, Color{219, 88, 96, 255});
    if (clearHover) hoveredTooltip = "Vider toute la grille";

    // Separator line
    context.Render.DrawLine(Vector2f{-halfW + 726.0f, barPos.Y - 20.0f},
                            Vector2f{-halfW + 726.0f, barPos.Y + 20.0f},
                            Color{65, 70, 80, 255}, 1.5f);

    // Active Level ID + Name badge (when saved in Robostock)
    if (!m_editingLevelId.empty())
    {
        Vector2f lvlBadgePos{-halfW + 905.0f, barPos.Y};
        std::string badgeTxt = m_editingLevelId + "  " + m_levelData.GetLevelName();
        context.Render.DrawRectangle(310.0f, 38.0f, Transform2D{lvlBadgePos}, Color{20, 26, 38, 255});
        context.Render.DrawRectangleOutline(310.0f, 38.0f, Transform2D{lvlBadgePos}, Color{80, 220, 255, 190}, 1.5f);
        context.Render.DrawTextCentered(badgeTxt, m_fontId, 18.0f, Transform2D{lvlBadgePos}, Color{190, 240, 255, 255});
    }

    // 7. Quit Button (Far Right)
    Vector2f quitPos{halfW - 55.0f, barPos.Y};
    bool quitHover = (std::abs(worldMouse.X - quitPos.X) <= btnSize.X * 0.5f && std::abs(worldMouse.Y - quitPos.Y) <= btnSize.Y * 0.5f);
    Color quitCol = quitHover ? Color{65, 40, 48, 255} : Color{43, 45, 48, 255};
    context.Render.DrawRectangle(btnSize.X, btnSize.Y, Transform2D{quitPos}, quitCol);
    context.Render.DrawRectangleOutline(btnSize.X, btnSize.Y, Transform2D{quitPos}, quitHover ? Color{219, 88, 96, 255} : Color{68, 73, 85, 255}, 1.5f);
    DrawIconExit(context, quitPos, 22.0f, Color{206, 210, 216, 255});
    if (quitHover) hoveredTooltip = "Quitter l'editeur [Echap]";

    // Status / Tooltip Message Display in Center-Right
    Vector2f statusPos{halfW - 360.0f, barPos.Y};
    if (!hoveredTooltip.empty())
    {
        context.Render.DrawTextCentered(hoveredTooltip, m_fontId, 20.0f, Transform2D{statusPos}, Color{255, 225, 100, 255});
    }
    else
    {
        context.Render.DrawTextCentered(m_statusMessage, m_fontId, 20.0f, Transform2D{statusPos}, m_statusColor);
    }
}

void EditorScene::RenderPalette(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = 74.0f;
    const float palH = viewSize.Y - toolbarH;

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    if (m_leftPanelOpen)
    {
        const float sideWidth = 360.0f;
        Vector2f palCenter{-halfW + sideWidth * 0.5f, -halfH + toolbarH + palH * 0.5f};
        Vector2f palSize{sideWidth, palH};

        // Modern dark panel background with outer border
        context.Render.DrawRectangle(palSize.X, palSize.Y, Transform2D{palCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{-halfW + sideWidth, -halfH + toolbarH},
                                Vector2f{-halfW + sideWidth, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        // Header Title
        Vector2f titlePos{-halfW + 140.0f, -halfH + toolbarH + 35.0f};
        context.Render.DrawTextCentered("PALETTE DE BRIQUES", m_fontId, 24.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

        // Header Collapse Tab Button on right edge
        Vector2f togglePos{-halfW + sideWidth - 22.0f, -halfH + toolbarH + 35.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 16.0f, false /* left */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        struct PaletteEntry
        {
            EditorTileType Type;
            const char* Label;
        };

        const PaletteEntry items[9] = {
            {EditorTileType::Light,   "1. LIGHT"},
            {EditorTileType::Medium,  "2. MEDIUM"},
            {EditorTileType::Hard,    "3. HARD"},
            {EditorTileType::Special, "*. BONUS"},
            {EditorTileType::Metal,   "M. METAL"},
            {EditorTileType::Bomb,    "B. BOMBE"},
            {EditorTileType::Turret,  "V. TOURELLE"},
            {EditorTileType::Lantern, "L. LANTERNE"},
            {EditorTileType::Empty,   "X. GOMME"}
        };

        const float palStartY = -halfH + toolbarH + 68.0f;
        const float itemH = 76.0f;
        const float cardW = sideWidth - 28.0f;

        for (int i = 0; i < 9; ++i)
        {
            float itemY = palStartY + static_cast<float>(i) * itemH + itemH * 0.5f;
            Vector2f itemPos{palCenter.X, itemY};
            bool isHover = (std::abs(worldMouse.X - itemPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - itemPos.Y) <= itemH * 0.45f);
            bool isSelected = (i == 8) ? (m_tool == EditorTool::Eraser) : (m_tool == EditorTool::Paint && m_selectedTile == items[i].Type);

            // Card body with 3D relief
            Color bg = isSelected ? Color{38, 52, 76, 255} : (isHover ? Color{30, 36, 50, 255} : Color{25, 29, 39, 255});
            context.Render.DrawRectangle(cardW, itemH - 6.0f, Transform2D{itemPos}, bg);

            // 3D bevel top highlight line
            context.Render.DrawLine(Vector2f{itemPos.X - cardW * 0.5f + 1.0f, itemPos.Y - (itemH - 6.0f) * 0.5f + 1.0f},
                                    Vector2f{itemPos.X + cardW * 0.5f - 1.0f, itemPos.Y - (itemH - 6.0f) * 0.5f + 1.0f},
                                    Color{255, 255, 255, static_cast<uint8_t>(isSelected ? 60 : 25)}, 1.5f);

            // Outer border
            context.Render.DrawRectangleOutline(cardW, itemH - 6.0f, Transform2D{itemPos}, isSelected ? Color{255, 215, 60, 255} : (isHover ? Color{120, 150, 190, 200} : Color{48, 54, 70, 200}), isSelected ? 2.0f : 1.0f);

            // Live preset data for brick preview & description
            const EditorCellData preset = (i < 8) ? m_levelData.GetPalettePreset(items[i].Type) : EditorCellData{};
            std::string desc;
            if (i == 8)
            {
                desc = "Effaceur brique";
            }
            else
            {
                switch (items[i].Type)
                {
                case EditorTileType::Light:
                case EditorTileType::Medium:
                case EditorTileType::Hard:
                    desc = std::to_string(preset.HitPoints) + " PV - " + GetHitPointsColorName(preset.HitPoints);
                    break;
                case EditorTileType::Special:
                    desc = preset.RandomPowerUp
                        ? "Capsule Aleatoire"
                        : ("Fixe: " + LevelData::GetPowerUpDisplayName(preset.SpecificPowerUp));
                    break;
                case EditorTileType::Metal:
                    desc = "Incassable";
                    break;
                case EditorTileType::Bomb:
                    desc = "Explosion AOE";
                    break;
                case EditorTileType::Turret:
                    desc = std::to_string(preset.HitPoints) + " PV | Tir " + FormatOneDecimal(preset.TurretCooldown) + "s";
                    break;
                case EditorTileType::Lantern:
                    desc = std::to_string(preset.HitPoints) + " PV - Emetteur";
                    break;
                default:
                    desc = "";
                    break;
                }
            }

            // Large brick preview block
            Vector2f iconPos{itemPos.X - cardW * 0.35f, itemPos.Y};
            if (i == 8)
            {
                DrawIconEraser(context, iconPos, 28.0f, Colors::White);
            }
            else
            {
                Color bCol = LevelData::GetDisplayColorForCell(preset);
                // Preview brick shadow & body
                context.Render.DrawRectangle(60.0f, 26.0f, Transform2D{iconPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 140});
                context.Render.DrawRectangle(60.0f, 26.0f, Transform2D{iconPos}, bCol);
                // 3D bevel on preview
                context.Render.DrawLine(Vector2f{iconPos.X - 30.0f, iconPos.Y - 12.0f}, Vector2f{iconPos.X + 30.0f, iconPos.Y - 12.0f}, Color{255, 255, 255, 120}, 1.5f);
                context.Render.DrawRectangleOutline(60.0f, 26.0f, Transform2D{iconPos}, Colors::White, 1.2f);

                if (items[i].Type == EditorTileType::Turret)
                {
                    std::vector<Vertex> iconVerts = {
                        Vertex{iconPos.X - 6.0f, iconPos.Y - 4.0f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
                        Vertex{iconPos.X + 6.0f, iconPos.Y - 4.0f, Color{255, 230, 230, 240}, 0.0f, 0.0f},
                        Vertex{iconPos.X, iconPos.Y + 7.0f, Color{255, 255, 255, 255}, 0.0f, 0.0f}
                    };
                    context.Render.DrawVertices(iconVerts, PrimitiveType::Triangles);
                }
            }

            // Labels
            Vector2f namePos{itemPos.X - cardW * 0.14f, itemPos.Y - 14.0f};
            context.Render.DrawText(items[i].Label, m_fontId, 20.0f, Transform2D{namePos}, isSelected ? Color{255, 225, 75, 255} : Colors::White);

            Vector2f descPos{itemPos.X - cardW * 0.14f, itemPos.Y + 11.0f};
            context.Render.DrawText(desc, m_fontId, 15.0f, Transform2D{descPos}, Color{165, 185, 210, 230});
        }

        // Page & Brick count indicator at bottom of palette
        Vector2f statsPos{palCenter.X, halfH - 35.0f};
        std::string statsTxt = "PAGE " + std::to_string(m_levelData.GetActivePageIndex() + 1) + "/" + std::to_string(m_levelData.GetPageCount())
                             + " : " + std::to_string(m_levelData.GetBrickCount()) + " BRIQUES";
        context.Render.DrawTextCentered(statsTxt, m_fontId, 19.0f, Transform2D{statsPos}, Color{100, 230, 255, 240});
    }
    else
    {
        // Collapsed Rail (44px wide)
        const float railW = 44.0f;
        Vector2f railCenter{-halfW + railW * 0.5f, -halfH + toolbarH + palH * 0.5f};
        context.Render.DrawRectangle(railW, palH, Transform2D{railCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{-halfW + railW, -halfH + toolbarH},
                                Vector2f{-halfW + railW, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        // Expand Button at top
        Vector2f togglePos{-halfW + railW * 0.5f, -halfH + toolbarH + 35.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 16.0f, true /* right */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        // Active tool preview block
        Vector2f previewPos{-halfW + railW * 0.5f, -halfH + toolbarH + 95.0f};
        if (m_tool == EditorTool::Eraser)
        {
            DrawIconEraser(context, previewPos, 26.0f, Colors::White);
        }
        else if (m_tool == EditorTool::Select)
        {
            DrawIconSelect(context, previewPos, 24.0f, Color{80, 230, 255, 255});
        }
        else
        {
            Color bCol = LevelData::GetDisplayColorForCell(m_levelData.GetPalettePreset(m_selectedTile));
            context.Render.DrawRectangle(30.0f, 18.0f, Transform2D{previewPos}, bCol);
            context.Render.DrawRectangleOutline(30.0f, 18.0f, Transform2D{previewPos}, Colors::White, 1.2f);
        }
    }
}

void EditorScene::RenderTabsAndPanels(GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float toolbarH = 74.0f;
    const float panelH = viewSize.Y - toolbarH;

    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f worldMouse{mx - halfW, my - halfH};

    if (m_rightPanelOpen)
    {
        const float sideWidth = 410.0f;
        Vector2f panelCenter{halfW - sideWidth * 0.5f, -halfH + toolbarH + panelH * 0.5f};
        Vector2f panelSize{sideWidth, panelH};

        // Panel background & 3D left boundary
        context.Render.DrawRectangle(panelSize.X, panelSize.Y, Transform2D{panelCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{halfW - sideWidth, -halfH + toolbarH},
                                Vector2f{halfW - sideWidth, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        const float tabH = 48.0f;
        const float tabY = -halfH + toolbarH + tabH * 0.5f + 10.0f;

        // Collapse Button on left edge of tab bar
        Vector2f togglePos{halfW - sideWidth + 20.0f, tabY};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 16.0f, true /* right */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        // 5 Segmented Tabs
        const char* tabNames[5] = {"INSP", "PROC", "THEME", "EVENTS", "REGLES"};
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

            // 3D Bevel highlight
            context.Render.DrawLine(Vector2f{tPos.X - (tabW - 4.0f) * 0.5f, tPos.Y - (tabH - 4.0f) * 0.5f},
                                    Vector2f{tPos.X + (tabW - 4.0f) * 0.5f, tPos.Y - (tabH - 4.0f) * 0.5f},
                                    Color{255, 255, 255, static_cast<uint8_t>(isCurTab ? 60 : 25)}, 1.5f);

            context.Render.DrawRectangleOutline(tabW - 4.0f, tabH - 4.0f, Transform2D{tPos}, isCurTab ? Color{255, 215, 60, 255} : Color{50, 56, 72, 200}, isCurTab ? 2.0f : 1.0f);
            context.Render.DrawTextCentered(tabNames[t], m_fontId, 17.0f, Transform2D{tPos}, isCurTab ? Color{255, 220, 75, 255} : Color{190, 205, 225, 220});
        }

        const float cardW = sideWidth - 36.0f;

        // Render Active Tab Content with 3D Relief
        switch (m_activeTab)
        {
        case EditorTab::Inspector:
        {
            RenderInspectorTab(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse);
            break;
        }
        case EditorTab::Procedural:
        {
            Vector2f titlePos{panelCenter.X, -halfH + toolbarH + 98.0f};
            context.Render.DrawTextCentered("ASSISTANT PROCEDURAL", m_fontId, 24.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

            // 1. Brick Count Control
            Vector2f cntPos{panelCenter.X, -halfH + toolbarH + 160.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{cntPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{cntPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{cntPos.X - cardW * 0.5f, cntPos.Y - 28.0f}, Vector2f{cntPos.X + cardW * 0.5f, cntPos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{cntPos}, Color{55, 65, 88, 200}, 1.5f);
            context.Render.DrawTextCentered("[-] " + std::to_string(m_procBrickCount) + " BRIQUES CIBLES [+]", m_fontId, 22.0f, Transform2D{cntPos}, Colors::White);

            // 2. Difficulty Control
            Vector2f diffPos{panelCenter.X, -halfH + toolbarH + 250.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{diffPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{diffPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{diffPos.X - cardW * 0.5f, diffPos.Y - 28.0f}, Vector2f{diffPos.X + cardW * 0.5f, diffPos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{diffPos}, Color{55, 65, 88, 200}, 1.5f);
            int diffPct = static_cast<int>(m_procDifficulty * 100.0f);
            context.Render.DrawTextCentered("[-] DENSITE : " + std::to_string(diffPct) + "% [+]", m_fontId, 22.0f, Transform2D{diffPos}, Colors::White);

            // 3. Symmetry Mode
            const char* symNames[4] = {"AUCUNE", "HORIZONTALE", "VERTICALE", "DUALE (4X)"};
            Vector2f symPos{panelCenter.X, -halfH + toolbarH + 340.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{symPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{symPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{symPos.X - cardW * 0.5f, symPos.Y - 28.0f}, Vector2f{symPos.X + cardW * 0.5f, symPos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{symPos}, Color{55, 65, 88, 200}, 1.5f);
            context.Render.DrawTextCentered("SYMETRIE : " + std::string(symNames[m_procSymmetry]), m_fontId, 22.0f, Transform2D{symPos}, Color{255, 195, 60, 255});

            // 4. Large Generate Button
            Vector2f genPos{panelCenter.X, -halfH + toolbarH + 450.0f};
            bool genHover = (std::abs(worldMouse.X - genPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - genPos.Y) <= 34.0f);
            context.Render.DrawRectangle(cardW, 68.0f, Transform2D{genPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 140});
            context.Render.DrawRectangle(cardW, 68.0f, Transform2D{genPos}, genHover ? Color{255, 175, 40, 255} : Color{215, 140, 30, 255});
            context.Render.DrawLine(Vector2f{genPos.X - cardW * 0.5f, genPos.Y - 33.0f}, Vector2f{genPos.X + cardW * 0.5f, genPos.Y - 33.0f}, Color{255, 255, 255, 90}, 2.0f);
            context.Render.DrawRectangleOutline(cardW, 68.0f, Transform2D{genPos}, Colors::White, 2.0f);
            context.Render.DrawTextCentered("GENERER NIVEAU", m_fontId, 24.0f, Transform2D{genPos}, Colors::White);
            break;
        }
        case EditorTab::ThemeLighting:
        {
            Vector2f titlePos{panelCenter.X, -halfH + toolbarH + 98.0f};
            context.Render.DrawTextCentered("THEMES & LUMIERE", m_fontId, 24.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

            // Theme Preset Selector
            Vector2f thmPos{panelCenter.X, -halfH + toolbarH + 160.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{thmPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{thmPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{thmPos.X - cardW * 0.5f, thmPos.Y - 28.0f}, Vector2f{thmPos.X + cardW * 0.5f, thmPos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{thmPos}, Color{55, 65, 88, 200}, 1.5f);
            context.Render.DrawTextCentered("< " + m_levelData.GetTheme().Name + " >", m_fontId, 22.0f, Transform2D{thmPos}, Color{255, 215, 75, 255});

            // Ambient Intensity
            Vector2f ambPos{panelCenter.X, -halfH + toolbarH + 250.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{ambPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{ambPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{ambPos.X - cardW * 0.5f, ambPos.Y - 28.0f}, Vector2f{ambPos.X + cardW * 0.5f, ambPos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{ambPos}, Color{55, 65, 88, 200}, 1.5f);
            int ambPct = static_cast<int>(m_levelData.GetTheme().AmbientIntensity * 100.0f);
            context.Render.DrawTextCentered("[-] AMBIANCE : " + std::to_string(ambPct) + "% [+]", m_fontId, 22.0f, Transform2D{ambPos}, Colors::White);

            // Paddle Spotlight Cone
            Vector2f conePos{panelCenter.X, -halfH + toolbarH + 340.0f};
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{conePos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 58.0f, Transform2D{conePos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{conePos.X - cardW * 0.5f, conePos.Y - 28.0f}, Vector2f{conePos.X + cardW * 0.5f, conePos.Y - 28.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 58.0f, Transform2D{conePos}, Color{55, 65, 88, 200}, 1.5f);
            int coneDeg = static_cast<int>(m_levelData.GetTheme().PaddleConeAngle);
            context.Render.DrawTextCentered("[-] FAISCEAU PADDLE : " + std::to_string(coneDeg) + "° [+]", m_fontId, 22.0f, Transform2D{conePos}, Colors::White);
            break;
        }
        case EditorTab::EventQueue:
        {
            RenderEventQueueTab(context, panelCenter, sideWidth, toolbarH, halfH, worldMouse);
            break;
        }
        case EditorTab::Gamerules:
        {
            Vector2f titlePos{panelCenter.X, -halfH + toolbarH + 98.0f};
            context.Render.DrawTextCentered("REGLES DE JEU", m_fontId, 24.0f, Transform2D{titlePos}, Color{255, 215, 60, 255});

            const bool manualOverride = context.Rules.GetRule(Rule::Gameplay::ManualOverride);
            Vector2f modePos{panelCenter.X, -halfH + toolbarH + 142.0f};
            if (manualOverride)
            {
                context.Render.DrawTextCentered("MODE : MANUEL (DEPUIS SETTINGS)", m_fontId, 20.0f, Transform2D{modePos}, Color{255, 160, 60, 255});
            }
            else
            {
                context.Render.DrawTextCentered("MODE : AUTOMATIQUE (NIVEAU)", m_fontId, 20.0f, Transform2D{modePos}, Color{80, 240, 140, 255});
            }

            const auto& gr = m_levelData.GetGamerules();

            Vector2f hzPos{panelCenter.X, -halfH + toolbarH + 210.0f};
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{hzPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{hzPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{hzPos.X - cardW * 0.5f, hzPos.Y - 29.0f}, Vector2f{hzPos.X + cardW * 0.5f, hzPos.Y - 29.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 60.0f, Transform2D{hzPos}, gr.HazardZones ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("HAZARDS : ") + (gr.HazardZones ? "[ACTIF]" : "[DESACTIVE]"), m_fontId, 22.0f, Transform2D{hzPos}, Colors::White);

            Vector2f arPos{panelCenter.X, -halfH + toolbarH + 300.0f};
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{arPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{arPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{arPos.X - cardW * 0.5f, arPos.Y - 29.0f}, Vector2f{arPos.X + cardW * 0.5f, arPos.Y - 29.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 60.0f, Transform2D{arPos}, gr.ArenaEvents ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("EVENTS ARENE : ") + (gr.ArenaEvents ? "[ACTIF]" : "[DESACTIVE]"), m_fontId, 22.0f, Transform2D{arPos}, Colors::White);

            Vector2f lfPos{panelCenter.X, -halfH + toolbarH + 390.0f};
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{lfPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
            context.Render.DrawRectangle(cardW, 60.0f, Transform2D{lfPos}, Color{28, 32, 44, 255});
            context.Render.DrawLine(Vector2f{lfPos.X - cardW * 0.5f, lfPos.Y - 29.0f}, Vector2f{lfPos.X + cardW * 0.5f, lfPos.Y - 29.0f}, Color{255, 255, 255, 30}, 1.5f);
            context.Render.DrawRectangleOutline(cardW, 60.0f, Transform2D{lfPos}, gr.InfiniteLives ? Color{80, 240, 140, 200} : Color{240, 60, 70, 200}, 2.0f);
            context.Render.DrawTextCentered(std::string("VIES : ") + (gr.InfiniteLives ? "INFINIES" : "STANDARD (3 VIES)"), m_fontId, 22.0f, Transform2D{lfPos}, Colors::White);
            break;
        }
        }
    }
    else
    {
        // Collapsed Rail (44px wide)
        const float railW = 44.0f;
        Vector2f railCenter{halfW - railW * 0.5f, -halfH + toolbarH + panelH * 0.5f};
        context.Render.DrawRectangle(railW, panelH, Transform2D{railCenter}, Color{22, 25, 33, 250});
        context.Render.DrawLine(Vector2f{halfW - railW, -halfH + toolbarH},
                                Vector2f{halfW - railW, halfH},
                                Color{60, 66, 80, 255}, 2.0f);

        // Expand Button at top
        Vector2f togglePos{halfW - railW * 0.5f, -halfH + toolbarH + 35.0f};
        bool toggleHover = (std::abs(worldMouse.X - togglePos.X) <= 16.0f && std::abs(worldMouse.Y - togglePos.Y) <= 16.0f);
        context.Render.DrawRectangle(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Color{55, 62, 78, 255} : Color{34, 38, 48, 255});
        context.Render.DrawRectangleOutline(30.0f, 30.0f, Transform2D{togglePos}, toggleHover ? Colors::White : Color{70, 76, 92, 255}, 1.2f);
        DrawChevron(context, togglePos, 16.0f, false /* left */, toggleHover ? Colors::White : Color{190, 195, 205, 255});

        // 5 Compact Vertical Tab Icons (I, P, T, E, R)
        const char* tabIcons[5] = {"I", "P", "T", "E", "R"};
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
    const float cardW = sideWidth - 36.0f;
    const EditorCellData target = GetInspectorTargetData();
    const Color targetCol = LevelData::GetDisplayColorForCell(target);

    const bool editingCell = (m_hasSelectedCell && m_levelData.GetCell(m_selectedCol, m_selectedRow).Type != EditorTileType::Empty);

    // Header
    context.Render.DrawTextCentered("INSPECTEUR DE BRIQUE", m_fontId, 24.0f, Transform2D{Vector2f{panelCenter.X, top + 92.0f}}, Color{255, 215, 60, 255});

    // Target Summary Card
    Vector2f cardPos{panelCenter.X, top + 148.0f};
    context.Render.DrawRectangle(cardW, 68.0f, Transform2D{cardPos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
    context.Render.DrawRectangle(cardW, 68.0f, Transform2D{cardPos}, Color{28, 33, 46, 255});
    context.Render.DrawRectangleOutline(cardW, 68.0f, Transform2D{cardPos}, Color{70, 95, 135, 220}, 1.5f);

    Vector2f swatchPos{cardPos.X - cardW * 0.36f, cardPos.Y};
    context.Render.DrawRectangle(56.0f, 26.0f, Transform2D{swatchPos}, targetCol);
    context.Render.DrawRectangleOutline(56.0f, 26.0f, Transform2D{swatchPos}, Colors::White, 1.3f);

    std::string modeHeader = editingCell
        ? ("CELLULE (" + std::to_string(m_selectedCol) + "," + std::to_string(m_selectedRow) + ") // PALETTE")
        : "PRESET PALETTE (PINCEAU)";
    context.Render.DrawText(modeHeader, m_fontId, 15.0f, Transform2D{Vector2f{cardPos.X - cardW * 0.20f, cardPos.Y - 19.0f}}, Color{120, 220, 255, 240});
    context.Render.DrawText(GetTileTypeName(target.Type), m_fontId, 21.0f, Transform2D{Vector2f{cardPos.X - cardW * 0.20f, cardPos.Y + 4.0f}}, Colors::White);

    // Section 1: HitPoints 1..9 Color Ramp
    const bool supportsHP = (target.Type != EditorTileType::Metal && target.Type != EditorTileType::Bomb && target.Type != EditorTileType::Empty);
    context.Render.DrawTextCentered("POINTS DE VIE (1 - 9 PV)", m_fontId, 18.0f, Transform2D{Vector2f{panelCenter.X, top + 206.0f}}, Color{210, 225, 245, 240});

    const float btnSize = 36.0f;
    const float btnGap = 4.0f;
    const float totalRowW = 9.0f * btnSize + 8.0f * btnGap;
    const float rowStartX = panelCenter.X - totalRowW * 0.5f + btnSize * 0.5f;
    const float hpRowY = top + 244.0f;

    for (int hp = 1; hp <= 9; ++hp)
    {
        float bx = rowStartX + static_cast<float>(hp - 1) * (btnSize + btnGap);
        Vector2f bPos{bx, hpRowY};
        bool isCurHP = (supportsHP && target.HitPoints == hp);
        bool isHover = supportsHP && (std::abs(worldMouse.X - bPos.X) <= btnSize * 0.5f && std::abs(worldMouse.Y - bPos.Y) <= btnSize * 0.5f);

        Color swatchCol = LevelData::GetColorForHitPoints(hp);
        if (!supportsHP)
        {
            swatchCol = Color{45, 50, 62, 160};
        }

        context.Render.DrawRectangle(btnSize, btnSize, Transform2D{bPos}, swatchCol);
        Color outline = isCurHP ? Colors::White : (isHover ? Color{255, 230, 120, 255} : Color{20, 24, 34, 220});
        context.Render.DrawRectangleOutline(btnSize, btnSize, Transform2D{bPos}, outline, isCurHP ? 3.0f : 1.2f);
        context.Render.DrawTextCentered(std::to_string(hp), m_fontId, 19.0f, Transform2D{bPos + Vector2f{1.0f, 1.0f}}, Color{0, 0, 0, 190});
        context.Render.DrawTextCentered(std::to_string(hp), m_fontId, 19.0f, Transform2D{bPos}, supportsHP ? Color{15, 18, 25, 255} : Color{130, 140, 160, 180});
    }

    std::string hpSummary;
    Color hpSummaryCol{185, 210, 235, 230};
    if (target.Type == EditorTileType::Metal)
    {
        hpSummary = "ETAT : INCASSABLE (METAL)";
        hpSummaryCol = Color{180, 195, 210, 220};
    }
    else if (target.Type == EditorTileType::Bomb)
    {
        hpSummary = "ETAT : 1 PV (EXPLOSION AOE)";
        hpSummaryCol = Color{255, 145, 70, 240};
    }
    else
    {
        hpSummary = std::to_string(target.HitPoints) + " PV - " + GetHitPointsColorName(target.HitPoints);
        hpSummaryCol = LevelData::GetColorForHitPoints(target.HitPoints);
    }
    context.Render.DrawTextCentered(hpSummary, m_fontId, 17.0f, Transform2D{Vector2f{panelCenter.X, top + 278.0f}}, hpSummaryCol);

    // Section 2: Type-Specific Properties
    auto drawControlCard = [&](const Vector2f& pos, float h, const std::string& txt, Color borderCol, Color textCol) {
        bool hov = (std::abs(worldMouse.X - pos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - pos.Y) <= h * 0.5f);
        context.Render.DrawRectangle(cardW, h, Transform2D{pos + Vector2f{2.0f, 2.0f}}, Color{0, 0, 0, 120});
        context.Render.DrawRectangle(cardW, h, Transform2D{pos}, hov ? Color{36, 42, 58, 255} : Color{28, 32, 44, 255});
        context.Render.DrawLine(Vector2f{pos.X - cardW * 0.5f, pos.Y - h * 0.5f + 1.0f}, Vector2f{pos.X + cardW * 0.5f, pos.Y - h * 0.5f + 1.0f}, Color{255, 255, 255, 30}, 1.5f);
        context.Render.DrawRectangleOutline(cardW, h, Transform2D{pos}, hov ? Colors::White : borderCol, hov ? 2.0f : 1.5f);
        context.Render.DrawTextCentered(txt, m_fontId, 19.0f, Transform2D{pos}, textCol);
    };

    if (target.Type == EditorTileType::Special)
    {
        context.Render.DrawTextCentered("CONFIGURATION POWER-UP", m_fontId, 18.0f, Transform2D{Vector2f{panelCenter.X, top + 318.0f}}, Color{240, 120, 255, 245});

        Vector2f modePos{panelCenter.X, top + 364.0f};
        std::string modeTxt = target.RandomPowerUp ? "MODE : ALEATOIRE" : "MODE : POWER-UP FIXE";
        Color modeBorder = target.RandomPowerUp ? Color{80, 220, 255, 210} : Color{255, 200, 75, 230};
        drawControlCard(modePos, 52.0f, modeTxt, modeBorder, Colors::White);

        Vector2f pwrPos{panelCenter.X, top + 432.0f};
        std::string pwrTxt = "<  " + LevelData::GetPowerUpDisplayName(target.SpecificPowerUp) + "  >";
        Color pwrTextCol = target.RandomPowerUp ? Color{140, 150, 175, 170} : Color{255, 220, 90, 255};
        drawControlCard(pwrPos, 56.0f, pwrTxt, target.RandomPowerUp ? Color{55, 62, 78, 170} : Color{230, 80, 245, 220}, pwrTextCol);

        context.Render.DrawTextCentered("CLIQUEZ < > POUR SELECTIONNER UN BONUS", m_fontId, 14.0f, Transform2D{Vector2f{panelCenter.X, top + 480.0f}}, Color{150, 170, 195, 200});
    }
    else if (target.Type == EditorTileType::Turret)
    {
        context.Render.DrawTextCentered("PARAMETRES TOURELLE (CAMPAGNE)", m_fontId, 18.0f, Transform2D{Vector2f{panelCenter.X, top + 318.0f}}, Color{255, 95, 110, 250});

        drawControlCard(Vector2f{panelCenter.X, top + 366.0f}, 52.0f,
                        "[-] CADENCE TIR : " + FormatOneDecimal(target.TurretCooldown) + "s [+]",
                        Color{225, 70, 85, 210}, Colors::White);

        drawControlCard(Vector2f{panelCenter.X, top + 430.0f}, 52.0f,
                        "[-] DELAI INITIAL : " + FormatOneDecimal(target.TurretInitialDelay) + "s [+]",
                        Color{225, 130, 70, 210}, Colors::White);

        drawControlCard(Vector2f{panelCenter.X, top + 494.0f}, 52.0f,
                        "[-] VITESSE MISSILE : " + std::to_string(static_cast<int>(target.TurretProjectileSpeed)) + " PX/S [+]",
                        Color{255, 185, 65, 210}, Colors::White);

        context.Render.DrawTextCentered("HALO + TIR TRIANGLE + DEGATS PADDLE", m_fontId, 14.0f, Transform2D{Vector2f{panelCenter.X, top + 544.0f}}, Color{160, 180, 205, 210});
    }
    else
    {
        std::string infoLine1 = "BRIQUE CONFIGURABLE";
        std::string infoLine2 = "SAUVEGARDEE DANS LA PALETTE";
        if (target.Type == EditorTileType::Lantern)
        {
            infoLine1 = "EMET UNE LUMIERE CHAUDE";
            infoLine2 = "PV PERSONNALISABLES (1..9)";
        }
        else if (target.Type == EditorTileType::Metal)
        {
            infoLine1 = "BLOC INCASSABLE";
            infoLine2 = "NE BLOQUE PAS LA VICTOIRE";
        }
        else if (target.Type == EditorTileType::Bomb)
        {
            infoLine1 = "DETRUIT LES BRIQUES VOISINES";
            infoLine2 = "ONDE DE CHOC A LA DESTRUCTION";
        }

        Vector2f infoPos{panelCenter.X, top + 372.0f};
        context.Render.DrawRectangle(cardW, 74.0f, Transform2D{infoPos}, Color{26, 30, 42, 230});
        context.Render.DrawRectangleOutline(cardW, 74.0f, Transform2D{infoPos}, Color{55, 65, 88, 190}, 1.2f);
        context.Render.DrawTextCentered(infoLine1, m_fontId, 17.0f, Transform2D{infoPos + Vector2f{0.0f, -13.0f}}, Color{210, 225, 245, 235});
        context.Render.DrawTextCentered(infoLine2, m_fontId, 15.0f, Transform2D{infoPos + Vector2f{0.0f, 13.0f}}, Color{145, 165, 195, 210});
    }

    // Bottom Reset Preset Button
    Vector2f resetPos{panelCenter.X, halfH - 58.0f};
    drawControlCard(resetPos, 48.0f, "REINITIALISER PRESET PAR DEFAUT", Color{190, 80, 95, 210}, Color{255, 200, 205, 255});

    context.Render.DrawTextCentered("SHIFT+CLIC OU MOLETTE = INSPECTER", m_fontId, 14.0f, Transform2D{Vector2f{panelCenter.X, halfH - 22.0f}}, Color{140, 165, 195, 200});
}

bool EditorScene::HandleInspectorTabInput(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse, bool leftClick)
{
    if (!leftClick) return false;

    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 36.0f;
    EditorCellData target = GetInspectorTargetData();
    const bool supportsHP = (target.Type != EditorTileType::Metal && target.Type != EditorTileType::Bomb && target.Type != EditorTileType::Empty);

    // 1. HitPoints 1..9 swatch row
    if (supportsHP)
    {
        const float btnSize = 36.0f;
        const float btnGap = 4.0f;
        const float totalRowW = 9.0f * btnSize + 8.0f * btnGap;
        const float rowStartX = panelCenter.X - totalRowW * 0.5f + btnSize * 0.5f;
        const float hpRowY = top + 244.0f;

        if (std::abs(worldMouse.Y - hpRowY) <= btnSize * 0.55f)
        {
            for (int hp = 1; hp <= 9; ++hp)
            {
                float bx = rowStartX + static_cast<float>(hp - 1) * (btnSize + btnGap);
                if (std::abs(worldMouse.X - bx) <= btnSize * 0.5f)
                {
                    target.HitPoints = hp;
                    ApplyInspectorTargetData(target);
                    m_statusMessage = std::to_string(hp) + " PV APPLIQUE (" + GetHitPointsColorName(hp) + ")";
                    m_statusTimer = 1.6f;
                    m_statusColor = LevelData::GetColorForHitPoints(hp);
                    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
                    return true;
                }
            }
        }
    }

    // 2. Special Brick Power-up controls
    if (target.Type == EditorTileType::Special)
    {
        if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 364.0f)) <= 26.0f)
        {
            target.RandomPowerUp = !target.RandomPowerUp;
            ApplyInspectorTargetData(target);
            m_statusMessage = target.RandomPowerUp ? "CAPSULE : ALEATOIRE" : ("CAPSULE FIXE : " + LevelData::GetPowerUpDisplayName(target.SpecificPowerUp));
            m_statusTimer = 1.8f;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            return true;
        }

        if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 432.0f)) <= 28.0f)
        {
            const auto& list = LevelData::GetSelectablePowerUps();
            if (!list.empty())
            {
                size_t curIdx = 0;
                for (size_t i = 0; i < list.size(); ++i)
                {
                    if (list[i] == target.SpecificPowerUp)
                    {
                        curIdx = i;
                        break;
                    }
                }
                if (worldMouse.X < panelCenter.X)
                {
                    curIdx = (curIdx == 0) ? (list.size() - 1) : (curIdx - 1);
                }
                else
                {
                    curIdx = (curIdx + 1) % list.size();
                }
                target.RandomPowerUp = false;
                target.SpecificPowerUp = list[curIdx];
                ApplyInspectorTargetData(target);
                m_statusMessage = "POWER-UP : " + LevelData::GetPowerUpDisplayName(target.SpecificPowerUp);
                m_statusTimer = 1.8f;
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 16.0f);
            }
            return true;
        }
    }
    // 3. Turret Brick Controls
    else if (target.Type == EditorTileType::Turret)
    {
        if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 366.0f)) <= 26.0f)
        {
            if (worldMouse.X < panelCenter.X) target.TurretCooldown = std::max(0.5f, target.TurretCooldown - 0.2f);
            else target.TurretCooldown = std::min(10.0f, target.TurretCooldown + 0.2f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }

        if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 430.0f)) <= 26.0f)
        {
            if (worldMouse.X < panelCenter.X) target.TurretInitialDelay = std::max(0.5f, target.TurretInitialDelay - 0.5f);
            else target.TurretInitialDelay = std::min(12.0f, target.TurretInitialDelay + 0.5f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }

        if (std::abs(worldMouse.X - panelCenter.X) <= cardW * 0.5f && std::abs(worldMouse.Y - (top + 494.0f)) <= 26.0f)
        {
            if (worldMouse.X < panelCenter.X) target.TurretProjectileSpeed = std::max(100.0f, target.TurretProjectileSpeed - 20.0f);
            else target.TurretProjectileSpeed = std::min(450.0f, target.TurretProjectileSpeed + 20.0f);
            ApplyInspectorTargetData(target);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
    }

    // 4. Reset Preset Button
    Vector2f resetPos{panelCenter.X, halfH - 58.0f};
    if (std::abs(worldMouse.X - resetPos.X) <= cardW * 0.5f && std::abs(worldMouse.Y - resetPos.Y) <= 24.0f)
    {
        EditorCellData def = EditorCellData::CreateDefaultForType(target.Type);
        ApplyInspectorTargetData(def);
        m_statusMessage = "PRESET REINITIALISE";
        m_statusTimer = 1.6f;
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 16.0f);
        return true;
    }

    return false;
}

void EditorScene::RenderEventQueueTab(GameContext& context, const Vector2f& panelCenter, float sideWidth, float toolbarH, float halfH, const Vector2f& worldMouse)
{
    const float top = -halfH + toolbarH;
    const float cardW = sideWidth - 36.0f;

    // Header + Count Badge
    const auto& q = m_levelData.GetEventQueue();
    context.Render.DrawText("EVENEMENTS", m_fontId, 23.0f, Transform2D{Vector2f{panelCenter.X - cardW * 0.5f, top + 78.0f}}, Color{255, 215, 60, 255});

    Vector2f cntBadge{panelCenter.X + cardW * 0.5f - 28.0f, top + 90.0f};
    context.Render.DrawRectangle(54.0f, 26.0f, Transform2D{cntBadge}, Color{28, 36, 52, 255});
    context.Render.DrawRectangleOutline(54.0f, 26.0f, Transform2D{cntBadge}, Color{80, 220, 255, 200}, 1.2f);
    context.Render.DrawTextCentered(std::to_string(q.size()), m_fontId, 18.0f, Transform2D{cntBadge}, Color{170, 235, 255, 255});

    // 2x3 Graphical Event Palette Chips
    struct EventChipDesc
    {
        EditorEventType Type;
        const char* ShortLabel;
        Color Accent;
    };
    const EventChipDesc chips[6] = {
        {EditorEventType::HazardSurge,      "METEORES", Color{255, 75, 75, 255}},
        {EditorEventType::LaserSweep,       "GRAVITE",  Color{255, 160, 40, 255}},
        {EditorEventType::DarknessEclipse,  "ECLIPSE",  Color{150, 75, 255, 255}},
        {EditorEventType::TurretOvercharge, "LOCKDOWN", Color{255, 45, 95, 255}},
        {EditorEventType::BossAlert,        "SEISME",   Color{255, 215, 0, 255}},
        {EditorEventType::SpeedBoost,       "TEMPO",    Color{65, 225, 255, 255}}
    };

    const float chipGap = 8.0f;
    const float chipW = (cardW - chipGap * 2.0f) / 3.0f;
    const float chipH = 42.0f;
    const float startX = panelCenter.X - cardW * 0.5f + chipW * 0.5f;

    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        const Vector2f cPos{startX + static_cast<float>(col) * (chipW + chipGap), top + 138.0f + static_cast<float>(row) * 50.0f};
        const bool hover = (std::abs(worldMouse.X - cPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - cPos.Y) <= chipH * 0.5f);

        context.Render.DrawRectangle(chipW, chipH, Transform2D{cPos}, hover ? Color{40, 48, 66, 255} : Color{28, 32, 44, 255});
        context.Render.DrawRectangle(5.0f, chipH - 4.0f, Transform2D{Vector2f{cPos.X - chipW * 0.5f + 3.5f, cPos.Y}}, chips[i].Accent);
        context.Render.DrawRectangleOutline(chipW, chipH, Transform2D{cPos}, hover ? chips[i].Accent : Color{55, 65, 86, 210}, hover ? 2.0f : 1.2f);
        context.Render.DrawTextCentered(std::string("+ ") + chips[i].ShortLabel, m_fontId, 16.0f, Transform2D{Vector2f{cPos.X + 3.0f, cPos.Y}}, Colors::White);
    }

    // 3 Compact Sort Pills (A-Z | Z-A | MIX)
    const char* sortLabels[3] = {"TRI A-Z", "TRI Z-A", "MELANGER"};
    const float sortY = top + 246.0f;
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

    // Queue Cards (up to 6 visible, scrollable)
    const int maxVisible = 6;
    const int maxScroll = std::max(0, static_cast<int>(q.size()) - maxVisible);
    const int clampedScroll = std::clamp(m_eventQueueScroll, 0, maxScroll);
    const float listStartY = top + 312.0f;
    const float itemH = 52.0f;

    if (q.empty())
    {
        Vector2f emptyBox{panelCenter.X, listStartY + 95.0f};
        context.Render.DrawRectangle(cardW, 110.0f, Transform2D{emptyBox}, Color{20, 24, 34, 200});
        context.Render.DrawRectangleOutline(cardW, 110.0f, Transform2D{emptyBox}, Color{48, 56, 74, 180}, 1.2f);
        context.Render.DrawTextCentered("AUCUN EVENEMENT", m_fontId, 20.0f, Transform2D{emptyBox}, Color{135, 155, 180, 190});
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
        const int maxScroll = std::max(0, static_cast<int>(q.size()) - 6);
        m_eventQueueScroll = std::clamp(m_eventQueueScroll - (wheel > 0.0f ? 1 : -1), 0, maxScroll);
    }

    if (!leftClick) return false;

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
        const Vector2f cPos{startX + static_cast<float>(col) * (chipW + chipGap), top + 138.0f + static_cast<float>(row) * 50.0f};
        if (std::abs(worldMouse.X - cPos.X) <= chipW * 0.5f && std::abs(worldMouse.Y - cPos.Y) <= chipH * 0.5f)
        {
            PushUndo();
            m_levelData.AddEvent(chipTypes[i]);
            const int maxScroll = std::max(0, static_cast<int>(m_levelData.GetEventQueue().size()) - 6);
            m_eventQueueScroll = maxScroll;
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 14.0f);
            return true;
        }
    }

    // 2. Sort Pills
    const float sortY = top + 246.0f;
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
    const int maxScroll = std::max(0, static_cast<int>(q.size()) - 6);
    m_eventQueueScroll = std::clamp(m_eventQueueScroll, 0, maxScroll);
    const float listStartY = top + 312.0f;
    const float itemH = 52.0f;

    for (int vis = 0; vis < 6; ++vis)
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
        ? (isHovered ? "RELACHER POUR SUPPRIMER" : "GLISSER ICI POUR SUPPRIMER")
        : "VIDER LA FILE";
    context.Render.DrawTextCentered(trashText, m_fontId, 19.0f, Transform2D{Vector2f{trashPos.X + 18.0f, trashPos.Y}}, isHovered ? Colors::White : Color{245, 190, 200, 240});
}

void EditorScene::UpdateTurrets(float dt, GameContext& context)
{
    // Match Campaign (StoryScene): do NOT advance turret timers during countdown
    if (IsCountdownActive())
    {
        return;
    }

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

        // Fire downward triangle missile!
        if (it->ShootTimer <= 0.0f)
        {
            it->ShootTimer = it->Cooldown;

            if (m_registry.HasComponent<Transform2D>(it->EntityId))
            {
                const auto& bTrans = m_registry.GetComponent<Transform2D>(it->EntityId);
                Vector2f spawnPos{bTrans.Position.X, bTrans.Position.Y + 18.0f};

                TriangleProjectile proj;
                proj.Position = spawnPos;
                proj.Velocity = Vector2f{0.0f, it->ProjectileSpeed};
                proj.Size = 16.0f;
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

    // Update Projectiles (Campaign-fidelity paddle collision & feedback)
    if (m_turretBullets.empty()) return;

    const bool hasPaddle = m_registry.IsAlive(m_paddle) && !m_isPaddleDestroyed &&
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

    for (auto it = m_turretBullets.begin(); it != m_turretBullets.end(); )
    {
        it->Position.Y += it->Velocity.Y * dt;

        if (it->Position.Y > 450.0f)
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
                Vector2f impactPos = it->Position;
                it = m_turretBullets.erase(it);

                // 1. Impact feedback (particles + camera shake + crack SFX)
                SpawnExplosionParticles(impactPos, Color{255, 60, 40, 255}, 28);
                TweenEffects::Shake(m_registry, m_camera, 0.35f, 16.0f);
                if (m_audioEnabled && m_paddleCrackSfxId != 0)
                {
                    context.Audio.PlaySfx(m_paddleCrackSfxId, 85.0f, 0.85f);
                }

                // 2. Paddle red damage flash
                m_paddleDamageFlashTimer = 0.28f;
                if (m_registry.HasComponent<SpriteComponent>(m_paddle))
                {
                    m_registry.GetComponent<SpriteComponent>(m_paddle).Tint = Color{255, 50, 50, 255};
                }

                // 3. Life loss (unless InfiniteLives rule is enabled)
                if (!context.Rules.GetRule(Rule::Gameplay::InfiniteLives))
                {
                    m_lives--;
                    context.Events.Publish(LivesChangedEvent(m_lives));

                    if (m_lives <= 0)
                    {
                        m_lives = 0;
                        TriggerPaddleExplosion(context);
                        return;
                    }
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

void EditorScene::TriggerPaddleExplosion(GameContext& context)
{
    m_isPaddleDestroyed = true;
    m_paddleDestroyedTimer = 0.85f;
    m_lives = 0;

    Vector2f paddlePos{0.0f, 350.0f};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<Transform2D>(m_paddle))
    {
        paddlePos = m_registry.GetComponent<Transform2D>(m_paddle).Position;
    }

    Color paddleThemeColor{200, 235, 255, 255};
    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<LightSourceComponent>(m_paddle))
    {
        paddleThemeColor = m_registry.GetComponent<LightSourceComponent>(m_paddle).LightColor;
    }
    else
    {
        std::string skin = ProfileManager::Get().GetEquippedCosmetic(CosmeticSlot::PaddleSkin);
        if (skin == "paddle_gold_plating")
        {
            paddleThemeColor = Color{255, 215, 0, 255};
        }
        else if (skin == "paddle_obsidian")
        {
            paddleThemeColor = Color{140, 60, 220, 255};
        }
    }

    Color secondaryColor{
        static_cast<uint8_t>(std::min(255, paddleThemeColor.r + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.g + 40)),
        static_cast<uint8_t>(std::min(255, paddleThemeColor.b + 40)),
        255
    };

    SpawnExplosionParticles(paddlePos, paddleThemeColor, 75);
    SpawnExplosionParticles(paddlePos, secondaryColor, 55);
    SpawnLightFlash(paddlePos, 420.0f, paddleThemeColor, 3.0f, 0.5f);
    TweenEffects::Shake(m_registry, m_camera, 0.55f, 22.0f);

    if (m_audioEnabled && m_explosionSfxId != 0)
    {
        context.Audio.PlaySfx(m_explosionSfxId, 100.0f, 0.85f);
    }

    if (m_registry.IsAlive(m_paddle) && m_registry.HasComponent<SpriteComponent>(m_paddle))
    {
        m_registry.GetComponent<SpriteComponent>(m_paddle).Tint.a = 0;
    }
    if (mp_paddleSystem)
    {
        mp_paddleSystem->SetInputEnabled(false);
    }
    if (mp_ballSystem)
    {
        mp_ballSystem->ClearBalls(m_ball);
    }
}

void EditorScene::RenderPaddleDurabilityOverlay(GameContext& context)
{
    if (!m_registry.IsAlive(m_paddle) || m_isPaddleDestroyed ||
        !m_registry.HasComponent<Transform2D>(m_paddle) || !m_registry.HasComponent<BoxCollider>(m_paddle))
        return;

    const auto& pTrans = m_registry.GetComponent<Transform2D>(m_paddle);
    const auto& pCol = m_registry.GetComponent<BoxCollider>(m_paddle);
    const Vector2f effSize = pCol.GetEffectiveSize(pTrans.Scale);

    if (m_lives < 3 && m_lives > 0)
    {
        Color crackCol = (m_lives == 1) ? Color{255, 50, 50, 220} : Color{220, 220, 220, 170};
        const float halfW = effSize.X * 0.5f;
        const float halfH = effSize.Y * 0.5f;

        context.Render.DrawLine(Vector2f{pTrans.Position.X - halfW * 0.35f, pTrans.Position.Y - halfH},
                                Vector2f{pTrans.Position.X - halfW * 0.15f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        if (m_lives == 1)
        {
            context.Render.DrawLine(Vector2f{pTrans.Position.X + halfW * 0.25f, pTrans.Position.Y - halfH},
                                    Vector2f{pTrans.Position.X + halfW * 0.40f, pTrans.Position.Y + halfH}, crackCol, 2.0f);
        }
    }
}
