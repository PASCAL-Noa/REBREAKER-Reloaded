#include "ECS/Systems/TweenSystem.h"
#include "ECS/Components/TweenComponent.h"
#include <algorithm>

#include "ECS/Registry.hpp"

TweenSystem::TweenSystem(Registry& registry) : System(registry) {}

void TweenSystem::OnUpdate(float dt)
{
    m_registry.View<TweenComponent>([dt](Entity, TweenComponent& tweenComp)
    {
        for (int i = static_cast<int>(tweenComp.ActiveTweens.size()) - 1; i >= 0; --i)
        {
            tweenComp.ActiveTweens[i]->Update(dt);
            
            if (tweenComp.ActiveTweens[i]->IsFinished())
            {
                tweenComp.ActiveTweens.erase(tweenComp.ActiveTweens.begin() + i);
            }
        }
    });
}