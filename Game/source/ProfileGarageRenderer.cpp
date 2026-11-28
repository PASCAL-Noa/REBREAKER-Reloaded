#include "UI/ProfileGarageRenderer.h"
#include "Core/AchievementManager.h"
#include "Core/PlayerPrefs.h"
#include "PowerUps/PowerUpConfig.h"
#include "Graphics/Renderer.h"
#include "ECS/Components/Transform2D.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    Color ScaleAlpha(Color c, float factor)
    {
        c.a = static_cast<uint8_t>(std::clamp(static_cast<float>(c.a) * factor, 0.0f, 255.0f));
        return c;
    }

    Color MixColor(Color a, Color b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color{
            static_cast<uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<uint8_t>(a.a + (b.a - a.a) * t)
        };
    }

    std::string FormatCommas(uint32_t value)
    {
        std::string s = std::to_string(value);
        int pos = static_cast<int>(s.length()) - 3;
        while (pos > 0)
        {
            s.insert(static_cast<size_t>(pos), ",");
            pos -= 3;
        }
        return s;
    }

    void DrawCornerBrackets(Renderer& r, Vector2f c, Vector2f s, float arm, float thick, Color col)
    {
        const float l = c.X - s.X * 0.5f;
        const float ri = c.X + s.X * 0.5f;
        const float t = c.Y - s.Y * 0.5f;
        const float b = c.Y + s.Y * 0.5f;
        r.DrawLine(Vector2f{l, t}, Vector2f{l + arm, t}, col, thick);
        r.DrawLine(Vector2f{l, t}, Vector2f{l, t + arm}, col, thick);
        r.DrawLine(Vector2f{ri, t}, Vector2f{ri - arm, t}, col, thick);
        r.DrawLine(Vector2f{ri, t}, Vector2f{ri, t + arm}, col, thick);
        r.DrawLine(Vector2f{l, b}, Vector2f{l + arm, b}, col, thick);
        r.DrawLine(Vector2f{l, b}, Vector2f{l, b - arm}, col, thick);
        r.DrawLine(Vector2f{ri, b}, Vector2f{ri - arm, b}, col, thick);
        r.DrawLine(Vector2f{ri, b}, Vector2f{ri, b - arm}, col, thick);
    }

    void DrawReliefPanel(Renderer& r, const UIRect& rect, Color baseFill, Color borderCol, float borderThick, bool hovered, bool pressed, bool recessed)
    {
        const float offsetY = pressed ? 3.0f : (hovered ? -2.0f : 0.0f);
        const float shadowOffY = pressed ? 1.5f : (hovered ? 7.0f : 5.0f);
        const Vector2f c{rect.Center.X, rect.Center.Y + offsetY};

        r.DrawRectangle(rect.Size.X + 2.0f, rect.Size.Y + 2.0f, Transform2D{Vector2f{rect.Center.X, rect.Center.Y + shadowOffY}}, Color{0, 0, 0, 145});
        r.DrawRectangle(rect.Size.X, rect.Size.Y, Transform2D{c}, baseFill);

        const float bevelH = std::max(3.0f, std::min(7.0f, rect.Size.Y * 0.06f));
        const Color topStrip = recessed ? Color{4, 7, 14, 130} : Color{255, 255, 255, pressed ? static_cast<uint8_t>(14) : static_cast<uint8_t>(42)};
        const Color botStrip = recessed ? Color{255, 255, 255, 22} : Color{4, 7, 14, 145};

        r.DrawRectangle(rect.Size.X - 6.0f, bevelH, Transform2D{Vector2f{c.X, c.Y - rect.Size.Y * 0.5f + bevelH * 0.5f + 2.0f}}, topStrip);
        r.DrawRectangle(rect.Size.X - 6.0f, bevelH, Transform2D{Vector2f{c.X, c.Y + rect.Size.Y * 0.5f - bevelH * 0.5f - 2.0f}}, botStrip);

        r.DrawRectangleOutline(rect.Size.X - 6.0f, rect.Size.Y - 6.0f, Transform2D{c}, Color{255, 255, 255, pressed ? static_cast<uint8_t>(18) : static_cast<uint8_t>(30)}, 1.0f);
        r.DrawRectangleOutline(rect.Size.X, rect.Size.Y, Transform2D{c}, borderCol, borderThick);
    }

    void DrawChevron(Renderer& r, Vector2f c, float size, bool pointRight, Color col, float thick)
    {
        const float dir = pointRight ? 1.0f : -1.0f;
        const Vector2f tip{c.X + dir * size * 0.34f, c.Y};
        const Vector2f top{c.X - dir * size * 0.34f, c.Y - size * 0.52f};
        const Vector2f bot{c.X - dir * size * 0.34f, c.Y + size * 0.52f};
        r.DrawLine(top, tip, col, thick);
        r.DrawLine(tip, bot, col, thick);
    }

    void DrawCreditTokenIcon(Renderer& r, Vector2f c, float radius)
    {
        const Color goldOuter{245, 185, 38, 255};
        const Color goldInner{255, 226, 102, 255};
        const Color goldDark{128, 86, 14, 255};

        r.DrawCircle(radius + 1.5f, Transform2D{Vector2f{c.X, c.Y + 2.0f}}, Color{0, 0, 0, 130});
        r.DrawCircle(radius, Transform2D{c}, goldOuter);
        r.DrawCircleOutline(radius, Transform2D{c}, Color{255, 246, 195, 240}, 2.0f);
        r.DrawCircle(radius * 0.72f, Transform2D{c}, goldInner);
        r.DrawCircleOutline(radius * 0.72f, Transform2D{c}, goldDark, 1.5f);

        const float w = radius * 0.52f;
        const float h = radius * 0.58f;
        r.DrawLine(Vector2f{c.X + w * 0.45f, c.Y - h * 0.5f}, Vector2f{c.X - w * 0.35f, c.Y - h * 0.5f}, goldDark, 2.4f);
        r.DrawLine(Vector2f{c.X - w * 0.35f, c.Y - h * 0.5f}, Vector2f{c.X - w * 0.35f, c.Y + h * 0.5f}, goldDark, 2.4f);
        r.DrawLine(Vector2f{c.X - w * 0.35f, c.Y + h * 0.5f}, Vector2f{c.X + w * 0.45f, c.Y + h * 0.5f}, goldDark, 2.4f);
        r.DrawLine(Vector2f{c.X - w * 0.05f, c.Y - h * 0.72f}, Vector2f{c.X - w * 0.05f, c.Y + h * 0.72f}, goldDark, 1.6f);
    }

    void DrawStaticSideIcon(Renderer& r, int idx, Vector2f c, Color primary, Color secondary)
    {
        if (idx == 0)
        {
            r.DrawRectangle(20.0f, 20.0f, Transform2D{Vector2f{c.X - 8.0f, c.Y - 6.0f}}, primary);
            r.DrawRectangleOutline(20.0f, 20.0f, Transform2D{Vector2f{c.X - 8.0f, c.Y - 6.0f}}, Colors::White, 1.6f);
            r.DrawRectangle(20.0f, 20.0f, Transform2D{Vector2f{c.X + 8.0f, c.Y + 6.0f}}, secondary);
            r.DrawRectangleOutline(20.0f, 20.0f, Transform2D{Vector2f{c.X + 8.0f, c.Y + 6.0f}}, Colors::White, 1.6f);
        }
        else if (idx == 1)
        {
            const Color gold{255, 212, 72, 255};
            r.DrawRectangle(22.0f, 17.0f, Transform2D{Vector2f{c.X, c.Y - 6.0f}}, gold);
            r.DrawCircleOutline(7.0f, Transform2D{Vector2f{c.X - 12.0f, c.Y - 6.0f}}, gold, 2.2f);
            r.DrawCircleOutline(7.0f, Transform2D{Vector2f{c.X + 12.0f, c.Y - 6.0f}}, gold, 2.2f);
            r.DrawRectangle(6.0f, 11.0f, Transform2D{Vector2f{c.X, c.Y + 8.0f}}, gold);
            r.DrawRectangle(20.0f, 5.0f, Transform2D{Vector2f{c.X, c.Y + 14.0f}}, Color{255, 236, 150, 255});
        }
        else
        {
            const float s = 30.0f;
            r.DrawRectangle(s, s, Transform2D{c}, Color{225, 236, 252, 245});
            r.DrawRectangleOutline(s, s, Transform2D{c}, primary, 2.0f);
            const Color pip{16, 24, 38, 255};
            const float d = 7.5f;
            r.DrawCircle(2.8f, Transform2D{c}, pip);
            r.DrawCircle(2.8f, Transform2D{Vector2f{c.X - d, c.Y - d}}, pip);
            r.DrawCircle(2.8f, Transform2D{Vector2f{c.X + d, c.Y - d}}, pip);
            r.DrawCircle(2.8f, Transform2D{Vector2f{c.X - d, c.Y + d}}, pip);
            r.DrawCircle(2.8f, Transform2D{Vector2f{c.X + d, c.Y + d}}, pip);
        }
    }

    void DrawStarIcon(Renderer& r, Vector2f c, float outerR, float innerR, Color fillCol, Color edgeCol)
    {
        constexpr float kPi = 3.14159265358979323846f;
        Vector2f pts[10];
        for (int i = 0; i < 10; ++i)
        {
            const float rad = (i % 2 == 0) ? outerR : innerR;
            const float ang = -kPi * 0.5f + static_cast<float>(i) * (kPi / 5.0f);
            pts[i] = Vector2f{c.X + std::cos(ang) * rad, c.Y + std::sin(ang) * rad};
        }

        std::vector<Vertex> verts;
        verts.reserve(30);
        for (int i = 0; i < 10; ++i)
        {
            const Vector2f& a = pts[i];
            const Vector2f& b = pts[(i + 1) % 10];
            verts.push_back(Vertex{c.X, c.Y, fillCol, 0.0f, 0.0f});
            verts.push_back(Vertex{a.X, a.Y, fillCol, 0.0f, 0.0f});
            verts.push_back(Vertex{b.X, b.Y, fillCol, 0.0f, 0.0f});
        }
        r.DrawVertices(verts, PrimitiveType::Triangles);

        for (int i = 0; i < 10; ++i)
        {
            r.DrawLine(pts[i], pts[(i + 1) % 10], edgeCol, 1.7f);
        }
    }

    void DrawSkullIcon(Renderer& r, Vector2f c, Color boneCol, Color edgeCol)
    {
        const Color dark{14, 20, 32, 255};
        const Vector2f cranium{c.X, c.Y - 4.0f};
        const Vector2f jaw{c.X, c.Y + 10.0f};

        r.DrawCircle(14.5f, Transform2D{cranium}, boneCol);
        r.DrawRectangle(16.0f, 11.0f, Transform2D{jaw}, boneCol);
        r.DrawCircleOutline(14.5f, Transform2D{cranium}, edgeCol, 1.5f);
        r.DrawLine(Vector2f{jaw.X - 8.0f, jaw.Y - 4.0f}, Vector2f{jaw.X - 8.0f, jaw.Y + 5.5f}, edgeCol, 1.5f);
        r.DrawLine(Vector2f{jaw.X + 8.0f, jaw.Y - 4.0f}, Vector2f{jaw.X + 8.0f, jaw.Y + 5.5f}, edgeCol, 1.5f);
        r.DrawLine(Vector2f{jaw.X - 8.0f, jaw.Y + 5.5f}, Vector2f{jaw.X + 8.0f, jaw.Y + 5.5f}, edgeCol, 1.5f);

        r.DrawCircle(4.1f, Transform2D{Vector2f{c.X - 5.6f, c.Y - 2.5f}}, dark);
        r.DrawCircle(4.1f, Transform2D{Vector2f{c.X + 5.6f, c.Y - 2.5f}}, dark);

        std::vector<Vertex> nose = {
            Vertex{c.X, c.Y + 1.8f, dark, 0.0f, 0.0f},
            Vertex{c.X - 2.5f, c.Y + 6.4f, dark, 0.0f, 0.0f},
            Vertex{c.X + 2.5f, c.Y + 6.4f, dark, 0.0f, 0.0f}
        };
        r.DrawVertices(nose, PrimitiveType::Triangles);

        r.DrawLine(Vector2f{c.X - 4.0f, c.Y + 10.2f}, Vector2f{c.X - 4.0f, c.Y + 15.6f}, dark, 1.8f);
        r.DrawLine(Vector2f{c.X, c.Y + 10.2f}, Vector2f{c.X, c.Y + 15.6f}, dark, 1.8f);
        r.DrawLine(Vector2f{c.X + 4.0f, c.Y + 10.2f}, Vector2f{c.X + 4.0f, c.Y + 15.6f}, dark, 1.8f);
    }

    void DrawStaticTabIcon(Renderer& r, int tabIdx, Vector2f c, const ProfileRenderAssets& assets, Color accent, bool active)
    {
        const Color mainCol = active ? Colors::White : Color{198, 214, 236, 238};
        const Color edgeCol = active ? accent : Color{110, 134, 168, 220};

        if (tabIdx == 0)
        {
            CosmeticRenderer::DrawPaddle(
                r, c, Vector2f{68.0f, 16.0f}, 0.0f,
                PaddleSkinStyle::ClassicSprite, mainCol, accent, 0.0f, 1.0f, assets.PaddleTexId);
        }
        else if (tabIdx == 1)
        {
            CosmeticRenderer::DrawBall(
                r, c, 17.5f,
                BallSkinStyle::ClassicSprite, mainCol, accent, 0.0f, 1.0f, false, false, false, assets.BallTexId);
        }
        else if (tabIdx == 2)
        {
            DrawStarIcon(r, c, 19.5f, 8.4f, mainCol, edgeCol);
        }
        else
        {
            DrawSkullIcon(r, c, mainCol, edgeCol);
        }
    }

    void DrawLockedBadge(Renderer& r, Vector2f center, float iconSize)
    {
        r.DrawCircle(iconSize * 1.08f, Transform2D{Vector2f{center.X, center.Y + 2.0f}}, Color{0, 0, 0, 140});
        r.DrawCircle(iconSize, Transform2D{center}, Color{14, 19, 30, 240});
        r.DrawCircleOutline(iconSize, Transform2D{center}, Color{255, 196, 72, 235}, 2.0f);
        CosmeticRenderer::DrawLockIcon(r, center, iconSize * 0.92f, Color{255, 214, 96, 255});
    }
}

void ProfileGarageRenderer::Render(
    Renderer& renderer,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const ProfileShowcaseSnapshot& showcase,
    const Vector2f& viewSize,
    const Vector2f& mouse)
{
    const Color pri = state.GetPreviewPrimaryColor();
    const Color sec = state.GetPreviewSecondaryColor();

    RenderBackground(renderer, viewSize, pri);
    RenderCornerFrames(renderer, viewSize, pri, sec);
    RenderTopHud(renderer, state, assets, showcase.AnimTime, viewSize, mouse, pri);
    RenderStatsPanels(renderer, assets, viewSize, pri);
    RenderSideRail(renderer, state, viewSize, mouse, pri, sec);
    RenderFocusedShowcase(renderer, state, assets, showcase, viewSize);
    RenderCategoryTabs(renderer, state, assets, viewSize, mouse, pri);
    RenderPaginatedDrawer(renderer, state, assets, viewSize, mouse, pri, sec);

    if (state.IsColorModalOpen())
    {
        RenderColorModal(renderer, state, assets, showcase, viewSize, mouse);
    }

    RenderToast(renderer, state, assets, viewSize);
}

void ProfileGarageRenderer::RenderBackground(Renderer& r, const Vector2f& viewSize, Color primary)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const Color gridCol = ScaleAlpha(MixColor(Color{26, 36, 54, 255}, primary, 0.12f), 0.38f);

    for (float gx = -halfW; gx <= halfW; gx += 72.0f)
    {
        r.DrawLine(Vector2f{gx, -halfH}, Vector2f{gx, halfH}, gridCol, 1.0f);
    }
    for (float gy = -halfH; gy <= halfH; gy += 72.0f)
    {
        r.DrawLine(Vector2f{-halfW, gy}, Vector2f{halfW, gy}, gridCol, 1.0f);
    }

    r.DrawRectangle(viewSize.X, 220.0f, Transform2D{Vector2f{0.0f, -halfH + 110.0f}}, ScaleAlpha(primary, 0.045f), BlendMode::Add);
}

void ProfileGarageRenderer::RenderCornerFrames(Renderer& r, const Vector2f& viewSize, Color primary, Color secondary)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    const float m = 18.0f;
    const float arm = 54.0f;
    const Color frameCol = ScaleAlpha(primary, 0.55f);
    const Color accentCol = ScaleAlpha(secondary, 0.35f);

    r.DrawLine(Vector2f{-halfW + m, -halfH + m}, Vector2f{-halfW + m + arm, -halfH + m}, frameCol, 2.5f);
    r.DrawLine(Vector2f{-halfW + m, -halfH + m}, Vector2f{-halfW + m, -halfH + m + arm}, frameCol, 2.5f);

    r.DrawLine(Vector2f{halfW - m, -halfH + m}, Vector2f{halfW - m - arm, -halfH + m}, frameCol, 2.5f);
    r.DrawLine(Vector2f{halfW - m, -halfH + m}, Vector2f{halfW - m, -halfH + m + arm}, frameCol, 2.5f);

    r.DrawLine(Vector2f{-halfW + m, halfH - m}, Vector2f{-halfW + m + arm, halfH - m}, frameCol, 2.5f);
    r.DrawLine(Vector2f{-halfW + m, halfH - m}, Vector2f{-halfW + m, halfH - m - arm}, frameCol, 2.5f);

    r.DrawLine(Vector2f{halfW - m, halfH - m}, Vector2f{halfW - m - arm, halfH - m}, frameCol, 2.5f);
    r.DrawLine(Vector2f{halfW - m, halfH - m}, Vector2f{halfW - m, halfH - m - arm}, frameCol, 2.5f);

    r.DrawLine(Vector2f{-halfW + m, -halfH + 92.0f}, Vector2f{-halfW + m + 16.0f, -halfH + 92.0f}, accentCol, 2.0f);
    r.DrawLine(Vector2f{halfW - m, -halfH + 92.0f}, Vector2f{halfW - m - 16.0f, -halfH + 92.0f}, accentCol, 2.0f);
}

void ProfileGarageRenderer::RenderTopHud(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    float animTime,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    Color primary)
{
    const UIRect backRect = ProfileGarageLayout::GetBackButtonRect(viewSize);
    const bool backHov = !state.IsColorModalOpen() && backRect.Contains(mouse);

    DrawReliefPanel(r, backRect, backHov ? Color{34, 48, 72, 245} : Color{20, 28, 44, 240}, backHov ? Colors::White : primary, 2.0f, backHov, false, false);
    const float backOffY = backHov ? -2.0f : 0.0f;
    DrawChevron(r, Vector2f{backRect.Center.X - 6.0f, backRect.Center.Y + backOffY}, 20.0f, false, Colors::White, 3.2f);
    DrawChevron(r, Vector2f{backRect.Center.X + 7.0f, backRect.Center.Y + backOffY}, 20.0f, false, ScaleAlpha(primary, 0.85f), 2.6f);

    const UIRect nickRect = ProfileGarageLayout::GetNicknameRect(viewSize);
    const bool nickHov = !state.IsColorModalOpen() && nickRect.Contains(mouse);
    const bool nickEdit = state.IsEditingNickname();
    const Color nickFill = nickEdit
        ? Color{22, 34, 56, 245}
        : (nickHov ? Color{24, 34, 54, 232} : Color{18, 26, 42, 224});
    const Color nickBorder = nickEdit
        ? primary
        : (nickHov ? Color{255, 236, 130, 255} : Color{255, 215, 75, 220});
    const float nickBorderThick = nickEdit ? 2.6f : (nickHov ? 2.4f : 2.0f);
    DrawReliefPanel(r, nickRect, nickFill, nickBorder, nickBorderThick, false, false, true);

    if (assets.FontId != 0)
    {
        const bool blink = (std::fmod(animTime * 2.4f, 1.0f) < 0.55f);
        const std::string displayNick = state.IsEditingNickname()
            ? (state.GetNicknameBuffer() + (blink ? "_" : " "))
            : ProfileManager::Get().GetUsername();

        r.DrawTextCentered(displayNick, assets.FontId, 42.0f, Transform2D{Vector2f{nickRect.Center.X + 2.0f, nickRect.Center.Y + 3.0f}}, Color{6, 10, 18, 220});
        r.DrawTextCentered(displayNick, assets.FontId, 42.0f, Transform2D{nickRect.Center}, Color{255, 220, 78, 255});
    }

    const UIRect creditsRect = ProfileGarageLayout::GetCreditsHudRect(viewSize);
    DrawCreditsBadge(r, creditsRect, assets.FontId, PowerUpManager::Get().GetCoins());
}

void ProfileGarageRenderer::DrawCreditsBadge(Renderer& r, const UIRect& rect, uint32_t fontId, int coins)
{
    DrawReliefPanel(r, rect, Color{24, 22, 16, 240}, Color{245, 194, 58, 235}, 2.0f, false, false, false);
    DrawCreditTokenIcon(r, Vector2f{rect.Left() + 28.0f, rect.Center.Y}, 13.0f);

    if (fontId != 0)
    {
        const std::string creditText = FormatCommas(static_cast<uint32_t>(std::max(0, coins)));
        const Vector2f sz = r.GetTextSize(creditText, fontId, 26.0f);
        r.DrawText(creditText, fontId, 26.0f, Transform2D{Vector2f{rect.Right() - 20.0f - sz.X, rect.Center.Y - 14.0f}}, Color{255, 232, 128, 255});
    }
}

void ProfileGarageRenderer::RenderSideRail(
    Renderer& r,
    const ProfileGarageState& state,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    Color primary,
    Color secondary)
{
    const Color accents[ProfileGarageState::kSideButtonCount] = {
        Color{255, 210, 80, 255},
        Color{110, 220, 255, 255},
        primary
    };

    for (int i = 0; i < ProfileGarageState::kSideButtonCount; ++i)
    {
        const UIRect rect = ProfileGarageLayout::GetSideButtonRect(i, viewSize);
        const bool hov = !state.IsColorModalOpen() && rect.Contains(mouse);
        const bool active = (i == 0 && state.IsColorModalOpen());
        const Color fill = active ? MixColor(Color{24, 34, 52, 245}, accents[i], 0.25f) : (hov ? Color{34, 46, 70, 245} : Color{20, 28, 44, 240});
        const Color border = active ? Colors::White : (hov ? Colors::White : accents[i]);

        DrawReliefPanel(r, rect, fill, border, (active || hov) ? 2.6f : 2.0f, hov, false, false);
        const Vector2f iconCenter{rect.Center.X, rect.Center.Y + (hov ? -2.0f : 0.0f)};
        DrawStaticSideIcon(r, i, iconCenter, primary, secondary);
    }
}

void ProfileGarageRenderer::RenderFocusedShowcase(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const ProfileShowcaseSnapshot& showcase,
    const Vector2f& viewSize)
{
    const UIRect bounds = ProfileGarageLayout::GetShowcaseBounds(viewSize);
    const Color pri = state.GetPreviewPrimaryColor();
    const Color sec = state.GetPreviewSecondaryColor();
    const CosmeticSlot slot = state.GetActiveSlot();

    r.DrawCircle(148.0f, Transform2D{bounds.Center}, ScaleAlpha(pri, 0.075f), BlendMode::Add);
    r.DrawCircle(88.0f, Transform2D{bounds.Center}, ScaleAlpha(sec, 0.055f), BlendMode::Add);

    if (slot == CosmeticSlot::PaddleSkin)
    {
        const float floatY = std::sin(showcase.AnimTime * 2.2f) * 5.0f;
        const float tilt = std::sin(showcase.AnimTime * 1.4f) * 3.5f;
        CosmeticRenderer::DrawPaddle(
            r,
            Vector2f{bounds.Center.X, bounds.Center.Y + floatY},
            Vector2f{310.0f, 46.0f},
            tilt,
            state.GetPreviewPaddleStyle(),
            pri,
            sec,
            showcase.AnimTime,
            1.0f,
            assets.PaddleTexId);
    }
    else if (slot == CosmeticSlot::BallSkin)
    {
        const float floatY = std::sin(showcase.AnimTime * 2.6f) * 6.0f;
        CosmeticRenderer::DrawBall(
            r,
            Vector2f{bounds.Center.X, bounds.Center.Y + floatY},
            48.0f,
            state.GetPreviewBallStyle(),
            pri,
            sec,
            showcase.AnimTime,
            1.0f,
            false,
            false,
            false,
            assets.BallTexId);
    }
    else if (slot == CosmeticSlot::BallTrail)
    {
        if (showcase.Trail && showcase.TrailScratch)
        {
            CosmeticRenderer::RenderTrail(r, *showcase.Trail, showcase.AnimTime, *showcase.TrailScratch);
        }
        CosmeticRenderer::DrawBall(
            r,
            showcase.BallPos,
            24.0f,
            state.GetPreviewBallStyle(),
            pri,
            sec,
            showcase.AnimTime,
            1.0f,
            false,
            false,
            false,
            assets.BallTexId);
    }
    else if (slot == CosmeticSlot::DeathEffect)
    {
        CosmeticRenderer::DrawBall(
            r,
            bounds.Center,
            32.0f,
            state.GetPreviewBallStyle(),
            pri,
            sec,
            showcase.AnimTime,
            1.0f,
            false,
            false,
            false,
            assets.BallTexId);
        if (showcase.Explosions)
        {
            showcase.Explosions->Render(r);
        }
    }

    const float railY = bounds.Bottom() + 4.0f;
    r.DrawLine(Vector2f{-bounds.Size.X * 0.48f, railY}, Vector2f{bounds.Size.X * 0.48f, railY}, Color{65, 86, 120, 145}, 2.0f);
    r.DrawLine(Vector2f{-bounds.Size.X * 0.22f, railY}, Vector2f{bounds.Size.X * 0.22f, railY}, ScaleAlpha(pri, 0.85f), 2.5f);
}

void ProfileGarageRenderer::RenderCategoryTabs(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    Color primary)
{
    for (int t = 0; t < ProfileGarageState::kTabCount; ++t)
    {
        const UIRect rect = ProfileGarageLayout::GetTabRect(t, viewSize);
        const bool active = (!state.IsColorModalOpen() && t == state.GetActiveTab());
        const bool hov = !state.IsColorModalOpen() && rect.Contains(mouse);

        const Color fill = active
            ? MixColor(Color{22, 34, 54, 245}, primary, 0.26f)
            : (hov ? Color{34, 46, 70, 245} : Color{19, 27, 42, 240});
        const Color border = active ? primary : (hov ? Colors::White : Color{72, 92, 126, 220});

        DrawReliefPanel(r, rect, fill, border, active ? 2.8f : 1.8f, hov && !active, false, active);

        const Vector2f iconCenter{rect.Center.X, rect.Center.Y + (hov && !active ? -2.0f : 0.0f)};
        DrawStaticTabIcon(r, t, iconCenter, assets, primary, active);

        if (active)
        {
            r.DrawRectangle(rect.Size.X - 18.0f, 3.5f, Transform2D{Vector2f{rect.Center.X, rect.Bottom() - 3.0f}}, primary);
        }
    }
}

void ProfileGarageRenderer::RenderPaginatedDrawer(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    Color pri,
    Color sec)
{
    const UIRect drawer = ProfileGarageLayout::GetDrawerRect(viewSize);
    DrawReliefPanel(r, drawer, Color{11, 16, 28, 232}, Color{58, 78, 112, 200}, 2.0f, false, false, true);

    const int pageCount = state.GetPageCount();
    const bool canPaginate = (pageCount > 1);
    for (int side = 0; side < 2; ++side)
    {
        const bool isRight = (side == 1);
        const UIRect arrowRect = ProfileGarageLayout::GetPageArrowRect(isRight, viewSize);
        const bool hov = canPaginate && !state.IsColorModalOpen() && arrowRect.Contains(mouse);
        const Color fill = !canPaginate
            ? Color{15, 20, 30, 175}
            : (hov ? Color{34, 50, 78, 245} : Color{20, 29, 46, 238});
        const Color border = !canPaginate
            ? Color{46, 56, 74, 155}
            : (hov ? Colors::White : ScaleAlpha(pri, 0.80f));

        DrawReliefPanel(r, arrowRect, fill, border, hov ? 2.5f : 1.8f, hov, false, !canPaginate);

        const float yOff = hov ? -2.0f : 0.0f;
        const Color glyphCol = canPaginate ? (hov ? Colors::White : Color{215, 232, 250, 240}) : Color{82, 94, 116, 150};
        DrawChevron(r, Vector2f{arrowRect.Center.X - 4.0f, arrowRect.Center.Y + yOff}, 20.0f, isRight, glyphCol, 3.0f);
        DrawChevron(r, Vector2f{arrowRect.Center.X + 5.0f, arrowRect.Center.Y + yOff}, 20.0f, isRight, ScaleAlpha(glyphCol, 0.55f), 2.2f);
    }

    const CosmeticSlot slot = state.GetActiveSlot();
    const std::string equippedId = ProfileManager::Get().GetEquippedCosmetic(slot);
    const auto& items = state.GetCurrentItems();
    const int slotsOnPage = state.GetPageItemCount();

    for (int s = 0; s < slotsOnPage; ++s)
    {
        const int globalIdx = state.GetGlobalIndexForSlot(s);
        if (globalIdx < 0 || globalIdx >= static_cast<int>(items.size())) continue;

        const CosmeticItem& item = items[static_cast<size_t>(globalIdx)];
        const UIRect rect = ProfileGarageLayout::GetSlotRect(s, slotsOnPage, viewSize);
        const bool isEquipped = (item.Id == equippedId);
        const bool isSelected = (globalIdx == state.GetSelectedIndex());
        const bool isHovered = !state.IsColorModalOpen() && rect.Contains(mouse);
        const bool isPressed = (s == state.GetPressedSlot());

        Color fill = item.Unlocked ? Color{24, 34, 54, 245} : Color{14, 18, 28, 242};
        if (isEquipped) fill = MixColor(fill, pri, 0.22f);
        else if (isHovered && item.Unlocked) fill = Color{34, 48, 74, 248};
        else if (isHovered) fill = Color{22, 28, 42, 245};

        Color border = item.Unlocked ? Color{82, 108, 148, 225} : Color{56, 66, 86, 200};
        float borderThick = 2.0f;
        if (isEquipped)
        {
            border = Color{88, 255, 168, 255};
            borderThick = 3.0f;
        }
        else if (isSelected)
        {
            border = item.Unlocked ? pri : Color{255, 194, 78, 245};
            borderThick = 2.6f;
        }
        else if (isHovered)
        {
            border = Colors::White;
            borderThick = 2.3f;
        }

        DrawReliefPanel(r, rect, fill, border, borderThick, isHovered, isPressed, !item.Unlocked);

        const float yOff = isPressed ? 3.0f : (isHovered ? -2.0f : 0.0f);
        const Vector2f previewCenter{rect.Center.X, rect.Center.Y + yOff};
        const float alphaScale = item.Unlocked ? 1.0f : 0.42f;

        if (isEquipped)
        {
            DrawCornerBrackets(r, previewCenter, Vector2f{rect.Size.X - 14.0f, rect.Size.Y - 14.0f}, 16.0f, 2.8f, Color{110, 255, 180, 240});
        }

        if (slot == CosmeticSlot::PaddleSkin)
        {
            CosmeticRenderer::DrawPaddle(
                r, previewCenter, Vector2f{std::min(212.0f, rect.Size.X * 0.72f), 28.0f}, 0.0f,
                ProfileManager::ResolvePaddleSkinStyle(item.Id), pri, sec, 0.0f, alphaScale, assets.PaddleTexId);
        }
        else if (slot == CosmeticSlot::BallSkin)
        {
            CosmeticRenderer::DrawBall(
                r, previewCenter, std::min(38.0f, rect.Size.Y * 0.24f),
                ProfileManager::ResolveBallSkinStyle(item.Id), pri, sec, 0.0f, alphaScale, false, false, false, assets.BallTexId);
        }
        else if (slot == CosmeticSlot::BallTrail)
        {
            CosmeticRenderer::DrawTrailPreviewIcon(
                r, previewCenter, Vector2f{rect.Size.X * 0.76f, rect.Size.Y * 0.56f},
                ProfileManager::ResolveTrailStyle(item.Id), ScaleAlpha(pri, alphaScale), ScaleAlpha(sec, alphaScale), 0.0f);
        }
        else if (slot == CosmeticSlot::DeathEffect)
        {
            CosmeticRenderer::DrawDeathEffectPreviewIcon(
                r, previewCenter, std::min(42.0f, rect.Size.Y * 0.25f),
                ProfileManager::ResolveDeathEffectStyle(item.Id), ScaleAlpha(pri, alphaScale), ScaleAlpha(sec, alphaScale), 0.42f);
        }

        if (!item.Unlocked)
        {
            r.DrawRectangle(rect.Size.X - 8.0f, rect.Size.Y - 8.0f, Transform2D{previewCenter}, Color{8, 12, 20, 95});
            DrawLockedBadge(r, Vector2f{rect.Right() - 26.0f, rect.Top() + 26.0f + yOff}, 16.0f);
        }
    }

    for (int p = 0; p < pageCount; ++p)
    {
        const UIRect pill = ProfileGarageLayout::GetPageIndicatorRect(p, pageCount, viewSize);
        const bool active = (p == state.GetCurrentPage());
        const bool hov = !state.IsColorModalOpen() && UIRect{pill.Center, Vector2f{pill.Size.X + 8.0f, pill.Size.Y + 14.0f}}.Contains(mouse);
        const Color fill = active ? pri : (hov ? Color{140, 165, 200, 230} : Color{48, 62, 86, 220});
        r.DrawRectangle(pill.Size.X, pill.Size.Y, Transform2D{pill.Center}, fill);
        r.DrawRectangleOutline(pill.Size.X, pill.Size.Y, Transform2D{pill.Center}, active ? Colors::White : Color{20, 28, 42, 230}, 1.5f);
    }
}

void ProfileGarageRenderer::RenderColorModal(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const ProfileShowcaseSnapshot& showcase,
    const Vector2f& viewSize,
    const Vector2f& mouse)
{
    const auto& pm = ProfileManager::Get();
    const Color pri = state.GetPreviewPrimaryColor();
    const Color sec = state.GetPreviewSecondaryColor();
    const UIRect modal = ProfileGarageLayout::GetColorModalRect(viewSize);

    r.DrawRectangle(viewSize.X, viewSize.Y, Transform2D{Vector2f{0.0f, 0.0f}}, Color{4, 7, 14, 200});
    DrawReliefPanel(r, modal, Color{16, 22, 34, 252}, ScaleAlpha(pri, 0.85f), 2.8f, false, false, false);

    const UIRect closeRect = ProfileGarageLayout::GetColorModalCloseRect(viewSize);
    const bool closeHov = closeRect.Contains(mouse);
    DrawReliefPanel(r, closeRect, closeHov ? Color{140, 38, 48, 245} : Color{32, 24, 34, 240}, closeHov ? Colors::White : Color{235, 85, 95, 235}, 2.0f, closeHov, false, false);
    const float xOff = closeHov ? -2.0f : 0.0f;
    r.DrawLine(Vector2f{closeRect.Center.X - 9.0f, closeRect.Center.Y - 9.0f + xOff}, Vector2f{closeRect.Center.X + 9.0f, closeRect.Center.Y + 9.0f + xOff}, Colors::White, 3.0f);
    r.DrawLine(Vector2f{closeRect.Center.X + 9.0f, closeRect.Center.Y - 9.0f + xOff}, Vector2f{closeRect.Center.X - 9.0f, closeRect.Center.Y + 9.0f + xOff}, Colors::White, 3.0f);

    const char* chanLabels[3] = {"COL 1", "COL 2", "RANDOM"};
    for (int c = 0; c < 3; ++c)
    {
        const UIRect cb = ProfileGarageLayout::GetColorChannelRect(c, viewSize);
        const bool active = (c == state.GetColorChannel());
        const bool hov = cb.Contains(mouse);
        const Color fill = active ? MixColor(Color{22, 46, 64, 248}, pri, 0.30f) : (hov ? Color{36, 48, 72, 245} : Color{22, 30, 46, 240});
        const Color border = active ? pri : (hov ? Colors::White : Color{84, 106, 142, 220});

        DrawReliefPanel(r, cb, fill, border, active ? 2.6f : 1.8f, hov && !active, false, active);
        const float cy = cb.Center.Y + (hov && !active ? -2.0f : 0.0f);
        if (c < 2)
        {
            const Color sw = (c == 0) ? pri : sec;
            const Vector2f swPos{cb.Left() + 22.0f, cy};
            r.DrawRectangle(18.0f, 18.0f, Transform2D{swPos}, sw);
            r.DrawRectangleOutline(18.0f, 18.0f, Transform2D{swPos}, Colors::White, 1.5f);
        }
        if (assets.FontId != 0)
        {
            r.DrawTextCentered(chanLabels[c], assets.FontId, 20.0f, Transform2D{Vector2f{cb.Center.X + (c < 2 ? 10.0f : 0.0f), cy}}, Colors::White);
        }
    }

    const UIRect previewArea = ProfileGarageLayout::GetColorPreviewArea(viewSize);
    r.DrawLine(Vector2f{previewArea.Left() + 32.0f, previewArea.Bottom()}, Vector2f{previewArea.Right() - 32.0f, previewArea.Bottom()}, ScaleAlpha(pri, 0.45f), 2.0f);

    CosmeticRenderer::DrawPaddle(
        r,
        Vector2f{previewArea.Center.X - 150.0f, previewArea.Center.Y},
        Vector2f{218.0f, 34.0f},
        0.0f,
        pm.GetEquippedPaddleSkinStyle(),
        pri,
        sec,
        showcase.AnimTime,
        1.0f,
        assets.PaddleTexId);

    CosmeticRenderer::DrawBall(
        r,
        Vector2f{previewArea.Center.X + 180.0f, previewArea.Center.Y},
        34.0f,
        pm.GetEquippedBallSkinStyle(),
        pri,
        sec,
        showcase.AnimTime,
        1.0f,
        false,
        false,
        false,
        assets.BallTexId);

    const CosmeticSlot slot = (state.GetColorChannel() == 0) ? CosmeticSlot::PrimaryColor : CosmeticSlot::SecondaryColor;
    const std::string equippedId = pm.GetEquippedCosmetic(slot);
    const auto& items = state.GetColorModalItems();
    const int count = static_cast<int>(items.size());

    for (int i = 0; i < count; ++i)
    {
        const CosmeticItem& item = items[static_cast<size_t>(i)];
        const UIRect rect = ProfileGarageLayout::GetColorSwatchRect(i, viewSize);
        const bool isEquipped = (item.Id == equippedId);
        const bool isSelected = (i == state.GetColorSelectedIndex());
        const bool isHovered = rect.Contains(mouse);
        const bool isPressed = (i == state.GetPressedColorSwatch());

        Color swCol = item.SwatchColor;
        if (!item.Unlocked)
        {
            swCol = MixColor(swCol, Color{14, 18, 28, 255}, 0.56f);
        }

        const Color border = isEquipped
            ? Color{90, 255, 168, 255}
            : (isSelected ? Colors::White : (isHovered ? Colors::White : Color{16, 22, 34, 240}));

        DrawReliefPanel(r, rect, swCol, border, (isEquipped || isSelected) ? 3.0f : 2.2f, isHovered, isPressed, !item.Unlocked);

        const float yOff = isPressed ? 3.0f : (isHovered ? -2.0f : 0.0f);
        const Vector2f c{rect.Center.X, rect.Center.Y + yOff};

        if (isEquipped)
        {
            DrawCornerBrackets(r, c, Vector2f{rect.Size.X - 10.0f, rect.Size.Y - 10.0f}, 14.0f, 3.0f, Colors::White);
        }
        if (!item.Unlocked)
        {
            DrawLockedBadge(r, c, 17.0f);
        }
    }
}

void ProfileGarageRenderer::RenderStatsPanels(
    Renderer& r,
    const ProfileRenderAssets& assets,
    const Vector2f& viewSize,
    Color primary)
{
    const auto& pm = ProfileManager::Get();
    const auto& am = AchievementManager::Get();
    const UIRect leftPanel = ProfileGarageLayout::GetProfileStatsLeftRect(viewSize);
    const UIRect rightPanel = ProfileGarageLayout::GetProfileStatsRightRect(viewSize);
    const Color amber{255, 196, 92, 255};

    DrawReliefPanel(r, leftPanel, Color{11, 16, 28, 224}, ScaleAlpha(primary, 0.55f), 1.8f, false, false, true);
    DrawReliefPanel(r, rightPanel, Color{11, 16, 28, 224}, ScaleAlpha(amber, 0.55f), 1.8f, false, false, true);

    if (assets.FontId == 0) return;

    const bool compact = (leftPanel.Size.X < 330.0f);
    const float headerFont = compact ? 18.0f : 20.0f;
    const float rowFont = compact ? 16.0f : 18.0f;
    const float padX = compact ? 14.0f : 18.0f;

    auto drawHeader = [&](const UIRect& panel, const char* title, Color accent)
    {
        const float xL = panel.Left() + padX;
        const float xR = panel.Right() - padX;
        const float titleY = panel.Top() + 12.0f;
        r.DrawText(title, assets.FontId, headerFont, Transform2D{Vector2f{xL, titleY}}, accent);
        const float divY = titleY + headerFont + 8.0f;
        r.DrawLine(Vector2f{xL, divY}, Vector2f{xR, divY}, ScaleAlpha(accent, 0.45f), 1.5f);
        return divY + 12.0f;
    };

    auto drawRow = [&](const UIRect& panel, float y, const char* label, const std::string& val, Color valCol)
    {
        const float xL = panel.Left() + padX;
        const float xR = panel.Right() - padX;
        r.DrawText(label, assets.FontId, rowFont, Transform2D{Vector2f{xL, y}}, Color{168, 188, 218, 235});
        const Vector2f sz = r.GetTextSize(val, assets.FontId, rowFont);
        r.DrawText(val, assets.FontId, rowFont, Transform2D{Vector2f{xR - sz.X, y}}, valCol);
    };

    const float leftStartY = drawHeader(leftPanel, "CAREER STATS", primary);
    const float rowStep = (leftPanel.Bottom() - 12.0f - leftStartY) / 5.0f;
    const int highestCleared = std::max(0, PlayerPrefs::GetInt("HighestClearedLevel", 0));
    const int achUnlocked = am.GetUnlockedCount();
    const int achTotal = am.GetTotalCount();
    const int achPct = static_cast<int>(std::round(am.GetCompletionPercentage()));

    drawRow(leftPanel, leftStartY + rowStep * 0.0f, "LIFETIME SCORE", FormatCommas(pm.GetLifetimeScore()) + " PTS", Colors::White);
    drawRow(leftPanel, leftStartY + rowStep * 1.0f, "CREDITS EARNED", FormatCommas(pm.GetLifetimeCredits()) + " CR", Color{255, 224, 95, 255});
    drawRow(leftPanel, leftStartY + rowStep * 2.0f, "HIGHEST STAGE", "STAGE " + std::to_string(highestCleared), primary);
    drawRow(leftPanel, leftStartY + rowStep * 3.0f, "ACHIEVEMENTS", std::to_string(achUnlocked) + "/" + std::to_string(achTotal) + " (" + std::to_string(achPct) + "%)", Color{255, 215, 80, 255});
    drawRow(leftPanel, leftStartY + rowStep * 4.0f, "COSMETICS", std::to_string(pm.GetTotalUnlockedCosmetics()) + " / " + std::to_string(pm.GetTotalCosmetics()), Color{110, 255, 175, 255});

    int upgradedPwrCount = 0;
    const PowerUpType allPwr[] = {
        PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::Storm,
        PowerUpType::TempoBall, PowerUpType::ExtraLife, PowerUpType::BigBall,
        PowerUpType::FireBall, PowerUpType::StickyPaddle, PowerUpType::BottomBarrier,
        PowerUpType::MagnetPaddle, PowerUpType::GhostBall, PowerUpType::ItemRegen,
        PowerUpType::ShieldBarrier
    };
    for (PowerUpType t : allPwr)
    {
        if (PowerUpManager::Get().GetPersistedLevel(t) > 1) ++upgradedPwrCount;
    }

    const float rightStartY = drawHeader(rightPanel, "ENDLESS & GEAR", amber);
    drawRow(rightPanel, rightStartY + rowStep * 0.0f, "BEST TIME", pm.GetEndlessBestTimeFormatted(), Color{255, 215, 120, 255});
    drawRow(rightPanel, rightStartY + rowStep * 1.0f, "BEST WAVE", "WAVE " + std::to_string(pm.GetEndlessBestWaves()), Color{255, 215, 120, 255});
    drawRow(rightPanel, rightStartY + rowStep * 2.0f, "ENDLESS SCORE", FormatCommas(pm.GetEndlessHighScore()) + " PTS", Colors::White);
    drawRow(rightPanel, rightStartY + rowStep * 3.0f, "ENDLESS RUNS", std::to_string(pm.GetEndlessTotalRuns()), Color{195, 210, 235, 255});
    drawRow(rightPanel, rightStartY + rowStep * 4.0f, "UPGRADES", std::to_string(upgradedPwrCount) + " / 13", Color{125, 235, 255, 255});
}

void ProfileGarageRenderer::RenderToast(
    Renderer& r,
    const ProfileGarageState& state,
    const ProfileRenderAssets& assets,
    const Vector2f& viewSize)
{
    if (state.GetToastTimer() <= 0.0f || state.GetToastText().empty() || assets.FontId == 0) return;

    const float alpha = std::clamp(state.GetToastTimer() / 0.45f, 0.0f, 1.0f);
    const float halfH = viewSize.Y * 0.5f;
    const Vector2f sz = r.GetTextSize(state.GetToastText(), assets.FontId, 21.0f);
    const float boxW = std::max(260.0f, sz.X + 46.0f);
    const Vector2f pos{0.0f, -halfH + 34.0f};

    r.DrawRectangle(boxW, 38.0f, Transform2D{pos}, ScaleAlpha(Color{10, 16, 28, 245}, alpha));
    r.DrawRectangleOutline(boxW, 38.0f, Transform2D{pos}, ScaleAlpha(state.GetToastColor(), alpha), 2.0f);
    r.DrawTextCentered(state.GetToastText(), assets.FontId, 21.0f, Transform2D{pos}, ScaleAlpha(state.GetToastColor(), alpha));
}
