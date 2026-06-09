#pragma once
#include "Core/GameContext.h"
#include <vector>
#include <string>

class PlaylistManager
{
public:
    void    AddTrack(GameContext& context, const std::string& filepath);
    void    PlayNext(const GameContext& context);
    void    Update(const GameContext& context);
    static void    Stop(const GameContext& context);

private:
    std::vector<std::string>    m_tracks;
    size_t  m_currentTrack = static_cast<size_t>(-1);
};