#include "LevelTheme.h"
#include <array>

static const std::array<LevelTheme, 7> s_themes = {{
    // Level 1: Cyber Odyssey (Neon Blue & Electric Cyan)
    LevelTheme{
        .Name = "CYBER ODYSSEY",
        .BgBase1 = Color{8, 12, 22, 255},
        .BgBase2 = Color{12, 18, 32, 255},
        .GridColor = Color{45, 80, 150, 24},
        .DustColor = Color{180, 220, 255, 45},
        .GridSpeed = 14.0f,
        .DustSpeedY = -16.0f,
        .ComboColors = {
            Color{8, 12, 22, 255},
            Color{10, 24, 38, 255}, // Cyan
            Color{14, 36, 56, 255}, // Electric Blue
            Color{24, 28, 68, 255}, // Deep Indigo
            Color{38, 20, 78, 255}  // Ultra Violet
        },
        .VignetteBaseColor = Color{0, 0, 0, 255},
        .VignetteBaseIntensity = 0.25f
    },

    // Level 2: Emerald Matrix (Toxic Jade & Bio-Digital Green)
    LevelTheme{
        .Name = "EMERALD MATRIX",
        .BgBase1 = Color{6, 18, 12, 255},
        .BgBase2 = Color{10, 28, 18, 255},
        .GridColor = Color{35, 120, 65, 24},
        .DustColor = Color{160, 255, 190, 50},
        .GridSpeed = 16.0f,
        .DustSpeedY = -14.0f,
        .ComboColors = {
            Color{6, 18, 12, 255},
            Color{8, 28, 18, 255},   // Soft Jade
            Color{12, 42, 24, 255},  // Emerald Neon
            Color{18, 56, 20, 255},  // Toxic Green
            Color{32, 68, 14, 255}   // Overcharged Acid
        },
        .VignetteBaseColor = Color{2, 12, 6, 255},
        .VignetteBaseIntensity = 0.26f
    },

    // Level 3: Solar Flare (Molten Amber & Radiant Gold)
    LevelTheme{
        .Name = "SOLAR FLARE",
        .BgBase1 = Color{18, 12, 6, 255},
        .BgBase2 = Color{28, 18, 8, 255},
        .GridColor = Color{140, 90, 30, 24},
        .DustColor = Color{255, 215, 140, 50},
        .GridSpeed = 18.0f,
        .DustSpeedY = -20.0f,
        .ComboColors = {
            Color{18, 12, 6, 255},
            Color{28, 18, 8, 255},   // Warm Bronze
            Color{44, 26, 10, 255},  // Radiant Amber
            Color{60, 34, 10, 255},  // Blazing Gold
            Color{76, 42, 14, 255}   // Solar Plasma
        },
        .VignetteBaseColor = Color{14, 8, 2, 255},
        .VignetteBaseIntensity = 0.27f
    },

    // Level 4: Amethyst Void (Cosmic Purple & Mystic Nebula)
    LevelTheme{
        .Name = "AMETHYST VOID",
        .BgBase1 = Color{14, 8, 20, 255},
        .BgBase2 = Color{22, 12, 30, 255},
        .GridColor = Color{110, 45, 140, 24},
        .DustColor = Color{230, 175, 255, 48},
        .GridSpeed = 12.0f,
        .DustSpeedY = -12.0f,
        .ComboColors = {
            Color{14, 8, 20, 255},
            Color{24, 12, 32, 255},  // Deep Lilac
            Color{38, 16, 48, 255},  // Glowing Amethyst
            Color{54, 18, 64, 255},  // Mystic Nebula
            Color{70, 22, 82, 255}   // Cosmic Supernova
        },
        .VignetteBaseColor = Color{10, 4, 16, 255},
        .VignetteBaseIntensity = 0.26f
    },

    // Level 5: Magma Inferno (Volcanic Core & Crimson Ash)
    LevelTheme{
        .Name = "MAGMA INFERNO",
        .BgBase1 = Color{20, 6, 8, 255},
        .BgBase2 = Color{30, 8, 10, 255},
        .GridColor = Color{140, 40, 35, 24},
        .DustColor = Color{255, 145, 110, 52},
        .GridSpeed = 22.0f,
        .DustSpeedY = -24.0f,
        .ComboColors = {
            Color{20, 6, 8, 255},
            Color{32, 10, 10, 255},  // Glowing Ember
            Color{48, 14, 12, 255},  // Liquid Magma
            Color{66, 18, 14, 255},  // Searing Inferno
            Color{85, 22, 16, 255}   // Volcanic Cataclysm
        },
        .VignetteBaseColor = Color{16, 3, 4, 255},
        .VignetteBaseIntensity = 0.28f
    },

    // Level 6: Void Core (Pitch Black Deep Space / Limited Vision)
    LevelTheme{
        .Name = "VOID CORE",
        .BgBase1 = Color{4, 4, 8, 255},
        .BgBase2 = Color{8, 8, 14, 255},
        .GridColor = Color{60, 40, 100, 20},
        .DustColor = Color{120, 100, 200, 35},
        .GridSpeed = 10.0f,
        .DustSpeedY = -10.0f,
        .ComboColors = {
            Color{4, 4, 8, 255},
            Color{10, 8, 20, 255},
            Color{18, 12, 36, 255},
            Color{30, 16, 54, 255},
            Color{45, 20, 75, 255}
        },
        .VignetteBaseColor = Color{0, 0, 0, 255},
        .VignetteBaseIntensity = 0.35f,
        .AmbientLight = Color{12, 12, 18, 255} // Deep ambient darkness mask
    },

    // Level 7: Storm Vault (Electric Tempest & Conductive Metal Circuits)
    LevelTheme{
        .Name = "STORM VAULT",
        .BgBase1 = Color{4, 6, 16, 255},
        .BgBase2 = Color{8, 12, 28, 255},
        .GridColor = Color{40, 100, 220, 28},
        .DustColor = Color{120, 210, 255, 55},
        .GridSpeed = 24.0f,
        .DustSpeedY = -28.0f,
        .ComboColors = {
            Color{4, 6, 16, 255},
            Color{10, 16, 40, 255},  // Electric Navy
            Color{16, 28, 65, 255},  // High-Voltage Cobalt
            Color{24, 48, 95, 255},  // Lightning Azure
            Color{36, 75, 130, 255}  // Thunder Zenith
        },
        .VignetteBaseColor = Color{2, 4, 12, 255},
        .VignetteBaseIntensity = 0.36f,
        .AmbientLight = Color{12, 14, 24, 255} // Atmospheric darkness for spectacular storm illumination
    }
}};

const LevelTheme& LevelThemeCatalog::GetThemeForLevel(int levelNumber)
{
    if (levelNumber <= 0) return s_themes[0];
    const size_t index = static_cast<size_t>(levelNumber - 1) % s_themes.size();
    return s_themes[index];
}
