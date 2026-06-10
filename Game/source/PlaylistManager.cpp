#include "PlaylistManager.h"
#include "AudioMixer.h"
#include "Resources/ResourceManager.h"

void PlaylistManager::AddTrack(GameContext& context, const std::string& filepath)
{
    m_tracks.push_back(filepath);
}

void PlaylistManager::PlayNext(const GameContext& context)
{
    if (m_tracks.empty()) return;

    m_currentTrack = (m_currentTrack + 1) % m_tracks.size();
    context.Audio.PlayMusic(m_tracks[m_currentTrack], 60.0f, false);
}

void PlaylistManager::Update(const GameContext& context)
{
    if (m_tracks.empty()) return;

    if (!context.Audio.IsMusicPlaying())
    {
        PlayNext(context);
    }
}

void PlaylistManager::Stop(const GameContext& context)
{
    context.Audio.StopMusic();
}