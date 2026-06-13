#pragma once
#include <string>

struct AnimationData
{
    int     FrameWidth = 0;
    int     FrameHeight = 0;
    int     FrameCount = 1;
    float   FrameDuration = 0.1f;
    int     StartX = 0;
    int     StartY = 0;
    bool    Loop = true;
};
