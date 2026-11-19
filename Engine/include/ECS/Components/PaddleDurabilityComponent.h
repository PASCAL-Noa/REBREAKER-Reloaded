#pragma once
#include <algorithm>
#include <cstdint>

enum class PaddleFractureState : uint8_t
{
    Pristine = 0,    // > 75% durability
    Scratched = 1,   // 50% - 75% durability
    Fractured = 2,   // 25% - 50% durability
    Critical = 3,    // 0% - 25% durability
    Destroyed = 4    // 0% durability (immediate defeat condition)
};

struct PaddleDurabilityComponent
{
    float MaxDurability = 100.0f;
    float CurrentDurability = 100.0f;
    float Armor = 0.0f; // Damage reduction factor (0.0f = 0%, 0.5f = 50% reduction)

    // Temporary Energy Shielding
    float MaxShield = 50.0f;
    float CurrentShield = 0.0f;
    float ShieldDuration = 0.0f;
    bool  IsShieldActive = false;

    // Visual & Hit Reaction Timers
    float InvulnerabilityTimer = 0.0f;
    float InvulnerabilityDuration = 0.35f;
    float DamageFlashTimer = 0.0f;
    float SparkTimer = 0.0f;

    // Minimum width factor when paddle is critically fractured
    float MinWidthMultiplier = 0.60f;

    [[nodiscard]] bool IsDestroyed() const noexcept { return CurrentDurability <= 0.0f; }
    [[nodiscard]] bool IsInvulnerable() const noexcept { return InvulnerabilityTimer > 0.0f; }

    [[nodiscard]] float GetDurabilityRatio() const noexcept
    {
        return MaxDurability > 0.0f ? std::clamp(CurrentDurability / MaxDurability, 0.0f, 1.0f) : 0.0f;
    }

    [[nodiscard]] float GetShieldRatio() const noexcept
    {
        return MaxShield > 0.0f ? std::clamp(CurrentShield / MaxShield, 0.0f, 1.0f) : 0.0f;
    }

    [[nodiscard]] PaddleFractureState GetFractureState() const noexcept
    {
        if (CurrentDurability <= 0.0f) return PaddleFractureState::Destroyed;
        const float ratio = GetDurabilityRatio();
        if (ratio > 0.75f) return PaddleFractureState::Pristine;
        if (ratio > 0.50f) return PaddleFractureState::Scratched;
        if (ratio > 0.25f) return PaddleFractureState::Fractured;
        return PaddleFractureState::Critical;
    }

    // Dynamic width multiplier: scales paddle width down as durability is lost
    [[nodiscard]] float GetEffectiveWidthMultiplier() const noexcept
    {
        const float ratio = GetDurabilityRatio();
        return MinWidthMultiplier + (1.0f - MinWidthMultiplier) * ratio;
    }

    // Applies incoming damage. Shields absorb first, remaining damage is mitigated by armor.
    // Returns true if damage was applied.
    bool TakeDamage(float rawDamage, bool ignoreInvulnerability = false)
    {
        if (IsDestroyed()) return false;
        if (!ignoreInvulnerability && IsInvulnerable()) return false;

        float incoming = rawDamage;

        // 1. Temporary shield absorbs first
        if (IsShieldActive && CurrentShield > 0.0f)
        {
            if (CurrentShield >= incoming)
            {
                CurrentShield -= incoming;
                incoming = 0.0f;
            }
            else
            {
                incoming -= CurrentShield;
                CurrentShield = 0.0f;
                IsShieldActive = false;
            }
        }

        // 2. Excess damage damages durability with armor mitigation
        if (incoming > 0.0f)
        {
            const float armorMitigation = std::clamp(Armor, 0.0f, 0.90f);
            const float mitigatedDamage = incoming * (1.0f - armorMitigation);
            CurrentDurability = std::max(0.0f, CurrentDurability - mitigatedDamage);
        }

        DamageFlashTimer = 0.20f;
        InvulnerabilityTimer = InvulnerabilityDuration;
        return true;
    }

    // Restores paddle durability up to MaxDurability
    void Repair(float amount)
    {
        if (IsDestroyed()) return;
        CurrentDurability = std::clamp(CurrentDurability + amount, 0.0f, MaxDurability);
    }

    // Grants temporary energy shield
    void AddShield(float amount, float duration)
    {
        CurrentShield = std::clamp(CurrentShield + amount, 0.0f, MaxShield);
        ShieldDuration = std::max(ShieldDuration, duration);
        IsShieldActive = (CurrentShield > 0.0f);
    }

    void Update(float dt)
    {
        if (InvulnerabilityTimer > 0.0f)
        {
            InvulnerabilityTimer = std::max(0.0f, InvulnerabilityTimer - dt);
        }
        if (DamageFlashTimer > 0.0f)
        {
            DamageFlashTimer = std::max(0.0f, DamageFlashTimer - dt);
        }
        if (IsShieldActive)
        {
            ShieldDuration -= dt;
            if (ShieldDuration <= 0.0f || CurrentShield <= 0.0f)
            {
                CurrentShield = 0.0f;
                IsShieldActive = false;
            }
        }
        SparkTimer += dt;
    }
};
