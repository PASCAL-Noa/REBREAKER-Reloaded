#pragma once
#include <cmath>

struct Vector2i
{
    int                     X = 0;
    int                     Y = 0;

    Vector2i                operator+(const Vector2i& other) const { return { X + other.X, Y + other.Y }; }
    Vector2i                operator-(const Vector2i& other) const { return { X - other.X, Y - other.Y }; }
    Vector2i                operator*(int scalar) const { return { X * scalar, Y * scalar }; }
    Vector2i                operator/(int scalar) const { return { X / scalar, Y / scalar }; }
    Vector2i&               operator+=(const Vector2i& other);
    Vector2i&               operator-=(const Vector2i& other);
    Vector2i&               operator*=(int scalar);
    Vector2i&               operator/=(int scalar);
    bool                    operator==(const Vector2i& other) const { return X == other.X && Y == other.Y; }
    bool                    operator!=(const Vector2i& other) const { return !(*this == other); }
};

struct Vector2f
{
    float                   X = 0.0f;
    float                   Y = 0.0f;

    Vector2f                operator+(const Vector2f& other) const { return { X + other.X, Y + other.Y }; }
    Vector2f                operator-(const Vector2f& other) const { return { X - other.X, Y - other.Y }; }
    Vector2f                operator*(float scalar) const { return { X * scalar, Y * scalar }; }
    Vector2f                operator/(float scalar) const { return { X / scalar, Y / scalar }; }
    Vector2f&               operator+=(const Vector2f& other);
    Vector2f&               operator-=(const Vector2f& other);
    Vector2f&               operator*=(float scalar);
    Vector2f&               operator/=(float scalar);
    bool                    operator==(const Vector2f& other) const { return X == other.X && Y == other.Y; }
    bool                    operator!=(const Vector2f& other) const { return !(*this == other); }
    [[nodiscard]] float     LengthSquared() const { return X * X + Y * Y; }
    [[nodiscard]] float     Length() const { return std::sqrt(LengthSquared()); }
    void                    Normalize();
    [[nodiscard]] Vector2f  Normalized() const;
    [[nodiscard]] float     Dot(const Vector2f& other) const { return X * other.X + Y * other.Y; }
};