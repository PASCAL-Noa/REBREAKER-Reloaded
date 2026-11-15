#pragma once

#include "Data/Color.h"
#include <string>
#include <vector>
#include <cstdint>

class EndlessScene;

enum class PerkRarity : int
{
    Common = 0,
    Uncommon = 1,
    Rare = 2,
    Epic = 3,
    Legendary = 4
};

enum class DraftType : int
{
    PeriodicSupply = 0, // Every minute: standard / less rare upgrades
    WaveClear = 1       // Full grid cleared: superior / higher rarity upgrades
};

enum class RoguelitePerkType : int
{
    // Stat Boosts
    TitanPaddle = 0,       // +20% Paddle Width (Common)
    HyperDrive = 1,        // +15% Paddle Speed (Common)
    ReinforcedHull = 2,    // +1 Extra Life (Rare)
    KineticCore = 3,       // +10% Ball Speed & +25% Score Multiplier (Uncommon)
    HeavyImpact = 4,       // +1 Ball Damage per hit (Epic)

    // Ball Modifiers & Buff Synergies
    ThermalRounds = 5,     // Ignites balls into fireballs (Rare)
    LightningRod = 6,      // 25% chance on brick break to chain lightning (Epic)
    SplitStream = 7,       // Spawns 2 additional balls into play (Uncommon)
    PlasmaField = 8,       // Hard brick hits trigger explosive shocks (Epic)
    MagneticTether = 9,    // Attracts dropped capsules to paddle (Uncommon)

    // Endless Utility & Economy
    EmergencyShield = 10,  // Activates a bottom safety shield (Rare)
    InstantOvercharge = 11,// Fully charges the active Ultimate meter (Legendary)
    JackpotCredits = 12,   // Grants +250 Credits to bank (Uncommon)
    ScoreFrenzy = 13,      // +25% Permanent Endless Score Multiplier (Common)
    ChronoStasis = 14,     // Slows down downward creep rate by 25% (Common)

    // Advanced Tactical Perks
    OverdriveCapacitor = 15, // +15% Ball Velocity & +1 Damage during Wave Climax (Rare)
    ShieldSiphon = 16,       // Breaking medium/hard bricks restores +5% Ultimate (Uncommon)
    VortexPulse = 17,        // Kinetic pulse detonates hazards & redirects balls (Epic)

    Count = 18
};

struct RogueliteCard
{
    RoguelitePerkType Type = RoguelitePerkType::TitanPaddle;
    std::string Title;
    std::string Category;
    std::string Description;
    Color CardColor = Colors::Cyan;
    PerkRarity Rarity = PerkRarity::Common;
    std::string RarityName = "[ COMMON ]";
    Color RarityColor = Color{180, 200, 220, 255};
};

class RogueliteDraftManager
{
public:
    static std::vector<RogueliteCard> GenerateDraft(int count = 3, DraftType draftType = DraftType::PeriodicSupply, uint32_t seed = 0);
    static RogueliteCard              GetCardInfo(RoguelitePerkType type);
    static PerkRarity                 GetPerkRarity(RoguelitePerkType type);
    static void                       ApplyPerk(RoguelitePerkType type, EndlessScene& scene);
};
