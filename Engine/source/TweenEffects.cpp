#include "TweenEffects/TweenEffects.h"
#include <cstdlib>

#include "ECS/Components/Camera2D.h"

namespace TweenEffects
{
    void Shake(TweenComponent& tweenComp, Transform2D& transform, float duration, float intensity)
    {
        TweenConfig<float> config;
        config.Start = intensity;
        config.End = 0.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseOutQuad;

        config.Setter = [&transform, currentOffset = Vector2f{0.0f, 0.0f}](float val) mutable
        {
            transform.Position.X -= currentOffset.X;
            transform.Position.Y -= currentOffset.Y;

            if (val <= 0.0f)
            {
                currentOffset = Vector2f{0.0f, 0.0f};
                return;
            }

            float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            float offsetY = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            currentOffset = Vector2f{offsetX, offsetY};

            transform.Position.X += currentOffset.X;
            transform.Position.Y += currentOffset.Y;
        };

        tweenComp.AddTween(config);
    }

    void Shake(TweenComponent& tweenComp, Camera2D& camera, float duration, float intensity)
    {
        TweenConfig<float> config;
        config.Start = intensity;
        config.End = 0.0f;
        config.Duration = duration;
        config.Ease = EasingFunctions::EasingType::EaseOutQuad;

        config.Setter = [&camera, currentOffset = Vector2f{0.0f, 0.0f}](float val) mutable
        {
            camera.Position.X -= currentOffset.X;
            camera.Position.Y -= currentOffset.Y;

            if (val <= 0.0f)
            {
                currentOffset = Vector2f{0.0f, 0.0f};
                return;
            }

            float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            float offsetY = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f * val;
            currentOffset = Vector2f{offsetX, offsetY};

            camera.Position.X += currentOffset.X;
            camera.Position.Y += currentOffset.Y;
        };

        tweenComp.AddTween(config);
    }

    void Spin(TweenComponent& tweenComp, Transform2D& transform, float duration)
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
}
