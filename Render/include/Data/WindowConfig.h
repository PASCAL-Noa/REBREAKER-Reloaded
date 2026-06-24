#pragma once
#include <string>

enum class WindowMode
{
    Windowed,
    Fullscreen,
    Borderless
};

struct Resolution
{
    unsigned int Width = 0;
    unsigned int Height = 0;
};

struct WindowConfig
{
    unsigned int    Width   =  2560;
    unsigned int    Height  =  1600;
    std::string     Title   =  "Rebreaker Reload";
    WindowMode      Mode    =  WindowMode::Fullscreen;
    unsigned int    MaxFPS  =  120;
    bool            VSync   =  false;
};