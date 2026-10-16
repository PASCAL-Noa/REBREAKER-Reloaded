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
    context.Audio.PlayMusic("Resources/audio/music/Game-menu.ogg", 60.0f, true);

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
    if (m_canvas != NULL_ENTITY && m_registry.IsAlive(m_canvas))
    {
        std::vector<Entity> toDestroy;
        m_registry.View<RectTransform>([&](Entity e, const RectTransform& rect) {
            if (rect.Parent == m_canvas || e == m_canvas)
            {
                toDestroy.push_back(e);
            }
        });
        for (Entity e : toDestroy)
        {
            m_registry.DestroyEntity(e);
        }
        m_canvas = NULL_ENTITY;
    }
}

void LevelSelectScene::RebuildUI(GameContext& context)
{
    ClearUI();

    m_canvas = m_registry.CreateEntity();
    m_registry.AddComponent<CanvasComponent>(m_canvas, CanvasComponent{.IsEnabled = true});

    // 1. Title & Header (properly spaced from accent line)
    UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
        .Text = "LEVEL SELECT",
        .Position = {0.0f, -380.0f},
        .FontId = m_fontId,
        .FontSize = 56.0f,
        .Tint = Color{255, 220, 80, 255}
    });

    // Accent line with generous breathing room
    UIFactory::CreatePanel(m_registry, m_canvas, PanelDescriptor{
        .Position = {0.0f, -332.0f},
        .Size = {680.0f, 3.0f},
        .Tint = Color{60, 210, 255, 230}
    });

    // Subtitle comfortably below the accent line
    UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
        .Text = "SELECT AN UNLOCKED STAGE  |  PRESS [1-5] OR CLICK CARD",
        .Position = {0.0f, -295.0f},
        .FontId = m_fontId,
        .FontSize = 22.0f,
        .Tint = Color{200, 225, 255, 240}
    });

    const int totalLevels = m_levelManager.GetLevelCount();
    const int totalPages = std::max(1, (totalLevels + kCardsPerPage - 1) / kCardsPerPage);
    if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
    if (m_currentPage < 0) m_currentPage = 0;

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

        const Vector2f cardCenter = { startX + static_cast<float>(slot) * stepX, kCardCenterY };

        // Card Base Panel (clean arcade frame, high contrast)
        const Color bgTint = isCleared
            ? Color{22, 34, 54, 245}
            : (isUnlocked ? Color{18, 28, 44, 245} : Color{14, 16, 24, 230});

        UIFactory::CreatePanel(m_registry, m_canvas, PanelDescriptor{
            .Position = cardCenter,
            .Size = {kCardW, kCardH},
            .Tint = bgTint
        });

        // Card Header Bar (visually structures and highlights the stage title)
        const Color headerTint = isCleared
            ? Color{32, 54, 84, 255}
            : (isUnlocked ? Color{26, 42, 68, 255} : Color{18, 22, 32, 255});

        UIFactory::CreatePanel(m_registry, m_canvas, PanelDescriptor{
            .Position = {cardCenter.X, cardCenter.Y - 170.0f},
            .Size = {kCardW - 8.0f, 62.0f},
            .Tint = headerTint
        });

        // Stage Title
        const Color stageColor = isUnlocked ? Color{255, 230, 90, 255} : Color{130, 135, 150, 255};
        std::string stageNumStr = (levelNum < 10) ? ("STAGE 0" + std::to_string(levelNum)) : ("STAGE " + std::to_string(levelNum));
        UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
            .Text = stageNumStr,
            .Position = {cardCenter.X, cardCenter.Y - 180.0f},
            .FontId = m_fontId,
            .FontSize = 32.0f,
            .Tint = stageColor
        });

        // Status Badge
        if (!isUnlocked)
        {
            UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
                .Text = "[ LOCKED ]",
                .Position = {cardCenter.X, cardCenter.Y - 146.0f},
                .FontId = m_fontId,
                .FontSize = 20.0f,
                .Tint = Color{255, 75, 85, 255}
            });
        }
        else if (isCleared)
        {
            UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
                .Text = "[ CLEARED ]",
                .Position = {cardCenter.X, cardCenter.Y - 146.0f},
                .FontId = m_fontId,
                .FontSize = 20.0f,
                .Tint = Color{80, 255, 150, 255}
            });
        }
        else
        {
            UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
                .Text = "[ UNLOCKED ]",
                .Position = {cardCenter.X, cardCenter.Y - 146.0f},
                .FontId = m_fontId,
                .FontSize = 20.0f,
                .Tint = Color{80, 220, 255, 255}
            });
        }

        // Thumbnail frame placeholder (crisp dark background)
        UIFactory::CreatePanel(m_registry, m_canvas, PanelDescriptor{
            .Position = {cardCenter.X, cardCenter.Y - 50.0f},
            .Size = {214.0f, 100.0f},
            .Tint = isUnlocked ? Color{8, 12, 18, 255} : Color{8, 8, 10, 255}
        });

        // High Score Section
        const Color highLabelColor = isUnlocked ? Color{190, 215, 255, 255} : Color{110, 115, 130, 200};
        UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
            .Text = "BEST SCORE",
            .Position = {cardCenter.X, cardCenter.Y + 32.0f},
            .FontId = m_fontId,
            .FontSize = 22.0f,
            .Tint = highLabelColor
        });

        const std::string scoreStr = (highScore > 0) ? std::to_string(highScore) : "---";
        const Color scoreColor = (highScore > 0)
            ? Colors::White
            : (isUnlocked ? Color{150, 165, 190, 220} : Color{90, 95, 110, 180});

        UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
            .Text = scoreStr,
            .Position = {cardCenter.X, cardCenter.Y + 70.0f},
            .FontId = m_fontId,
            .FontSize = 32.0f,
            .Tint = scoreColor
        });

        // Action Button
        if (isUnlocked)
        {
            UIFactory::CreateButton(m_registry, m_canvas, ButtonDescriptor{
                .Text = "PLAY [" + std::to_string(levelNum) + "]",
                .OnClick = [&context, this, levelIndex]() {
                    m_selectedLevelIndex = levelIndex;
                    LaunchLevel(context, levelIndex);
                },
                .Position = {cardCenter.X, cardCenter.Y + 155.0f},
                .Size = {200.0f, 48.0f},
                .DefaultColor = Color{25, 135, 80, 255},
                .HoverColor = Color{45, 190, 115, 255},
                .PressedColor = Color{15, 95, 55, 255},
                .TextColor = Colors::White,
                .FontId = m_fontId,
                .FontSize = 26.0f
            });
        }
        else
        {
            UIFactory::CreateButton(m_registry, m_canvas, ButtonDescriptor{
                .Text = "LOCKED",
                .OnClick = nullptr,
                .Position = {cardCenter.X, cardCenter.Y + 155.0f},
                .Size = {200.0f, 48.0f},
                .DefaultColor = Color{35, 40, 50, 200},
                .HoverColor = Color{35, 40, 50, 200},
                .PressedColor = Color{35, 40, 50, 200},
                .TextColor = Color{120, 125, 140, 255},
                .FontId = m_fontId,
                .FontSize = 22.0f
            });
        }
    }

    // 2. Navigation Arrows (Left & Right)
    constexpr float arrowX = (totalSpan * 0.5f) + 60.0f;
    const bool canGoPrev = (m_currentPage > 0);
    const bool canGoNext = (m_currentPage < totalPages - 1);

    // Left Arrow Button
    UIFactory::CreateButton(m_registry, m_canvas, ButtonDescriptor{
        .Text = "<",
        .OnClick = [&context, this, canGoPrev]() {
            if (canGoPrev)
            {
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
                m_currentPage--;
                RebuildUI(context);
            }
        },
        .Position = {-arrowX, kCardCenterY},
        .Size = {54.0f, 74.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = canGoPrev ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120},
        .HoverColor   = canGoPrev ? Color{60, 110, 170, 255} : Color{25, 28, 36, 120},
        .PressedColor = canGoPrev ? Color{25, 45, 75, 255} : Color{25, 28, 36, 120},
        .TextColor    = canGoPrev ? Colors::White : Color{75, 80, 95, 160},
        .FontId = m_fontId,
        .FontSize = 46.0f
    });

    // Right Arrow Button
    UIFactory::CreateButton(m_registry, m_canvas, ButtonDescriptor{
        .Text = ">",
        .OnClick = [&context, this, canGoNext]() {
            if (canGoNext)
            {
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
                m_currentPage++;
                RebuildUI(context);
            }
        },
        .Position = {arrowX, kCardCenterY},
        .Size = {54.0f, 74.0f},
        .TextOffset = {0.0f, -8.0f},
        .DefaultColor = canGoNext ? Color{40, 65, 100, 240} : Color{25, 28, 36, 120},
        .HoverColor   = canGoNext ? Color{60, 110, 170, 255} : Color{25, 28, 36, 120},
        .PressedColor = canGoNext ? Color{25, 45, 75, 255} : Color{25, 28, 36, 120},
        .TextColor    = canGoNext ? Colors::White : Color{75, 80, 95, 160},
        .FontId = m_fontId,
        .FontSize = 46.0f
    });

    // Page indicator (e.g. "PAGE 1 / 1")
    if (totalPages > 1)
    {
        std::string pageStr = "PAGE " + std::to_string(m_currentPage + 1) + " / " + std::to_string(totalPages);
        UIFactory::CreateText(m_registry, m_canvas, TextDescriptor{
            .Text = pageStr,
            .Position = {0.0f, 255.0f},
            .FontId = m_fontId,
            .FontSize = 24.0f,
            .Tint = Color{180, 200, 230, 220}
        });
    }

    // 3. Back Button
    UIFactory::CreateButton(m_registry, m_canvas, ButtonDescriptor{
        .Text = "<- BACK TO MAIN MENU [ESC]",
        .OnClick = [&context, this]() {
            if (m_clickSfxId != 0)
            {
                context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            }
            context.Scenes.LoadScene<MenuScene>();
        },
        .Position = {0.0f, 320.0f},
        .Size = {360.0f, 52.0f},
        .DefaultColor = Color{95, 35, 42, 255},
        .HoverColor = Color{135, 50, 60, 255},
        .PressedColor = Color{65, 25, 30, 255},
        .TextColor = Colors::White,
        .FontId = m_fontId,
        .FontSize = 28.0f
    });
}

void LevelSelectScene::OnUpdate(float dt, GameContext& context)
{
    DefaultScene::OnUpdate(dt, context);
    mp_context = &context;

    if (context.Input.IsMenuCancelTriggered() || context.Input.IsKeyPress(KeyCode::B))
    {
        if (m_clickSfxId != 0)
        {
            context.Audio.PlaySfx(m_clickSfxId, 80.0f);
        }
        context.Scenes.LoadScene<MenuScene>();
        return;
    }

    const int totalLevels = m_levelManager.GetLevelCount();
    const int totalPages = std::max(1, (totalLevels + kCardsPerPage - 1) / kCardsPerPage);\

    // Left / Right page flip (keyboard or gamepad bumpers)
    if (context.Input.IsKeyPress(KeyCode::Left) || context.Input.IsJoystickButtonPress(0, 4))
    {
        if (m_currentPage > 0)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            m_currentPage--;
            RebuildUI(context);
        }
    }
    else if (context.Input.IsKeyPress(KeyCode::Right) || context.Input.IsJoystickButtonPress(0, 5))
    {
        if (m_currentPage < totalPages - 1)
        {
            if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 80.0f);
            m_currentPage++;
            RebuildUI(context);
        }
    }

    // Direct key shortcuts 1 through 5 for current page
    const int startLevelIndex = m_currentPage * kCardsPerPage;
    if (context.Input.IsKeyPress(KeyCode::Num1) || context.Input.IsKeyPress(KeyCode::Numpad1)) LaunchLevel(context, startLevelIndex + 0);
    else if (context.Input.IsKeyPress(KeyCode::Num2) || context.Input.IsKeyPress(KeyCode::Numpad2)) LaunchLevel(context, startLevelIndex + 1);
    else if (context.Input.IsKeyPress(KeyCode::Num3) || context.Input.IsKeyPress(KeyCode::Numpad3)) LaunchLevel(context, startLevelIndex + 2);
    else if (context.Input.IsKeyPress(KeyCode::Num4) || context.Input.IsKeyPress(KeyCode::Numpad4)) LaunchLevel(context, startLevelIndex + 3);
    else if (context.Input.IsKeyPress(KeyCode::Num5) || context.Input.IsKeyPress(KeyCode::Numpad5)) LaunchLevel(context, startLevelIndex + 4);

    // Track mouse hover and selection on cards
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

    for (int slot = 0; slot < levelsOnThisPage; ++slot)
    {
        const float cX = startX + static_cast<float>(slot) * stepX;
        const float cY = kCardCenterY;

        if (centeredMouse.X >= cX - kCardW * 0.5f && centeredMouse.X <= cX + kCardW * 0.5f &&
            centeredMouse.Y >= cY - kCardH * 0.5f && centeredMouse.Y <= cY + kCardH * 0.5f)
        {
            m_hoveredCardIndex = startLevelIndex + slot;
            if (context.Input.IsMouseButtonPress(MouseButton::Left))
            {
                m_selectedLevelIndex = m_hoveredCardIndex;
            }
            break;
        }
    }

    UpdateRetroBackground(dt, context);
    UISystem::OnUpdate(dt, m_registry, context);
}

void LevelSelectScene::RenderThumbnailsAndEffects(const GameContext& context) const
{
    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    const float screenCenterX = viewSize.X * 0.5f;
    const float screenCenterY = viewSize.Y * 0.5f;

    const int totalLevels = m_levelManager.GetLevelCount();
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
        const bool isSelected = (levelIndex == m_selectedLevelIndex);
        const bool isHovered = (levelIndex == m_hoveredCardIndex);

        const float cX = screenCenterX + startX + static_cast<float>(slot) * stepX;
        const float cY = screenCenterY + kCardCenterY;

        // 1. Hover & Selection Outline Feedbacks
        if (isSelected)
        {
            // Vivid pulsating gold outline for selected card
            const float pulse = (std::sin(m_animTime * 6.0f) + 1.0f) * 0.5f;
            const uint8_t alpha = static_cast<uint8_t>(210 + pulse * 45.0f);
            context.Render.DrawRectangleOutline(kCardW + 6.0f, kCardH + 6.0f, Transform2D{Vector2f{cX, cY}}, Color{255, 225, 50, alpha}, 3.5f);
        }
        else if (isHovered && isUnlocked)
        {
            // Crisp glowing cyan outline on hovered unlocked card
            context.Render.DrawRectangleOutline(kCardW + 4.0f, kCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{60, 230, 255, 255}, 3.0f);
        }
        else if (isHovered && !isUnlocked)
        {
            // Red tint outline on hovered locked card
            context.Render.DrawRectangleOutline(kCardW + 4.0f, kCardH + 4.0f, Transform2D{Vector2f{cX, cY}}, Color{240, 70, 80, 220}, 2.5f);
        }
        else
        {
            // Subtle default border outline
            const Color borderCol = isUnlocked ? Color{70, 115, 170, 200} : Color{50, 55, 75, 150};
            context.Render.DrawRectangleOutline(kCardW, kCardH, Transform2D{Vector2f{cX, cY}}, borderCol, 1.5f);
        }

        // 2. Thumbnail Frame Outline
        const float thumbBoxX = cX;
        const float thumbBoxY = cY - 50.0f;
        const Color frameColor = isSelected ? Color{255, 225, 50, 240} : (isUnlocked ? Color{80, 160, 240, 200} : Color{60, 65, 80, 150});
        context.Render.DrawRectangleOutline(214.0f, 100.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}}, frameColor, 1.8f);

        // 3. Level Pattern Miniature (Brick layout preview)
        if (static_cast<size_t>(levelIndex) < m_levelThumbnails.size())
        {
            const auto& thumb = m_levelThumbnails[static_cast<size_t>(levelIndex)];
            for (const auto& b : thumb.Bricks)
            {
                Color brickTint = b.Tint;
                if (!isUnlocked)
                {
                    // Dark grayscale if level is locked
                    uint8_t gray = static_cast<uint8_t>(b.Tint.r * 0.15f + b.Tint.g * 0.15f + b.Tint.b * 0.15f);
                    brickTint = Color{gray, gray, gray, 120};
                }
                context.Render.DrawRectangle(b.W, b.H, Transform2D{Vector2f{thumbBoxX + b.RelX, thumbBoxY + b.RelY}}, brickTint);
            }
        }

        // 4. Locked overlay lock symbol or text over preview if locked
        if (!isUnlocked)
        {
            context.Render.DrawRectangle(206.0f, 92.0f, Transform2D{Vector2f{thumbBoxX, thumbBoxY}}, Color{0, 0, 0, 160});
            context.Render.DrawText("LOCKED", m_fontId, 22.0f, Transform2D{Vector2f{thumbBoxX - 35.0f, thumbBoxY - 10.0f}}, Color{200, 60, 70, 220});
        }
    }
}

void LevelSelectScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);
    mp_context = &context;

    RenderRetroBackground(context);
    context.Render.ResetCamera();
    UISystem::OnRender(m_registry, context);
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
