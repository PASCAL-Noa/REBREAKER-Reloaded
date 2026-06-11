#pragma once
#include "Math/Vector2.h"
#include "Data/Color.h"

struct ParticleComponent
{
    Vector2f    Velocity{0.0f, 0.0f};
    float   Life = 1.0f;
    float   MaxLife = 1.0f;
    float   Size = 10.0f;
    Color   Tint = Colors::White;
};
