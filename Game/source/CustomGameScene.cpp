#include "Scenes/CustomGameScene.h"
#include "Scenes/MenuScene.h"
#include "Scenes/EndlessScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/TetrisScene.h"
#include "Core/GameContext.h"
#include "Core/GameRules.h"
#include "Core/SceneManager.h"
#include "ECS/Components/Transform2D.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "AudioMixer.h"
#include <algorithm>
#include <cmath>

CustomGameScene::CustomGameScene()
    : m_difficultyPreset(CustomDifficultyPreset::Normal),
      m_gameMode(CustomTargetGameMode::Endless),
      m_hazardZones(true),
      m_ultimateDrain(true),
      m_screenGlitch(true),
      m_cameraTilt(true),
      m_zoomPulse(true),
      m_audioWarp(true),
      m_arenaEvents(true),
      m_ultimateLockdown(true),
      m_infiniteLives(false),
      m_infiniteUltimate(false),
      m_creepingCeiling(true),
      m_selectedRow(0),
      m_animTimer(0.0f),
      m_fontId(0),
      m_clickSfxId(0),
      m_hoverSfxId(0)
{
}

void CustomGameScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_hoverSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_hover.wav");

    // Sync from existing GameRules
    m_hazardZones = context.Rules.GetRule(Rule::Gameplay::HazardZonesEnabled);
    m_ultimateDrain = context.Rules.GetRule(Rule::Gameplay::HazardUltimateDrainEnabled);
    m_screenGlitch = context.Rules.GetRule(Rule::Gameplay::HazardScreenGlitchEnabled);
    m_cameraTilt = context.Rules.GetRule(Rule::Gameplay::HazardCameraTiltEnabled);
    m_zoomPulse = context.Rules.GetRule(Rule::Gameplay::HazardZoomPulseEnabled);
    m_audioWarp = context.Rules.GetRule(Rule::Gameplay::HazardAudioWarpEnabled);
    m_arenaEvents = context.Rules.GetRule(Rule::Gameplay::ArenaEventsEnabled);
    m_ultimateLockdown = context.Rules.GetRule(Rule::Gameplay::UltimateLockdownEnabled);
    m_infiniteLives = context.Rules.GetRule(Rule::Gameplay::InfiniteLives);
    m_infiniteUltimate = context.Rules.GetRule(Rule::Gameplay::InfiniteUltimate);
    m_creepingCeiling = context.Rules.GetRule(Rule::Gameplay::EndlessCreepingCeiling);

    InitOptions();
}

void CustomGameScene::InitOptions()
{
    m_options.clear();
    m_options.push_back(CustomRuleOption{ "TACTICAL HAZARD ZONES", "Enable or disable tactical hazard zone generation on the playing field", &m_hazardZones });
    m_options.push_back(CustomRuleOption{ "ULTIMATE DRAIN ZONE", "Enable purple EMP zone that drains paddle ultimate charge", &m_ultimateDrain });
    m_options.push_back(CustomRuleOption{ "TRIPLE CHROMATIC GLITCH", "Enable glitch visual aberration creating optical triple-vision", &m_screenGlitch });
    m_options.push_back(CustomRuleOption{ "DYNAMIC CAMERA TILT", "Enable dynamic camera angular tilt upon ball impacts", &m_cameraTilt });
    m_options.push_back(CustomRuleOption{ "CAMERA ZOOM PULSE", "Enable rhythmic camera zoom pulsation effects", &m_zoomPulse });
    m_options.push_back(CustomRuleOption{ "AUDIO WARP DISTORTION", "Enable acoustic temporal time-warp distortion", &m_audioWarp });
    m_options.push_back(CustomRuleOption{ "ARENA EVENTS", "Enable dynamic arena anomalies (Meteors, Gravity Shifts, Flux)", &m_arenaEvents });
    m_options.push_back(CustomRuleOption{ "EMP LOCKDOWN (45s)", "Enable EMP wave locking out ultimate abilities for 45s", &m_ultimateLockdown });
    m_options.push_back(CustomRuleOption{ "CREEPING CEILING", "Enable continuous procedural downward ceiling descent", &m_creepingCeiling });
    m_options.push_back(CustomRuleOption{ "INFINITE LIVES", "Prevent game over on lost balls (Training Mode)", &m_infiniteLives });
    m_options.push_back(CustomRuleOption{ "INFINITE ULTIMATE", "Keep ultimate ability meter permanently charged", &m_infiniteUltimate });
}

void CustomGameScene::ApplyDifficultyPreset(CustomDifficultyPreset preset, GameContext& context)
{
    (void)context;
    m_difficultyPreset = preset;

    switch (preset)
    {
    case CustomDifficultyPreset::Easy:
        m_hazardZones = false;
        m_ultimateDrain = false;
        m_screenGlitch = false;
        m_cameraTilt = false;
        m_zoomPulse = false;
        m_audioWarp = false;
        m_arenaEvents = false;
        m_ultimateLockdown = false;
        m_creepingCeiling = false;
        m_infiniteLives = false;
        m_infiniteUltimate = false;
        break;

    case CustomDifficultyPreset::Normal:
        m_hazardZones = true;
        m_ultimateDrain = true;
        m_screenGlitch = true;
        m_cameraTilt = true;
        m_zoomPulse = true;
        m_audioWarp = true;
        m_arenaEvents = true;
        m_ultimateLockdown = true;
        m_creepingCeiling = true;
        m_infiniteLives = false;
        m_infiniteUltimate = false;
        break;

    case CustomDifficultyPreset::Hard:
        m_hazardZones = true;
        m_ultimateDrain = true;
        m_screenGlitch = true;
        m_cameraTilt = true;
        m_zoomPulse = true;
        m_audioWarp = true;
        m_arenaEvents = true;
        m_ultimateLockdown = true;
        m_creepingCeiling = true;
        m_infiniteLives = false;
        m_infiniteUltimate = false;
        break;

    case CustomDifficultyPreset::Nightmare:
        m_hazardZones = true;
        m_ultimateDrain = true;
        m_screenGlitch = true;
        m_cameraTilt = true;
        m_zoomPulse = true;
        m_audioWarp = true;
        m_arenaEvents = true;
        m_ultimateLockdown = true;
        m_creepingCeiling = true;
        m_infiniteLives = false;
        m_infiniteUltimate = false;
        break;

    case CustomDifficultyPreset::Custom:
    default:
        break;
    }
}

void CustomGameScene::LaunchCustomMatch(GameContext& context)
{
    // Apply all customized rules to the global GameRules context
    context.Rules.SetRule(Rule::Gameplay::HazardZonesEnabled, m_hazardZones);
    context.Rules.SetRule(Rule::Gameplay::HazardUltimateDrainEnabled, m_ultimateDrain);
    context.Rules.SetRule(Rule::Gameplay::HazardScreenGlitchEnabled, m_screenGlitch);
    context.Rules.SetRule(Rule::Gameplay::HazardCameraTiltEnabled, m_cameraTilt);
    context.Rules.SetRule(Rule::Gameplay::HazardZoomPulseEnabled, m_zoomPulse);
    context.Rules.SetRule(Rule::Gameplay::HazardAudioWarpEnabled, m_audioWarp);
    context.Rules.SetRule(Rule::Gameplay::ArenaEventsEnabled, m_arenaEvents);
    context.Rules.SetRule(Rule::Gameplay::UltimateLockdownEnabled, m_ultimateLockdown);
    context.Rules.SetRule(Rule::Gameplay::InfiniteLives, m_infiniteLives);
    context.Rules.SetRule(Rule::Gameplay::InfiniteUltimate, m_infiniteUltimate);
    context.Rules.SetRule(Rule::Gameplay::EndlessCreepingCeiling, m_creepingCeiling);

    if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);

    if (m_gameMode == CustomTargetGameMode::Tetris)
    {
        context.Scenes.LoadSceneWithTransition<TetrisScene>(SceneTransitionType::StaticSnow, 0.65f);
    }
    else if (m_gameMode == CustomTargetGameMode::Endless)
    {
        context.Scenes.LoadSceneWithTransition<EndlessScene>(SceneTransitionType::StaticSnow, 0.65f);
    }
    else
    {
        context.Scenes.LoadSceneWithTransition<GameScene>(SceneTransitionType::StaticSnow, 0.65f);
    }
}

void CustomGameScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    m_animTimer += dt;
    HandleInput(context);
}

void CustomGameScene::HandleInput(GameContext& context)
{
    const int totalOptions = static_cast<int>(m_options.size());
    const int totalRows = totalOptions + 4; // 0: Mode, 1: Preset, 2..N+1: Rules, N+2: Launch, N+3: Return

    // Up / Down Navigation
    if (context.Input.IsKeyPress(KeyCode::Up) || context.Input.IsKeyPress(KeyCode::W) || context.Input.IsMenuUpTriggered())
    {
        m_selectedRow = (m_selectedRow - 1 + totalRows) % totalRows;
        if (m_hoverSfxId != 0) context.Audio.PlaySfx(m_hoverSfxId, 40.0f);
    }
    else if (context.Input.IsKeyPress(KeyCode::Down) || context.Input.IsKeyPress(KeyCode::S) || context.Input.IsMenuDownTriggered())
    {
        m_selectedRow = (m_selectedRow + 1) % totalRows;
        if (m_hoverSfxId != 0) context.Audio.PlaySfx(m_hoverSfxId, 40.0f);
    }

    // Left / Right / Enter Interaction
    bool triggerToggle = context.Input.IsKeyPress(KeyCode::Left) || context.Input.IsKeyPress(KeyCode::Right) ||
                         context.Input.IsKeyPress(KeyCode::A) || context.Input.IsKeyPress(KeyCode::D) ||
                         context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsKeyPress(KeyCode::Enter) ||
                         context.Input.IsMenuSelectTriggered();

    if (triggerToggle)
    {
        if (m_selectedRow == 0) // Mode selector
        {
            int nextMode = (static_cast<int>(m_gameMode) + 1) % 3;
            m_gameMode = static_cast<CustomTargetGameMode>(nextMode);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 60.0f);
        }
        else if (m_selectedRow == 1) // Difficulty Preset
        {
            int nextPreset = (static_cast<int>(m_difficultyPreset) + 1) % static_cast<int>(CustomDifficultyPreset::Count);
            ApplyDifficultyPreset(static_cast<CustomDifficultyPreset>(nextPreset), context);
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 60.0f);
        }
        else if (m_selectedRow >= 2 && m_selectedRow < 2 + totalOptions) // Rule Toggles
        {
            int optIdx = m_selectedRow - 2;
            if (m_options[optIdx].pFlag)
            {
                *m_options[optIdx].pFlag = !(*m_options[optIdx].pFlag);
                m_difficultyPreset = CustomDifficultyPreset::Custom;
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 60.0f);
            }
        }
        else if (m_selectedRow == 2 + totalOptions) // Launch Button
        {
            LaunchCustomMatch(context);
        }
        else if (m_selectedRow == 3 + totalOptions) // Return Button
        {
            context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
        }
    }

    // Escape shortcut: Return to Menu
    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
    }

    // Mouse click navigation
    const auto mousePos = context.Input.GetMousePosition();
    const Vector2f mPos{mousePos.first, mousePos.second};
    bool isLeftClick = context.Input.IsMouseButtonPress(MouseButton::Left);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float centerX = viewSize.X * 0.5f;

    // Check Launch & Return button bounds
    const float launchY = 820.0f;
    const float returnY = 880.0f;

    if (std::abs(mPos.X - centerX) <= 220.0f && std::abs(mPos.Y - launchY) <= 25.0f)
    {
        m_selectedRow = 2 + totalOptions;
        if (isLeftClick) LaunchCustomMatch(context);
    }
    else if (std::abs(mPos.X - centerX) <= 150.0f && std::abs(mPos.Y - returnY) <= 22.0f)
    {
        m_selectedRow = 3 + totalOptions;
        if (isLeftClick) context.Scenes.LoadSceneWithTransition<MenuScene>(SceneTransitionType::StaticSnow, 0.65f);
    }
}

void CustomGameScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    RenderUI(context);
}

void CustomGameScene::RenderUI(GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float centerX = viewSize.X * 0.5f;

    // Cyber Dark Backdrop
    context.Render.DrawRectangle(viewSize.X, viewSize.Y, Transform2D{Vector2f{centerX, viewSize.Y * 0.5f}}, Color{10, 14, 22, 255});

    // Decorative retro grid lines
    for (float y = 50.0f; y < viewSize.Y; y += 45.0f)
    {
        context.Render.DrawRectangle(viewSize.X, 1.0f, Transform2D{Vector2f{centerX, y}}, Color{35, 60, 95, 25});
    }

    // Header Title Banner
    context.Render.DrawTextCentered("CUSTOM MATCH LAB", m_fontId, 42.0f, Transform2D{Vector2f{centerX, 55.0f}}, Colors::Gold);
    context.Render.DrawTextCentered("CONFIGURE GAME RULES, HAZARDS AND MODES BEFORE LAUNCHING", m_fontId, 18.0f, Transform2D{Vector2f{centerX, 90.0f}}, Color{160, 205, 240, 220});

    // Separator line
    context.Render.DrawRectangle(viewSize.X - 180.0f, 2.0f, Transform2D{Vector2f{centerX, 115.0f}}, Color{0, 220, 255, 140});

    const float startY = 150.0f;
    const float rowSpacing = 38.0f;
    const float leftColX = centerX - 320.0f;
    const float rightColX = centerX + 320.0f;

    // Row 0: Target Game Mode
    bool isRow0 = (m_selectedRow == 0);
    Color row0Col = isRow0 ? Colors::Gold : Color{210, 225, 245, 240};
    std::string modeText = (m_gameMode == CustomTargetGameMode::Endless) ? "PROCEDURAL ENDLESS" :
                           (m_gameMode == CustomTargetGameMode::Tetris  ? "TETRIS MATRIX GIMMICK" : "CLASSIC CAMPAIGN");
    context.Render.DrawText("TARGET GAME MODE:", m_fontId, 22.0f, Transform2D{Vector2f{leftColX, startY}}, row0Col);
    context.Render.DrawTextCentered("< " + modeText + " >", m_fontId, 22.0f, Transform2D{Vector2f{rightColX, startY}}, isRow0 ? Colors::Cyan : Color{80, 220, 255, 255});

    // Row 1: Difficulty Preset
    bool isRow1 = (m_selectedRow == 1);
    Color row1Col = isRow1 ? Colors::Gold : Color{210, 225, 245, 240};
    const char* presetNames[] = { "EASY", "NORMAL", "HARD", "NIGHTMARE", "CUSTOM" };
    Color presetColors[] = { Color{80, 255, 140, 255}, Color{80, 220, 255, 255}, Color{255, 180, 50, 255}, Color{255, 45, 80, 255}, Colors::Gold };
    int pIdx = static_cast<int>(m_difficultyPreset);
    context.Render.DrawText("DIFFICULTY PRESET:", m_fontId, 22.0f, Transform2D{Vector2f{leftColX, startY + rowSpacing}}, row1Col);
    context.Render.DrawTextCentered("< " + std::string(presetNames[pIdx]) + " >", m_fontId, 22.0f, Transform2D{Vector2f{rightColX, startY + rowSpacing}}, presetColors[pIdx]);

    // Separator line
    context.Render.DrawRectangle(viewSize.X - 220.0f, 1.0f, Transform2D{Vector2f{centerX, startY + rowSpacing * 1.6f}}, Color{45, 75, 110, 150});

    // Rows 2..N: Rules options
    for (size_t i = 0; i < m_options.size(); ++i)
    {
        float ry = startY + rowSpacing * 2.2f + static_cast<float>(i) * rowSpacing;
        bool isSelected = (m_selectedRow == static_cast<int>(i) + 2);
        const auto& opt = m_options[i];
        bool val = opt.pFlag ? *opt.pFlag : false;

        Color titleCol = isSelected ? Colors::Gold : Color{200, 215, 235, 220};
        context.Render.DrawText(opt.Name, m_fontId, 20.0f, Transform2D{Vector2f{leftColX, ry}}, titleCol);

        std::string statusStr = val ? "[ ACTIVE ]" : "[ DISABLED ]";
        Color statusCol = val ? opt.ColorOn : opt.ColorOff;
        if (isSelected)
        {
            statusCol = Colors::White;
            context.Render.DrawRectangle(140.0f, 26.0f, Transform2D{Vector2f{rightColX, ry}}, val ? Color{30, 140, 75, 230} : Color{150, 30, 45, 230});
        }
        context.Render.DrawTextCentered(statusStr, m_fontId, 20.0f, Transform2D{Vector2f{rightColX, ry}}, statusCol);
    }

    // Launch Button
    bool isLaunch = (m_selectedRow == static_cast<int>(m_options.size()) + 2);
    const float launchY = 820.0f;
    Color launchBg = isLaunch ? Colors::Gold : Color{25, 120, 65, 240};
    Color launchTextCol = isLaunch ? Colors::Black : Colors::White;
    context.Render.DrawRectangle(440.0f, 48.0f, Transform2D{Vector2f{centerX, launchY}}, launchBg);
    context.Render.DrawRectangleOutline(440.0f, 48.0f, Transform2D{Vector2f{centerX, launchY}}, Colors::Gold, isLaunch ? 3.0f : 1.5f);
    context.Render.DrawTextCentered("LAUNCH CUSTOM MATCH", m_fontId, 24.0f, Transform2D{Vector2f{centerX, launchY}}, launchTextCol);

    // Return to Menu Button
    bool isReturn = (m_selectedRow == static_cast<int>(m_options.size()) + 3);
    const float returnY = 880.0f;
    Color returnBg = isReturn ? Color{55, 65, 80, 240} : Color{20, 28, 38, 220};
    context.Render.DrawRectangle(300.0f, 40.0f, Transform2D{Vector2f{centerX, returnY}}, returnBg);
    context.Render.DrawRectangleOutline(300.0f, 40.0f, Transform2D{Vector2f{centerX, returnY}}, isReturn ? Colors::Cyan : Color{100, 130, 160, 200}, 1.5f);
    context.Render.DrawTextCentered("BACK TO MENU [ ESC ]", m_fontId, 20.0f, Transform2D{Vector2f{centerX, returnY}}, isReturn ? Colors::Cyan : Color{180, 205, 225, 220});
}

void CustomGameScene::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}
