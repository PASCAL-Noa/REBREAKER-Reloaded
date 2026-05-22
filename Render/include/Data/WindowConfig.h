#pragma once
#include <string>

enum class WindowMode
{
    Windowed,
    Fullscreen,
    Borderless
};

struct WindowConfig
{
    unsigned int    Width   =  1920;
    unsigned int    Height  =  1080;
    std::string     Title   =  "Rebreaker Reload";
    WindowMode      Mode    =  WindowMode::Windowed;
    unsigned int    MaxFPS  =  120;
    bool            VSync   =  false;
};