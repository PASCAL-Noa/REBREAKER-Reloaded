#include "TweenEffects/TweenEffects.h"
#include <cstdlib>

#include "ECS/Registry.hpp"
#include "ECS/Components/Transform2D.h"
#include "ECS/Components/Camera2D.h"
#include "ECS/Components/TweenComponent.h"

namespace TweenEffects
{
    void Shake(TweenComponent& tweenComp, Registry& registry, Entity entity, const float duration, const float intensity)
    {
        TweenConfig<float> config;
        config.Start = intensity;
        config.End = 0.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseOutQuad;

        config.Setter = [&registry, entity, currentOffset = Vector2f{0.0f, 0.0f}](const float val) mutable
        {
            const bool hasTransform = registry.HasComponent<Transform2D>(entity);
            const bool hasCamera = registry.HasComponent<Camera2D>(entity);

            if (!hasTransform && !hasCamera)
                return;

            if (hasTransform)
            {
                auto& transform = registry.GetComponent<Transform2D>(entity);
                transform.Position.X -= currentOffset.X;
                transform.Position.Y -= currentOffset.Y;
            }
            if (hasCamera)
            {
                auto& camera = registry.GetComponent<Camera2D>(entity);
                camera.Position.X -= currentOffset.X;
                camera.Position.Y -= currentOffset.Y;
            }

            if (val <= 0.0f)
            {
                currentOffset = Vector2f{0.0f, 0.0f};
                return;
            }

            const float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            const float offsetY = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            currentOffset = Vector2f{offsetX, offsetY};

            if (hasTransform)
            {
                auto& transform = registry.GetComponent<Transform2D>(entity);
                transform.Position.X += currentOffset.X;
                transform.Position.Y += currentOffset.Y;
            }
            if (hasCamera)
            {
                auto& camera = registry.GetComponent<Camera2D>(entity);
                camera.Position.X += currentOffset.X;
                camera.Position.Y += currentOffset.Y;
            }
        };

        tweenComp.AddTween(config);
    }

    void Shake(Registry& registry, Entity entity, const float duration, const float intensity)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        Shake(registry.GetComponent<TweenComponent>(entity), registry, entity, duration, intensity);
    }

    void Spin(TweenComponent& tweenComp, Registry& registry, Entity entity, const float duration)
    {
        TweenConfig<float> config;
        config.Start = 0.0f;
        config.End = 360.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseInOutSine;

        config.Setter = [&registry, entity](float angle)
        {
            if (!registry.HasComponent<Transform2D>(entity))
                return;
            registry.GetComponent<Transform2D>(entity).Rotation = angle;
        };

        tweenComp.AddTween(config);
    }

    void Spin(Registry& registry, Entity entity, const float duration)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        Spin(registry.GetComponent<TweenComponent>(entity), registry, entity, duration);
    }

    void ComboFlameScale(TweenComponent& tweenComp, Registry& registry, Entity entity, const float targetScale)
    {
        float startScale = 0.0f;
        if (registry.HasComponent<Transform2D>(entity))
        {
            startScale = registry.GetComponent<Transform2D>(entity).Scale.X;
        }

        TweenConfig<float> config;
        config.Start = startScale;
        config.End = targetScale;
        config.Duration = 0.4f;
        
        if (targetScale <= 0.0f)
            config.Ease = EasingFunctions::EasingType::EaseOutQuad;
        else
            config.Ease = EasingFunctions::EasingType::EaseOutBack;
        
        config.Setter = [&registry, entity](float val)
        {
            if (!registry.HasComponent<Transform2D>(entity))
                return;
            if (val < 0.0f) val = 0.0f;
            registry.GetComponent<Transform2D>(entity).Scale = {val, val};
        };

        tweenComp.AddTween(config);
    }

    void ComboFlameScale(Registry& registry, Entity entity, const float targetScale)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        ComboFlameScale(registry.GetComponent<TweenComponent>(entity), registry, entity, targetScale);
    }

    void BallIn(TweenComponent& tweenComp, Registry& registry, Entity entity, const std::function<void()>& onComplete, const float duration)
    {
        TweenConfig<Vector2f> scaleTween;
        scaleTween.Start = Vector2f{0.0f, 0.0f};
        scaleTween.End = Vector2f{1.0f, 1.0f};
        scaleTween.Duration = duration;
        scaleTween.Ease = EasingFunctions::EasingType::EaseOutBack;
        scaleTween.Setter = [&registry, entity](const Vector2f v) {
            if (!registry.HasComponent<Transform2D>(entity))
                return;
            registry.GetComponent<Transform2D>(entity).Scale = v;
        };
        scaleTween.OnComplete = onComplete;
        
        tweenComp.AddTween(scaleTween);
    }

    void BallIn(Registry& registry, Entity entity, const std::function<void()>& onComplete, const float duration)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        BallIn(registry.GetComponent<TweenComponent>(entity), registry, entity, onComplete, duration);
    }

    void BallOut(TweenComponent& tweenComp, Registry& registry, Entity entity, std::function<void()> onComplete, float duration)
    {
        Vector2f startScale{1.0f, 1.0f};
        if (registry.HasComponent<Transform2D>(entity))
        {
            startScale = registry.GetComponent<Transform2D>(entity).Scale;
        }

        TweenConfig<Vector2f> scaleTween;
        scaleTween.Start = startScale;
        scaleTween.End = Vector2f{0.0f, 0.0f};
        scaleTween.Duration = duration;
        scaleTween.Ease = EasingFunctions::EasingType::EaseInBack;
        scaleTween.Setter = [&registry, entity](Vector2f v) {
            if (!registry.HasComponent<Transform2D>(entity))
                return;
            registry.GetComponent<Transform2D>(entity).Scale = v;
        };
        scaleTween.OnComplete = onComplete;
        
        tweenComp.AddTween(scaleTween);
    }

    void BallOut(Registry& registry, Entity entity, std::function<void()> onComplete, float duration)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        BallOut(registry.GetComponent<TweenComponent>(entity), registry, entity, onComplete, duration);
    }

    void CameraBreathing(TweenComponent& tweenComp, Registry& registry, Entity entity, const float minZoom, const float maxZoom, const float duration)
    {
        TweenConfig<float> config;
        config.Start = minZoom;
        config.End = maxZoom;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseInOutSine;
        config.Yoyo = true;
        
        config.Setter = [&registry, entity](const float val) {
            if (!registry.HasComponent<Camera2D>(entity))
                return;
            registry.GetComponent<Camera2D>(entity).Zoom = val;
        };
        
        tweenComp.AddTween(config);
    }

    void CameraBreathing(Registry& registry, Entity entity, const float minZoom, const float maxZoom, const float duration)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        CameraBreathing(registry.GetComponent<TweenComponent>(entity), registry, entity, minZoom, maxZoom, duration);
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

    void BackgroundColorShift(Registry& registry, Entity entity, Color color1, Color color2, std::function<void(Color)> colorSetter, const float duration)
    {
        if (!registry.HasComponent<TweenComponent>(entity))
        {
            registry.AddComponent<TweenComponent>(entity, TweenComponent{});
        }
        BackgroundColorShift(registry.GetComponent<TweenComponent>(entity), color1, color2, colorSetter, duration);
    }
}

