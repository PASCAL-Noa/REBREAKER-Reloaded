#pragma once
#include "Data/Color.h"
#include "Data/BlendMode.h"
#include <cmath>

enum class LightType
{
    Point, // Radial omnidirectional falloff
    Cone   // Directional cone (e.g. upward beam from paddle)
};

struct LightSourceComponent
{
    LightType Type = LightType::Point;
    Color LightColor{255, 255, 255, 255};
    float Radius = 250.0f;
    float Intensity = 1.0f;
    float InnerRadius = 0.0f;     // Radius of full intensity before falloff starts
    float DirectionDeg = -90.0f;  // Direction in degrees (-90 = upward)
    float ConeAngleDeg = 65.0f;   // Total cone spread angle
    BlendMode Blend = BlendMode::Add;
    bool IsActive = true;
};
