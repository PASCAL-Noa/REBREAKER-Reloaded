#pragma once
#include <string>
#include <cstdint>

class ResourceManager;

class AudioMixer
{
public:
    explicit AudioMixer(ResourceManager& resourceManager);
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    void    PlaySfx(uint32_t soundId, float volume = 100.0f, float pitch = 1.0f) const;
    void    PlayMusic(const std::string& filepath, float volume = 100.0f, bool loop = true) const;
    void    StopMusic() const;

    void    SetMasterVolume(float volume) const;
    void    SetSfxVolume(float volume) const;
    void    SetMusicVolume(float volume) const;
    void    StopAll() const;

    [[nodiscard]] bool  IsMusicPlaying() const;
private:
    struct  Impl;
    Impl*   mp_impl;
};