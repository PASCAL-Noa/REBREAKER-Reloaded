#include "PlaylistManager.h"
#include "AudioMixer.h"
#include "Resources/ResourceManager.h"
#include <algorithm>

void PlaylistManager::AddTrack(GameContext& /*context*/, const std::string& filepath)
{
    if (std::find(m_tracks.begin(), m_tracks.end(), filepath) == m_tracks.end())
    {
        m_tracks.push_back(filepath);
    }
}

void PlaylistManager::Clear()
{
    m_tracks.clear();
    m_currentTrack = static_cast<size_t>(-1);
    m_isCrossfading = false;
}

void PlaylistManager::PlayNext(const GameContext& context, float fadeDuration)
{
    if (m_tracks.empty()) return;

    m_currentTrack = (m_currentTrack + 1) % m_tracks.size();
    context.Audio.FadeToMusic(m_tracks[m_currentTrack], fadeDuration, AudioMixer::kNominalMusicLevel, false);
}

void PlaylistManager::Update(const GameContext& context)
{
    if (m_tracks.empty()) return;

    if (!context.Audio.IsMusicPlaying())
    {
        m_isCrossfading = false;
        PlayNext(context, 1.0f);
        return;
    }

    const std::string& currentPath = context.Audio.GetCurrentMusicPath();
    bool isInPlaylist = false;
    for (size_t i = 0; i < m_tracks.size(); ++i)
    {
        if (m_tracks[i] == currentPath)
        {
            isInPlaylist = true;
            if (!m_isCrossfading)
            {
                m_currentTrack = i;
            }
            break;
        }
    }

    // If an ultimate ability theme or special stinger is currently playing, do not interrupt it
    if (!isInPlaylist)
    {
        return;
    }

    // When a playlist has multiple tracks, disable looping on the current track so it advances at track end;
    // when it has only 1 track, keep it looping seamlessly.
    if (m_tracks.size() > 1)
    {
        if (context.Audio.IsMusicLooping())
        {
            context.Audio.SetMusicLooping(false);
        }
    }
    else
    {
        if (!context.Audio.IsMusicLooping())
        {
            context.Audio.SetMusicLooping(true);
        }
        return;
    }

    float remaining = context.Audio.GetMusicTimeRemaining();
    if (remaining >= 0.0f && remaining <= 2.0f)
    {
        if (!m_isCrossfading)
        {
            m_isCrossfading = true;
            PlayNext(context, 2.0f);
        }
    }
    else if (remaining > 2.5f)
    {
        m_isCrossfading = false;
    }
}

void PlaylistManager::Stop(const GameContext& context)
{
    context.Audio.StopMusic();
}