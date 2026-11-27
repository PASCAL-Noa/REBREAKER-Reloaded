#pragma once

#include "Math/Vector2.h"

struct UIRect
{
    Vector2f Center{0.0f, 0.0f};
    Vector2f Size{0.0f, 0.0f};

    [[nodiscard]] bool Contains(const Vector2f& p) const noexcept
    {
        return (p.X >= Center.X - Size.X * 0.5f && p.X <= Center.X + Size.X * 0.5f &&
                p.Y >= Center.Y - Size.Y * 0.5f && p.Y <= Center.Y + Size.Y * 0.5f);
    }

    [[nodiscard]] float Left() const noexcept { return Center.X - Size.X * 0.5f; }
    [[nodiscard]] float Right() const noexcept { return Center.X + Size.X * 0.5f; }
    [[nodiscard]] float Top() const noexcept { return Center.Y - Size.Y * 0.5f; }
    [[nodiscard]] float Bottom() const noexcept { return Center.Y + Size.Y * 0.5f; }
};

class ProfileGarageLayout
{
public:
    [[nodiscard]] static UIRect GetBackButtonRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetNicknameRect(const Vector2f& viewSize, bool inCareerMode = false);
    [[nodiscard]] static UIRect GetCreditsHudRect(const Vector2f& viewSize);

    [[nodiscard]] static UIRect GetSideButtonRect(int idx, const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetShowcaseBounds(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetTabRect(int tabIdx, const Vector2f& viewSize);

    [[nodiscard]] static UIRect GetDrawerRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetPageArrowRect(bool isRight, const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetSlotRect(int slotIdx, int slotsOnPage, const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetPageIndicatorRect(int pageIdx, int pageCount, const Vector2f& viewSize);

    [[nodiscard]] static UIRect GetColorModalRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetColorModalCloseRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetColorChannelRect(int idx, const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetColorPreviewArea(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetColorSwatchRect(int itemIdx, const Vector2f& viewSize);

    [[nodiscard]] static UIRect GetCareerLeftPanel(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetCareerRightPanel(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetCareerEditNameRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetCareerResetRect(const Vector2f& viewSize);
    [[nodiscard]] static UIRect GetCareerMilestoneRect(int index, float scrollY, const Vector2f& viewSize);
    [[nodiscard]] static float  GetCareerListTop(const Vector2f& viewSize);
    [[nodiscard]] static float  GetCareerListBottom(const Vector2f& viewSize);
};
