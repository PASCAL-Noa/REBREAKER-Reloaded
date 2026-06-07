#pragma once
#include "ECS/Tween.h"
#include <vector>
#include <memory>

struct TweenComponent
{
    std::vector<std::shared_ptr<ITween>> ActiveTweens;

    template<typename T>
    void AddTween(const TweenConfig<T>& config)
    {
        ActiveTweens.push_back(std::make_shared<Tween<T>>(config));
    }

    void Clear()
    {
        ActiveTweens.clear();
    }
};