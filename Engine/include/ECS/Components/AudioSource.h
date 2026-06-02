#pragma once
#include <cstdint>
#include <string>

struct AudioSource
{
    uint32_t    SoundId = 0;
    std::string MusicPath = "";
    
    float       Volume = 100.0f;
    float       Pitch = 1.0f;
    bool        IsMusic = false;
    bool        Loop = false;
    bool        PlayRequested = false;
    bool        StopRequested = false;
};