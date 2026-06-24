#pragma once

struct IntRect
{
    int Left = 0;
    int Top = 0;
    int Width = 0;
    int Height = 0;

    IntRect() = default;
    IntRect(int left, int top, int width, int height)
        : Left(left), Top(top), Width(width), Height(height)
    {
    }
};
