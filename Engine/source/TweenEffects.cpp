#include "TweenEffects/TweenEffects.h"
#include <cstdlib>

#include "ECS/Components/Camera2D.h"

namespace TweenEffects
{
    void Shake(TweenComponent& tweenComp, Transform2D& transform, const float duration, const float intensity)
    {
        TweenConfig<float> config;
        config.Start = intensity;
        config.End = 0.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseOutQuad;

        config.Setter = [&transform, currentOffset = Vector2f{0.0f, 0.0f}](const float val) mutable
        {
            transform.Position.X -= currentOffset.X;
            transform.Position.Y -= currentOffset.Y;

            if (val <= 0.0f)
            {
                currentOffset = Vector2f{0.0f, 0.0f};
                return;
            }

            const float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            const float offsetY = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            currentOffset = Vector2f{offsetX, offsetY};

            transform.Position.X += currentOffset.X;
            transform.Position.Y += currentOffset.Y;
        };

        tweenComp.AddTween(config);
    }

    void Shake(TweenComponent& tweenComp, Camera2D& camera, const float duration, const float intensity)
    {
        TweenConfig<float> config;
        config.Start = intensity;
        config.End = 0.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseOutQuad;

        config.Setter = [&camera, currentOffset = Vector2f{0.0f, 0.0f}](const float val) mutable
        {
            camera.Position.X -= currentOffset.X;
            camera.Position.Y -= currentOffset.Y;

            if (val <= 0.0f)
            {
                currentOffset = Vector2f{0.0f, 0.0f};
                return;
            }

            const float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            const float offsetY = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            currentOffset = Vector2f{offsetX, offsetY};

            camera.Position.X += currentOffset.X;
            camera.Position.Y += currentOffset.Y;
        };

        tweenComp.AddTween(config);
    }

    void Spin(TweenComponent& tweenComp, Transform2D& transform, const float duration)
    {
        TweenConfig<float> config;
        config.Start = 0.0f;
        config.End = 360.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseInOutSine;

        config.Setter = [&transform](float angle)
        {
            transform.Rotation = angle;
        };

        tweenComp.AddTween(config);
    }

    void ComboFlameScale(TweenComponent& tweenComp, Transform2D& transform, const float targetScale)
    {
        TweenConfig<float> config;
        config.Start = transform.Scale.X;
        config.End = targetScale;
        config.Duration = 0.4f;
        
        if (targetScale <= 0.0f)
            config.Ease = EasingFunctions::EasingType::EaseOutQuad;
        else
            config.Ease = EasingFunctions::EasingType::EaseOutBack;
        
        config.Setter = [&transform](float val)
        {
            if (val < 0.0f) val = 0.0f;
            transform.Scale = {val, val};
        };

        tweenComp.AddTween(config);
    }

    void BallIn(TweenComponent& tweenComp, Transform2D& transform, const std::function<void()>& onComplete, const float duration)
    {
        TweenConfig<Vector2f> scaleTween;
        scaleTween.Start = Vector2f{0.0f, 0.0f};
        scaleTween.End = Vector2f{1.0f, 1.0f};
        scaleTween.Duration = duration;
        scaleTween.Ease = EasingFunctions::EasingType::EaseOutBack;
        scaleTween.Setter = [&transform](const Vector2f v) {
            transform.Scale = v;
        };
        scaleTween.OnComplete = onComplete;
        
        tweenComp.AddTween(scaleTween);
    }

    void BallOut(TweenComponent& tweenComp, Transform2D& transform, std::function<void()> onComplete, float duration)
    {
        TweenConfig<Vector2f> scaleTween;
        scaleTween.Start = Vector2f{1.0f, 1.0f};
        scaleTween.End = Vector2f{0.0f, 0.0f};
        scaleTween.Duration = duration;
        scaleTween.Ease = EasingFunctions::EasingType::EaseInBack;
        scaleTween.Setter = [&transform](Vector2f v) {
            transform.Scale = v;
        };
        scaleTween.OnComplete = onComplete;
        
        tweenComp.AddTween(scaleTween);
    }

    void CameraBreathing(TweenComponent& tweenComp, Camera2D& camera, const float minZoom, const float maxZoom, const float duration)
    {
        TweenConfig<float> config;
        config.Start = minZoom;
        config.End = maxZoom;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseInOutSine;
        config.Yoyo = true;
        
        config.Setter = [&camera](const float val) {
            camera.Zoom = val;
        };
        
        tweenComp.AddTween(config);
    }

    void BackgroundColorShift(TweenComponent& tweenComp, Color color1, Color color2, std::function<void(Color)> colorSetter, const float duration)
    {
        TweenConfig<float> config;
        config.Start = 0.0f;
        config.End = 1.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseInOutSine;
        config.Yoyo = true;
        
        config.Setter = [color1, color2, colorSetter](const float val) {
            Color currentColor;
            currentColor.r = static_cast<uint8_t>(color1.r + (color2.r - color1.r) * val);
            currentColor.g = static_cast<uint8_t>(color1.g + (color2.g - color1.g) * val);
            currentColor.b = static_cast<uint8_t>(color1.b + (color2.b - color1.b) * val);
            currentColor.a = static_cast<uint8_t>(color1.a + (color2.a - color1.a) * val);
            colorSetter(currentColor);
        };
        
        tweenComp.AddTween(config);
    }
}
