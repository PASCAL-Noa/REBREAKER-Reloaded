#pragma once

#include "Graphics/Animation.h"

struct AnimatorComponent
{
    AnimationData   CurrentAnimation;
    
    float   CurrentTime = 0.0f;
    int     CurrentFrame = 0;
    bool    IsPlaying = false;

    void Play(const AnimationData& anim)
    {
        CurrentAnimation = anim;
        CurrentTime = 0.0f;
        CurrentFrame = 0;
        IsPlaying = true;
    }
};
