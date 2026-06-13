#pragma once
#include "ECS/Components/TweenComponent.h"
#include "ECS/Components/Transform2D.h"

struct Camera2D;

namespace TweenEffects
{
    void    Shake(TweenComponent& tweenComp, Transform2D& transform, float duration = 0.35f, float intensity = 15.0f);
    void    Shake(TweenComponent& tweenComp, Camera2D& camera, float duration, float intensity = 15.0f);
    void    Spin(TweenComponent& tweenComp, Transform2D& transform, float duration = 1.0f);
    void    BallIn(TweenComponent& tweenComp, Transform2D& transform, const std::function<void()>& onComplete = nullptr, float duration = 0.5f);
    void    BallOut(TweenComponent& tweenComp, Transform2D& transform, std::function<void()> onComplete = nullptr, float duration = 0.5f);
    void    ComboFlameScale(TweenComponent& tweenComp, Transform2D& transform, float targetScale);
}
