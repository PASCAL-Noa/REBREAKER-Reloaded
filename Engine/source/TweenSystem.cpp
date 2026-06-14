#include "ECS/Systems/TweenSystem.h"
#include "ECS/Components/TweenComponent.h"
#include <algorithm>

#include "ECS/Registry.hpp"

TweenSystem::TweenSystem(Registry& registry) : System(registry) {}

void TweenSystem::OnUpdate(float dt)
{
    m_registry.View<TweenComponent>([this, dt](Entity entity, TweenComponent& tweenComp)
    {
        auto tweensCopy = tweenComp.ActiveTweens;

        for (int i = static_cast<int>(tweensCopy.size()) - 1; i >= 0; --i)
        {
            if (!m_registry.HasComponent<TweenComponent>(entity)) {
                break;
            }

            tweensCopy[i]->Update(dt);
            
            if (!m_registry.HasComponent<TweenComponent>(entity)) {
                break;
            }

            if (tweensCopy[i]->IsFinished())
            {
                auto& realComp = m_registry.GetComponent<TweenComponent>(entity);
                auto it = std::find(realComp.ActiveTweens.begin(), realComp.ActiveTweens.end(), tweensCopy[i]);
                if (it != realComp.ActiveTweens.end()) {
                    realComp.ActiveTweens.erase(it);
                }
            }
        }
    });
}