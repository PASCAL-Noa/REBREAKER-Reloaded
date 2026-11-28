#include "UI/ProfileGarageInput.h"
#include "UI/ProfileGarageLayout.h"
#include "Core/InputManager.h"
#include <cmath>

ProfileInputResult ProfileGarageInput::Process(
    ProfileGarageState& state,
    const InputManager& input,
    const Vector2f& viewSize,
    const Vector2f& mouse)
{
    ProfileInputResult out{};
    if (state.IsEditingNickname())
    {
        HandleNickname(state, input, viewSize, mouse, out);
        return out;
    }
    if (state.IsColorModalOpen())
    {
        HandleColorModal(state, input, viewSize, mouse, out);
        return out;
    }
    HandleGarage(state, input, viewSize, mouse, out);
    return out;
}

void ProfileGarageInput::ApplyActionFeedback(
    ProfileActionResult action,
    bool isDeathSlot,
    const Vector2f& burstPos,
    ProfileInputResult& out)
{
    switch (action)
    {
        case ProfileActionResult::Equipped:
            out.PlayClickSfx = true;
            if (isDeathSlot)
            {
                out.TriggerDeathBurst = true;
                out.BurstPos = burstPos;
            }
            break;
        case ProfileActionResult::AlreadyEquipped:
            if (isDeathSlot)
            {
                out.TriggerDeathBurst = true;
                out.BurstPos = burstPos;
                out.PlayUnlockSfx = true;
            }
            break;
        case ProfileActionResult::UnlockedAndEquipped:
            out.PlayUnlockSfx = true;
            out.TriggerDeathBurst = true;
            out.BurstPos = burstPos;
            break;
        case ProfileActionResult::InsufficientFunds:
        case ProfileActionResult::MilestoneLocked:
            out.PlayErrorSfx = true;
            break;
        default:
            break;
    }
}

void ProfileGarageInput::HandleNickname(
    ProfileGarageState& state,
    const InputManager& input,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    ProfileInputResult& out)
{
    state.AppendNicknameChars(input.GetEnteredText());
    if (input.IsKeyPress(KeyCode::Backspace))
    {
        state.BackspaceNickname();
    }
    if (input.IsKeyPress(KeyCode::Enter))
    {
        state.CommitNicknameEdit();
        out.PlayClickSfx = true;
        return;
    }
    if (input.IsKeyPress(KeyCode::Escape))
    {
        state.CancelNicknameEdit();
        out.PlaySwitchSfx = true;
        return;
    }
    if (input.IsMouseButtonPress(MouseButton::Left) &&
        !ProfileGarageLayout::GetNicknameRect(viewSize).Contains(mouse))
    {
        state.CommitNicknameEdit();
        out.PlayClickSfx = true;
    }
}

void ProfileGarageInput::HandleColorModal(
    ProfileGarageState& state,
    const InputManager& input,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    ProfileInputResult& out)
{
    if (input.IsKeyPress(KeyCode::Escape) || input.IsKeyPress(KeyCode::C))
    {
        state.CloseColorModal();
        out.PlaySwitchSfx = true;
        return;
    }
    if (input.IsKeyPress(KeyCode::Tab) || input.IsKeyPress(KeyCode::Left) || input.IsKeyPress(KeyCode::Right))
    {
        if (state.SwitchColorChannel(1 - state.GetColorChannel()))
        {
            out.PlayClickSfx = true;
        }
    }
    if (input.IsKeyPress(KeyCode::Num1) && state.SwitchColorChannel(0)) out.PlayClickSfx = true;
    if (input.IsKeyPress(KeyCode::Num2) && state.SwitchColorChannel(1)) out.PlayClickSfx = true;

    int hovered = -1;
    const int count = static_cast<int>(state.GetColorModalItems().size());
    for (int i = 0; i < count; ++i)
    {
        if (ProfileGarageLayout::GetColorSwatchRect(i, viewSize).Contains(mouse))
        {
            hovered = i;
            break;
        }
    }
    state.SetColorHoveredIndex(hovered);
    state.SetPressedColorSwatch(input.IsMouseButtonDown(MouseButton::Left) ? hovered : -1);

    if (!input.IsMouseButtonPress(MouseButton::Left)) return;

    if (ProfileGarageLayout::GetColorModalCloseRect(viewSize).Contains(mouse) ||
        !ProfileGarageLayout::GetColorModalRect(viewSize).Contains(mouse))
    {
        state.CloseColorModal();
        out.PlaySwitchSfx = true;
        return;
    }

    if (ProfileGarageLayout::GetColorChannelRect(0, viewSize).Contains(mouse))
    {
        if (state.SwitchColorChannel(0)) out.PlayClickSfx = true;
        return;
    }
    if (ProfileGarageLayout::GetColorChannelRect(1, viewSize).Contains(mouse))
    {
        if (state.SwitchColorChannel(1)) out.PlayClickSfx = true;
        return;
    }
    if (ProfileGarageLayout::GetColorChannelRect(2, viewSize).Contains(mouse))
    {
        state.RandomizeEquippedColors();
        out.PlayClickSfx = true;
        return;
    }

    if (hovered >= 0 && hovered < count)
    {
        const bool alreadySelected = (state.GetColorSelectedIndex() == hovered);
        state.SetColorSelectedIndex(hovered);
        const CosmeticItem& item = state.GetColorModalItems()[static_cast<size_t>(hovered)];
        if (item.Unlocked || (alreadySelected && item.UnlockType == CosmeticUnlockType::Shop))
        {
            const Vector2f showcaseCenter = ProfileGarageLayout::GetShowcaseBounds(viewSize).Center;
            ApplyActionFeedback(state.ActivateSelectedColor(), false, showcaseCenter, out);
        }
        else
        {
            state.NotifySelectedLockedColor();
            out.PlayClickSfx = true;
        }
        return;
    }
}

void ProfileGarageInput::HandleGarage(
    ProfileGarageState& state,
    const InputManager& input,
    const Vector2f& viewSize,
    const Vector2f& mouse,
    ProfileInputResult& out)
{
    const Vector2f showcaseCenter = ProfileGarageLayout::GetShowcaseBounds(viewSize).Center;

    if (input.IsKeyPress(KeyCode::Escape))
    {
        out.ReturnToMenu = true;
        return;
    }

    auto trySwitchTab = [&](int tabIdx)
    {
        if (state.SwitchTab(tabIdx))
        {
            out.PlaySwitchSfx = true;
            if (state.GetActiveSlot() == CosmeticSlot::DeathEffect)
            {
                out.TriggerDeathBurst = true;
                out.BurstPos = showcaseCenter;
            }
        }
    };

    if (input.IsKeyPress(KeyCode::Num1)) trySwitchTab(0);
    if (input.IsKeyPress(KeyCode::Num2)) trySwitchTab(1);
    if (input.IsKeyPress(KeyCode::Num3)) trySwitchTab(2);
    if (input.IsKeyPress(KeyCode::Num4)) trySwitchTab(3);
    if (input.IsKeyPress(KeyCode::Q)) trySwitchTab((state.GetActiveTab() + ProfileGarageState::kTabCount - 1) % ProfileGarageState::kTabCount);
    if (input.IsKeyPress(KeyCode::E)) trySwitchTab((state.GetActiveTab() + 1) % ProfileGarageState::kTabCount);
    if (input.IsKeyPress(KeyCode::C))
    {
        state.OpenColorModal(state.GetColorChannel());
        out.PlayClickSfx = true;
        return;
    }
    if (input.IsKeyPress(KeyCode::Left) && state.PrevPage()) out.PlaySwitchSfx = true;
    if (input.IsKeyPress(KeyCode::Right) && state.NextPage()) out.PlaySwitchSfx = true;

    const int prevHovered = state.GetHoveredIndex();
    const int slotsOnPage = state.GetPageItemCount();
    int hoveredSlot = -1;
    int hoveredGlobal = -1;
    for (int s = 0; s < slotsOnPage; ++s)
    {
        if (ProfileGarageLayout::GetSlotRect(s, slotsOnPage, viewSize).Contains(mouse))
        {
            hoveredSlot = s;
            hoveredGlobal = state.GetGlobalIndexForSlot(s);
            break;
        }
    }
    state.SetHoveredIndex(hoveredGlobal);
    state.SetPressedSlot(input.IsMouseButtonDown(MouseButton::Left) ? hoveredSlot : -1);

    if (hoveredGlobal != prevHovered && hoveredGlobal >= 0 && state.GetActiveSlot() == CosmeticSlot::DeathEffect)
    {
        out.TriggerDeathBurst = true;
        out.BurstPos = showcaseCenter;
    }

    if (!input.IsMouseButtonPress(MouseButton::Left)) return;

    if (ProfileGarageLayout::GetBackButtonRect(viewSize).Contains(mouse))
    {
        out.ReturnToMenu = true;
        return;
    }
    if (ProfileGarageLayout::GetNicknameRect(viewSize).Contains(mouse))
    {
        state.BeginNicknameEdit();
        out.PlayClickSfx = true;
        return;
    }

    if (ProfileGarageLayout::GetSideButtonRect(0, viewSize).Contains(mouse))
    {
        state.OpenColorModal(state.GetColorChannel());
        out.PlayClickSfx = true;
        return;
    }
    if (ProfileGarageLayout::GetSideButtonRect(1, viewSize).Contains(mouse))
    {
        out.OpenAchievementShowcase = true;
        out.PlayClickSfx = true;
        return;
    }
    if (ProfileGarageLayout::GetSideButtonRect(2, viewSize).Contains(mouse))
    {
        state.RandomizeEquippedLoadout();
        out.PlayClickSfx = true;
        if (state.GetActiveSlot() == CosmeticSlot::DeathEffect)
        {
            out.TriggerDeathBurst = true;
            out.BurstPos = showcaseCenter;
        }
        return;
    }

    for (int t = 0; t < ProfileGarageState::kTabCount; ++t)
    {
        if (ProfileGarageLayout::GetTabRect(t, viewSize).Contains(mouse))
        {
            trySwitchTab(t);
            return;
        }
    }

    if (ProfileGarageLayout::GetPageArrowRect(false, viewSize).Contains(mouse))
    {
        if (state.PrevPage()) out.PlaySwitchSfx = true;
        return;
    }
    if (ProfileGarageLayout::GetPageArrowRect(true, viewSize).Contains(mouse))
    {
        if (state.NextPage()) out.PlaySwitchSfx = true;
        return;
    }

    const int pageCount = state.GetPageCount();
    for (int p = 0; p < pageCount; ++p)
    {
        UIRect hit = ProfileGarageLayout::GetPageIndicatorRect(p, pageCount, viewSize);
        hit.Size = Vector2f{hit.Size.X + 8.0f, hit.Size.Y + 14.0f};
        if (hit.Contains(mouse))
        {
            if (state.SetPage(p)) out.PlaySwitchSfx = true;
            return;
        }
    }

    if (hoveredGlobal >= 0)
    {
        const bool alreadySelected = (state.GetSelectedIndex() == hoveredGlobal);
        state.SetSelectedIndex(hoveredGlobal);
        const CosmeticItem& item = state.GetCurrentItems()[static_cast<size_t>(hoveredGlobal)];
        const bool isDeath = (state.GetActiveSlot() == CosmeticSlot::DeathEffect);
        if (item.Unlocked || (alreadySelected && item.UnlockType == CosmeticUnlockType::Shop))
        {
            ApplyActionFeedback(state.ActivateSelectedItem(), isDeath, showcaseCenter, out);
        }
        else
        {
            state.NotifySelectedLockedItem();
            out.PlayClickSfx = true;
            if (isDeath)
            {
                out.TriggerDeathBurst = true;
                out.BurstPos = showcaseCenter;
            }
        }
        return;
    }
}
