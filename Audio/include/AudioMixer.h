#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "ECS/Tween.h"

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
    void    PlayLoopingSfx(uint32_t soundId, float volume = 100.0f, float pitch = 1.0f) const;
    void    StopLoopingSfx(uint32_t soundId) const;
    [[nodiscard]] bool IsLoopingSfxPlaying(uint32_t soundId) const;

    void    PlayMusic(const std::string& filepath, float volume = 100.0f, bool loop = true) const;
    void    FadeToMusic(const std::string& filepath, float fadeDuration = 1.5f, float volume = 100.0f, bool loop = true) const;
    void    FadeToMusic(const std::string& filepath, const TweenConfig<float>& config, bool loop = true) const;
    void    FadeOutMusic(float fadeDuration = 1.0f) const;
    void    FadeOutMusic(const TweenConfig<float>& config) const;
    void    FadeMusicVolume(float targetVolume, float duration = 1.0f, EasingFunctions::EasingType ease = EasingFunctions::EasingType::Linear) const;
    void    StopMusic() const;

    // Audio Ducking
    void    TriggerDucking(float duckAmount = 0.4f, float restoreDuration = 0.5f) const;
    void    SetDuckingEnabled(bool enabled) const;
    [[nodiscard]] bool  IsDuckingEnabled() const;
    [[nodiscard]] float GetDuckingMultiplier() const;

    void    SetMasterVolume(float volume) const;
    void    SetSfxVolume(float volume) const;
    void    SetMusicVolume(float volume) const;
    void    StopAll() const;

    [[nodiscard]] float GetSfxVolume() const;
    [[nodiscard]] float GetMusicVolume() const;

    [[nodiscard]] bool  IsMusicPlaying() const;
    [[nodiscard]] const std::string& GetCurrentMusicPath() const;
    [[nodiscard]] float GetMusicTimeRemaining() const;

    [[nodiscard]] std::vector<std::string> GetAvailableAudioDevices() const;
    [[nodiscard]] std::string GetCurrentAudioDevice() const;
    bool SetAudioDevice(const std::string& name) const;
private:
    struct  Impl;
    std::unique_ptr<Impl> mp_impl;
};