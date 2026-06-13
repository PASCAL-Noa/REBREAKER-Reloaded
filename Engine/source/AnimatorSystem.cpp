#include "ECS/Systems/AnimatorSystem.h"
#include "ECS/Registry.hpp"
#include "ECS/Components/AnimatorComponent.h"
#include "ECS/Components/SpriteComponent.h"

AnimatorSystem::AnimatorSystem(Registry& registry) : System(registry) {}

void AnimatorSystem::OnUpdate(float dt)
{
    m_registry.View<AnimatorComponent, SpriteComponent>([dt](Entity, AnimatorComponent& anim, SpriteComponent& sprite)
    {
        auto& data = anim.CurrentAnimation;
        if (!anim.IsPlaying || data.FrameCount <= 1)
        {
            sprite.TextureRect = IntRect(data.StartX + anim.CurrentFrame * data.FrameWidth, data.StartY, data.FrameWidth, data.FrameHeight);
            return;
        }

        anim.CurrentTime += dt;
        if (anim.CurrentTime >= data.FrameDuration)
        {
            anim.CurrentTime -= data.FrameDuration;
            anim.CurrentFrame++;

            if (anim.CurrentFrame >= data.FrameCount)
            {
                if (data.Loop)
                    anim.CurrentFrame = 0;
                else
                {
                    anim.CurrentFrame = data.FrameCount - 1;
                    anim.IsPlaying = false;
                }
            }
        }

        sprite.TextureRect = IntRect(data.StartX + anim.CurrentFrame * data.FrameWidth, data.StartY, data.FrameWidth, data.FrameHeight);
    });
}
