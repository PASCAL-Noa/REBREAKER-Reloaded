#include "Math/Vector2.h"

Vector2i& Vector2i::operator+=(const Vector2i& other)
{
    X += other.X;
    Y += other.Y;
    return *this;
}

Vector2i& Vector2i::operator-=(const Vector2i& other)
{
    X -= other.X;
    Y -= other.Y;
    return *this;
}

Vector2i& Vector2i::operator*=(int scalar)
{
    X *= scalar;
    Y *= scalar;
    return *this;
}

Vector2i& Vector2i::operator/=(int scalar)
{
    X /= scalar;
    Y /= scalar;
    return *this;
}

Vector2f& Vector2f::operator+=(const Vector2f& other)
{
    X += other.X;
    Y += other.Y;
    return *this;
}

Vector2f& Vector2f::operator-=(const Vector2f& other)
{
    X -= other.X;
    Y -= other.Y;
    return *this;
}

Vector2f& Vector2f::operator*=(float scalar)
{
    X *= scalar;
    Y *= scalar;
    return *this;
}

Vector2f& Vector2f::operator/=(float scalar)
{
    X /= scalar;
    Y /= scalar;
    return *this;
}

void Vector2f::Normalize()
{
    float len = Length();
    if (len > 0.0f)
    {
        X /= len;
        Y /= len;
    }
}

Vector2f Vector2f::Normalized() const
{
    Vector2f vec = *this;
    vec.Normalize();
    return vec;
}
