#include "ECS/Systems/TweenSystem.h"
#include "ECS/Components/TweenComponent.h"
#include <algorithm>

#include "ECS/Registry.hpp"

TweenSystem::TweenSystem(Registry& registry) : System(registry) {}

void TweenSystem::OnUpdate(float dt)
{
    m_registry.View<TweenComponent>([this, dt](Entity entity, TweenComponent& tweenComp)
    {
        if (tweenComp.ActiveTweens.empty()) return;

        // Iterate over existing tweens without heap allocating a full vector copy
        const size_t initialSize = tweenComp.ActiveTweens.size();
        for (size_t i = 0; i < initialSize; ++i)
        {
            if (!m_registry.HasComponent<TweenComponent>(entity)) {
                break;
            }
            if (i >= tweenComp.ActiveTweens.size()) {
                break;
            }

            auto tween = tweenComp.ActiveTweens[i];
            if (tween)
            {
                tween->Update(dt);
            }
        }

        if (m_registry.HasComponent<TweenComponent>(entity))
        {
            auto& realComp = m_registry.GetComponent<TweenComponent>(entity);
            std::erase_if(realComp.ActiveTweens, [](const auto& tw) {
                return !tw || tw->IsFinished();
            });
        }
    });
}