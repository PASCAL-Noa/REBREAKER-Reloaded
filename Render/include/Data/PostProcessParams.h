#pragma once
#include "Data/Color.h"

struct PostProcessParams
{
    float ChromaticAberration = 0.005f;
    float VignetteIntensity = 0.25f;
    Color VignetteColor{0, 0, 0, 255};
    float FlashIntensity = 0.0f;
    Color FlashColor{255, 255, 255, 255};
};
