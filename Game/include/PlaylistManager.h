#pragma once
#include "Core/GameContext.h"
#include <vector>
#include <string>

class PlaylistManager
{
public:
    void           AddTrack(GameContext& context, const std::string& filepath);
    void           PlayNext(const GameContext& context, float fadeDuration = 1.5f);
    void           Update(const GameContext& context);
    static void    Stop(const GameContext& context);
    void           Clear();
    [[nodiscard]] size_t GetTrackCount() const noexcept { return m_tracks.size(); }
    [[nodiscard]] size_t GetCurrentTrackIndex() const noexcept { return m_currentTrack; }
    [[nodiscard]] const std::vector<std::string>& GetTracks() const noexcept { return m_tracks; }

private:
    std::vector<std::string>    m_tracks;
    size_t  m_currentTrack = static_cast<size_t>(-1);
    bool    m_isCrossfading = false;
};