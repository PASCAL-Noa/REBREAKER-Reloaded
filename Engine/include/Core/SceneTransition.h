#pragma once
#include "Data/SceneTransitionType.h"
#include "Data/Color.h"
#include "Math/EasingFunctions.h"

enum class SceneTransitionPhase
{
    Idle,
    Covering,   // Outro: screen being masked (progress 0 -> 1)
    Uncovering  // Intro: screen being revealed (progress 1 -> 0)
};

struct SceneTransitionConfig
{
    SceneTransitionType Type = SceneTransitionType::FadeBlack;
    float Duration = 0.45f; // Total transition duration in seconds
    Color FadeColor = Colors::Black;
    EasingFunctions::EasingType Ease = EasingFunctions::EasingType::EaseInOutQuad;
};
