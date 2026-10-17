#include "Scenes/LevelSelectScene.h"
#include "Scenes/GameScene.h"
#include "Scenes/MenuScene.h"
#include "Core/GameContext.h"
#include "Core/GameData.h"
#include "Core/SceneManager.h"
#include "Core/InputManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/GameRules.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "AudioMixer.h"
#include "UI/UIFactory.h"
#include "ECS/Systems/UISystem.h"
#include "ECS/Components/UI/CanvasComponent.h"
#include "ECS/Components/UI/RectTransform.h"
#include "ECS/Components/UI/PanelComponent.h"
#include "ECS/Components/UI/TextComponent.h"
#include "ECS/Components/UI/ButtonComponent.h"
#include "Data/Color.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

constexpr int kCardsPerPage = 5;
constexpr float kCardW = 250.0f;
constexpr float kCardH = 430.0f;
constexpr float kCardGap = 35.0f;
constexpr float kCardCenterY = 0.0f;

void LevelSelectScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    mp_context = &context;

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/paddle_hit.wav");
    m_shaderId = context.Resources.LoadResource("Resources/shaders/fx.frag");

    m_levelManager.Initialize("Resources/levels");
    m_levelManager.LoadFromPrefs();
    m_levelManager.PreloadLevels(context.Resources);

    // Keep menu music playing across menu navigation
    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 1.0f, 60.0f, true);

    m_selectedLevelIndex = 0;
    m_currentPage = 0;
    m_hoveredCardIndex = -1;

    LoadLevelThumbnails(context);
    InitRetroBackground(context);
    RebuildUI(context);
}

void LevelSelectScene::LoadLevelThumbnails(GameContext& /*context*/)
{
    m_levelThumbnails = LoadAllLevelThumbnails(m_levelManager);
}

void LevelSelectScene::ClearUI()
{
}

void LevelSelectScene::RebuildUI(GameContext& /*context*/)
{
}

void LevelSelectScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;
    m_animTime += dt;

    if (context.Input.IsMenuCancelTriggered() || context.Input.IsKeyPress(KeyCode::B) || context.Input.IsJoystickButtonPress(0, 1))
    {
        if (m_clickSfxId != 0)
        {
            context.Audio.PlaySfx(m_clickSfxId, 80.0f);
        }
        context.Scenes.LoadScene<MenuScene>();
        return;
    }

    const int totalLevels = m_levelManager.GetLevelCount();
    const int totalPages = std::max(1, (totalLevels + kCardsPerPage - 1) / kCardsPerPage);

    // Left / Right page flip (LB/RB or left/right arrow or left stick)
    if (context.Input.IsKeyPress(KeyCode::Left) || context.Input.IsJoystickButtonPress(0, 4) || context.Input.IsJoystickButtonPress(0, 100))
    {
        if (m_currentPage > 0)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            m_currentPage--;
            m_selectedLevelIndex = m_currentPage * kCardsPerPage;
        }
    }
    else if (context.Input.IsKeyPress(KeyCode::Right) || context.Input.IsJoystickButtonPress(0, 5) || context.Input.IsJoystickButtonPress(0, 101))
    {
        if (m_currentPage < totalPages - 1)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            m_currentPage++;
            m_selectedLevelIndex = m_currentPage * kCardsPerPage;
        }
    }

    // Direct key shortcuts 1 through 5 for current page
    const int startLevelIndex = m_currentPage * kCardsPerPage;
    if (context.Input.IsKeyPress(KeyCode::Num1) || context.Input.IsKeyPress(KeyCode::Numpad1)) LaunchLevel(context, startLevelIndex + 0);
    else if (context.Input.IsKeyPress(KeyCode::Num2) || context.Input.IsKeyPress(KeyCode::Numpad2)) LaunchLevel(context, startLevelIndex + 1);
    else if (context.Input.IsKeyPress(KeyCode::Num3) || context.Input.IsKeyPress(KeyCode::Numpad3)) LaunchLevel(context, startLevelIndex + 2);
    else if (context.Input.IsKeyPress(KeyCode::Num4) || context.Input.IsKeyPress(KeyCode::Numpad4)) LaunchLevel(context, startLevelIndex + 3);
    else if (context.Input.IsKeyPress(KeyCode::Num5) || context.Input.IsKeyPress(KeyCode::Numpad5)) LaunchLevel(context, startLevelIndex + 4);

    // Enter / Space or Gamepad A to launch selected
    if (context.Input.IsKeyPress(KeyCode::Enter) || context.Input.IsKeyPress(KeyCode::Space) || context.Input.IsJoystickButtonPress(0, 0))
    {
        LaunchLevel(context, m_selectedLevelIndex);
        return;
    }

    // Track mouse hover and click on cards, arrows, and back button
    auto [mouseX, mouseY] = context.Input.GetMousePosition();
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;
    const Vector2f centeredMouse{ mouseX - screenCenterX, mouseY - screenCenterY };

    constexpr float totalSpan = static_cast<float>(kCardsPerPage) * kCardW + static_cast<float>(kCardsPerPage - 1) * kCardGap;
    constexpr float startX = -(totalSpan - kCardW) * 0.5f;
    constexpr float stepX = kCardW + kCardGap;

    m_hoveredCardIndex = -1;
    const int levelsOnThisPage = std::clamp(totalLevels - startLevelIndex, 0, kCardsPerPage);

    const bool mouseClicked = context.Input.IsMouseButtonPress(MouseButton::Left);

    for (int slot = 0; slot < levelsOnThisPage; ++slot)
    {
        const float cX = startX + static_cast<float>(slot) * stepX;
        const float cY = kCardCenterY;

        if (centeredMouse.X >= cX - kCardW * 0.5f && centeredMouse.X <= cX + kCardW * 0.5f &&
            centeredMouse.Y >= cY - kCardH * 0.5f && centeredMouse.Y <= cY + kCardH * 0.5f)
        {
            m_hoveredCardIndex = startLevelIndex + slot;
            if (mouseClicked)
            {
                m_selectedLevelIndex = m_hoveredCardIndex;
                LaunchLevel(context, m_selectedLevelIndex);
            }
            break;
        }
    }

    // Navigation arrows click
    if (totalPages > 1 && mouseClicked)
    {
        constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
        if (centeredMouse.X >= -arrowX - 27.0f && centeredMouse.X <= -arrowX + 27.0f &&
            centeredMouse.Y >= kCardCenterY - 37.0f && centeredMouse.Y <= kCardCenterY + 37.0f)
        {
            if (m_currentPage > 0)
            {
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
                m_currentPage--;
                m_selectedLevelIndex = m_currentPage * kCardsPerPage;
            }
        }
        else if (centeredMouse.X >= arrowX - 27.0f && centeredMouse.X <= arrowX + 27.0f &&
                 centeredMouse.Y >= kCardCenterY - 37.0f && centeredMouse.Y <= kCardCenterY + 37.0f)
        {
            if (m_currentPage < totalPages - 1)
            {
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
                m_currentPage++;
                m_selectedLevelIndex = m_currentPage * kCardsPerPage;
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
        if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
        context.Scenes.LoadScene<MenuScene>();
        return;
    }

    UpdateRetroBackground(dt, context);
}

void LevelSelectScene::RenderThumbnailsAndEffects(const GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

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
    const int totalLevels = m_levelManager.GetLevelCount();
    const int totalPages = std::max(1, (totalLevels + kCardsPerPage - 1) / kCardsPerPage);
    const int startLevelIndex = m_currentPage * kCardsPerPage;
    const int levelsOnThisPage = std::clamp(totalLevels - startLevelIndex, 0, kCardsPerPage);

    constexpr float totalSpan = static_cast<float>(kCardsPerPage) * kCardW + static_cast<float>(kCardsPerPage - 1) * kCardGap;
    constexpr float startX = -(totalSpan - kCardW) * 0.5f;
    constexpr float stepX = kCardW + kCardGap;

    for (int slot = 0; slot < levelsOnThisPage; ++slot)
    {
        const int levelIndex = startLevelIndex + slot;
        const int levelNum = levelIndex + 1;
        const bool isUnlocked = m_levelManager.IsLevelUnlocked(levelNum);
        const uint32_t highScore = m_levelManager.GetHighScoreForLevel(levelNum);
        const bool isCleared = (levelNum <= m_levelManager.GetHighestClearedLevel()) || (highScore > 0);
        const bool isSelected = (levelIndex == m_selectedLevelIndex);
        const bool isHovered = (levelIndex == m_hoveredCardIndex);

        const float cX = screenCenterX + startX + static_cast<float>(slot) * stepX;
        const float cY = screenCenterY + kCardCenterY;

        // Card Base Panel (opaque dark background with subtle tint)
        const Color bgTint = isCleared
            ? Color{20, 32, 52, 245}
            : (isUnlocked ? Color{16, 26, 42, 245} : Color{12, 14, 20, 245});
        context.Render.DrawRectangle(kCardW, kCardH, Transform2D{Vector2f{cX, cY}}, bgTint);

        // Card Header Bar
        const Color headerTint = isCleared
            ? Color{32, 54, 86, 255}
            : (isUnlocked ? Color{26, 42, 68, 255} : Color{18, 22, 32, 255});
        context.Render.DrawRectangle(kCardW - 8.0f, 50.0f, Transform2D{Vector2f{cX, cY - 165.0f}}, headerTint);

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
            const float pulse = (std::sin(m_animTime * 6.0f) + 1.0f) * 0.5f;
            const uint8_t alpha = static_cast<uint8_t>(200 + pulse * 55.0f);
            context.Render.DrawRectangleOutline(kCardW + 6.0f, kCardH + 6.0f, Transform2D{Vector2f{cX, cY}}, Color{255, 215, 0, alpha}, 3.0f);
        }
        else if (isHovered && isUnlocked)
        {
            context.Render.DrawRectangleOutline(kCardW + 4.0f, kCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{80, 220, 255, 230}, 2.5f);
        }
        else if (isHovered && !isUnlocked)
        {
            context.Render.DrawRectangleOutline(kCardW + 4.0f, kCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{220, 60, 70, 180}, 2.0f);
        }
        else
        {
            const Color borderCol = isUnlocked ? Color{60, 95, 140, 140} : Color{45, 50, 65, 120};
            context.Render.DrawRectangleOutline(kCardW, kCardH, Transform2D{Vector2f{cX, cY}}, borderCol, 1.5f);
        }
    }

    // --- 3. PAGE NAVIGATION ARROWS ---
    if (totalPages > 1)
    {
        constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
        const bool canGoPrev = m_currentPage > 0;
        const bool canGoNext = m_currentPage < (totalPages - 1);

        // Left Arrow
        const Color leftBg = canGoPrev ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120};
        context.Render.DrawRectangle(54.0f, 74.0f, Transform2D{Vector2f{screenCenterX - arrowX, screenCenterY + kCardCenterY}}, leftBg);
        context.Render.DrawTextCentered("<", m_fontId, 46.0f,
            Transform2D{Vector2f{screenCenterX - arrowX, screenCenterY + kCardCenterY}},
            canGoPrev ? Colors::White : Color{75, 80, 95, 160});

        // Right Arrow
        const Color rightBg = canGoNext ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120};
        context.Render.DrawRectangle(54.0f, 74.0f, Transform2D{Vector2f{screenCenterX + arrowX, screenCenterY + kCardCenterY}}, rightBg);
        context.Render.DrawTextCentered(">", m_fontId, 46.0f,
            Transform2D{Vector2f{screenCenterX + arrowX, screenCenterY + kCardCenterY}},
            canGoNext ? Colors::White : Color{75, 80, 95, 160});

        // Page Indicator
        const std::string pageStr = "PAGE " + std::to_string(m_currentPage + 1) + " / " + std::to_string(totalPages);
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

void LevelSelectScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    mp_context = &context;

    RenderRetroBackground(context);
    context.Render.ResetCamera();
    RenderThumbnailsAndEffects(context);
}

void LevelSelectScene::OnDestroy(GameContext& context)
{
    DefaultScene::OnDestroy(context);
}

uint32_t LevelSelectScene::GetPostProcessShader() const
{
    if (mp_context && mp_context->Rules.GetRule(Rule::Graphics::EnableShader))
    {
        return m_shaderId;
    }
    return 0;
}

void LevelSelectScene::LaunchLevel(GameContext& context, int levelIndex)
{
    const int levelNum = levelIndex + 1;
    if (!m_levelManager.IsLevelUnlocked(levelNum))
    {
        return;
    }

    if (m_clickSfxId != 0)
    {
        context.Audio.PlaySfx(m_clickSfxId, 90.0f);
    }

    PlayerPrefs::SetInt("SelectedLevel", levelIndex);
    PlayerPrefs::Save();
    context.Scenes.LoadScene<GameScene>();
}

void LevelSelectScene::InitRetroBackground(const GameContext& context)
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    m_stars.clear();
    m_stars.reserve(100);

    uint32_t seed = 987654321;
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

void LevelSelectScene::UpdateRetroBackground(float dt, const GameContext& context)
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

void LevelSelectScene::RenderRetroBackground(const GameContext& context) const
{
    context.Render.ResetCamera();
    const Vector2f viewSize = context.Render.GetLogicalViewSize();

    // 1. Subtle retro arcade perspective grid at the bottom
    const float horizonY = viewSize.Y * 0.60f;
    const float bottomY = viewSize.Y;
    const float centerX = viewSize.X * 0.5f;

    context.Render.DrawLine(Vector2f{0.0f, horizonY}, Vector2f{viewSize.X, horizonY}, Color{45, 80, 150, 70}, 2.0f);

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

    constexpr int kPerspectiveVLines = 16;
    for (int i = -kPerspectiveVLines / 2; i <= kPerspectiveVLines / 2; ++i)
    {
        float bottomOffset = static_cast<float>(i) * (viewSize.X / static_cast<float>(kPerspectiveVLines * 0.8f));
        Vector2f start{centerX + static_cast<float>(i) * 15.0f, horizonY};
        Vector2f end{centerX + bottomOffset, bottomY};
        context.Render.DrawLine(start, end, Color{30, 65, 130, 35}, 1.2f);
    }

    // 2. Twinkling starfield in upper sky
    for (const auto& star : m_stars)
    {
        float twinkle = (std::sin(m_animTime * star.TwinkleFreq + star.X) + 1.0f) * 0.5f;
        uint8_t a = static_cast<uint8_t>(std::clamp(star.Alpha * (0.5f + 0.5f * twinkle), 30.0f, 255.0f));
        Color col{200, 220, 255, a};
        context.Render.DrawRectangle(star.Size, star.Size, Transform2D{Vector2f{star.X, star.Y}}, col);
    }
}
