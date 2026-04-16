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
    unsigned int    Width   =  1280;
    unsigned int    Height  =  720;
    std::string     Title   =  "Rebreaker Reload";
    WindowMode      Mode    =  WindowMode::Windowed;
    unsigned int    MaxFPS  =  60;
};