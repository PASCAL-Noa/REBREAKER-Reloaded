#pragma once

struct BallComponent
{
    bool  IsActive = true;
    bool  IsBig = false;
    bool  IsFireBall = false;
    bool  IsFuseActive = false;
    float FuseTimer = 0.0f;
    float TrailTimer = 0.0f;
};
