#pragma once

#include "UI/ProfileGarageState.h"
#include "Math/Vector2.h"

class InputManager;

struct ProfileInputResult
{
    bool ReturnToMenu = false;
    bool PlayClickSfx = false;
    bool PlaySwitchSfx = false;
    bool PlayUnlockSfx = false;
    bool PlayErrorSfx = false;
    bool TriggerDeathBurst = false;
    bool FullResetConfirmed = false;
    Vector2f BurstPos{0.0f, 0.0f};
};

class ProfileGarageInput
{
public:
    static ProfileInputResult Process(
        ProfileGarageState& state,
        const InputManager& input,
        const Vector2f& viewSize,
        const Vector2f& mouse);

private:
    static void HandleNickname(
        ProfileGarageState& state,
        const InputManager& input,
        const Vector2f& viewSize,
        const Vector2f& mouse,
        ProfileInputResult& out);

    static void HandleColorModal(
        ProfileGarageState& state,
        const InputManager& input,
        const Vector2f& viewSize,
        const Vector2f& mouse,
        ProfileInputResult& out);

    static void HandleGarage(
        ProfileGarageState& state,
        const InputManager& input,
        const Vector2f& viewSize,
        const Vector2f& mouse,
        ProfileInputResult& out);

    static void HandleCareer(
        ProfileGarageState& state,
        const InputManager& input,
        const Vector2f& viewSize,
        const Vector2f& mouse,
        ProfileInputResult& out);

    static void ApplyActionFeedback(
        ProfileActionResult action,
        bool isDeathSlot,
        const Vector2f& burstPos,
        ProfileInputResult& out);
};
