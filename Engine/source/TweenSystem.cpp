#include "ECS/Systems/TweenSystem.h"
#include "ECS/Components/TweenComponent.h"
#include <algorithm>

#include "ECS/Registry.hpp"

TweenSystem::TweenSystem(Registry& registry) : System(registry) {}

void TweenSystem::OnUpdate(float dt)
{
    m_registry.View<TweenComponent>([dt](Entity, TweenComponent& tweenComp)
    {
        std::erase_if(tweenComp.ActiveTweens, [dt](const std::shared_ptr<ITween>& tween)
        {
            tween->Update(dt);
            return tween->IsFinished();
        });
    });
}