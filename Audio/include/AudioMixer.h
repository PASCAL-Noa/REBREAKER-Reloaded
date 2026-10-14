#pragma once
#include <string>
#include <cstdint>
#include <memory>

class ResourceManager;

class AudioMixer
{
public:
    explicit AudioMixer(ResourceManager& resourceManager);
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    AudioMixer(AudioMixer&&) noexcept;
    AudioMixer& operator=(AudioMixer&&) noexcept;

    void    Update(float dt) const;

    void    PlaySfx(uint32_t soundId, float volume = 100.0f, float pitch = 1.0f, float pan = 0.0f) const;
    void    StopSfx() const;

    void    PlayMusic(const std::string& filepath, float volume = 100.0f, bool loop = true) const;
    void    FadeToMusic(const std::string& filepath, float fadeDuration = 1.5f, float volume = 100.0f, bool loop = true) const;
    void    FadeOutMusic(float fadeDuration = 1.0f) const;
    void    StopMusic() const;

    void    SetMasterVolume(float volume) const;
    void    SetSfxVolume(float volume) const;
    void    SetMusicVolume(float volume) const;
    void    StopAll() const;

    [[nodiscard]] float GetSfxVolume() const;
    [[nodiscard]] float GetMusicVolume() const;

    [[nodiscard]] bool  IsMusicPlaying() const;
    [[nodiscard]] const std::string& GetCurrentMusicPath() const;
    [[nodiscard]] float GetMusicTimeRemaining() const;
private:
    struct  Impl;
    std::unique_ptr<Impl> mp_impl;
};