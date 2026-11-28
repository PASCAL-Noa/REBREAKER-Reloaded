#include "Core/ProfileManager.h"
#include "Core/AchievementManager.h"
#include "Core/PlayerPrefs.h"
#include "Core/json.hpp"
#include "PowerUps/PowerUpConfig.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

using json = nlohmann::json;

std::string ProfileManager::s_customProfilePath;

namespace
{
    bool AtomicWriteJson(const std::filesystem::path& targetPath, const json& payload)
    {
        try
        {
            std::error_code ec;
            if (targetPath.has_parent_path())
            {
                std::filesystem::create_directories(targetPath.parent_path(), ec);
            }

            std::filesystem::path tmpPath = targetPath;
            tmpPath += ".tmp";

            std::ofstream out(tmpPath);
            if (!out.is_open())
            {
                return false;
            }
            out << payload.dump(4);
            out.close();

#if defined(_WIN32)
            if (!MoveFileExW(tmpPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::filesystem::copy_file(tmpPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(tmpPath, ec);
                return !ec;
            }
            return true;
#else
            std::filesystem::copy_file(tmpPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
            return !ec;
#endif
        }
        catch (...)
        {
            return false;
        }
    }

    int GetTotalPowerUpTiersPurchased()
    {
        int total = 0;
        const PowerUpType types[] = {
            PowerUpType::MultiBall, PowerUpType::ExpandPaddle, PowerUpType::ShrinkPaddle,
            PowerUpType::Storm, PowerUpType::TempoBall, PowerUpType::ExtraLife,
            PowerUpType::BigBall, PowerUpType::FireBall, PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier, PowerUpType::MagnetPaddle, PowerUpType::GhostBall,
            PowerUpType::ItemRegen
        };
        for (PowerUpType t : types)
        {
            total += std::max(0, PowerUpManager::Get().GetPersistedLevel(t) - 1);
        }
        return total;
    }
}

ProfileManager& ProfileManager::Get()
{
    static ProfileManager instance;
    return instance;
}

ProfileManager::ProfileManager()
{
    InitCosmetics();
}

void ProfileManager::SetCustomProfilePath(const std::string& path)
{
    s_customProfilePath = path;
}

std::string ProfileManager::GetProfileFilePath()
{
    if (!s_customProfilePath.empty())
    {
        return s_customProfilePath;
    }

    // If PlayerPrefs is redirected (e.g. unit tests), automatically isolate profile.json alongside it
    const std::string prefsPath = PlayerPrefs::GetFilePath();
    if (prefsPath != "playerprefs.json")
    {
        std::filesystem::path p(prefsPath);
        return (p.parent_path() / (p.stem().string() + "_profile.json")).string();
    }

    return "SaveData/profile.json";
}

std::string ProfileManager::GetSlotPrefsKey(CosmeticSlot slot)
{
    switch (slot)
    {
        case CosmeticSlot::PaddleSkin:     return "Equipped_PaddleSkin";
        case CosmeticSlot::BallTrail:      return "Equipped_BallTrail";
        case CosmeticSlot::SoundPack:      return "Equipped_SoundPack";
        case CosmeticSlot::PrimaryColor:   return "Equipped_PrimaryColor";
        case CosmeticSlot::SecondaryColor: return "Equipped_SecondaryColor";
        case CosmeticSlot::BallSkin:       return "Equipped_BallSkin";
        case CosmeticSlot::DeathEffect:    return "Equipped_DeathEffect";
        default:                           return "Equipped_PaddleSkin";
    }
}

std::string ProfileManager::GetDefaultCosmeticId(CosmeticSlot slot)
{
    switch (slot)
    {
        case CosmeticSlot::PaddleSkin:     return "paddle_classic_default";
        case CosmeticSlot::BallTrail:      return "trail_classic_white";
        case CosmeticSlot::SoundPack:      return "default_audio";
        case CosmeticSlot::PrimaryColor:   return "color_pri_cyan";
        case CosmeticSlot::SecondaryColor: return "color_sec_white";
        case CosmeticSlot::BallSkin:       return "ball_classic_default";
        case CosmeticSlot::DeathEffect:    return "death_classic_default";
        default:                           return "paddle_classic_default";
    }
}

void ProfileManager::InitCosmetics()
{
    m_cosmetics = {
        // =====================================================================
        // 1. PRIMARY COLOR PALETTE (16 Swatches)
        // =====================================================================
        {"color_pri_cyan",      "Cyber Cyan",     "Signature electric arcade cyan",                CosmeticSlot::PrimaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",              Color{0, 230, 255, 255},   Color{140, 250, 255, 255}},
        {"color_pri_white",     "Pure Nova",      "High-contrast luminous phosphor white",         CosmeticSlot::PrimaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",              Color{245, 250, 255, 255}, Color{190, 215, 240, 255}},
        {"color_pri_lime",      "Acid Lime",      "High-voltage neon lime green",                  CosmeticSlot::PrimaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",              Color{80, 255, 95, 255},   Color{190, 255, 160, 255}},
        {"color_pri_amber",     "Solar Amber",    "Warm arcade cabinet amber gold",                CosmeticSlot::PrimaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",              Color{255, 195, 40, 255},  Color{255, 235, 130, 255}},
        {"color_pri_crimson",   "Crimson Pulse",  "Aggressive laser-red combat finish",            CosmeticSlot::PrimaryColor, 150, false, CosmeticUnlockType::Shop,        "SHOP: 150 CREDITS",                Color{255, 55, 85, 255},   Color{255, 145, 160, 255}},
        {"color_pri_magenta",   "Hyper Magenta",  "Synthwave neon magenta overdrive",              CosmeticSlot::PrimaryColor, 150, false, CosmeticUnlockType::Shop,        "SHOP: 150 CREDITS",                Color{255, 65, 215, 255},  Color{255, 165, 240, 255}},
        {"color_pri_violet",    "Plasma Violet",  "Deep high-energy ultraviolet glow",             CosmeticSlot::PrimaryColor, 200, false, CosmeticUnlockType::Shop,        "SHOP: 200 CREDITS",                Color{165, 85, 255, 255},  Color{215, 165, 255, 255}},
        {"color_pri_cobalt",    "Deep Cobalt",    "Cool cybernetic sapphire blue",                 CosmeticSlot::PrimaryColor, 200, false, CosmeticUnlockType::Shop,        "SHOP: 200 CREDITS",                Color{45, 125, 255, 255},  Color{130, 195, 255, 255}},
        {"color_pri_emerald",   "Matrix Green",   "Digital terminal phosphor emerald",             CosmeticSlot::PrimaryColor, 250, false, CosmeticUnlockType::Shop,        "SHOP: 250 CREDITS",                Color{30, 225, 145, 255},  Color{135, 255, 205, 255}},
        {"color_pri_coral",     "Sunset Coral",   "Outrun sunset orange-coral lacquer",            CosmeticSlot::PrimaryColor, 250, false, CosmeticUnlockType::Shop,        "SHOP: 250 CREDITS",                Color{255, 115, 75, 255},  Color{255, 185, 140, 255}},
        {"color_pri_copper",    "Steam Brass",    "Burnished industrial bronze alloy",             CosmeticSlot::PrimaryColor, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                Color{215, 140, 70, 255},  Color{250, 205, 130, 255}},
        {"color_pri_rose",      "Sakura Neon",    "Soft glowing cyber-sakura rose",                CosmeticSlot::PrimaryColor, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                Color{255, 145, 200, 255}, Color{255, 210, 235, 255}},
        {"color_pri_ice",       "Glacial Ice",    "Sub-zero crystalline frost blue",               CosmeticSlot::PrimaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Clear Stage 2",       Color{155, 240, 255, 255}, Color{220, 250, 255, 255}},
        {"color_pri_obsidian",  "Shadow Alloy",   "Stealth gunmetal titanium coating",             CosmeticSlot::PrimaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Reach Wave 5 Endless",Color{85, 98, 128, 255},   Color{155, 175, 215, 255}},
        {"color_pri_gold",      "Champion Gold",  "Radiant trophy gold champion finish",           CosmeticSlot::PrimaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 15,000 Lifetime Pts", Color{255, 215, 55, 255},  Color{255, 245, 165, 255}},
        {"color_pri_overdrive", "Overdrive Sun",  "Blinding prestige solar-gold luminescence",     CosmeticSlot::PrimaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 50,000 Lifetime Pts", Color{255, 245, 120, 255}, Color{255, 255, 220, 255}},

        // =====================================================================
        // 2. SECONDARY COLOR PALETTE (16 Swatches)
        // =====================================================================
        {"color_sec_white",     "Starlight White","Crisp luminous white trim & core accents",      CosmeticSlot::SecondaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",               Color{255, 255, 255, 255}, Color{200, 220, 240, 255}},
        {"color_sec_blue",      "Electric Blue",  "High-energy azure reactor trim",                CosmeticSlot::SecondaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",               Color{60, 175, 255, 255},  Color{140, 220, 255, 255}},
        {"color_sec_gold",      "Reactor Gold",   "Rich golden metallic accents",                  CosmeticSlot::SecondaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",               Color{255, 215, 65, 255},  Color{255, 240, 160, 255}},
        {"color_sec_pink",      "Neon Pink",      "Vibrant arcade hot-pink trim",                  CosmeticSlot::SecondaryColor, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",               Color{255, 80, 185, 255},  Color{255, 170, 225, 255}},
        {"color_sec_cyan",      "Laser Cyan",     "Pure cyan optical edge lighting",               CosmeticSlot::SecondaryColor, 150, false, CosmeticUnlockType::Shop,        "SHOP: 150 CREDITS",                 Color{0, 255, 240, 255},   Color{160, 255, 250, 255}},
        {"color_sec_orange",    "Blaze Orange",   "Incandescent thruster-orange highlights",       CosmeticSlot::SecondaryColor, 150, false, CosmeticUnlockType::Shop,        "SHOP: 150 CREDITS",                 Color{255, 130, 35, 255},  Color{255, 195, 120, 255}},
        {"color_sec_green",     "Toxic Mint",     "Radioactive mint-green energy veins",           CosmeticSlot::SecondaryColor, 200, false, CosmeticUnlockType::Shop,        "SHOP: 200 CREDITS",                 Color{75, 255, 150, 255},  Color{170, 255, 210, 255}},
        {"color_sec_red",       "Danger Red",     "Warning-beacon crimson accent core",            CosmeticSlot::SecondaryColor, 200, false, CosmeticUnlockType::Shop,        "SHOP: 200 CREDITS",                 Color{255, 45, 65, 255},   Color{255, 135, 145, 255}},
        {"color_sec_purple",    "Void Purple",    "Hyper-dimensional violet glow trim",            CosmeticSlot::SecondaryColor, 250, false, CosmeticUnlockType::Shop,        "SHOP: 250 CREDITS",                 Color{190, 75, 255, 255},  Color{225, 160, 255, 255}},
        {"color_sec_yellow",    "Volt Yellow",    "High-voltage arc yellow highlights",            CosmeticSlot::SecondaryColor, 250, false, CosmeticUnlockType::Shop,        "SHOP: 250 CREDITS",                 Color{255, 250, 95, 255},  Color{255, 255, 190, 255}},
        {"color_sec_bronze",    "Clockwork Copper","Riveted copper steampunk piping trim",         CosmeticSlot::SecondaryColor, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                 Color{205, 135, 65, 255},  Color{245, 190, 125, 255}},
        {"color_sec_indigo",    "Deep Indigo",    "Cosmic midnight indigo accents",                CosmeticSlot::SecondaryColor, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                 Color{95, 105, 255, 255},  Color{165, 175, 255, 255}},
        {"color_sec_teal",      "Abyss Teal",     "Deep-grid turquoise core trim",                 CosmeticSlot::SecondaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Clear Stage 1",        Color{25, 205, 205, 255},  Color{135, 245, 245, 255}},
        {"color_sec_silver",    "Chrome Silver",  "Mirror-polished titanium edge finish",          CosmeticSlot::SecondaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 3 Endless Runs",       Color{205, 220, 235, 255}, Color{240, 248, 255, 255}},
        {"color_sec_crimson",   "Core Ruby",      "Reactor-grade ruby crystal glow",               CosmeticSlot::SecondaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Clear Stage 3",        Color{235, 35, 105, 255},  Color{255, 130, 175, 255}},
        {"color_sec_aurora",    "Aurora Mint",    "Celestial aurora borealis shimmer",             CosmeticSlot::SecondaryColor, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 1,000 Lifetime CR",    Color{140, 255, 220, 255}, Color{215, 255, 245, 255}},

        // =====================================================================
        // 3. PADDLE SKINS (8 Distinct Archetypes — Original Default + 7 Custom)
        // =====================================================================
        {"paddle_classic_default","Classic Default", "Original minimalist REBREAKER arcade paddle sprite tinted by your Primary Color.", CosmeticSlot::PaddleSkin, 0, true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{245, 250, 255, 255}, Color{200, 220, 240, 255}},
        {"paddle_neon_cyan",      "Cyber MK-II",     "Sleek retro-futuristic cyber hull with twin thruster caps & reactor core.",         CosmeticSlot::PaddleSkin, 0, true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{0, 230, 255, 255},   Color{255, 255, 255, 255}},
        {"paddle_neon_pulse",     "Neon Tube",       "Hollow pulsing neon-gas frame with luminous plasma conduit & double halo.",         CosmeticSlot::PaddleSkin, 300, false, CosmeticUnlockType::Shop,      "SHOP: 300 CREDITS",                    Color{255, 70, 210, 255},  Color{0, 255, 240, 255}},
        {"paddle_retro_wood",     "Retro Wood",      "Classic 1980s woodgrain CRT cabinet finish with brass rivets & glowing inlay.",     CosmeticSlot::PaddleSkin, 350, false, CosmeticUnlockType::Shop,      "SHOP: 350 CREDITS",                    Color{165, 105, 55, 255},  Color{235, 185, 95, 255}},
        {"paddle_gold_plating",   "Golden Sovereign","Gleaming polished gold champion chassis with animated specular glint sweep.",       CosmeticSlot::PaddleSkin, 500, false, CosmeticUnlockType::Shop,      "SHOP: 500 CREDITS",                    Color{255, 215, 55, 255},  Color{255, 248, 195, 255}},
        {"paddle_steampunk",      "Steampunk Brass", "Riveted copper boiler chassis with dual spinning brass gears & steam gauge.",       CosmeticSlot::PaddleSkin, 650, false, CosmeticUnlockType::Shop,      "SHOP: 650 CREDITS",                    Color{210, 135, 65, 255},  Color{255, 210, 110, 255}},
        {"paddle_obsidian",       "Void Obsidian",   "Matte stealth frame forged from OMEGA-01's armor with pulsing void vents.",         CosmeticSlot::PaddleSkin, 0,   false, CosmeticUnlockType::Achievement, "STORY: Defeat OMEGA-01 (Chapter 3)", Color{95, 75, 145, 255},   Color{210, 95, 255, 255}},
        {"paddle_prism_wing",     "Prism Striker",   "Swept crystal chevron chassis with shifting prismatic energy blades.",              CosmeticSlot::PaddleSkin, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Reach Wave 10 Endless", Color{120, 245, 255, 255}, Color{255, 120, 220, 255}},

        // =====================================================================
        // 4. BALL SKINS (8 Distinct Archetypes — Original Default + 7 Custom)
        // =====================================================================
        {"ball_classic_default",  "Classic Default", "Original minimalist REBREAKER arcade ball sprite tinted by your Primary Color.",    CosmeticSlot::BallSkin, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{245, 250, 255, 255}, Color{200, 220, 240, 255}},
        {"ball_classic_orb",      "Classic Core",    "Pure high-gloss arcade sphere with crisp rim highlight and energy sheen.",          CosmeticSlot::BallSkin, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{0, 230, 255, 255},   Color{255, 255, 255, 255}},
        {"ball_cyber_ring",       "Cyber Gyro",      "Counter-rotating holographic reticle rings locking onto a plasma nucleus.",         CosmeticSlot::BallSkin, 250, false, CosmeticUnlockType::Shop,        "SHOP: 250 CREDITS",                    Color{0, 240, 255, 255},   Color{90, 180, 255, 255}},
        {"ball_retro_pixel",      "8-Bit Pixel",     "Chunky retro arcade square-pixel orb with crisp bevelled pixel shading.",           CosmeticSlot::BallSkin, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                    Color{90, 255, 110, 255},  Color{255, 255, 255, 255}},
        {"ball_plasma_star",      "Nova Star",       "Spinning four-point starflare nested inside a radiant energy shell.",               CosmeticSlot::BallSkin, 400, false, CosmeticUnlockType::Shop,        "SHOP: 400 CREDITS",                    Color{255, 200, 50, 255},  Color{255, 250, 190, 255}},
        {"ball_sawblade",         "Neon Sawblade",   "Geometry-inspired spinning serrated energy disc with glowing center hub.",         CosmeticSlot::BallSkin, 450, false, CosmeticUnlockType::Shop,        "SHOP: 450 CREDITS",                    Color{255, 65, 145, 255},  Color{255, 255, 255, 255}},
        {"ball_steampunk_gear",   "Clockwork Gear",  "Spinning industrial brass cogwheel orb with riveted pressure core.",                CosmeticSlot::BallSkin, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Buy 5 Workshop Upgrades", Color{220, 150, 70, 255},  Color{255, 225, 135, 255}},
        {"ball_void_eye",         "Omega Core",      "Dark singularity sphere ringed by orbiting sparks and a piercing iris.",            CosmeticSlot::BallSkin, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 25,000 Lifetime Pts",     Color{175, 70, 255, 255},  Color{255, 65, 120, 255}},

        // =====================================================================
        // 5. PARTICLE TRAILS (7 Styles — Original Classic White + 6 Custom)
        // =====================================================================
        {"trail_classic_white",   "Classic White",   "Original pure-white neon motion ribbon from classic REBREAKER.",                    CosmeticSlot::BallTrail, 0,    true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{255, 255, 255, 255}, Color{200, 220, 240, 255}},
        {"trail_plasma_blue",     "Energy Ribbon",   "Smooth tapered additive light ribbon tinted with your Primary & Secondary.",        CosmeticSlot::BallTrail, 0,    true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{0, 225, 255, 255},   Color{80, 150, 255, 255}},
        {"trail_stardust",        "Stardust Glitter","Twinkling cosmic diamond sparkles & shimmering dust trailing in your wake.",        CosmeticSlot::BallTrail, 350,  false, CosmeticUnlockType::Shop,        "SHOP: 350 CREDITS",                    Color{255, 235, 120, 255}, Color{150, 230, 255, 255}},
        {"trail_matrix_binary",   "Matrix Binary",   "Digital cyber-stream emitting glowing 0/1 binary glyphs & data fragments.",         CosmeticSlot::BallTrail, 550,  false, CosmeticUnlockType::Shop,        "SHOP: 550 CREDITS",                    Color{45, 255, 130, 255},  Color{170, 255, 200, 255}},
        {"trail_inferno_sparks",  "Flame Comet",     "Blazing incandescent fire ribbon with dancing ember sparks trailing behind.",       CosmeticSlot::BallTrail, 600,  false, CosmeticUnlockType::Shop,        "SHOP: 600 CREDITS",                    Color{255, 125, 25, 255},  Color{255, 220, 75, 255}},
        {"trail_rainbow_warp",    "Rainbow Prism",   "Chromatic spectral light ribbon cycling dynamically across the HSV prism.",         CosmeticSlot::BallTrail, 1200, false, CosmeticUnlockType::Shop,        "SHOP: 1,200 CREDITS",                  Color{255, 70, 175, 255},  Color{70, 230, 255, 255}},
        {"trail_void_lightning",  "Arc Pulse",       "High-voltage crackling jagged lightning segments trailing every bounce.",           CosmeticSlot::BallTrail, 0,    false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: Reach Wave 8 Endless",    Color{140, 220, 255, 255}, Color{220, 120, 255, 255}},

        // =====================================================================
        // 6. DEATH / EXPLOSION EFFECTS (7 Distinct Visual Bursts — Classic Minimal + 6 Custom)
        // =====================================================================
        {"death_classic_default", "Classic Minimal", "Clean minimalist arcade square sparks matching the original impact particles.",     CosmeticSlot::DeathEffect, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{245, 250, 255, 255}, Color{200, 220, 240, 255}},
        {"death_classic_burst",   "Arcade Burst",    "Crisp radial shower of glowing neon sparks in your equipped palette.",              CosmeticSlot::DeathEffect, 0,   true,  CosmeticUnlockType::Default,     "UNLOCKED BY DEFAULT",                  Color{0, 230, 255, 255},   Color{255, 255, 255, 255}},
        {"death_shockwave_ring",  "Sonic Shockwave", "Expanding twin kinetic shockwave rings accompanied by directional shards.",         CosmeticSlot::DeathEffect, 300, false, CosmeticUnlockType::Shop,        "SHOP: 300 CREDITS",                    Color{75, 235, 255, 255},  Color{255, 215, 80, 255}},
        {"death_pixel_shatter",   "Pixel Shatter",   "Retro 8-bit chunky pixel blocks tumbling outward with arcade gravity.",             CosmeticSlot::DeathEffect, 400, false, CosmeticUnlockType::Shop,        "SHOP: 400 CREDITS",                    Color{90, 255, 115, 255},  Color{255, 195, 60, 255}},
        {"death_confetti_prism",  "Prism Confetti",  "Celebratory multi-color geometric confetti diamonds & spinning streamers.",         CosmeticSlot::DeathEffect, 450, false, CosmeticUnlockType::Shop,        "SHOP: 450 CREDITS",                    Color{255, 85, 195, 255},  Color{85, 240, 255, 255}},
        {"death_nova_implosion",  "Supernova Cross", "Radiant four-point star flare beams & corona flash with hot embers.",               CosmeticSlot::DeathEffect, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 20,000 Lifetime Pts",     Color{255, 225, 85, 255},  Color{255, 110, 50, 255}},
        {"death_matrix_glitch",   "System Glitch",   "Horizontal chromatic glitch scan-bars shattering into binary 0/1 debris.",          CosmeticSlot::DeathEffect, 0,   false, CosmeticUnlockType::Achievement, "ACHIEVEMENT: 5 Endless Runs",          Color{45, 255, 145, 255},  Color{255, 65, 195, 255}}
    };

    // Load unlocked states from PlayerPrefs
    for (auto& item : m_cosmetics)
    {
        if (item.UnlockType == CosmeticUnlockType::Default)
        {
            item.Unlocked = true;
        }
        else
        {
            item.Unlocked = PlayerPrefs::GetBool("Cosmetic_Unlocked_" + item.Id, false);
        }
    }
}

void ProfileManager::LoadProfile()
{
    const std::filesystem::path profilePath(GetProfileFilePath());
    std::error_code ec;
    if (!std::filesystem::exists(profilePath, ec))
    {
        std::filesystem::path tmpPath = profilePath;
        tmpPath += ".tmp";
        if (std::filesystem::exists(tmpPath, ec))
        {
            std::filesystem::copy_file(tmpPath, profilePath, std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    std::ifstream in(profilePath);
    if (!in.is_open())
    {
        // Sync unlocked states from PlayerPrefs and bootstrap profile.json
        for (auto& item : m_cosmetics)
        {
            item.Unlocked = (item.UnlockType == CosmeticUnlockType::Default)
                ? true
                : PlayerPrefs::GetBool("Cosmetic_Unlocked_" + item.Id, item.Unlocked);
        }
        EvaluateAchievementUnlocks();
        SaveProfile();
        return;
    }

    try
    {
        json data;
        in >> data;
        in.close();

        if (!data.is_object())
        {
            return;
        }

        if (data.contains("username") && data["username"].is_string())
        {
            std::string u = data["username"].get<std::string>();
            if (!u.empty()) PlayerPrefs::SetString("PlayerProfile_Username", u);
        }
        if (data.contains("lifetimeScore") && data["lifetimeScore"].is_number_integer())
        {
            PlayerPrefs::SetInt("Economy_LifetimeScore", std::max(0, data["lifetimeScore"].get<int>()));
        }
        if (data.contains("lifetimeCredits") && data["lifetimeCredits"].is_number_integer())
        {
            PlayerPrefs::SetInt("Economy_LifetimeCredits", std::max(0, data["lifetimeCredits"].get<int>()));
        }
        if (data.contains("equippedUltimate") && data["equippedUltimate"].is_number_integer())
        {
            PlayerPrefs::SetInt("Profile_EquippedUltimate", data["equippedUltimate"].get<int>());
        }
        if (data.contains("ultimateLevels") && data["ultimateLevels"].is_array())
        {
            const auto& arr = data["ultimateLevels"];
            for (size_t i = 0; i < arr.size() && i < static_cast<size_t>(UltimateAbilityType::Count); ++i)
            {
                if (arr[i].is_number_integer())
                {
                    PlayerPrefs::SetInt("Profile_UltimateLevel_" + std::to_string(i), std::clamp(arr[i].get<int>(), 1, 5));
                }
            }
        }
        if (data.contains("endlessStats") && data["endlessStats"].is_object())
        {
            const auto& es = data["endlessStats"];
            if (es.contains("highScore") && es["highScore"].is_number_integer())
                PlayerPrefs::SetInt("Endless_HighScore", std::max(0, es["highScore"].get<int>()));
            if (es.contains("bestTime") && es["bestTime"].is_number())
                PlayerPrefs::SetFloat("Endless_BestTime", std::max(0.0f, es["bestTime"].get<float>()));
            if (es.contains("bestWaves") && es["bestWaves"].is_number_integer())
                PlayerPrefs::SetInt("Endless_BestWaves", std::max(0, es["bestWaves"].get<int>()));
            if (es.contains("totalRuns") && es["totalRuns"].is_number_integer())
                PlayerPrefs::SetInt("Endless_TotalRuns", std::max(0, es["totalRuns"].get<int>()));
            if (es.contains("totalWaves") && es["totalWaves"].is_number_integer())
                PlayerPrefs::SetInt("Endless_TotalWaves", std::max(0, es["totalWaves"].get<int>()));
        }
        if (data.contains("endlessPerks") && data["endlessPerks"].is_array())
        {
            const auto& ep = data["endlessPerks"];
            for (size_t i = 0; i < ep.size() && i < static_cast<size_t>(EndlessPerkType::Count); ++i)
            {
                if (ep[i].is_number_integer())
                {
                    PlayerPrefs::SetInt("Endless_PerkLevel_" + std::to_string(i), std::max(0, ep[i].get<int>()));
                }
            }
        }

        // Reset non-default unlock cache before reading unlockedIds
        for (auto& item : m_cosmetics)
        {
            item.Unlocked = (item.UnlockType == CosmeticUnlockType::Default);
        }

        if (data.contains("cosmetics") && data["cosmetics"].is_object())
        {
            const auto& cos = data["cosmetics"];
            if (cos.contains("unlockedIds") && cos["unlockedIds"].is_array())
            {
                for (const auto& entry : cos["unlockedIds"])
                {
                    if (!entry.is_string()) continue;
                    const std::string id = entry.get<std::string>();
                    for (auto& item : m_cosmetics)
                    {
                        if (item.Id == id)
                        {
                            item.Unlocked = true;
                            PlayerPrefs::SetBool("Cosmetic_Unlocked_" + id, true);
                            break;
                        }
                    }
                }
            }

            auto loadEquipped = [&](const char* jsonKey, CosmeticSlot slot) {
                if (cos.contains(jsonKey) && cos[jsonKey].is_string())
                {
                    const std::string id = cos[jsonKey].get<std::string>();
                    if (IsCosmeticUnlocked(id))
                    {
                        PlayerPrefs::SetString(GetSlotPrefsKey(slot), id);
                    }
                }
            };

            loadEquipped("primaryColor",   CosmeticSlot::PrimaryColor);
            loadEquipped("secondaryColor", CosmeticSlot::SecondaryColor);
            loadEquipped("paddleSkin",     CosmeticSlot::PaddleSkin);
            loadEquipped("ballSkin",       CosmeticSlot::BallSkin);
            loadEquipped("ballTrail",      CosmeticSlot::BallTrail);
            loadEquipped("deathEffect",    CosmeticSlot::DeathEffect);
            loadEquipped("soundPack",      CosmeticSlot::SoundPack);
        }

        // Also respect any unlocks previously saved in PlayerPrefs
        for (auto& item : m_cosmetics)
        {
            if (PlayerPrefs::GetBool("Cosmetic_Unlocked_" + item.Id, false))
            {
                item.Unlocked = true;
            }
        }

        if (data.contains("achievements") && data["achievements"].is_object())
        {
            AchievementManager::Get().DeserializeFromJson(data["achievements"].dump());
        }

        EvaluateAchievementUnlocks();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ProfileManager] Failed to parse profile.json: " << e.what() << std::endl;
    }
}

void ProfileManager::SaveProfile() const
{
    json data = json::object();
    data["version"] = 3;
    data["username"] = GetUsername();
    data["lifetimeScore"] = GetLifetimeScore();
    data["lifetimeCredits"] = GetLifetimeCredits();
    data["equippedUltimate"] = static_cast<int>(GetEquippedUltimate());

    json ultLevels = json::array();
    for (int i = 0; i < static_cast<int>(UltimateAbilityType::Count); ++i)
    {
        ultLevels.push_back(GetUltimateLevel(static_cast<UltimateAbilityType>(i)));
    }
    data["ultimateLevels"] = ultLevels;

    data["endlessStats"] = {
        {"highScore",  GetEndlessHighScore()},
        {"bestTime",   GetEndlessBestTime()},
        {"bestWaves",  GetEndlessBestWaves()},
        {"totalRuns",  GetEndlessTotalRuns()},
        {"totalWaves", GetEndlessTotalWaves()}
    };

    json perkLevels = json::array();
    for (int i = 0; i < static_cast<int>(EndlessPerkType::Count); ++i)
    {
        perkLevels.push_back(GetEndlessPerkLevel(static_cast<EndlessPerkType>(i)));
    }
    data["endlessPerks"] = perkLevels;

    json unlockedIds = json::array();
    for (const auto& item : m_cosmetics)
    {
        if (item.Unlocked)
        {
            unlockedIds.push_back(item.Id);
        }
    }

    data["cosmetics"] = {
        {"primaryColor",   GetEquippedCosmetic(CosmeticSlot::PrimaryColor)},
        {"secondaryColor", GetEquippedCosmetic(CosmeticSlot::SecondaryColor)},
        {"paddleSkin",     GetEquippedCosmetic(CosmeticSlot::PaddleSkin)},
        {"ballSkin",       GetEquippedCosmetic(CosmeticSlot::BallSkin)},
        {"ballTrail",      GetEquippedCosmetic(CosmeticSlot::BallTrail)},
        {"deathEffect",    GetEquippedCosmetic(CosmeticSlot::DeathEffect)},
        {"soundPack",      GetEquippedCosmetic(CosmeticSlot::SoundPack)},
        {"unlockedIds",    unlockedIds}
    };

    try
    {
        data["achievements"] = json::parse(AchievementManager::Get().SerializeToJson());
    }
    catch (...)
    {
        data["achievements"] = json::object();
    }

    AtomicWriteJson(std::filesystem::path(GetProfileFilePath()), data);
}

std::string ProfileManager::GetUsername() const
{
    std::string name = PlayerPrefs::GetString("PlayerProfile_Username", "PLAYER");
    if (name.empty()) name = "PLAYER";
    return name;
}

void ProfileManager::SetUsername(const std::string& name)
{
    std::string trimmed = name;
    const size_t first = trimmed.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
    {
        trimmed.clear();
    }
    else
    {
        trimmed.erase(0, first);
        const size_t last = trimmed.find_last_not_of(" \t\n\r");
        if (last != std::string::npos)
        {
            trimmed.erase(last + 1);
        }
    }

    if (trimmed.empty())
    {
        trimmed = "PLAYER";
    }
    if (trimmed.length() > 16)
    {
        trimmed = trimmed.substr(0, 16);
    }

    PlayerPrefs::SetString("PlayerProfile_Username", trimmed);
    PlayerPrefs::Save();
    SaveProfile();
}

uint32_t ProfileManager::ConvertScoreToCredits(uint32_t score)
{
    if (score == 0) return 0;
    uint32_t credits = score / 50;
    return (credits == 0) ? 1 : credits;
}

uint32_t ProfileManager::GetLifetimeScore() const
{
    return static_cast<uint32_t>(std::max(0, PlayerPrefs::GetInt("Economy_LifetimeScore", 0)));
}

void ProfileManager::AddLifetimeScore(uint32_t score)
{
    uint32_t current = GetLifetimeScore();
    PlayerPrefs::SetInt("Economy_LifetimeScore", static_cast<int>(current + score));
    EvaluateAchievementUnlocks();
    PlayerPrefs::Save();
    SaveProfile();
}

uint32_t ProfileManager::GetLifetimeCredits() const
{
    return static_cast<uint32_t>(std::max(0, PlayerPrefs::GetInt("Economy_LifetimeCredits", 0)));
}

void ProfileManager::AddLifetimeCredits(uint32_t credits)
{
    uint32_t current = GetLifetimeCredits();
    PlayerPrefs::SetInt("Economy_LifetimeCredits", static_cast<int>(current + credits));
    EvaluateAchievementUnlocks();
    PlayerPrefs::Save();
    SaveProfile();
}

UltimateAbilityType ProfileManager::GetEquippedUltimate() const
{
    int val = PlayerPrefs::GetInt("Profile_EquippedUltimate", 0);
    if (val < 0 || val >= static_cast<int>(UltimateAbilityType::Count))
    {
        val = 0;
    }
    return static_cast<UltimateAbilityType>(val);
}

void ProfileManager::SetEquippedUltimate(UltimateAbilityType type)
{
    int val = static_cast<int>(type);
    if (val < 0 || val >= static_cast<int>(UltimateAbilityType::Count))
    {
        val = 0;
    }
    PlayerPrefs::SetInt("Profile_EquippedUltimate", val);
    PlayerPrefs::Save();
    SaveProfile();
}

int ProfileManager::GetUltimateLevel(UltimateAbilityType type) const
{
    std::string key = "Profile_UltimateLevel_" + std::to_string(static_cast<int>(type));
    int lvl = PlayerPrefs::GetInt(key, 1);
    return std::clamp(lvl, 1, 5);
}

void ProfileManager::SetUltimateLevel(UltimateAbilityType type, int level)
{
    std::string key = "Profile_UltimateLevel_" + std::to_string(static_cast<int>(type));
    PlayerPrefs::SetInt(key, std::clamp(level, 1, 5));
    PlayerPrefs::Save();
    SaveProfile();
}

int ProfileManager::GetUltimateMaxLevel(UltimateAbilityType /*type*/) const
{
    return 5;
}

int ProfileManager::GetUltimateUpgradeCost(UltimateAbilityType type) const
{
    int lvl = GetUltimateLevel(type);
    if (lvl >= 5) return 0;
    constexpr int kCosts[4] = { 250, 400, 650, 950 };
    return kCosts[std::clamp(lvl - 1, 0, 3)];
}

bool ProfileManager::CanUpgradeUltimate(UltimateAbilityType type) const
{
    int lvl = GetUltimateLevel(type);
    if (lvl >= 5) return false;
    return PowerUpManager::Get().GetCoins() >= GetUltimateUpgradeCost(type);
}

bool ProfileManager::UpgradeUltimate(UltimateAbilityType type)
{
    if (!CanUpgradeUltimate(type)) return false;
    int cost = GetUltimateUpgradeCost(type);
    if (PowerUpManager::Get().SpendCoins(cost))
    {
        SetUltimateLevel(type, GetUltimateLevel(type) + 1);
        return true;
    }
    return false;
}

std::string ProfileManager::GetUltimateName(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "PLASMA ORB";
        case UltimateAbilityType::DiscoBall:
            return "DISCO BALL";
        case UltimateAbilityType::PowerUpRain:
            return "POWER-UP RAIN";
        default:
            return "PLASMA ORB";
    }
}

std::string ProfileManager::GetUltimateCategory(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "[DESTRUCTION / CHAIN REACTION]";
        case UltimateAbilityType::DiscoBall:
            return "[ACOUSTIC SHOCKWAVE / LIGHT SHOW]";
        case UltimateAbilityType::PowerUpRain:
            return "[SUPPORT / ORBITAL DROP]";
        default:
            return "[ULTIMATE ABILITY]";
    }
}

std::string ProfileManager::GetUltimateDescription(UltimateAbilityType type)
{
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            return "Launches high-energy blue-violet plasma orbs that vaporize bricks on contact and split into cascading clusters of explosive child orbs upon detonation.";
        case UltimateAbilityType::DiscoBall:
            return "Ignites disco fever: devastating acoustic shockwaves from paddle, cycling spotlights, dancing rainbow big balls and a spinning mirror ball climax.";
        case UltimateAbilityType::PowerUpRain:
            return "Summons a torrential orbital drop of upgraded power-up capsules raining down across the entire playfield.";
        default:
            return "";
    }
}

std::string ProfileManager::GetUltimateTierInfo(UltimateAbilityType type, int level)
{
    int safeLvl = std::clamp(level, 1, 5);
    switch (type)
    {
        case UltimateAbilityType::PlasmaOrb:
            switch (safeLvl)
            {
                case 1: return "1 plasma orb (2 dmg), splits into 2 small explosive orbs";
                case 2: return "2 plasma orbs (2 dmg), each splits into 2 small explosive orbs";
                case 3: return "2 enlarged plasma orbs (3 dmg), splits into 4 then 2 orbs";
                case 4: return "3 enlarged plasma orbs (3 dmg), splits into 4 then 2 orbs";
                case 5: return "3 mega plasma orbs (4 dmg), splits into 8 then 4 then 2";
                default: break;
            }
            break;
        case UltimateAbilityType::DiscoBall:
            switch (safeLvl)
            {
                case 1: return "Shockwave 30% height (1 dmg), 2 dancing rainbow big balls & funk beat";
                case 2: return "Shockwave 40% height (1 dmg), 3 dancing rainbow big balls";
                case 3: return "Shockwave 50% height (2 dmg), 4 dancing rainbow big balls";
                case 4: return "Shockwave 60% height (2 dmg), 5 dancing rainbow big balls";
                case 5: return "2 shockwaves up to 70% height (2 dmg), 6 big balls & Mirror Ball unleash (5s)";
                default: break;
            }
            break;
        case UltimateAbilityType::PowerUpRain:
            switch (safeLvl)
            {
                case 1: return "Cascade of 5 to 12 power-ups (levels 1 to 3)";
                case 2: return "Cascade of 10 to 24 power-ups (levels 1 to 3)";
                case 3: return "Cascade of 15 to 36 power-ups (levels 1 to 4)";
                case 4: return "Cascade of 20 to 48 power-ups (levels 2 to 5)";
                case 5: return "Cascade of 25 to 60 power-ups (levels 2 to 5)";
                default: break;
            }
            break;
        default:
            break;
    }
    return "Standard Tier";
}

const std::vector<CosmeticItem>& ProfileManager::GetAvailableCosmetics()
{
    return m_cosmetics;
}

std::vector<CosmeticItem> ProfileManager::GetCosmeticsBySlot(CosmeticSlot slot) const
{
    std::vector<CosmeticItem> result;
    for (const auto& item : m_cosmetics)
    {
        if (item.Slot == slot)
        {
            result.push_back(item);
        }
    }
    return result;
}

const CosmeticItem* ProfileManager::GetCosmeticById(const std::string& id) const
{
    for (const auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            return &item;
        }
    }
    return nullptr;
}

bool ProfileManager::IsCosmeticUnlocked(const std::string& id) const
{
    for (const auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            return item.Unlocked;
        }
    }
    return false;
}

bool ProfileManager::UnlockCosmetic(const std::string& id)
{
    for (auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            if (item.Unlocked) return true;
            if (item.UnlockType == CosmeticUnlockType::Achievement)
            {
                return false;
            }
            if (PowerUpManager::Get().SpendCoins(item.Price))
            {
                item.Unlocked = true;
                PlayerPrefs::SetBool("Cosmetic_Unlocked_" + id, true);
                PlayerPrefs::Save();
                SaveProfile();
                return true;
            }
            return false;
        }
    }
    return false;
}

void ProfileManager::GrantCosmetic(const std::string& id)
{
    for (auto& item : m_cosmetics)
    {
        if (item.Id == id)
        {
            item.Unlocked = true;
            PlayerPrefs::SetBool("Cosmetic_Unlocked_" + id, true);
            PlayerPrefs::Save();
            SaveProfile();
            return;
        }
    }
}

std::string ProfileManager::GetEquippedCosmetic(CosmeticSlot slot) const
{
    const std::string key = GetSlotPrefsKey(slot);
    const std::string fallback = GetDefaultCosmeticId(slot);
    const std::string stored = PlayerPrefs::GetString(key, fallback);
    if (stored.empty()) return fallback;
    if (slot == CosmeticSlot::SoundPack) return stored;
    if (GetCosmeticById(stored) == nullptr) return fallback;
    return stored;
}

void ProfileManager::EquipCosmetic(CosmeticSlot slot, const std::string& id)
{
    if (slot != CosmeticSlot::SoundPack)
    {
        const CosmeticItem* item = GetCosmeticById(id);
        if (!item || item->Slot != slot || !item->Unlocked) return;
    }
    PlayerPrefs::SetString(GetSlotPrefsKey(slot), id);
    PlayerPrefs::Save();
    SaveProfile();
}

int ProfileManager::GetUnlockedCosmeticCount(CosmeticSlot slot) const
{
    int count = 0;
    for (const auto& item : m_cosmetics)
    {
        if (item.Slot == slot && item.Unlocked) ++count;
    }
    return count;
}

int ProfileManager::GetTotalCosmeticCount(CosmeticSlot slot) const
{
    int count = 0;
    for (const auto& item : m_cosmetics)
    {
        if (item.Slot == slot) ++count;
    }
    return count;
}

int ProfileManager::GetTotalUnlockedCosmetics() const
{
    int count = 0;
    for (const auto& item : m_cosmetics)
    {
        if (item.Unlocked) ++count;
    }
    return count;
}

int ProfileManager::GetTotalCosmetics() const
{
    return static_cast<int>(m_cosmetics.size());
}

CosmeticMilestoneProgress ProfileManager::GetCosmeticMilestoneProgress(const std::string& id) const
{
    const int highestCleared = std::max(0, PlayerPrefs::GetInt("HighestClearedLevel", 0));
    const int lifetimeScore  = static_cast<int>(GetLifetimeScore());
    const int lifetimeCreds  = static_cast<int>(GetLifetimeCredits());
    const int endlessWaves   = GetEndlessBestWaves();
    const int endlessRuns    = GetEndlessTotalRuns();
    const int storyChapter   = std::max(0, PlayerPrefs::GetInt("Story_CompletedChapter", 0));
    const int upgradesBought = GetTotalPowerUpTiersPurchased();

    if (id == "color_pri_ice")       return {highestCleared, 2, "STAGES"};
    if (id == "color_pri_obsidian")  return {endlessWaves, 5, "WAVES"};
    if (id == "color_pri_gold")      return {lifetimeScore, 15000, "PTS"};
    if (id == "color_pri_overdrive") return {lifetimeScore, 50000, "PTS"};

    if (id == "color_sec_teal")      return {highestCleared, 1, "STAGE"};
    if (id == "color_sec_silver")    return {endlessRuns, 3, "RUNS"};
    if (id == "color_sec_crimson")   return {highestCleared, 3, "STAGES"};
    if (id == "color_sec_aurora")    return {lifetimeCreds, 1000, "CR"};

    if (id == "paddle_obsidian")     return {IsCosmeticUnlocked("paddle_obsidian") ? 1 : (storyChapter >= 3 ? 1 : 0), 1, "BOSS"};
    if (id == "paddle_prism_wing")   return {endlessWaves, 10, "WAVES"};

    if (id == "ball_steampunk_gear") return {upgradesBought, 5, "UPGRADES"};
    if (id == "ball_void_eye")       return {lifetimeScore, 25000, "PTS"};

    if (id == "trail_void_lightning")return {endlessWaves, 8, "WAVES"};

    if (id == "death_nova_implosion")return {lifetimeScore, 20000, "PTS"};
    if (id == "death_matrix_glitch") return {endlessRuns, 5, "RUNS"};

    return {IsCosmeticUnlocked(id) ? 1 : 0, 1, ""};
}

int ProfileManager::EvaluateAchievementUnlocks()
{
    int newlyUnlocked = 0;
    for (auto& item : m_cosmetics)
    {
        if (item.UnlockType != CosmeticUnlockType::Achievement || item.Unlocked)
        {
            continue;
        }
        // Story boss skin is granted explicitly via StoryScene::GrantCosmetic, unless already completed
        if (item.Id == "paddle_obsidian")
        {
            if (PlayerPrefs::GetInt("Story_CompletedChapter", 0) >= 3)
            {
                item.Unlocked = true;
                PlayerPrefs::SetBool("Cosmetic_Unlocked_" + item.Id, true);
                ++newlyUnlocked;
            }
            continue;
        }

        const CosmeticMilestoneProgress prog = GetCosmeticMilestoneProgress(item.Id);
        if (prog.IsAchieved())
        {
            item.Unlocked = true;
            PlayerPrefs::SetBool("Cosmetic_Unlocked_" + item.Id, true);
            ++newlyUnlocked;
        }
    }

    if (newlyUnlocked > 0)
    {
        PlayerPrefs::Save();
        SaveProfile();
    }
    return newlyUnlocked;
}

Color ProfileManager::ResolveColorById(const std::string& id, Color fallback) const
{
    if (const CosmeticItem* item = GetCosmeticById(id))
    {
        return item->SwatchColor;
    }
    return fallback;
}

Color ProfileManager::GetEquippedPrimaryColor() const
{
    return ResolveColorById(GetEquippedCosmetic(CosmeticSlot::PrimaryColor), Color{0, 230, 255, 255});
}

Color ProfileManager::GetEquippedSecondaryColor() const
{
    return ResolveColorById(GetEquippedCosmetic(CosmeticSlot::SecondaryColor), Colors::White);
}

PaddleSkinStyle ProfileManager::ResolvePaddleSkinStyle(const std::string& id)
{
    if (id == "paddle_classic_default") return PaddleSkinStyle::ClassicSprite;
    if (id == "paddle_neon_cyan")       return PaddleSkinStyle::Cyber;
    if (id == "paddle_neon_pulse")      return PaddleSkinStyle::Neon;
    if (id == "paddle_retro_wood")      return PaddleSkinStyle::RetroWood;
    if (id == "paddle_gold_plating")    return PaddleSkinStyle::Golden;
    if (id == "paddle_steampunk")       return PaddleSkinStyle::Steampunk;
    if (id == "paddle_obsidian")        return PaddleSkinStyle::VoidObsidian;
    if (id == "paddle_prism_wing")      return PaddleSkinStyle::PrismStriker;
    return PaddleSkinStyle::ClassicSprite;
}

BallSkinStyle ProfileManager::ResolveBallSkinStyle(const std::string& id)
{
    if (id == "ball_classic_default") return BallSkinStyle::ClassicSprite;
    if (id == "ball_classic_orb")     return BallSkinStyle::ClassicCore;
    if (id == "ball_cyber_ring")      return BallSkinStyle::CyberGyro;
    if (id == "ball_plasma_star")     return BallSkinStyle::NovaStar;
    if (id == "ball_retro_pixel")     return BallSkinStyle::RetroPixel;
    if (id == "ball_sawblade")        return BallSkinStyle::NeonSawblade;
    if (id == "ball_steampunk_gear")  return BallSkinStyle::ClockworkGear;
    if (id == "ball_void_eye")        return BallSkinStyle::OmegaCore;
    return BallSkinStyle::ClassicSprite;
}

TrailStyle ProfileManager::ResolveTrailStyle(const std::string& id)
{
    if (id == "trail_classic_white")  return TrailStyle::ClassicWhite;
    if (id == "trail_plasma_blue")    return TrailStyle::Ribbon;
    if (id == "trail_stardust")       return TrailStyle::Stardust;
    if (id == "trail_inferno_sparks") return TrailStyle::Flame;
    if (id == "trail_matrix_binary")  return TrailStyle::MatrixBinary;
    if (id == "trail_rainbow_warp")   return TrailStyle::Rainbow;
    if (id == "trail_void_lightning") return TrailStyle::ArcPulse;
    return TrailStyle::ClassicWhite;
}

DeathEffectStyle ProfileManager::ResolveDeathEffectStyle(const std::string& id)
{
    if (id == "death_classic_default") return DeathEffectStyle::ClassicMinimal;
    if (id == "death_classic_burst")   return DeathEffectStyle::ClassicBurst;
    if (id == "death_shockwave_ring")  return DeathEffectStyle::ShockwaveRing;
    if (id == "death_pixel_shatter")   return DeathEffectStyle::PixelShatter;
    if (id == "death_confetti_prism")  return DeathEffectStyle::PrismConfetti;
    if (id == "death_nova_implosion")  return DeathEffectStyle::NovaImplosion;
    if (id == "death_matrix_glitch")   return DeathEffectStyle::MatrixGlitch;
    return DeathEffectStyle::ClassicMinimal;
}

PaddleSkinStyle ProfileManager::GetEquippedPaddleSkinStyle() const
{
    return ResolvePaddleSkinStyle(GetEquippedCosmetic(CosmeticSlot::PaddleSkin));
}

BallSkinStyle ProfileManager::GetEquippedBallSkinStyle() const
{
    return ResolveBallSkinStyle(GetEquippedCosmetic(CosmeticSlot::BallSkin));
}

TrailStyle ProfileManager::GetEquippedTrailStyle() const
{
    return ResolveTrailStyle(GetEquippedCosmetic(CosmeticSlot::BallTrail));
}

DeathEffectStyle ProfileManager::GetEquippedDeathEffectStyle() const
{
    return ResolveDeathEffectStyle(GetEquippedCosmetic(CosmeticSlot::DeathEffect));
}

uint32_t ProfileManager::GetEndlessHighScore() const
{
    return static_cast<uint32_t>(std::max(0, PlayerPrefs::GetInt("Endless_HighScore", 0)));
}

float ProfileManager::GetEndlessBestTime() const
{
    return std::max(0.0f, PlayerPrefs::GetFloat("Endless_BestTime", 0.0f));
}

std::string ProfileManager::GetEndlessBestTimeFormatted() const
{
    float totalSec = GetEndlessBestTime();
    int minutes = static_cast<int>(totalSec) / 60;
    int seconds = static_cast<int>(totalSec) % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
    return std::string(buffer);
}

int ProfileManager::GetEndlessBestWaves() const
{
    return std::max(0, PlayerPrefs::GetInt("Endless_BestWaves", 0));
}

int ProfileManager::GetEndlessTotalRuns() const
{
    return std::max(0, PlayerPrefs::GetInt("Endless_TotalRuns", 0));
}

int ProfileManager::GetEndlessTotalWaves() const
{
    return std::max(0, PlayerPrefs::GetInt("Endless_TotalWaves", 0));
}

void ProfileManager::RecordEndlessRun(float survivalTime, int wavesCleared, uint32_t score)
{
    uint32_t curHighScore = GetEndlessHighScore();
    if (score > curHighScore)
    {
        PlayerPrefs::SetInt("Endless_HighScore", static_cast<int>(score));
    }

    float curBestTime = GetEndlessBestTime();
    if (survivalTime > curBestTime)
    {
        PlayerPrefs::SetFloat("Endless_BestTime", survivalTime);
    }

    int curBestWaves = GetEndlessBestWaves();
    if (wavesCleared > curBestWaves)
    {
        PlayerPrefs::SetInt("Endless_BestWaves", wavesCleared);
    }

    PlayerPrefs::SetInt("Endless_TotalRuns", GetEndlessTotalRuns() + 1);
    PlayerPrefs::SetInt("Endless_TotalWaves", GetEndlessTotalWaves() + wavesCleared);

    AddLifetimeScore(score);
    uint32_t earnedCredits = ConvertScoreToCredits(score);
    AddLifetimeCredits(earnedCredits);
    PowerUpManager::Get().AddCoins(static_cast<int>(earnedCredits));

    EvaluateAchievementUnlocks();
    PlayerPrefs::Save();
    SaveProfile();
}

int ProfileManager::GetEndlessPerkLevel(EndlessPerkType type) const
{
    return std::clamp(PlayerPrefs::GetInt("Endless_PerkLevel_" + std::to_string(static_cast<int>(type)), 0), 0, GetEndlessPerkMaxLevel(type));
}

void ProfileManager::SetEndlessPerkLevel(EndlessPerkType type, int level)
{
    int clamped = std::clamp(level, 0, GetEndlessPerkMaxLevel(type));
    PlayerPrefs::SetInt("Endless_PerkLevel_" + std::to_string(static_cast<int>(type)), clamped);
    PlayerPrefs::Save();
    SaveProfile();
}

int ProfileManager::GetEndlessPerkMaxLevel(EndlessPerkType type) const
{
    switch (type)
    {
        case EndlessPerkType::InitialRerolls:  return 3;
        case EndlessPerkType::ShieldRecovery:  return 3;
        case EndlessPerkType::ScoreMultiplier: return 5;
        case EndlessPerkType::StartingLives:   return 2;
        default: return 1;
    }
}

int ProfileManager::GetEndlessPerkUpgradeCost(EndlessPerkType type) const
{
    int curLevel = GetEndlessPerkLevel(type);
    if (curLevel >= GetEndlessPerkMaxLevel(type)) return 0;

    switch (type)
    {
        case EndlessPerkType::InitialRerolls:  return 200 * (curLevel + 1);
        case EndlessPerkType::ShieldRecovery:  return 300 * (curLevel + 1);
        case EndlessPerkType::ScoreMultiplier: return 150 * (curLevel + 1);
        case EndlessPerkType::StartingLives:   return 400 * (curLevel + 1);
        default: return 200;
    }
}

bool ProfileManager::CanUpgradeEndlessPerk(EndlessPerkType type) const
{
    int curLevel = GetEndlessPerkLevel(type);
    if (curLevel >= GetEndlessPerkMaxLevel(type)) return false;
    return PowerUpManager::Get().GetCoins() >= GetEndlessPerkUpgradeCost(type);
}

bool ProfileManager::UpgradeEndlessPerk(EndlessPerkType type)
{
    if (!CanUpgradeEndlessPerk(type)) return false;
    int cost = GetEndlessPerkUpgradeCost(type);
    if (PowerUpManager::Get().SpendCoins(cost))
    {
        SetEndlessPerkLevel(type, GetEndlessPerkLevel(type) + 1);
        return true;
    }
    return false;
}

std::string ProfileManager::GetEndlessPerkName(EndlessPerkType type)
{
    switch (type)
    {
        case EndlessPerkType::InitialRerolls:  return "DRAFT REROLLS";
        case EndlessPerkType::ShieldRecovery:  return "SHIELD RECOVERY";
        case EndlessPerkType::ScoreMultiplier: return "SCORE OVERDRIVE";
        case EndlessPerkType::StartingLives:   return "REINFORCED CHASSIS";
        default: return "PERK";
    }
}

std::string ProfileManager::GetEndlessPerkDescription(EndlessPerkType type)
{
    switch (type)
    {
        case EndlessPerkType::InitialRerolls:
            return "Grants tactical draft reroll tokens per run to re-spin undesirable card offerings.";
        case EndlessPerkType::ShieldRecovery:
            return "Deploys a protective bottom arena energy shield periodically to catch stray balls.";
        case EndlessPerkType::ScoreMultiplier:
            return "Permanently escalates all point multipliers and combat bounty rewards in Endless Mode.";
        case EndlessPerkType::StartingLives:
            return "Augments initial paddle durability with bonus reserve lives at run launch.";
        default:
            return "";
    }
}

std::string ProfileManager::GetEndlessPerkTierInfo(EndlessPerkType type, int level)
{
    if (level <= 0) return "LOCKED - NO PERK ACTIVE";
    switch (type)
    {
        case EndlessPerkType::InitialRerolls:
            return "TIER " + std::to_string(level) + ": " + std::to_string(level) + " DRAFT REROLL" + (level > 1 ? "S" : "") + " PER RUN";
        case EndlessPerkType::ShieldRecovery:
            return "TIER " + std::to_string(level) + ": BOTTOM SHIELD RECOVERS EVERY " + std::to_string(std::max(1, 6 - level)) + " WAVES";
        case EndlessPerkType::ScoreMultiplier:
            return "TIER " + std::to_string(level) + ": +" + std::to_string(level * 10) + "% PERMANENT ENDLESS SCORE BONUS";
        case EndlessPerkType::StartingLives:
            return "TIER " + std::to_string(level) + ": +" + std::to_string(level) + " EXTRA STARTING RESERVE LIFE";
        default:
            return "TIER " + std::to_string(level);
    }
}

void ProfileManager::ResetToDefaults()
{
    PlayerPrefs::SetString("PlayerProfile_Username", "PLAYER");
    PlayerPrefs::SetInt("Economy_LifetimeScore", 0);
    PlayerPrefs::SetInt("Economy_LifetimeCredits", 0);
    PlayerPrefs::SetInt("Profile_EquippedUltimate", 0);
    for (int i = 0; i < static_cast<int>(UltimateAbilityType::Count); ++i)
    {
        PlayerPrefs::SetInt("Profile_UltimateLevel_" + std::to_string(i), 1);
    }

    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::PrimaryColor),   GetDefaultCosmeticId(CosmeticSlot::PrimaryColor));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::SecondaryColor), GetDefaultCosmeticId(CosmeticSlot::SecondaryColor));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::PaddleSkin),     GetDefaultCosmeticId(CosmeticSlot::PaddleSkin));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::BallSkin),       GetDefaultCosmeticId(CosmeticSlot::BallSkin));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::BallTrail),      GetDefaultCosmeticId(CosmeticSlot::BallTrail));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::DeathEffect),    GetDefaultCosmeticId(CosmeticSlot::DeathEffect));
    PlayerPrefs::SetString(GetSlotPrefsKey(CosmeticSlot::SoundPack),      GetDefaultCosmeticId(CosmeticSlot::SoundPack));

    PlayerPrefs::SetInt("Endless_HighScore", 0);
    PlayerPrefs::SetFloat("Endless_BestTime", 0.0f);
    PlayerPrefs::SetInt("Endless_BestWaves", 0);
    PlayerPrefs::SetInt("Endless_TotalRuns", 0);
    PlayerPrefs::SetInt("Endless_TotalWaves", 0);
    for (int i = 0; i < static_cast<int>(EndlessPerkType::Count); ++i)
    {
        PlayerPrefs::SetInt("Endless_PerkLevel_" + std::to_string(i), 0);
    }

    for (auto& item : m_cosmetics)
    {
        item.Unlocked = (item.UnlockType == CosmeticUnlockType::Default);
        PlayerPrefs::SetBool("Cosmetic_Unlocked_" + item.Id, item.Unlocked);
    }

    AchievementManager::Get().ResetToDefaults();

    PlayerPrefs::Save();
    SaveProfile();
}
