#pragma once
#include "Data/Color.h"
#include "ECS/Components/TweenComponent.h"
#include "ECS/Entity.h"
#include <functional>

class Registry;

namespace TweenEffects
{
    void    Shake(TweenComponent& tweenComp, Registry& registry, Entity entity, float duration = 0.35f, float intensity = 15.0f);
    void    Shake(Registry& registry, Entity entity, float duration = 0.35f, float intensity = 15.0f);

    void    Spin(TweenComponent& tweenComp, Registry& registry, Entity entity, float duration = 1.0f);
    void    Spin(Registry& registry, Entity entity, float duration = 1.0f);

    void    BallIn(TweenComponent& tweenComp, Registry& registry, Entity entity, const std::function<void()>& onComplete = nullptr, float duration = 0.5f);
    void    BallIn(Registry& registry, Entity entity, const std::function<void()>& onComplete = nullptr, float duration = 0.5f);

    void    BallOut(TweenComponent& tweenComp, Registry& registry, Entity entity, std::function<void()> onComplete = nullptr, float duration = 0.5f);
    void    BallOut(Registry& registry, Entity entity, std::function<void()> onComplete = nullptr, float duration = 0.5f);

    void    ComboFlameScale(TweenComponent& tweenComp, Registry& registry, Entity entity, float targetScale);
    void    ComboFlameScale(Registry& registry, Entity entity, float targetScale);

    void    CameraBreathing(TweenComponent& tweenComp, Registry& registry, Entity entity, float minZoom = 0.74f, float maxZoom = 0.76f, float duration = 2.0f);
    void    CameraBreathing(Registry& registry, Entity entity, float minZoom = 0.74f, float maxZoom = 0.76f, float duration = 2.0f);

    void    BackgroundColorShift(TweenComponent& tweenComp, Color color1, Color color2, std::function<void(Color)> colorSetter, float duration = 3.0f);
    void    BackgroundColorShift(Registry& registry, Entity entity, Color color1, Color color2, std::function<void(Color)> colorSetter, float duration = 3.0f);
}
