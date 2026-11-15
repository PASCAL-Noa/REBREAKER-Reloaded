#include "Endless/RogueliteDraft.h"
#include <algorithm>
#include <random>

PerkRarity RogueliteDraftManager::GetPerkRarity(RoguelitePerkType type)
{
    switch (type)
    {
    case RoguelitePerkType::TitanPaddle:
    case RoguelitePerkType::HyperDrive:
    case RoguelitePerkType::ScoreFrenzy:
    case RoguelitePerkType::ChronoStasis:
        return PerkRarity::Common;

    case RoguelitePerkType::KineticCore:
    case RoguelitePerkType::SplitStream:
    case RoguelitePerkType::MagneticTether:
    case RoguelitePerkType::JackpotCredits:
    case RoguelitePerkType::ShieldSiphon:
        return PerkRarity::Uncommon;

    case RoguelitePerkType::ReinforcedHull:
    case RoguelitePerkType::ThermalRounds:
    case RoguelitePerkType::EmergencyShield:
    case RoguelitePerkType::OverdriveCapacitor:
        return PerkRarity::Rare;

    case RoguelitePerkType::HeavyImpact:
    case RoguelitePerkType::LightningRod:
    case RoguelitePerkType::PlasmaField:
    case RoguelitePerkType::VortexPulse:
        return PerkRarity::Epic;

    case RoguelitePerkType::InstantOvercharge:
        return PerkRarity::Legendary;

    default:
        return PerkRarity::Common;
    }
}

RogueliteCard RogueliteDraftManager::GetCardInfo(RoguelitePerkType type)
{
    RogueliteCard card;
    card.Type = type;
    card.Rarity = GetPerkRarity(type);

    switch (card.Rarity)
    {
    case PerkRarity::Common:
        card.RarityName = "[ COMMON ]";
        card.RarityColor = Color{170, 195, 220, 255};
        break;
    case PerkRarity::Uncommon:
        card.RarityName = "[ UNCOMMON ]";
        card.RarityColor = Color{75, 230, 120, 255};
        break;
    case PerkRarity::Rare:
        card.RarityName = "[ RARE ]";
        card.RarityColor = Color{50, 185, 255, 255};
        break;
    case PerkRarity::Epic:
        card.RarityName = "[ EPIC ]";
        card.RarityColor = Color{215, 80, 255, 255};
        break;
    case PerkRarity::Legendary:
        card.RarityName = "[ LEGENDARY ]";
        card.RarityColor = Color{255, 190, 30, 255};
        break;
    }
    card.CardColor = card.RarityColor;

    switch (type)
    {
    case RoguelitePerkType::TitanPaddle:
        card.Title = "TITAN CHASSIS";
        card.Category = "[STAT BOOST]";
        card.Description = "+20% Permanent paddle width for maximum defensive reach.";
        break;

    case RoguelitePerkType::HyperDrive:
        card.Title = "HYPER SERVO";
        card.Category = "[STAT BOOST]";
        card.Description = "+15% Lateral paddle velocity for rapid response.";
        break;

    case RoguelitePerkType::ReinforcedHull:
        card.Title = "HULL REINFORCEMENT";
        card.Category = "[STAT BOOST]";
        card.Description = "+1 Extra reserve life added immediately.";
        break;

    case RoguelitePerkType::KineticCore:
        card.Title = "KINETIC OVERDRIVE";
        card.Category = "[STAT BOOST]";
        card.Description = "+25% Score multiplier and increased ball kinetic momentum.";
        break;

    case RoguelitePerkType::HeavyImpact:
        card.Title = "HEAVY IMPACT";
        card.Category = "[BALL MODIFIER]";
        card.Description = "+1 Ball damage per hit (destroys medium bricks in 1 hit).";
        break;

    case RoguelitePerkType::ThermalRounds:
        card.Title = "THERMAL ROUNDS";
        card.Category = "[BALL MODIFIER]";
        card.Description = "Ignites active balls into incendiary explosive fireballs.";
        break;

    case RoguelitePerkType::LightningRod:
        card.Title = "TESLA DISCHARGE";
        card.Category = "[BUFF SYNERGY]";
        card.Description = "25% chance on each impact to trigger chain lightning arcs.";
        break;

    case RoguelitePerkType::SplitStream:
        card.Title = "SPLIT STREAM";
        card.Category = "[BUFF SYNERGY]";
        card.Description = "Instantly deploys 2 additional kinetic balls into play.";
        break;

    case RoguelitePerkType::PlasmaField:
        card.Title = "PLASMA DETONATION";
        card.Category = "[BUFF SYNERGY]";
        card.Description = "Direct impacts on hard bricks trigger explosive shockwaves.";
        break;

    case RoguelitePerkType::MagneticTether:
        card.Title = "MAGNETIC TETHER";
        card.Category = "[BUFF SYNERGY]";
        card.Description = "Projects a magnetic field pulling bonus capsules to the paddle.";
        break;

    case RoguelitePerkType::EmergencyShield:
        card.Title = "AEGIS BARRIER";
        card.Category = "[ENDLESS UTILITY]";
        card.Description = "Deploys an energy barrier across the floor to prevent ball loss.";
        break;

    case RoguelitePerkType::InstantOvercharge:
        card.Title = "REACTOR SURGE";
        card.Category = "[ENDLESS UTILITY]";
        card.Description = "Instantly supercharges the Ultimate meter to 100% capacity.";
        break;

    case RoguelitePerkType::JackpotCredits:
        card.Title = "WAR CHEST BOUNTY";
        card.Category = "[ENDLESS UTILITY]";
        card.Description = "+250 Credits deposited directly into your Workshop vault.";
        break;

    case RoguelitePerkType::ScoreFrenzy:
        card.Title = "COMBAT MULTIPLIER";
        card.Category = "[ENDLESS UTILITY]";
        card.Description = "+25% Permanent Endless score multiplier for this entire run.";
        break;

    case RoguelitePerkType::ChronoStasis:
        card.Title = "CHRONO DILATION";
        card.Category = "[ENDLESS UTILITY]";
        card.Description = "Slows down time dilation and retards ceiling descent rate.";
        break;

    case RoguelitePerkType::OverdriveCapacitor:
        card.Title = "OVERDRIVE CAPACITOR";
        card.Category = "[TACTICAL BOOST]";
        card.Description = "+15% Ball velocity and +1 bonus damage during wave climax.";
        break;

    case RoguelitePerkType::ShieldSiphon:
        card.Title = "SHIELD SIPHON";
        card.Category = "[ENERGY SIPHON]";
        card.Description = "Breaking hardened bricks has a 20% chance to restore +5% Ultimate.";
        break;

    case RoguelitePerkType::VortexPulse:
        card.Title = "VORTEX PULSE";
        card.Category = "[ARENA DEFENSE]";
        card.Description = "Clearing brick clusters releases shockwaves that clear hazard zones.";
        break;

    default:
        card.Title = "TACTICAL PERK";
        card.Category = "[GENERAL]";
        card.Description = "Enhances active combat parameters.";
        break;
    }

    return card;
}

std::vector<RogueliteCard> RogueliteDraftManager::GenerateDraft(int count, DraftType draftType, uint32_t seed)
{
    const int totalPerks = static_cast<int>(RoguelitePerkType::Count);
    int draftCount = std::min(count, totalPerks);
    if (draftCount <= 0) return {};

    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }
    std::mt19937 rng(seed);

    // Probability weights per rarity tier:
    // PeriodicSupply (1 minute timer): Common 50%, Uncommon 35%, Rare 12%, Epic 3%, Legendary 0%
    // WaveClear (Grid destroyed):     Common  0%, Uncommon 20%, Rare 45%, Epic 25%, Legendary 10%
    const int periodicWeights[5]  = { 50, 35, 12,  3,  0 };
    const int waveClearWeights[5] = {  0, 20, 45, 25, 10 };

    const int* weights = (draftType == DraftType::WaveClear) ? waveClearWeights : periodicWeights;

    std::vector<RoguelitePerkType> chosenPerks;
    chosenPerks.reserve(draftCount);

    for (int slot = 0; slot < draftCount; ++slot)
    {
        // 1. Group remaining available perks by rarity
        std::vector<RoguelitePerkType> availableByRarity[5];
        for (int i = 0; i < totalPerks; ++i)
        {
            auto perk = static_cast<RoguelitePerkType>(i);
            if (std::find(chosenPerks.begin(), chosenPerks.end(), perk) == chosenPerks.end())
            {
                int r = static_cast<int>(GetPerkRarity(perk));
                if (r >= 0 && r < 5)
                {
                    availableByRarity[r].push_back(perk);
                }
            }
        }

        // 2. Compute effective active weights for tiers with available perks
        int activeWeights[5] = { 0, 0, 0, 0, 0 };
        int totalWeight = 0;
        for (int r = 0; r < 5; ++r)
        {
            if (!availableByRarity[r].empty())
            {
                activeWeights[r] = weights[r];
                totalWeight += weights[r];
            }
        }

        // Fallback if all weighted tiers are depleted
        if (totalWeight <= 0)
        {
            for (int r = 0; r < 5; ++r)
            {
                if (!availableByRarity[r].empty())
                {
                    activeWeights[r] = 1;
                    totalWeight += 1;
                }
            }
        }

        if (totalWeight <= 0)
        {
            break; // No more available perks
        }

        // 3. Roll rarity
        std::uniform_int_distribution<int> dist(0, totalWeight - 1);
        int roll = dist(rng);
        int chosenRarity = -1;
        int accum = 0;
        for (int r = 0; r < 5; ++r)
        {
            if (activeWeights[r] > 0)
            {
                accum += activeWeights[r];
                if (roll < accum)
                {
                    chosenRarity = r;
                    break;
                }
            }
        }

        if (chosenRarity < 0 || chosenRarity >= 5 || availableByRarity[chosenRarity].empty())
        {
            for (int r = 0; r < 5; ++r)
            {
                if (!availableByRarity[r].empty())
                {
                    chosenRarity = r;
                    break;
                }
            }
        }

        if (chosenRarity < 0 || availableByRarity[chosenRarity].empty())
        {
            break;
        }

        // 4. Uniformly pick a perk from the chosen rarity tier
        const auto& candidates = availableByRarity[chosenRarity];
        std::uniform_int_distribution<size_t> perkDist(0, candidates.size() - 1);
        RoguelitePerkType chosenPerk = candidates[perkDist(rng)];
        chosenPerks.push_back(chosenPerk);
    }

    std::vector<RogueliteCard> cards;
    cards.reserve(chosenPerks.size());
    for (auto perk : chosenPerks)
    {
        cards.push_back(GetCardInfo(perk));
    }
    return cards;
}
