#include "Scenes/AchievementScene.h"
#include "AudioMixer.h"
#include "Core/GameContext.h"
#include "Core/InputManager.h"
#include "Core/ProfileManager.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/Transform2D.h"
#include "Graphics/Renderer.h"
#include "Resources/ResourceManager.h"
#include "Scenes/ProfileScene.h"
#include "UI/ProfileGarageLayout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    constexpr int kFilterCount = static_cast<int>(AchievementCategory::Count) + 1;
    constexpr int kGridCols = 2;
    constexpr float kCardGapX = 18.0f;
    constexpr float kCardGapY = 14.0f;
    constexpr float kCardHeight = 132.0f;

    Color ScaleAlpha(Color c, float factor) noexcept
    {
        const float clamped = std::clamp(factor, 0.0f, 1.0f);
        return Color{c.r, c.g, c.b, static_cast<uint8_t>(static_cast<float>(c.a) * clamped)};
    }

    Color CategoryAccent(AchievementCategory cat, bool isSecret) noexcept
    {
        if (isSecret) return Color{230, 110, 255, 255};
        switch (cat)
        {
            case AchievementCategory::Combat:      return Color{255, 115, 75, 255};
            case AchievementCategory::Endless:     return Color{165, 115, 255, 255};
            case AchievementCategory::Campaign:    return Color{255, 205, 65, 255};
            case AchievementCategory::PowerUps:    return Color{55, 235, 255, 255};
            case AchievementCategory::Progression: return Color{95, 245, 165, 255};
            case AchievementCategory::Secret:      return Color{230, 110, 255, 255};
            default:                               return Color{255, 215, 75, 255};
        }
    }

    std::string FormatCommas(int value)
    {
        std::string s = std::to_string(std::max(0, value));
        int pos = static_cast<int>(s.length()) - 3;
        while (pos > 0)
        {
            s.insert(static_cast<size_t>(pos), ",");
            pos -= 3;
        }
        return s;
    }

    std::string FormatDuration(int totalSeconds)
    {
        const int clamped = std::max(0, totalSeconds);
        const int mins = clamped / 60;
        const int secs = clamped % 60;
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        return std::string(buf);
    }

    std::string FormatProgressLabel(const AchievementDefinition& def, const AchievementState& st)
    {
        if (def.ProgressUnit == "SEC")
        {
            return FormatDuration(st.Progress) + " / " + FormatDuration(def.Target);
        }
        return FormatCommas(st.Progress) + " / " + FormatCommas(def.Target) + " " + def.ProgressUnit;
    }

    void DrawReliefBox(Renderer& r, const UIRect& rect, Color fill, Color border, float borderW, bool hovered, bool recessed)
    {
        const float yOff = hovered ? -2.0f : 0.0f;
        const Vector2f c{rect.Center.X, rect.Center.Y + yOff};

        if (!recessed)
        {
            r.DrawRectangle(rect.Size.X + 2.0f, rect.Size.Y + 2.0f,
                            Transform2D{Vector2f{c.X, c.Y + 4.0f}}, Color{3, 6, 12, 185});
        }
        r.DrawRectangle(rect.Size.X, rect.Size.Y, Transform2D{c}, fill);
        if (!recessed)
        {
            r.DrawRectangle(rect.Size.X - 4.0f, std::max(4.0f, rect.Size.Y * 0.28f),
                            Transform2D{Vector2f{c.X, c.Y - rect.Size.Y * 0.34f}},
                            Color{255, 255, 255, hovered ? static_cast<uint8_t>(26) : static_cast<uint8_t>(14)});
        }
        r.DrawRectangleOutline(rect.Size.X, rect.Size.Y, Transform2D{c}, border, borderW);
    }

    void DrawTrophyBadge(Renderer& r, const Vector2f& center, float scale, Color gold, Color highlight)
    {
        const Color rim{150, 95, 24, 255};
        r.DrawRectangle(26.0f * scale, 5.0f * scale, Transform2D{Vector2f{center.X, center.Y + 17.5f * scale}}, rim);
        r.DrawRectangle(22.0f * scale, 4.0f * scale, Transform2D{Vector2f{center.X, center.Y + 14.0f * scale}}, gold);
        r.DrawRectangle(7.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X, center.Y + 8.0f * scale}}, gold);

        r.DrawRectangle(26.0f * scale, 17.0f * scale, Transform2D{Vector2f{center.X, center.Y - 4.0f * scale}}, gold);
        r.DrawRectangle(18.0f * scale, 6.0f * scale, Transform2D{Vector2f{center.X, center.Y + 4.5f * scale}}, gold);
        r.DrawRectangle(28.0f * scale, 3.5f * scale, Transform2D{Vector2f{center.X, center.Y - 12.0f * scale}}, highlight);

        r.DrawRectangleOutline(8.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X - 15.5f * scale, center.Y - 5.0f * scale}}, gold, 2.0f);
        r.DrawRectangleOutline(8.0f * scale, 10.0f * scale, Transform2D{Vector2f{center.X + 15.5f * scale, center.Y - 5.0f * scale}}, gold, 2.0f);

        const float d = 5.2f * scale;
        r.DrawRectangle(d, d, Transform2D{Vector2f{center.X, center.Y - 4.0f * scale}, 45.0f}, highlight);
    }

    void DrawPadlockBadge(Renderer& r, const Vector2f& center, float scale)
    {
        const Color shackle{165, 182, 210, 235};
        const Color body{92, 106, 134, 240};
        const Color border{175, 194, 224, 230};
        const Color keyhole{18, 24, 38, 255};

        r.DrawRectangleOutline(15.0f * scale, 14.0f * scale, Transform2D{Vector2f{center.X, center.Y - 10.0f * scale}}, shackle, 3.0f);
        r.DrawRectangle(25.0f * scale, 20.0f * scale, Transform2D{Vector2f{center.X, center.Y + 5.0f * scale}}, body);
        r.DrawRectangleOutline(25.0f * scale, 20.0f * scale, Transform2D{Vector2f{center.X, center.Y + 5.0f * scale}}, border, 1.8f);
        r.DrawCircle(3.2f * scale, Transform2D{Vector2f{center.X, center.Y + 3.0f * scale}}, keyhole);
        r.DrawRectangle(3.0f * scale, 6.0f * scale, Transform2D{Vector2f{center.X, center.Y + 8.0f * scale}}, keyhole);
    }

    UIRect GetBackRect(const Vector2f& viewSize)
    {
        return UIRect{Vector2f{-viewSize.X * 0.5f + 78.0f, -viewSize.Y * 0.5f + 42.0f}, Vector2f{112.0f, 44.0f}};
    }

    UIRect GetSummaryRect(const Vector2f& viewSize)
    {
        const float panelW = std::min(1160.0f, viewSize.X - 64.0f);
        return UIRect{Vector2f{0.0f, -viewSize.Y * 0.5f + 126.0f}, Vector2f{panelW, 84.0f}};
    }

    UIRect GetFilterTabRect(int idx, const Vector2f& viewSize)
    {
        const float totalW = std::min(1160.0f, viewSize.X - 64.0f);
        const float gap = 8.0f;
        const float tabW = (totalW - gap * static_cast<float>(kFilterCount - 1)) / static_cast<float>(kFilterCount);
        const float startX = -totalW * 0.5f + tabW * 0.5f;
        const float y = -viewSize.Y * 0.5f + 194.0f;
        return UIRect{Vector2f{startX + static_cast<float>(idx) * (tabW + gap), y}, Vector2f{tabW, 38.0f}};
    }

    float GetListTop(const Vector2f& viewSize) { return -viewSize.Y * 0.5f + 226.0f; }
    float GetListBottom(const Vector2f& viewSize) { return viewSize.Y * 0.5f - 24.0f; }

    UIRect GetCardRect(int index, float scrollY, const Vector2f& viewSize)
    {
        const float gridW = std::min(1160.0f, viewSize.X - 64.0f);
        const float cardW = (gridW - kCardGapX * static_cast<float>(kGridCols - 1)) / static_cast<float>(kGridCols);
        const int col = index % kGridCols;
        const int row = index / kGridCols;
        const float leftX = -gridW * 0.5f + cardW * 0.5f + static_cast<float>(col) * (cardW + kCardGapX);
        const float topY = GetListTop(viewSize) + kCardHeight * 0.5f + static_cast<float>(row) * (kCardHeight + kCardGapY) - scrollY;
        return UIRect{Vector2f{leftX, topY}, Vector2f{cardW, kCardHeight}};
    }
}

void AchievementScene::Launch(SceneManager& scenes, SceneTransitionType type, float duration)
{
    if (type == SceneTransitionType::None || duration <= 0.0f)
    {
        scenes.LoadSceneInstant<AchievementScene>();
        return;
    }
    scenes.LoadSceneWithTransition<AchievementScene>(type, duration);
}

void AchievementScene::OnInit(GameContext& context)
{
    DefaultScene::OnInit(context);
    m_bgColor = Color{10, 15, 25, 255};

    m_fontId = context.Resources.LoadResource("Resources/font/vt323.ttf");
    m_clickSfxId = context.Resources.LoadResource("Resources/audio/sfx/ui_click.wav");
    m_switchSfxId = context.Resources.LoadResource("Resources/audio/sfx/tv_switch.wav");

    context.Audio.FadeToMusic("Resources/audio/music/Game-menu.ogg", 0.8f, AudioMixer::kNominalMusicLevel, true);

    AchievementManager::Get().EvaluateProfileMilestones();
    m_activeFilter = 0;
    m_scrollY = 0.0f;
    m_animTime = 0.0f;
    RefreshFilteredList();
}

void AchievementScene::OnDestroy(GameContext& context)
{
    ProfileManager::Get().SaveProfile();
    DefaultScene::OnDestroy(context);
}

void AchievementScene::RefreshFilteredList()
{
    m_filteredDefs.clear();
    const auto& all = AchievementManager::Get().GetDefinitions();
    for (const auto& def : all)
    {
        if (m_activeFilter == 0 || static_cast<int>(def.Category) + 1 == m_activeFilter)
        {
            m_filteredDefs.push_back(def);
        }
    }
    m_scrollY = 0.0f;
}

void AchievementScene::ClampScroll(const Vector2f& viewSize)
{
    const int count = static_cast<int>(m_filteredDefs.size());
    const int rows = (count + kGridCols - 1) / kGridCols;
    const float contentH = static_cast<float>(rows) * (kCardHeight + kCardGapY);
    const float visibleH = std::max(100.0f, GetListBottom(viewSize) - GetListTop(viewSize));
    const float maxScroll = std::max(0.0f, contentH - visibleH + 12.0f);
    m_scrollY = std::clamp(m_scrollY, 0.0f, maxScroll);
}

void AchievementScene::ReturnToProfile(GameContext& context)
{
    if (m_clickSfxId != 0)
    {
        context.Audio.PlaySfx(m_clickSfxId, 18.0f);
    }
    context.Scenes.LoadSceneInstant<ProfileScene>();
}

void AchievementScene::OnUpdate(float dt, GameContext& context)
{
    m_animTime += dt;
    DefaultScene::OnUpdate(dt, context);

    const Vector2f viewSize = context.Render.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    if (context.Input.IsKeyPress(KeyCode::Escape))
    {
        ReturnToProfile(context);
        return;
    }

    if (context.Input.IsKeyPress(KeyCode::Left))
    {
        m_activeFilter = (m_activeFilter + kFilterCount - 1) % kFilterCount;
        RefreshFilteredList();
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 15.0f);
    }
    else if (context.Input.IsKeyPress(KeyCode::Right))
    {
        m_activeFilter = (m_activeFilter + 1) % kFilterCount;
        RefreshFilteredList();
        if (m_switchSfxId != 0) context.Audio.PlaySfx(m_switchSfxId, 15.0f);
    }

    const float wheel = context.Input.GetMouseWheelDelta();
    if (std::abs(wheel) > 0.01f)
    {
        m_scrollY -= wheel * 62.0f;
    }
    if (context.Input.IsKeyDown(KeyCode::Down)) m_scrollY += 420.0f * dt;
    if (context.Input.IsKeyDown(KeyCode::Up)) m_scrollY -= 420.0f * dt;
    ClampScroll(viewSize);

    if (!context.Input.IsMouseButtonPress(MouseButton::Left)) return;

    if (GetBackRect(viewSize).Contains(mouse))
    {
        ReturnToProfile(context);
        return;
    }

    for (int i = 0; i < kFilterCount; ++i)
    {
        if (GetFilterTabRect(i, viewSize).Contains(mouse))
        {
            if (m_activeFilter != i)
            {
                m_activeFilter = i;
                RefreshFilteredList();
                if (m_clickSfxId != 0) context.Audio.PlaySfx(m_clickSfxId, 18.0f);
            }
            return;
        }
    }
}

void AchievementScene::OnRender(GameContext& context)
{
    DefaultScene::OnRender(context);

    Camera2D cam{};
    cam.Position = Vector2f{0.0f, 0.0f};
    cam.Zoom = 1.0f;
    cam.Rotation = 0.0f;
    context.Render.SetCamera(cam);

    Renderer& r = context.Render;
    const Vector2f viewSize = r.GetLogicalViewSize();
    auto [mx, my] = context.Input.GetMousePosition();
    const Vector2f mouse{mx - viewSize.X * 0.5f, my - viewSize.Y * 0.5f};

    const auto& am = AchievementManager::Get();
    const auto& pm = ProfileManager::Get();
    const Color primary = pm.GetEquippedPrimaryColor();
    const Color gold{255, 215, 70, 255};
    const Color goldLight{255, 245, 175, 255};

    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    for (float gx = -halfW; gx <= halfW; gx += 64.0f)
    {
        r.DrawLine(Vector2f{gx, -halfH}, Vector2f{gx, halfH}, Color{24, 34, 54, 55}, 1.0f);
    }
    for (float gy = -halfH; gy <= halfH; gy += 64.0f)
    {
        r.DrawLine(Vector2f{-halfW, gy}, Vector2f{halfW, gy}, Color{24, 34, 54, 55}, 1.0f);
    }

    const UIRect backRect = GetBackRect(viewSize);
    const bool backHov = backRect.Contains(mouse);
    DrawReliefBox(r, backRect, backHov ? Color{30, 44, 72, 245} : Color{18, 26, 44, 240},
                  backHov ? Colors::White : ScaleAlpha(primary, 0.85f), 2.0f, backHov, false);

    if (m_fontId != 0)
    {
        r.DrawTextCentered("< BACK", m_fontId, 20.0f, Transform2D{Vector2f{backRect.Center.X, backRect.Center.Y + (backHov ? -2.0f : 0.0f)}}, Colors::White);
        r.DrawTextCentered("ACHIEVEMENT SHOWCASE", m_fontId, 34.0f, Transform2D{Vector2f{0.0f, -halfH + 40.0f}}, goldLight);
    }

    const UIRect summary = GetSummaryRect(viewSize);
    DrawReliefBox(r, summary, Color{14, 21, 36, 246}, ScaleAlpha(gold, 0.75f), 2.0f, false, false);
    DrawTrophyBadge(r, Vector2f{summary.Left() + 44.0f, summary.Center.Y}, 1.05f, gold, goldLight);

    const int unlocked = am.GetUnlockedCount();
    const int total = am.GetTotalCount();
    const float pct = am.GetCompletionPercentage();
    const int bonusEarned = am.GetTotalRewardCreditsEarned();

    if (m_fontId != 0)
    {
        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", pct);

        const float textLeft = summary.Left() + 86.0f;
        const float textRight = summary.Right() - 26.0f;

        r.DrawText("OVERALL COMPLETION: " + std::to_string(unlocked) + " / " + std::to_string(total) + "  (" + std::string(pctBuf) + ")",
                   m_fontId, 22.0f, Transform2D{Vector2f{textLeft, summary.Top() + 12.0f}}, Colors::White);

        const std::string rewardSummary = "REWARDS EARNED: +" + FormatCommas(bonusEarned) + " CR";
        const Vector2f rSz = r.GetTextSize(rewardSummary, m_fontId, 20.0f);
        r.DrawText(rewardSummary, m_fontId, 20.0f, Transform2D{Vector2f{textRight - rSz.X, summary.Top() + 14.0f}}, gold);

        const float barW = textRight - textLeft;
        const float barH = 18.0f;
        const Vector2f barCenter{textLeft + barW * 0.5f, summary.Bottom() - 24.0f};
        r.DrawRectangle(barW, barH, Transform2D{barCenter}, Color{10, 14, 24, 245});
        r.DrawRectangleOutline(barW, barH, Transform2D{barCenter}, ScaleAlpha(gold, 0.55f), 1.4f);

        const float fillW = std::max(0.0f, (barW - 4.0f) * std::clamp(pct / 100.0f, 0.0f, 1.0f));
        if (fillW > 1.0f)
        {
            r.DrawRectangle(fillW, barH - 4.0f, Transform2D{Vector2f{textLeft + 2.0f + fillW * 0.5f, barCenter.Y}}, gold);
            r.DrawRectangle(fillW, 5.0f, Transform2D{Vector2f{textLeft + 2.0f + fillW * 0.5f, barCenter.Y - 4.0f}}, goldLight);
        }
    }

    for (int i = 0; i < kFilterCount; ++i)
    {
        const UIRect tab = GetFilterTabRect(i, viewSize);
        const bool active = (m_activeFilter == i);
        const bool hov = tab.Contains(mouse);
        const Color catCol = (i == 0) ? gold : CategoryAccent(static_cast<AchievementCategory>(i - 1), i == 6);
        const Color fill = active ? ScaleAlpha(catCol, 0.28f) : (hov ? Color{26, 36, 58, 245} : Color{15, 22, 36, 238});
        const Color border = active ? catCol : (hov ? Colors::White : Color{60, 76, 108, 200});

        DrawReliefBox(r, tab, fill, border, active ? 2.2f : 1.5f, hov && !active, false);

        if (m_fontId != 0)
        {
            std::string label;
            if (i == 0)
            {
                label = "ALL " + std::to_string(unlocked) + "/" + std::to_string(total);
            }
            else
            {
                const auto cat = static_cast<AchievementCategory>(i - 1);
                label = std::string(AchievementManager::GetCategoryName(cat)) + " "
                      + std::to_string(am.GetUnlockedCountByCategory(cat)) + "/"
                      + std::to_string(am.GetTotalCountByCategory(cat));
            }
            r.DrawTextCentered(label, m_fontId, 16.0f,
                               Transform2D{Vector2f{tab.Center.X, tab.Center.Y + (hov && !active ? -2.0f : 0.0f)}},
                               active ? Colors::White : Color{190, 206, 232, 245});
        }
    }

    const float listTop = GetListTop(viewSize);
    const float listBot = GetListBottom(viewSize);

    for (size_t i = 0; i < m_filteredDefs.size(); ++i)
    {
        const UIRect card = GetCardRect(static_cast<int>(i), m_scrollY, viewSize);
        if (card.Top() < listTop - 6.0f || card.Bottom() > listBot + 6.0f) continue;

        const auto& def = m_filteredDefs[i];
        const AchievementState st = am.GetState(def.Id);
        const bool isHiddenSecret = def.IsSecret && !st.Unlocked;
        const Color accent = CategoryAccent(def.Category, def.IsSecret);

        const Color bg = st.Unlocked
            ? Color{18, 28, 46, 245}
            : (isHiddenSecret ? Color{18, 14, 28, 240} : Color{13, 18, 30, 238});
        const Color border = st.Unlocked
            ? gold
            : (isHiddenSecret ? ScaleAlpha(accent, 0.55f) : Color{62, 76, 104, 200});

        DrawReliefBox(r, card, bg, border, st.Unlocked ? 2.2f : 1.5f, false, !st.Unlocked);

        r.DrawRectangle(5.0f, card.Size.Y - 4.0f,
                        Transform2D{Vector2f{card.Left() + 3.5f, card.Center.Y}},
                        st.Unlocked ? gold : ScaleAlpha(accent, 0.45f));

        const Vector2f iconCenter{card.Left() + 52.0f, card.Center.Y};
        r.DrawRectangle(74.0f, 88.0f, Transform2D{iconCenter}, Color{9, 14, 24, 235});
        r.DrawRectangleOutline(74.0f, 88.0f, Transform2D{iconCenter}, ScaleAlpha(border, 0.65f), 1.4f);

        if (st.Unlocked)
        {
            const float shimmer = 1.0f + 0.04f * std::sin(m_animTime * 4.5f + static_cast<float>(i));
            DrawTrophyBadge(r, iconCenter, 1.05f * shimmer, gold, goldLight);
        }
        else if (isHiddenSecret)
        {
            if (m_fontId != 0)
            {
                r.DrawTextCentered("?", m_fontId, 46.0f, Transform2D{Vector2f{iconCenter.X, iconCenter.Y - 2.0f}}, accent);
            }
        }
        else
        {
            DrawPadlockBadge(r, iconCenter, 1.0f);
        }

        if (m_fontId == 0) continue;

        const float tx = card.Left() + 102.0f;
        const float rightLimit = card.Right() - 16.0f;
        const float ty = card.Top() + 11.0f;

        const std::string titleText = isHiddenSecret ? "CLASSIFIED ACHIEVEMENT" : def.Title;
        const std::string descText = isHiddenSecret ? "Complete a hidden objective to reveal." : def.Description;

        r.DrawText(titleText, m_fontId, 22.0f, Transform2D{Vector2f{tx, ty}},
                   st.Unlocked ? Colors::White : Color{185, 198, 220, 235});

        const std::string statusTag = st.Unlocked ? "UNLOCKED" : (def.IsSecret ? "SECRET" : "LOCKED");
        const Color statusCol = st.Unlocked ? Color{95, 255, 165, 255} : (def.IsSecret ? accent : Color{135, 150, 178, 220});
        const Vector2f tagSz = r.GetTextSize(statusTag, m_fontId, 15.0f);
        r.DrawText(statusTag, m_fontId, 15.0f, Transform2D{Vector2f{rightLimit - tagSz.X, ty + 4.0f}}, statusCol);

        r.DrawText(descText, m_fontId, 16.0f, Transform2D{Vector2f{tx, ty + 27.0f}}, Color{168, 188, 218, 240});

        const float barW = rightLimit - tx;
        const float barH = 12.0f;
        const Vector2f barC{tx + barW * 0.5f, ty + 62.0f};
        r.DrawRectangle(barW, barH, Transform2D{barC}, Color{9, 13, 22, 245});
        const float ratio = st.Unlocked ? 1.0f : (def.Target > 0 ? std::clamp(static_cast<float>(st.Progress) / static_cast<float>(def.Target), 0.0f, 1.0f) : 0.0f);
        const float fillW = std::max(0.0f, (barW - 2.0f) * (isHiddenSecret ? 0.0f : ratio));
        if (fillW > 1.0f)
        {
            r.DrawRectangle(fillW, barH - 2.0f, Transform2D{Vector2f{tx + 1.0f + fillW * 0.5f, barC.Y}},
                            st.Unlocked ? Color{85, 245, 155, 250} : accent);
        }

        const std::string metaText = st.Unlocked
            ? ("UNLOCKED " + (st.UnlockedTimestamp.empty() ? std::string("COMPLETE") : st.UnlockedTimestamp))
            : (isHiddenSecret ? "HIDDEN OBJECTIVE" : FormatProgressLabel(def, st));
        r.DrawText(metaText, m_fontId, 14.0f, Transform2D{Vector2f{tx, ty + 74.0f}},
                   st.Unlocked ? Color{125, 250, 185, 245} : Color{215, 200, 140, 235});

        float chipX = tx;
        const float chipY = card.Bottom() - 18.0f;
        if (def.RewardCredits > 0)
        {
            const std::string crChip = "+" + FormatCommas(def.RewardCredits) + " CR";
            const Vector2f sz = r.GetTextSize(crChip, m_fontId, 14.0f);
            const float cw = sz.X + 14.0f;
            r.DrawRectangle(cw, 20.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY}},
                            st.Unlocked ? Color{56, 46, 14, 245} : Color{24, 28, 42, 235});
            r.DrawRectangleOutline(cw, 20.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY}},
                                   st.Unlocked ? gold : Color{110, 118, 140, 180}, 1.2f);
            r.DrawTextCentered(crChip, m_fontId, 14.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY - 1.0f}},
                               st.Unlocked ? goldLight : Color{185, 188, 205, 220});
            chipX += cw + 8.0f;
        }
        if (!def.RewardCosmeticId.empty())
        {
            if (const CosmeticItem* item = pm.GetCosmeticById(def.RewardCosmeticId))
            {
                const std::string cosChip = "COSMETIC: " + item->Name;
                const Vector2f sz = r.GetTextSize(cosChip, m_fontId, 14.0f);
                const float cw = std::min(rightLimit - chipX, sz.X + 14.0f);
                if (cw > 40.0f)
                {
                    const Color cFill = st.Unlocked ? Color{18, 42, 62, 245} : Color{22, 26, 40, 235};
                    const Color cBorder = st.Unlocked ? primary : Color{95, 108, 136, 180};
                    r.DrawRectangle(cw, 20.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY}}, cFill);
                    r.DrawRectangleOutline(cw, 20.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY}}, cBorder, 1.2f);
                    r.DrawTextCentered(cosChip, m_fontId, 14.0f, Transform2D{Vector2f{chipX + cw * 0.5f, chipY - 1.0f}},
                                       st.Unlocked ? Colors::White : Color{175, 188, 210, 220});
                }
            }
        }
    }

    const int count = static_cast<int>(m_filteredDefs.size());
    const int rows = (count + kGridCols - 1) / kGridCols;
    const float contentH = static_cast<float>(rows) * (kCardHeight + kCardGapY);
    const float visibleH = std::max(100.0f, listBot - listTop);
    if (contentH > visibleH + 4.0f)
    {
        const float gridW = std::min(1160.0f, viewSize.X - 64.0f);
        const float trackX = gridW * 0.5f + 14.0f;
        const float trackCenterY = (listTop + listBot) * 0.5f;
        r.DrawRectangle(6.0f, visibleH, Transform2D{Vector2f{trackX, trackCenterY}}, Color{22, 30, 48, 210});
        const float thumbH = std::max(36.0f, visibleH * (visibleH / contentH));
        const float maxScroll = std::max(1.0f, contentH - visibleH + 12.0f);
        const float scrollT = std::clamp(m_scrollY / maxScroll, 0.0f, 1.0f);
        const float thumbY = listTop + thumbH * 0.5f + scrollT * (visibleH - thumbH);
        r.DrawRectangle(6.0f, thumbH, Transform2D{Vector2f{trackX, thumbY}}, ScaleAlpha(gold, 0.85f));
    }
}
