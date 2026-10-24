#pragma once

struct BallComponent
{
    bool  IsActive = true;
    bool  IsBig = false;
    bool  IsFireBall = false;
    bool  IsFuseActive = false;
    bool  IsGhost = false;
    bool  IsStuck = false;
    float StuckOffsetX = 0.0f;
    float FuseTimer = 0.0f;
    float TrailTimer = 0.0f;
};
