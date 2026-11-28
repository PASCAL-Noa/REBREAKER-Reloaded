#include "UI/ProfileGarageLayout.h"
#include "UI/ProfileGarageState.h"
#include <algorithm>

namespace
{
    struct GarageStackMetrics
    {
        float NickCenterY = 0.0f;
        float NickH = 56.0f;
        float ShowcaseCenterY = 0.0f;
        float ShowcaseH = 220.0f;
        float ActionCenterY = 0.0f;
        float ActionH = 54.0f;
        float TabCenterY = 0.0f;
        float TabH = 60.0f;
        float DrawerCenterY = 0.0f;
        float DrawerH = 412.0f;
        float PillsCenterY = 0.0f;
    };

    GarageStackMetrics ComputeGarageStack(const Vector2f& viewSize)
    {
        GarageStackMetrics m{};
        m.NickH = 56.0f;
        m.ShowcaseH = std::clamp(viewSize.Y * 0.204f, 130.0f, 220.0f);
        m.ActionH = 54.0f;
        m.TabH = 60.0f;
        m.DrawerH = std::clamp(viewSize.Y * 0.382f, 250.0f, 412.0f);

        const float gapNickToShow = 14.0f;
        const float gapShowToAction = 20.0f;
        const float gapActionToTab = 14.0f;
        const float gapTabToDrawer = 16.0f;
        const float gapDrawerToPills = 22.0f;
        const float pillHalfH = 5.0f;

        const float totalH = m.NickH + gapNickToShow + m.ShowcaseH + gapShowToAction +
                             m.ActionH + gapActionToTab + m.TabH + gapTabToDrawer +
                             m.DrawerH + gapDrawerToPills + pillHalfH;

        float cursor = -totalH * 0.5f;
        m.NickCenterY = cursor + m.NickH * 0.5f - 22.0f;
        cursor += m.NickH + gapNickToShow;
        m.ShowcaseCenterY = cursor + m.ShowcaseH * 0.5f;
        cursor += m.ShowcaseH + gapShowToAction;
        m.ActionCenterY = cursor + m.ActionH * 0.5f;
        cursor += m.ActionH + gapActionToTab;
        m.TabCenterY = cursor + m.TabH * 0.5f;
        cursor += m.TabH + gapTabToDrawer;
        m.DrawerCenterY = cursor + m.DrawerH * 0.5f;
        cursor += m.DrawerH + gapDrawerToPills;
        m.PillsCenterY = cursor;
        return m;
    }
}

UIRect ProfileGarageLayout::GetBackButtonRect(const Vector2f& viewSize)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    return UIRect{Vector2f{-halfW + 84.0f, -halfH + 50.0f}, Vector2f{92.0f, 52.0f}};
}

UIRect ProfileGarageLayout::GetNicknameRect(const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    return UIRect{Vector2f{0.0f, m.NickCenterY}, Vector2f{440.0f, m.NickH}};
}

UIRect ProfileGarageLayout::GetCreditsHudRect(const Vector2f& viewSize)
{
    const float halfW = viewSize.X * 0.5f;
    const float halfH = viewSize.Y * 0.5f;
    return UIRect{Vector2f{halfW - 144.0f, -halfH + 50.0f}, Vector2f{216.0f, 50.0f}};
}

UIRect ProfileGarageLayout::GetSideButtonRect(int idx, const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    const float btnW = 84.0f;
    const float gap = 20.0f;
    const float totalW = btnW * ProfileGarageState::kSideButtonCount + gap * (ProfileGarageState::kSideButtonCount - 1);
    const float startX = -totalW * 0.5f + btnW * 0.5f;
    return UIRect{Vector2f{startX + static_cast<float>(idx) * (btnW + gap), m.ActionCenterY}, Vector2f{btnW, m.ActionH}};
}

UIRect ProfileGarageLayout::GetShowcaseBounds(const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    const float w = std::min(484.0f, viewSize.X - 360.0f);
    return UIRect{Vector2f{0.0f, m.ShowcaseCenterY}, Vector2f{w, m.ShowcaseH}};
}

UIRect ProfileGarageLayout::GetTabRect(int tabIdx, const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    const float btnW = 106.0f;
    const float gap = 20.0f;
    const float totalW = btnW * ProfileGarageState::kTabCount + gap * (ProfileGarageState::kTabCount - 1);
    const float startX = -totalW * 0.5f + btnW * 0.5f;
    return UIRect{Vector2f{startX + static_cast<float>(tabIdx) * (btnW + gap), m.TabCenterY}, Vector2f{btnW, m.TabH}};
}

UIRect ProfileGarageLayout::GetDrawerRect(const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    const float w = std::min(1380.0f, viewSize.X - 300.0f);
    return UIRect{Vector2f{0.0f, m.DrawerCenterY}, Vector2f{w, m.DrawerH}};
}

UIRect ProfileGarageLayout::GetPageArrowRect(bool isRight, const Vector2f& viewSize)
{
    const UIRect drawer = GetDrawerRect(viewSize);
    const float offset = drawer.Size.X * 0.5f + 56.0f;
    return UIRect{Vector2f{isRight ? offset : -offset, drawer.Center.Y}, Vector2f{62.0f, 62.0f}};
}

UIRect ProfileGarageLayout::GetSlotRect(int slotIdx, int, const Vector2f& viewSize)
{
    const UIRect drawer = GetDrawerRect(viewSize);
    const float padX = 24.0f;
    const float padY = 20.0f;
    const float innerW = drawer.Size.X - padX * 2.0f;
    const float innerH = drawer.Size.Y - padY * 2.0f;
    const int cols = ProfileGarageState::kGridCols;
    const int rows = ProfileGarageState::kGridRows;
    const float gapX = 20.0f;
    const float gapY = 18.0f;
    const float cellW = (innerW - gapX * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float cellH = (innerH - gapY * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    const int col = slotIdx % cols;
    const int row = slotIdx / cols;
    const float startX = drawer.Left() + padX + cellW * 0.5f;
    const float startY = drawer.Top() + padY + cellH * 0.5f;
    return UIRect{
        Vector2f{
            startX + static_cast<float>(col) * (cellW + gapX),
            startY + static_cast<float>(row) * (cellH + gapY)
        },
        Vector2f{cellW, cellH}
    };
}

UIRect ProfileGarageLayout::GetPageIndicatorRect(int pageIdx, int pageCount, const Vector2f& viewSize)
{
    const GarageStackMetrics m = ComputeGarageStack(viewSize);
    const float pillW = 38.0f;
    const float pillH = 10.0f;
    const float gap = 14.0f;
    const int count = std::max(1, pageCount);
    const float totalW = pillW * static_cast<float>(count) + gap * static_cast<float>(count - 1);
    const float startX = -totalW * 0.5f + pillW * 0.5f;
    return UIRect{Vector2f{startX + static_cast<float>(pageIdx) * (pillW + gap), m.PillsCenterY}, Vector2f{pillW, pillH}};
}

UIRect ProfileGarageLayout::GetColorModalRect(const Vector2f& viewSize)
{
    const float w = std::min(1120.0f, viewSize.X - 220.0f);
    const float h = std::min(580.0f, viewSize.Y - 180.0f);
    return UIRect{Vector2f{0.0f, 0.0f}, Vector2f{w, h}};
}

UIRect ProfileGarageLayout::GetColorModalCloseRect(const Vector2f& viewSize)
{
    const UIRect modal = GetColorModalRect(viewSize);
    return UIRect{Vector2f{modal.Left() + 42.0f, modal.Top() + 40.0f}, Vector2f{46.0f, 46.0f}};
}

UIRect ProfileGarageLayout::GetColorChannelRect(int idx, const Vector2f& viewSize)
{
    const UIRect modal = GetColorModalRect(viewSize);
    const float topY = modal.Top() + 40.0f;
    const float rightX = modal.Right() - 28.0f;
    const float btnW = 138.0f;
    const float btnH = 42.0f;
    const float gap = 12.0f;
    const float cx = rightX - btnW * 0.5f - static_cast<float>(2 - idx) * (btnW + gap);
    return UIRect{Vector2f{cx, topY}, Vector2f{btnW, btnH}};
}

UIRect ProfileGarageLayout::GetColorPreviewArea(const Vector2f& viewSize)
{
    const UIRect modal = GetColorModalRect(viewSize);
    return UIRect{Vector2f{modal.Center.X, modal.Top() + 138.0f}, Vector2f{modal.Size.X - 72.0f, 118.0f}};
}

UIRect ProfileGarageLayout::GetColorSwatchRect(int itemIdx, const Vector2f& viewSize)
{
    const UIRect modal = GetColorModalRect(viewSize);
    const float gridTop = modal.Top() + 224.0f;
    const float gridBot = modal.Bottom() - 34.0f;
    const float availH = std::max(180.0f, gridBot - gridTop);
    const float innerW = modal.Size.X - 80.0f;

    const int col = itemIdx % ProfileGarageState::kColorSwatchesPerRow;
    const int row = itemIdx / ProfileGarageState::kColorSwatchesPerRow;
    const float blockGap = 28.0f;
    const float gapX = 14.0f;
    const float gapY = 18.0f;

    const float cellW = (innerW - gapX * 6.0f - blockGap) / 8.0f;
    const float cellH = std::min(144.0f, (availH - gapY) * 0.5f);
    const float totalGridH = cellH * 2.0f + gapY;
    const float startY = gridTop + (availH - totalGridH) * 0.5f;
    const float extraX = (col >= 4) ? (blockGap - gapX) : 0.0f;

    return UIRect{
        Vector2f{
            modal.Center.X - innerW * 0.5f + cellW * 0.5f + static_cast<float>(col) * (cellW + gapX) + extraX,
            startY + cellH * 0.5f + static_cast<float>(row) * (cellH + gapY)
        },
        Vector2f{cellW, cellH}
    };
}

UIRect ProfileGarageLayout::GetProfileStatsLeftRect(const Vector2f& viewSize)
{
    const UIRect back = GetBackButtonRect(viewSize);
    const UIRect drawer = GetDrawerRect(viewSize);
    const UIRect tab0 = GetTabRect(0, viewSize);
    const float innerEdge = std::abs(tab0.Left()) + 22.0f;
    const float outerEdge = std::min(viewSize.X * 0.5f - 26.0f, std::max(drawer.Right(), innerEdge + 310.0f));
    const float panelW = std::clamp(outerEdge - innerEdge, 220.0f, 370.0f);
    const float topBound = back.Bottom() + 18.0f;
    const float botBound = drawer.Top() - 18.0f;
    const float panelH = std::min(242.0f, std::max(160.0f, botBound - topBound));
    const float centerX = -(outerEdge - panelW * 0.5f);
    const float centerY = (topBound + botBound) * 0.5f;
    return UIRect{Vector2f{centerX, centerY}, Vector2f{panelW, panelH}};
}

UIRect ProfileGarageLayout::GetProfileStatsRightRect(const Vector2f& viewSize)
{
    UIRect rect = GetProfileStatsLeftRect(viewSize);
    rect.Center.X = -rect.Center.X;
    return rect;
}
