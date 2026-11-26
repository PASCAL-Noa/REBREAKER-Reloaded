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

    // Music levels are RELATIVE fade levels (0..kNominalMusicLevel). The absolute loudness of
    // every track is decided centrally: level * bus headroom * MusicVolume * MasterVolume * ducking.
    static constexpr float kNominalMusicLevel = 100.0f;

    void    PlayMusic(const std::string& filepath, float volume = kNominalMusicLevel, bool loop = true) const;
    void    PlayMusic(const std::string& filepath, bool loop) const = delete; // prevents bool->float volume bugs
    void    FadeToMusic(const std::string& filepath, float fadeDuration = 1.5f, float volume = kNominalMusicLevel, bool loop = true) const;
    void    FadeToMusic(const std::string& filepath, bool loop) const = delete;
    void    FadeToMusic(const std::string& filepath, const TweenConfig<float>& config, bool loop = true) const;
    void    FadeOutMusic(float fadeDuration = 1.0f) const;
    void    FadeOutMusic(const TweenConfig<float>& config) const;
    void    FadeMusicVolume(float targetVolume, float duration = 1.0f, EasingFunctions::EasingType ease = EasingFunctions::EasingType::Linear) const;
    void    StopMusic() const;
    [[nodiscard]] float GetMusicLevel() const;     // current relative level of the active track
    [[nodiscard]] float GetEffectiveMusicGain() const; // final 0..100 gain sent to the device (for diagnostics/tests)

    // Audio Ducking
    void    TriggerDucking(float duckAmount = 0.4f, float restoreDuration = 0.5f) const;
    void    SetDuckingEnabled(bool enabled) const;
    [[nodiscard]] bool  IsDuckingEnabled() const;
    [[nodiscard]] float GetDuckingMultiplier() const;

    // Super Mario Maker style Edit Mode vs. Play Mode Music Variant Treatment
    // Softens, quiets, and low-pass filters the active music track in Edit Mode while preserving playback offset.
    static constexpr float kEditModeGainMultiplier = 0.48f;
    void    SetMusicEditMode(bool enabled, float transitionDuration = 0.32f) const;
    [[nodiscard]] bool  IsMusicEditMode() const;
    [[nodiscard]] float GetMusicEditModeMix() const;

    // Temporal Pitch Distortion & Time-Warp (Tempo Ball / Tape Stop)
    void    SetMusicPitch(float pitch) const;
    void    FadeMusicPitch(float targetPitch, float duration = 0.5f) const;
    void    ResetMusicPitch(float duration = 0.5f) const;
    [[nodiscard]] float GetMusicPitch() const;
    [[nodiscard]] float GetTargetMusicPitch() const;
    void    SetTemporalSfxPitchMultiplier(float multiplier) const;
    [[nodiscard]] float GetTemporalSfxPitchMultiplier() const;

    void    SetMasterVolume(float volume) const;
    void    SetSfxVolume(float volume) const;
    void    SetMusicVolume(float volume) const;
    void    StopAll() const;

    [[nodiscard]] float GetMasterVolume() const;
    [[nodiscard]] float GetSfxVolume() const;
    [[nodiscard]] float GetMusicVolume() const;

    [[nodiscard]] bool  IsMusicPlaying() const;
    [[nodiscard]] const std::string& GetCurrentMusicPath() const;
    [[nodiscard]] float GetMusicTimeRemaining() const;
    void                SetMusicLooping(bool loop) const;
    [[nodiscard]] bool  IsMusicLooping() const;

    [[nodiscard]] std::vector<std::string> GetAvailableAudioDevices() const;
    [[nodiscard]] std::string GetCurrentAudioDevice() const;
    bool SetAudioDevice(const std::string& name) const;
private:
    struct  Impl;
    std::unique_ptr<Impl> mp_impl;
};