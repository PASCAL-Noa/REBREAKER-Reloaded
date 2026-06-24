#pragma once
#include "Data/Color.h"
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
    void    CameraBreathing(TweenComponent& tweenComp, Camera2D& camera, float minZoom = 0.74f, float maxZoom = 0.76f, float duration = 2.0f);
    void    BackgroundColorShift(TweenComponent& tweenComp, Color color1, Color color2, std::function<void(Color)> colorSetter, float duration = 3.0f);
}
