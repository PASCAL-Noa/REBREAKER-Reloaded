#pragma once

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include "Data/Color.h"
#include "Math/Vector2.h"
#include "ECS/Components/PowerUpComponent.h"
#include "Events/ArenaEvents.h"
#include "LevelTheme.h"

enum class EditorTileType : uint8_t
{
    Empty = 0,
    Light = 1,     // 1+ HP
    Medium = 2,    // 2+ HP
    Hard = 3,      // 3+ HP
    Special = 4,   // Power-Up brick
    Metal = 5,     // Indestructible block
    Bomb = 6,      // Explosive brick
    Turret = 7,    // Firing turret
    Lantern = 8,   // Light emitter
    Event = 9,     // Instant event trigger block
    Count = 10
};

enum class EditorEventType : uint8_t
{
    HazardSurge = 0,
    LaserSweep = 1,
    DarknessEclipse = 2,
    TurretOvercharge = 3,
    BossAlert = 4,
    SpeedBoost = 5,
    Count = 6
};

enum class EventSortFilter : uint8_t
{
    Clockwise = 0,        // Natural clockwise / forward order
    CounterClockwise = 1, // Reverse / anti-clockwise order
    RandomShuffle = 2     // Random shuffle
};

enum class EditorThemePreset : uint8_t
{
    DeepSpace = 0,
    CyberNeon = 1,
    SolarFlare = 2,
    ToxicMatrix = 3,
    VoidAbyss = 4,
    ArcticPulse = 5,
    CrimsonAlert = 6,
    GoldenCathedral = 7,
    Count = 8
};

struct EditorCellData
{
    EditorTileType Type = EditorTileType::Empty;
    int HitPoints = 1;                    // 1..9 HP for destructible bricks

    // Standard brick appearance override
    bool  UseCustomColor = false;
    Color CustomColor{80, 210, 255, 255};

    // Power-Up brick configuration
    bool        RandomPowerUp = true;
    PowerUpType SpecificPowerUp = PowerUpType::MultiBall;
    bool        PowerUpRegenEnabled = true;
    float       PowerUpRegenCooldown = 30.0f;

    // Event brick configuration (extensible via ArenaEventType catalog)
    bool           RandomEvent = true;
    ArenaEventType SpecificEvent = ArenaEventType::MeteorShower;
    bool           EventTriggerOnHit = false;

    // Bomb brick configuration
    int BombBlastTier = 1;                // 0: Small (1x1), 1: Standard (3x3), 2: Large (5x5)

    // Metal block configuration
    float MetalBounceBoost = 1.0f;        // 1.0x .. 1.4x bounce impulse multiplier

    // Turret configuration
    float TurretCooldown = 3.2f;          // Firing cadence in seconds
    float TurretInitialDelay = 3.5f;      // Initial delay before first shot
    float TurretProjectileSpeed = 210.0f; // Projectile speed px/s
    float TurretDamage = 34.0f;           // Hull damage per hit (15..50)
    bool  TurretAimed = false;            // false = straight down, true = aimed toward paddle

    // Lantern light configuration
    Color LanternLightColor{255, 235, 130, 255};
    float LanternLightRadius = 230.0f;    // 120 .. 420
    float LanternLightIntensity = 1.25f;  // 0.5 .. 2.5

    bool operator==(const EditorCellData&) const = default;

    static EditorCellData CreateDefaultForType(EditorTileType type)
    {
        EditorCellData cell;
        cell.Type = type;
        switch (type)
        {
        case EditorTileType::Empty:   cell.HitPoints = 0; break;
        case EditorTileType::Light:   cell.HitPoints = 1; break;
        case EditorTileType::Medium:  cell.HitPoints = 2; break;
        case EditorTileType::Hard:    cell.HitPoints = 3; break;
        case EditorTileType::Special:
            cell.HitPoints = 1;
            cell.RandomPowerUp = true;
            cell.SpecificPowerUp = PowerUpType::MultiBall;
            cell.PowerUpRegenEnabled = true;
            cell.PowerUpRegenCooldown = 30.0f;
            break;
        case EditorTileType::Metal:
            cell.HitPoints = 999999;
            cell.MetalBounceBoost = 1.0f;
            break;
        case EditorTileType::Bomb:
            cell.HitPoints = 1;
            cell.BombBlastTier = 1;
            break;
        case EditorTileType::Turret:
            cell.HitPoints = 3;
            cell.TurretCooldown = 3.2f;
            cell.TurretInitialDelay = 3.5f;
            cell.TurretProjectileSpeed = 210.0f;
            cell.TurretDamage = 34.0f;
            cell.TurretAimed = false;
            break;
        case EditorTileType::Lantern:
            cell.HitPoints = 1;
            cell.LanternLightColor = Color{255, 235, 130, 255};
            cell.LanternLightRadius = 230.0f;
            cell.LanternLightIntensity = 1.25f;
            break;
        case EditorTileType::Event:
            cell.HitPoints = 1;
            cell.RandomEvent = true;
            cell.SpecificEvent = ArenaEventType::MeteorShower;
            cell.EventTriggerOnHit = false;
            break;
        default: break;
        }
        return cell;
    }

    void ClampValues()
    {
        if (Type == EditorTileType::Empty)
        {
            HitPoints = 0;
        }
        else if (Type == EditorTileType::Metal)
        {
            HitPoints = 999999;
        }
        else if (Type == EditorTileType::Bomb)
        {
            HitPoints = std::clamp(HitPoints, 1, 3);
        }
        else
        {
            HitPoints = std::clamp(HitPoints, 1, 9);
        }
        PowerUpRegenCooldown = std::clamp(PowerUpRegenCooldown, 10.0f, 60.0f);
        if (SpecificEvent <= ArenaEventType::None || SpecificEvent >= ArenaEventType::Count)
        {
            SpecificEvent = ArenaEventType::MeteorShower;
        }
        BombBlastTier = std::clamp(BombBlastTier, 0, 2);
        MetalBounceBoost = std::clamp(MetalBounceBoost, 1.0f, 1.4f);
        TurretCooldown = std::clamp(TurretCooldown, 0.5f, 10.0f);
        TurretInitialDelay = std::clamp(TurretInitialDelay, 0.5f, 12.0f);
        TurretProjectileSpeed = std::clamp(TurretProjectileSpeed, 100.0f, 450.0f);
        TurretDamage = std::clamp(TurretDamage, 15.0f, 50.0f);
        LanternLightRadius = std::clamp(LanternLightRadius, 120.0f, 420.0f);
        LanternLightIntensity = std::clamp(LanternLightIntensity, 0.5f, 2.5f);
    }
};

struct EditorGridPage
{
    std::array<std::array<EditorCellData, 12>, 10> Cells{};
    std::string Name = "PAGE 1";

    bool operator==(const EditorGridPage&) const = default;
};

struct EditorEventItem
{
    EditorEventType Type = EditorEventType::HazardSurge;
    std::string Name = "METEOR SHOWER";
    float TriggerDelay = 15.0f;
    Color ThemeColor{255, 75, 75, 255};
};

struct EditorThemeData
{
    EditorThemePreset Preset = EditorThemePreset::DeepSpace;
    LightingMode Lighting = LightingMode::Atmospheric;
    std::string ThemeId; // Empty for built-in presets, "THM-XXX" for saved custom themes
    std::string Name = "DEEP SPACE";
    Color BgColor{10, 14, 26, 255};
    Color BgSecondaryColor{22, 34, 62, 255};
    Color AmbientColor{45, 75, 140, 255};
    float AmbientIntensity = 0.38f;
    Color PaddleConeColor{80, 200, 255, 255};
    float PaddleConeAngle = 48.0f;
    float PaddleLightIntensity = 1.30f;
    float PaddleLightRange = 430.0f;
    float VignetteIntensity = 0.25f;
    Color VignetteColor{0, 0, 0, 255};
    std::string MusicTrack = "Resources/audio/music/Game-1.ogg";
    std::vector<std::string> MusicPlaylist = {"Resources/audio/music/Game-1.ogg"};

    bool operator==(const EditorThemeData&) const = default;

    [[nodiscard]] bool IsTrackInPlaylist(const std::string& track) const
    {
        if (MusicPlaylist.empty())
        {
            return track == MusicTrack;
        }
        return std::find(MusicPlaylist.begin(), MusicPlaylist.end(), track) != MusicPlaylist.end();
    }

    void SetSingleTrack(const std::string& track)
    {
        MusicTrack = track.empty() ? "Resources/audio/music/Game-1.ogg" : track;
        MusicPlaylist = {MusicTrack};
    }

    void TogglePlaylistTrack(const std::string& track)
    {
        if (track.empty()) return;
        ClampValues();
        auto it = std::find(MusicPlaylist.begin(), MusicPlaylist.end(), track);
        if (it != MusicPlaylist.end())
        {
            // Keep at least 1 track in the level playlist
            if (MusicPlaylist.size() > 1)
            {
                MusicPlaylist.erase(it);
                if (MusicTrack == track)
                {
                    MusicTrack = MusicPlaylist.front();
                }
            }
        }
        else
        {
            MusicPlaylist.push_back(track);
        }
        ClampValues();
    }

    void EnableAllPlaylistTracks(const std::vector<std::string>& allTracks)
    {
        if (allTracks.empty()) return;
        MusicPlaylist.clear();
        if (!MusicTrack.empty() && std::find(allTracks.begin(), allTracks.end(), MusicTrack) != allTracks.end())
        {
            MusicPlaylist.push_back(MusicTrack);
        }
        for (const auto& t : allTracks)
        {
            if (std::find(MusicPlaylist.begin(), MusicPlaylist.end(), t) == MusicPlaylist.end())
            {
                MusicPlaylist.push_back(t);
            }
        }
        ClampValues();
    }

    void ClampValues()
    {
        const int lm = std::clamp(static_cast<int>(Lighting), 0, 2);
        Lighting = static_cast<LightingMode>(lm);
        AmbientIntensity = std::clamp(AmbientIntensity, 0.10f, 1.00f);
        PaddleConeAngle = std::clamp(PaddleConeAngle, 20.0f, 110.0f);
        PaddleLightIntensity = std::clamp(PaddleLightIntensity, 0.40f, 2.40f);
        PaddleLightRange = std::clamp(PaddleLightRange, 220.0f, 650.0f);
        VignetteIntensity = std::clamp(VignetteIntensity, 0.0f, 0.75f);
        if (MusicTrack.empty())
        {
            MusicTrack = "Resources/audio/music/Game-1.ogg";
        }

        // Deduplicate & sanitize playlist while preserving order and ensuring MusicTrack is included
        std::vector<std::string> cleaned;
        for (const auto& trk : MusicPlaylist)
        {
            if (!trk.empty() && std::find(cleaned.begin(), cleaned.end(), trk) == cleaned.end())
            {
                cleaned.push_back(trk);
            }
        }
        if (cleaned.empty())
        {
            cleaned.push_back(MusicTrack);
        }
        else if (std::find(cleaned.begin(), cleaned.end(), MusicTrack) == cleaned.end())
        {
            cleaned.insert(cleaned.begin(), MusicTrack);
        }
        MusicPlaylist = std::move(cleaned);
        MusicTrack = MusicPlaylist.front();
    }
};

struct EditorGamerulesData
{
    bool HazardZones = true;
    bool ArenaEvents = true;
    bool InfiniteLives = false;
    float BallSpeedMult = 1.0f;
};

class LevelData
{
public:
    static constexpr int GRID_COLS = 12;
    static constexpr int GRID_ROWS = 10;
    static constexpr float BRICK_WIDTH = 100.0f;
    static constexpr float BRICK_HEIGHT = 30.0f;
    static constexpr float BRICK_PADDING = 10.0f;
    static constexpr float START_X = -605.0f;
    static constexpr float START_Y = -350.0f;

    struct Snapshot
    {
        std::vector<EditorGridPage> Pages;
        size_t ActivePageIndex = 0;
        std::vector<EditorEventItem> EventQueue;
        ArenaEventType InitialEvent = ArenaEventType::None;
    };

    static ArenaEventType MapEditorEventToArenaEvent(EditorEventType type) noexcept
    {
        switch (type)
        {
        case EditorEventType::HazardSurge:      return ArenaEventType::MeteorShower;
        case EditorEventType::LaserSweep:       return ArenaEventType::GravityShiftDown;
        case EditorEventType::DarknessEclipse:  return ArenaEventType::SolarFlareBlackout;
        case EditorEventType::TurretOvercharge: return ArenaEventType::UltimateLockdown;
        case EditorEventType::BossAlert:        return ArenaEventType::CameraEarthquake;
        case EditorEventType::SpeedBoost:       return ArenaEventType::TemporalFlux;
        default:                                return ArenaEventType::MeteorShower;
        }
    }

    static EditorEventItem CreateEventItem(EditorEventType type, float delay)
    {
        EditorEventItem item;
        item.Type = type;
        item.TriggerDelay = std::clamp(delay, 3.0f, 300.0f);
        switch (type)
        {
        case EditorEventType::HazardSurge:
            item.Name = "METEOR SHOWER";
            item.ThemeColor = Color{255, 75, 75, 255};
            break;
        case EditorEventType::LaserSweep:
            item.Name = "HEAVY GRAVITY";
            item.ThemeColor = Color{255, 160, 40, 255};
            break;
        case EditorEventType::DarknessEclipse:
            item.Name = "SOLAR ECLIPSE";
            item.ThemeColor = Color{150, 75, 255, 255};
            break;
        case EditorEventType::TurretOvercharge:
            item.Name = "EMP LOCKDOWN";
            item.ThemeColor = Color{255, 45, 95, 255};
            break;
        case EditorEventType::BossAlert:
            item.Name = "EARTHQUAKE";
            item.ThemeColor = Color{255, 215, 0, 255};
            break;
        case EditorEventType::SpeedBoost:
            item.Name = "TEMPORAL FLUX";
            item.ThemeColor = Color{65, 225, 255, 255};
            break;
        default:
            break;
        }
        return item;
    }

    static LevelTheme BuildLevelTheme(const EditorThemeData& thm)
    {
        LevelTheme out{};
        out.Name = thm.Name;
        out.BgBase1 = thm.BgColor;
        out.BgBase2 = thm.BgSecondaryColor;
        out.GridColor = Color{
            static_cast<uint8_t>(std::min(255, static_cast<int>(thm.BgSecondaryColor.r) + 35)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(thm.BgSecondaryColor.g) + 40)),
            static_cast<uint8_t>(std::min(255, static_cast<int>(thm.BgSecondaryColor.b) + 55)),
            32
        };
        out.DustColor = Color{
            static_cast<uint8_t>(std::min(255, (static_cast<int>(thm.PaddleConeColor.r) + 255) / 2)),
            static_cast<uint8_t>(std::min(255, (static_cast<int>(thm.PaddleConeColor.g) + 255) / 2)),
            static_cast<uint8_t>(std::min(255, (static_cast<int>(thm.PaddleConeColor.b) + 255) / 2)),
            55
        };
        out.VignetteBaseColor = thm.VignetteColor;
        out.VignetteBaseIntensity = thm.VignetteIntensity;

        if (thm.Lighting == LightingMode::Lit)
        {
            out.AmbientLight = GetAmbientLightForLightingMode(LightingMode::Lit);
        }
        else if (thm.Lighting == LightingMode::PitchBlack)
        {
            out.AmbientLight = GetAmbientLightForLightingMode(LightingMode::PitchBlack);
        }
        else
        {
            const float mult = std::clamp(thm.AmbientIntensity * 1.95f, 0.18f, 1.0f);
            out.AmbientLight = Color{
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.r) * mult, 16.0f, 254.0f)),
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.g) * mult, 16.0f, 254.0f)),
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.AmbientColor.b) * mult, 16.0f, 254.0f)),
                255
            };
        }

        for (int i = 0; i < 5; ++i)
        {
            const float f = 1.0f + static_cast<float>(i) * 0.18f;
            out.ComboColors[i] = Color{
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.BgColor.r) * f + static_cast<float>(i) * 4.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.BgColor.g) * f + static_cast<float>(i) * 4.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(static_cast<float>(thm.BgColor.b) * f + static_cast<float>(i) * 6.0f, 0.0f, 255.0f)),
                255
            };
        }
        return out;
    }

    static const std::vector<std::string>& GetAvailableMusicTracks()
    {
        static const std::vector<std::string> tracks = {
            "Resources/audio/music/Game-1.ogg",
            "Resources/audio/music/Game-2.ogg",
            "Resources/audio/music/Game-3.ogg",
            "Resources/audio/music/Game-4.ogg"
        };
        return tracks;
    }

    static std::string GetMusicTrackLabel(const std::string& path)
    {
        if (path.find("Game-1") != std::string::npos) return "TRACK 01";
        if (path.find("Game-2") != std::string::npos) return "TRACK 02";
        if (path.find("Game-3") != std::string::npos) return "TRACK 03";
        if (path.find("Game-4") != std::string::npos) return "TRACK 04";
        return "TRACK 01";
    }

    static const char* GetTileDisplayName(EditorTileType type)
    {
        switch (type)
        {
        case EditorTileType::Light:   return "Light Brick";
        case EditorTileType::Medium:  return "Medium Brick";
        case EditorTileType::Hard:    return "Hard Brick";
        case EditorTileType::Special: return "Power-Up Brick";
        case EditorTileType::Event:   return "Event Brick";
        case EditorTileType::Metal:   return "Metal Block";
        case EditorTileType::Bomb:    return "Bomb Brick";
        case EditorTileType::Turret:  return "Turret";
        case EditorTileType::Lantern: return "Lantern";
        default:                      return "Empty";
        }
    }

    static const char* GetBombBlastLabel(int tier)
    {
        switch (std::clamp(tier, 0, 2))
        {
        case 0: return "Small (1x1)";
        case 1: return "Standard (3x3)";
        case 2: return "Large (5x5)";
        default: return "Standard (3x3)";
        }
    }

    LevelData()
    {
        ResetPalettePresets();
        m_pages.clear();
        EditorGridPage page1;
        page1.Name = "PAGE 1";
        m_pages.push_back(page1);
        m_activePageIndex = 0;
        ApplyThemePreset(EditorThemePreset::DeepSpace);
    }

    // Power-up Inspector Helpers
    static const std::vector<PowerUpType>& GetSelectablePowerUps()
    {
        static const std::vector<PowerUpType> list = {
            PowerUpType::MultiBall,
            PowerUpType::ExpandPaddle,
            PowerUpType::ShrinkPaddle,
            PowerUpType::Storm,
            PowerUpType::TempoBall,
            PowerUpType::ExtraLife,
            PowerUpType::BigBall,
            PowerUpType::FireBall,
            PowerUpType::StickyPaddle,
            PowerUpType::BottomBarrier,
            PowerUpType::MagnetPaddle,
            PowerUpType::GhostBall,
            PowerUpType::ItemRegen,
            PowerUpType::GlitchMalus
        };
        return list;
    }

    static std::string GetPowerUpDisplayName(PowerUpType type)
    {
        switch (type)
        {
        case PowerUpType::MultiBall:     return "Multi-Ball";
        case PowerUpType::ExpandPaddle:  return "Expand Paddle";
        case PowerUpType::ShrinkPaddle:  return "Shrink (Debuff)";
        case PowerUpType::Storm:         return "Storm / Laser";
        case PowerUpType::TempoBall:     return "Tempo Ball";
        case PowerUpType::ExtraLife:     return "Extra Life";
        case PowerUpType::BigBall:       return "Big Ball";
        case PowerUpType::FireBall:      return "Fire Ball";
        case PowerUpType::StickyPaddle:  return "Sticky Paddle";
        case PowerUpType::BottomBarrier: return "Bottom Barrier";
        case PowerUpType::MagnetPaddle:  return "Magnet Paddle";
        case PowerUpType::GhostBall:     return "Ghost Ball";
        case PowerUpType::ItemRegen:     return "Item Regen";
        case PowerUpType::GlitchMalus:   return "Glitch (Debuff)";
        default:                         return "Power-Up";
        }
    }

    static Color GetColorForHitPoints(int hitPoints)
    {
        const int clamped = std::clamp(hitPoints, 1, 9);
        switch (clamped)
        {
        case 1:  return Color{65, 225, 95, 255};   // 1 HP: Emerald Green
        case 2:  return Color{250, 215, 45, 255};  // 2 HP: Solar Yellow
        case 3:  return Color{245, 65, 65, 255};   // 3 HP: Crimson Red
        case 4:  return Color{255, 135, 35, 255};  // 4 HP: Blaze Orange
        case 5:  return Color{40, 225, 255, 255};  // 5 HP: Electric Cyan
        case 6:  return Color{55, 125, 255, 255};  // 6 HP: Royal Azure
        case 7:  return Color{175, 70, 255, 255};  // 7 HP: Plasma Violet
        case 8:  return Color{255, 65, 195, 255};  // 8 HP: Neon Magenta
        case 9:  return Color{255, 235, 140, 255}; // 9 HP: Radiant Gold
        default: return Colors::White;
        }
    }

    static Color GetDisplayColorForCell(const EditorCellData& cell)
    {
        switch (cell.Type)
        {
        case EditorTileType::Light:
        case EditorTileType::Medium:
        case EditorTileType::Hard:
            return cell.UseCustomColor ? cell.CustomColor : GetColorForHitPoints(cell.HitPoints);
        case EditorTileType::Special:
            return Color{255, 0, 255, 255};
        case EditorTileType::Event:
            return cell.RandomEvent ? Color{45, 220, 235, 255} : GetArenaEventColor(cell.SpecificEvent);
        case EditorTileType::Metal:
            return Color{180, 195, 210, 255};
        case EditorTileType::Bomb:
            return Color{255, 80, 20, 255};
        case EditorTileType::Turret:
            return (cell.HitPoints > 3) ? GetColorForHitPoints(cell.HitPoints) : Color{255, 45, 55, 255};
        case EditorTileType::Lantern:
            return cell.LanternLightColor;
        default:
            return Color{35, 40, 55, 255};
        }
    }

    // Palette Preset Memory
    void ResetPalettePresets()
    {
        for (size_t i = 0; i < static_cast<size_t>(EditorTileType::Count); ++i)
        {
            m_palettePresets[i] = EditorCellData::CreateDefaultForType(static_cast<EditorTileType>(i));
        }
    }

    [[nodiscard]] const EditorCellData& GetPalettePreset(EditorTileType type) const
    {
        const size_t idx = std::min(static_cast<size_t>(type), m_palettePresets.size() - 1);
        return m_palettePresets[idx];
    }

    [[nodiscard]] EditorCellData& GetPalettePresetMutable(EditorTileType type)
    {
        const size_t idx = std::min(static_cast<size_t>(type), m_palettePresets.size() - 1);
        return m_palettePresets[idx];
    }

    void SetPalettePreset(EditorTileType type, const EditorCellData& preset)
    {
        const size_t idx = std::min(static_cast<size_t>(type), m_palettePresets.size() - 1);
        EditorCellData copy = preset;
        copy.Type = type;
        copy.ClampValues();
        m_palettePresets[idx] = copy;
    }

    // Multi-Page Grid Management
    [[nodiscard]] size_t GetPageCount() const noexcept { return m_pages.size(); }
    [[nodiscard]] size_t GetActivePageIndex() const noexcept { return m_activePageIndex; }

    void SetActivePageIndex(size_t idx)
    {
        if (m_pages.empty()) return;
        m_activePageIndex = std::min(idx, m_pages.size() - 1);
    }

    size_t AddPage(bool duplicateCurrent = false)
    {
        EditorGridPage newPage;
        if (duplicateCurrent && !m_pages.empty())
        {
            newPage = m_pages[m_activePageIndex];
        }
        newPage.Name = "PAGE " + std::to_string(m_pages.size() + 1);
        m_pages.push_back(newPage);
        m_activePageIndex = m_pages.size() - 1;
        return m_activePageIndex;
    }

    bool RemovePage(size_t idx)
    {
        if (m_pages.size() <= 1 || idx >= m_pages.size()) return false;
        m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(idx));
        if (m_activePageIndex >= m_pages.size())
        {
            m_activePageIndex = m_pages.size() - 1;
        }
        return true;
    }

    bool RemoveActivePage()
    {
        return RemovePage(m_activePageIndex);
    }

    bool MovePage(size_t fromIdx, size_t toIdx)
    {
        if (fromIdx >= m_pages.size() || toIdx >= m_pages.size() || fromIdx == toIdx) return false;
        EditorGridPage moved = m_pages[fromIdx];
        m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(fromIdx));
        m_pages.insert(m_pages.begin() + static_cast<std::ptrdiff_t>(toIdx), moved);

        if (m_activePageIndex == fromIdx)
        {
            m_activePageIndex = toIdx;
        }
        else if (fromIdx < m_activePageIndex && toIdx >= m_activePageIndex)
        {
            m_activePageIndex--;
        }
        else if (fromIdx > m_activePageIndex && toIdx <= m_activePageIndex)
        {
            m_activePageIndex++;
        }
        return true;
    }

    bool MoveActivePageLeft()
    {
        if (m_activePageIndex == 0) return false;
        return MovePage(m_activePageIndex, m_activePageIndex - 1);
    }

    bool MoveActivePageRight()
    {
        if (m_activePageIndex + 1 >= m_pages.size()) return false;
        return MovePage(m_activePageIndex, m_activePageIndex + 1);
    }

    [[nodiscard]] const EditorGridPage& GetPage(size_t pageIdx) const
    {
        return m_pages[std::min(pageIdx, m_pages.size() - 1)];
    }

    [[nodiscard]] EditorGridPage& GetPageMutable(size_t pageIdx)
    {
        return m_pages[std::min(pageIdx, m_pages.size() - 1)];
    }

    void ClearGrid()
    {
        if (m_pages.empty()) return;
        for (auto& row : m_pages[m_activePageIndex].Cells)
        {
            row.fill(EditorCellData{});
        }
    }

    void ClearAllPages()
    {
        m_pages.clear();
        EditorGridPage page1;
        page1.Name = "PAGE 1";
        m_pages.push_back(page1);
        m_activePageIndex = 0;
    }

    [[nodiscard]] EditorCellData GetCell(int col, int row, size_t pageIdx) const
    {
        if (pageIdx >= m_pages.size() || col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS)
        {
            return EditorCellData{};
        }
        return m_pages[pageIdx].Cells[row][col];
    }

    [[nodiscard]] EditorCellData GetCell(int col, int row) const
    {
        return GetCell(col, row, m_activePageIndex);
    }

    void SetCell(int col, int row, const EditorCellData& data, size_t pageIdx)
    {
        if (pageIdx >= m_pages.size() || col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return;
        EditorCellData copy = data;
        copy.ClampValues();
        m_pages[pageIdx].Cells[row][col] = copy;
    }

    void SetCell(int col, int row, const EditorCellData& data)
    {
        SetCell(col, row, data, m_activePageIndex);
    }

    [[nodiscard]] EditorTileType GetTile(int col, int row) const
    {
        return GetCell(col, row).Type;
    }

    void SetTile(int col, int row, EditorTileType type)
    {
        if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS || m_pages.empty()) return;
        if (type == EditorTileType::Empty)
        {
            m_pages[m_activePageIndex].Cells[row][col] = EditorCellData{};
        }
        else
        {
            m_pages[m_activePageIndex].Cells[row][col] = GetPalettePreset(type);
        }
    }

    [[nodiscard]] int GetBrickCount(size_t pageIdx) const
    {
        if (pageIdx >= m_pages.size()) return 0;
        int count = 0;
        for (int r = 0; r < GRID_ROWS; ++r)
        {
            for (int c = 0; c < GRID_COLS; ++c)
            {
                if (m_pages[pageIdx].Cells[r][c].Type != EditorTileType::Empty)
                {
                    count++;
                }
            }
        }
        return count;
    }

    [[nodiscard]] int GetBrickCount() const
    {
        return GetBrickCount(m_activePageIndex);
    }

    [[nodiscard]] int GetTotalBrickCount() const
    {
        int total = 0;
        for (size_t i = 0; i < m_pages.size(); ++i)
        {
            total += GetBrickCount(i);
        }
        return total;
    }

    [[nodiscard]] int GetBreakableBrickCount(size_t pageIdx) const
    {
        if (pageIdx >= m_pages.size()) return 0;
        int count = 0;
        for (int r = 0; r < GRID_ROWS; ++r)
        {
            for (int c = 0; c < GRID_COLS; ++c)
            {
                EditorTileType t = m_pages[pageIdx].Cells[r][c].Type;
                if (t != EditorTileType::Empty && t != EditorTileType::Metal && t != EditorTileType::Special)
                {
                    count++;
                }
            }
        }
        return count;
    }

    [[nodiscard]] Vector2f GetCellCenter(int col, int row) const
    {
        float x = START_X + static_cast<float>(col) * (BRICK_WIDTH + BRICK_PADDING);
        float y = START_Y + static_cast<float>(row) * (BRICK_HEIGHT + BRICK_PADDING);
        return Vector2f{x, y};
    }

    static EditorThemeData CreatePresetTheme(EditorThemePreset preset)
    {
        EditorThemeData thm{};
        thm.Preset = preset;
        thm.ThemeId.clear();
        switch (preset)
        {
        case EditorThemePreset::DeepSpace:
            thm.Name = "DEEP SPACE";
            thm.BgColor = Color{10, 14, 26, 255};
            thm.BgSecondaryColor = Color{24, 38, 72, 255};
            thm.AmbientColor = Color{55, 95, 165, 255};
            thm.AmbientIntensity = 0.42f;
            thm.PaddleConeColor = Color{80, 200, 255, 255};
            thm.PaddleConeAngle = 56.0f;
            thm.PaddleLightIntensity = 1.30f;
            thm.PaddleLightRange = 430.0f;
            thm.VignetteIntensity = 0.25f;
            thm.VignetteColor = Color{5, 8, 20, 255};
            thm.MusicTrack = "Resources/audio/music/Game-1.ogg";
            break;
        case EditorThemePreset::CyberNeon:
            thm.Name = "CYBER NEON";
            thm.BgColor = Color{18, 10, 26, 255};
            thm.BgSecondaryColor = Color{58, 18, 76, 255};
            thm.AmbientColor = Color{145, 55, 180, 255};
            thm.AmbientIntensity = 0.45f;
            thm.PaddleConeColor = Color{255, 65, 190, 255};
            thm.PaddleConeAngle = 62.0f;
            thm.PaddleLightIntensity = 1.45f;
            thm.PaddleLightRange = 450.0f;
            thm.VignetteIntensity = 0.30f;
            thm.VignetteColor = Color{20, 4, 28, 255};
            thm.MusicTrack = "Resources/audio/music/Game-2.ogg";
            break;
        case EditorThemePreset::SolarFlare:
            thm.Name = "SOLAR FLARE";
            thm.BgColor = Color{26, 14, 10, 255};
            thm.BgSecondaryColor = Color{72, 34, 16, 255};
            thm.AmbientColor = Color{185, 105, 42, 255};
            thm.AmbientIntensity = 0.48f;
            thm.PaddleConeColor = Color{255, 185, 45, 255};
            thm.PaddleConeAngle = 68.0f;
            thm.PaddleLightIntensity = 1.50f;
            thm.PaddleLightRange = 470.0f;
            thm.VignetteIntensity = 0.28f;
            thm.VignetteColor = Color{28, 10, 4, 255};
            thm.MusicTrack = "Resources/audio/music/Game-3.ogg";
            break;
        case EditorThemePreset::ToxicMatrix:
            thm.Name = "TOXIC MATRIX";
            thm.BgColor = Color{8, 22, 14, 255};
            thm.BgSecondaryColor = Color{18, 62, 36, 255};
            thm.AmbientColor = Color{45, 155, 80, 255};
            thm.AmbientIntensity = 0.42f;
            thm.PaddleConeColor = Color{65, 255, 135, 255};
            thm.PaddleConeAngle = 54.0f;
            thm.PaddleLightIntensity = 1.35f;
            thm.PaddleLightRange = 440.0f;
            thm.VignetteIntensity = 0.26f;
            thm.VignetteColor = Color{4, 18, 10, 255};
            thm.MusicTrack = "Resources/audio/music/Game-4.ogg";
            break;
        case EditorThemePreset::VoidAbyss:
            thm.Name = "VOID ABYSS";
            thm.BgColor = Color{6, 6, 10, 255};
            thm.BgSecondaryColor = Color{18, 20, 32, 255};
            thm.AmbientColor = Color{28, 32, 48, 255};
            thm.AmbientIntensity = 0.20f;
            thm.PaddleConeColor = Color{220, 235, 255, 255};
            thm.PaddleConeAngle = 44.0f;
            thm.PaddleLightIntensity = 1.75f;
            thm.PaddleLightRange = 500.0f;
            thm.VignetteIntensity = 0.48f;
            thm.VignetteColor = Color{0, 0, 0, 255};
            thm.MusicTrack = "Resources/audio/music/Game-1.ogg";
            break;
        case EditorThemePreset::ArcticPulse:
            thm.Name = "ARCTIC PULSE";
            thm.BgColor = Color{10, 22, 30, 255};
            thm.BgSecondaryColor = Color{24, 68, 86, 255};
            thm.AmbientColor = Color{110, 195, 225, 255};
            thm.AmbientIntensity = 0.56f;
            thm.PaddleConeColor = Color{135, 240, 255, 255};
            thm.PaddleConeAngle = 72.0f;
            thm.PaddleLightIntensity = 1.25f;
            thm.PaddleLightRange = 460.0f;
            thm.VignetteIntensity = 0.18f;
            thm.VignetteColor = Color{6, 18, 28, 255};
            thm.MusicTrack = "Resources/audio/music/Game-2.ogg";
            break;
        case EditorThemePreset::CrimsonAlert:
            thm.Name = "CRIMSON ALERT";
            thm.BgColor = Color{24, 8, 12, 255};
            thm.BgSecondaryColor = Color{68, 18, 28, 255};
            thm.AmbientColor = Color{175, 42, 58, 255};
            thm.AmbientIntensity = 0.36f;
            thm.PaddleConeColor = Color{255, 95, 70, 255};
            thm.PaddleConeAngle = 52.0f;
            thm.PaddleLightIntensity = 1.55f;
            thm.PaddleLightRange = 440.0f;
            thm.VignetteIntensity = 0.38f;
            thm.VignetteColor = Color{32, 4, 8, 255};
            thm.MusicTrack = "Resources/audio/music/Game-3.ogg";
            break;
        case EditorThemePreset::GoldenCathedral:
            thm.Name = "GOLDEN VAULT";
            thm.BgColor = Color{18, 16, 28, 255};
            thm.BgSecondaryColor = Color{52, 42, 24, 255};
            thm.AmbientColor = Color{210, 175, 95, 255};
            thm.AmbientIntensity = 0.52f;
            thm.PaddleConeColor = Color{255, 225, 120, 255};
            thm.PaddleConeAngle = 66.0f;
            thm.PaddleLightIntensity = 1.40f;
            thm.PaddleLightRange = 480.0f;
            thm.VignetteIntensity = 0.22f;
            thm.VignetteColor = Color{16, 12, 20, 255};
            thm.MusicTrack = "Resources/audio/music/Game-4.ogg";
            break;
        default:
            break;
        }
        thm.MusicPlaylist = {thm.MusicTrack};
        return thm;
    }

    void ApplyThemePreset(EditorThemePreset preset)
    {
        m_theme = CreatePresetTheme(preset);
    }

    void SetTheme(const EditorThemeData& theme)
    {
        m_theme = theme;
        m_theme.ClampValues();
    }

    void NextThemePreset()
    {
        int p = (static_cast<int>(m_theme.Preset) + 1) % static_cast<int>(EditorThemePreset::Count);
        ApplyThemePreset(static_cast<EditorThemePreset>(p));
    }

    void PrevThemePreset()
    {
        int count = static_cast<int>(EditorThemePreset::Count);
        int p = (static_cast<int>(m_theme.Preset) - 1 + count) % count;
        ApplyThemePreset(static_cast<EditorThemePreset>(p));
    }

    // Procedural Generation Assistant (populates active page using current palette presets)
    void ProceduralGenerate(int targetBrickCount, float difficulty, int symmetryMode, unsigned int seed = 0)
    {
        ClearGrid();
        if (targetBrickCount <= 0 || m_pages.empty()) return;

        std::mt19937 rng(seed != 0 ? seed : 1337);
        std::uniform_int_distribution<int> distCol(0, GRID_COLS - 1);
        std::uniform_int_distribution<int> distRow(0, GRID_ROWS - 1);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

        int placed = 0;
        int maxAttempts = 1000;
        auto& activeCells = m_pages[m_activePageIndex].Cells;

        while (placed < targetBrickCount && maxAttempts-- > 0)
        {
            int c = distCol(rng);
            int r = distRow(rng);

            if (activeCells[r][c].Type != EditorTileType::Empty) continue;

            float roll = dist01(rng);
            EditorTileType type = EditorTileType::Light;
            if (roll < 0.35f - difficulty * 0.20f)
            {
                type = EditorTileType::Light;
            }
            else if (roll < 0.70f - difficulty * 0.15f)
            {
                type = EditorTileType::Medium;
            }
            else if (roll < 0.86f)
            {
                type = EditorTileType::Hard;
            }
            else if (roll < 0.92f)
            {
                type = (difficulty > 0.4f) ? EditorTileType::Turret : EditorTileType::Special;
            }
            else if (roll < 0.96f)
            {
                type = EditorTileType::Event;
            }
            else
            {
                type = EditorTileType::Bomb;
            }

            auto placeWithSymmetry = [&](int col, int row, EditorTileType t) {
                if (col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS)
                {
                    if (activeCells[row][col].Type == EditorTileType::Empty)
                    {
                        activeCells[row][col] = GetPalettePreset(t);
                        placed++;
                    }
                }
            };

            placeWithSymmetry(c, r, type);

            if (symmetryMode == 1)
            {
                placeWithSymmetry(GRID_COLS - 1 - c, r, type);
            }
            else if (symmetryMode == 2)
            {
                placeWithSymmetry(c, GRID_ROWS - 1 - r, type);
            }
            else if (symmetryMode == 3)
            {
                placeWithSymmetry(GRID_COLS - 1 - c, r, type);
                placeWithSymmetry(c, GRID_ROWS - 1 - r, type);
                placeWithSymmetry(GRID_COLS - 1 - c, GRID_ROWS - 1 - r, type);
            }
        }
    }

    // Event Queue Management
    void AddEvent(EditorEventType type, float customDelay = -1.0f)
    {
        const float nextDelay = (customDelay > 0.0f)
            ? customDelay
            : (m_eventQueue.empty() ? 15.0f : (m_eventQueue.back().TriggerDelay + 12.0f));
        m_eventQueue.push_back(CreateEventItem(type, nextDelay));
    }

    void AdjustEventDelay(size_t index, float delta)
    {
        if (index < m_eventQueue.size())
        {
            m_eventQueue[index].TriggerDelay = std::clamp(m_eventQueue[index].TriggerDelay + delta, 3.0f, 300.0f);
        }
    }

    void RemoveEvent(size_t index)
    {
        if (index < m_eventQueue.size())
        {
            m_eventQueue.erase(m_eventQueue.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void ClearEventQueue()
    {
        m_eventQueue.clear();
    }

    void ReorderEvent(size_t fromIdx, size_t toIdx)
    {
        if (fromIdx >= m_eventQueue.size() || toIdx >= m_eventQueue.size() || fromIdx == toIdx) return;
        std::vector<float> delays;
        delays.reserve(m_eventQueue.size());
        for (const auto& ev : m_eventQueue)
        {
            delays.push_back(ev.TriggerDelay);
        }
        std::sort(delays.begin(), delays.end());

        auto item = m_eventQueue[fromIdx];
        m_eventQueue.erase(m_eventQueue.begin() + static_cast<std::ptrdiff_t>(fromIdx));
        m_eventQueue.insert(m_eventQueue.begin() + static_cast<std::ptrdiff_t>(toIdx), item);

        for (size_t i = 0; i < m_eventQueue.size(); ++i)
        {
            m_eventQueue[i].TriggerDelay = delays[i];
        }
    }

    void ApplyEventFilter(EventSortFilter filter)
    {
        m_sortFilter = filter;
        if (m_eventQueue.empty()) return;

        std::vector<float> delays;
        delays.reserve(m_eventQueue.size());
        for (const auto& ev : m_eventQueue)
        {
            delays.push_back(ev.TriggerDelay);
        }
        std::sort(delays.begin(), delays.end());

        switch (filter)
        {
        case EventSortFilter::Clockwise:
            std::sort(m_eventQueue.begin(), m_eventQueue.end(), [](const EditorEventItem& a, const EditorEventItem& b) {
                return static_cast<int>(a.Type) < static_cast<int>(b.Type);
            });
            break;
        case EventSortFilter::CounterClockwise:
            std::sort(m_eventQueue.begin(), m_eventQueue.end(), [](const EditorEventItem& a, const EditorEventItem& b) {
                return static_cast<int>(a.Type) > static_cast<int>(b.Type);
            });
            break;
        case EventSortFilter::RandomShuffle:
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(m_eventQueue.begin(), m_eventQueue.end(), g);
            break;
        }
        }

        for (size_t i = 0; i < m_eventQueue.size(); ++i)
        {
            m_eventQueue[i].TriggerDelay = delays[i];
        }
    }

    // Single-page ASCII representation for classic campaign compatibility
    [[nodiscard]] std::string ExportPageToAscii(size_t pageIdx, bool trimTrailingEmpty = true) const
    {
        if (pageIdx >= m_pages.size()) return "";
        const auto& grid = m_pages[pageIdx].Cells;
        std::string result;
        for (int r = 0; r < GRID_ROWS; ++r)
        {
            if (trimTrailingEmpty && r >= 5)
            {
                bool anyContentBelow = false;
                for (int rr = r; rr < GRID_ROWS; ++rr)
                {
                    for (int cc = 0; cc < GRID_COLS; ++cc)
                    {
                        if (grid[rr][cc].Type != EditorTileType::Empty)
                        {
                            anyContentBelow = true;
                            break;
                        }
                    }
                    if (anyContentBelow) break;
                }
                if (!anyContentBelow) break;
            }

            for (int c = 0; c < GRID_COLS; ++c)
            {
                switch (grid[r][c].Type)
                {
                case EditorTileType::Empty:   result += '0'; break;
                case EditorTileType::Light:   result += '1'; break;
                case EditorTileType::Medium:  result += '2'; break;
                case EditorTileType::Hard:    result += '3'; break;
                case EditorTileType::Special: result += 'S'; break;
                case EditorTileType::Metal:   result += 'M'; break;
                case EditorTileType::Bomb:    result += 'B'; break;
                case EditorTileType::Turret:  result += 'T'; break;
                case EditorTileType::Lantern: result += 'L'; break;
                case EditorTileType::Event:   result += 'E'; break;
                default:                      result += '0'; break;
                }
            }
            result += '\n';
        }
        return result;
    }

    [[nodiscard]] std::string ExportToAscii() const
    {
        return ExportPageToAscii(m_activePageIndex, true);
    }

    void ImportFromAscii(const std::string& text)
    {
        ClearGrid();
        std::istringstream stream(text);
        std::string line;
        int row = 0;
        while (std::getline(stream, line) && row < GRID_ROWS)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            for (size_t col = 0; col < line.size() && col < static_cast<size_t>(GRID_COLS); ++col)
            {
                char c = line[col];
                EditorTileType t = EditorTileType::Empty;
                switch (c)
                {
                case '1': t = EditorTileType::Light; break;
                case '2': t = EditorTileType::Medium; break;
                case '3': t = EditorTileType::Hard; break;
                case 'S': case 's': t = EditorTileType::Special; break;
                case 'M': case 'm': case 'U': case 'u': t = EditorTileType::Metal; break;
                case 'B': case 'b': case 'X': case 'x': t = EditorTileType::Bomb; break;
                case 'T': case 't': t = EditorTileType::Turret; break;
                case 'L': case 'l': t = EditorTileType::Lantern; break;
                case 'E': case 'e': t = EditorTileType::Event; break;
                default:  t = EditorTileType::Empty; break;
                }
                SetTile(static_cast<int>(col), row, t);
            }
            row++;
        }
    }

    // Full V2 Multi-Page + Extended Environment + Cell Metadata Serialization
    [[nodiscard]] std::string ExportLevelV2() const
    {
        std::ostringstream oss;
        oss << "#REBREAKER_LEVEL_V2\n";
        if (!m_levelId.empty())
        {
            oss << "LEVEL_ID " << m_levelId << "\n";
        }
        if (!m_levelName.empty())
        {
            oss << "LEVEL_NAME " << m_levelName << "\n";
        }
        oss << "THEME " << static_cast<int>(m_theme.Preset) << " "
            << m_theme.AmbientIntensity << " " << m_theme.PaddleConeAngle << "\n";
        oss << "THEME_EXT "
            << static_cast<int>(m_theme.BgColor.r) << " " << static_cast<int>(m_theme.BgColor.g) << " " << static_cast<int>(m_theme.BgColor.b) << " "
            << static_cast<int>(m_theme.BgSecondaryColor.r) << " " << static_cast<int>(m_theme.BgSecondaryColor.g) << " " << static_cast<int>(m_theme.BgSecondaryColor.b) << " "
            << static_cast<int>(m_theme.AmbientColor.r) << " " << static_cast<int>(m_theme.AmbientColor.g) << " " << static_cast<int>(m_theme.AmbientColor.b) << " "
            << static_cast<int>(m_theme.PaddleConeColor.r) << " " << static_cast<int>(m_theme.PaddleConeColor.g) << " " << static_cast<int>(m_theme.PaddleConeColor.b) << " "
            << m_theme.PaddleLightIntensity << " " << m_theme.PaddleLightRange << " "
            << m_theme.VignetteIntensity << " "
            << static_cast<int>(m_theme.VignetteColor.r) << " " << static_cast<int>(m_theme.VignetteColor.g) << " " << static_cast<int>(m_theme.VignetteColor.b) << " "
            << (m_theme.ThemeId.empty() ? "-" : m_theme.ThemeId) << " "
            << m_theme.MusicTrack << "\n";
        if (!m_theme.MusicPlaylist.empty())
        {
            oss << "MUSIC_PLAYLIST";
            for (const auto& trk : m_theme.MusicPlaylist)
            {
                oss << " " << trk;
            }
            oss << "\n";
        }
        if (!m_theme.Name.empty())
        {
            oss << "THEME_NAME " << m_theme.Name << "\n";
        }
        oss << "LIGHTING_MODE " << static_cast<int>(m_theme.Lighting) << "\n";
        if (m_initialEvent != ArenaEventType::None)
        {
            oss << "INIT_EVENT " << static_cast<int>(m_initialEvent) << "\n";
        }
        oss << "RULES " << (m_rules.HazardZones ? 1 : 0) << " "
            << (m_rules.ArenaEvents ? 1 : 0) << " "
            << (m_rules.InfiniteLives ? 1 : 0) << " "
            << m_rules.BallSpeedMult << "\n";
        for (const auto& ev : m_eventQueue)
        {
            oss << "EVENT " << static_cast<int>(ev.Type) << " " << ev.TriggerDelay << "\n";
        }

        oss << "PAGES " << m_pages.size() << "\n";
        for (size_t p = 0; p < m_pages.size(); ++p)
        {
            oss << "PAGE " << p << "\n";
            oss << ExportPageToAscii(p, false);
            for (int r = 0; r < GRID_ROWS; ++r)
            {
                for (int c = 0; c < GRID_COLS; ++c)
                {
                    const auto& cell = m_pages[p].Cells[r][c];
                    if (cell.Type == EditorTileType::Empty) continue;
                    const auto def = EditorCellData::CreateDefaultForType(cell.Type);
                    if (!(cell == def))
                    {
                        oss << "META " << p << " " << r << " " << c << " "
                            << cell.HitPoints << " "
                            << (cell.RandomPowerUp ? 1 : 0) << " "
                            << static_cast<int>(cell.SpecificPowerUp) << " "
                            << cell.TurretCooldown << " "
                            << cell.TurretInitialDelay << " "
                            << cell.TurretProjectileSpeed << " "
                            // Extended properties (optional on read for full backward compat)
                            << (cell.UseCustomColor ? 1 : 0) << " "
                            << static_cast<int>(cell.CustomColor.r) << " "
                            << static_cast<int>(cell.CustomColor.g) << " "
                            << static_cast<int>(cell.CustomColor.b) << " "
                            << (cell.PowerUpRegenEnabled ? 1 : 0) << " "
                            << cell.PowerUpRegenCooldown << " "
                            << (cell.RandomEvent ? 1 : 0) << " "
                            << static_cast<int>(cell.SpecificEvent) << " "
                            << (cell.EventTriggerOnHit ? 1 : 0) << " "
                            << cell.BombBlastTier << " "
                            << cell.MetalBounceBoost << " "
                            << cell.TurretDamage << " "
                            << (cell.TurretAimed ? 1 : 0) << " "
                            << static_cast<int>(cell.LanternLightColor.r) << " "
                            << static_cast<int>(cell.LanternLightColor.g) << " "
                            << static_cast<int>(cell.LanternLightColor.b) << " "
                            << cell.LanternLightRadius << " "
                            << cell.LanternLightIntensity << "\n";
                    }
                }
            }
        }
        return oss.str();
    }

    void ImportLevelV2(const std::string& text)
    {
        if (text.find("#REBREAKER_LEVEL_V2") == std::string::npos)
        {
            m_levelId.clear();
            m_levelName.clear();
            m_initialEvent = ArenaEventType::None;
            ClearAllPages();
            ImportFromAscii(text);
            return;
        }

        m_levelId.clear();
        m_levelName.clear();
        m_pages.clear();
        m_eventQueue.clear();
        m_initialEvent = ArenaEventType::None;
        m_activePageIndex = 0;

        std::istringstream stream(text);
        std::string line;
        int currentPage = -1;
        int currentRow = 0;

        while (std::getline(stream, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            std::istringstream ls(line);
            std::string tag;
            ls >> tag;

            if (tag == "LEVEL_ID")
            {
                ls >> m_levelId;
            }
            else if (tag == "LEVEL_NAME")
            {
                std::string rest;
                std::getline(ls, rest);
                while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
                m_levelName = rest;
            }
            else if (tag == "THEME")
            {
                int presetInt = 0;
                float amb = 0.38f;
                float cone = 48.0f;
                if (ls >> presetInt)
                {
                    ApplyThemePreset(static_cast<EditorThemePreset>(std::clamp(presetInt, 0, static_cast<int>(EditorThemePreset::Count) - 1)));
                    if (ls >> amb) m_theme.AmbientIntensity = amb;
                    if (ls >> cone) m_theme.PaddleConeAngle = cone;
                    m_theme.ClampValues();
                }
            }
            else if (tag == "THEME_EXT")
            {
                int br = 10, bg = 14, bb = 26;
                int sr = 22, sg = 34, sb = 62;
                int ar = 45, ag = 75, ab = 140;
                int pr = 80, pg = 200, pb = 255;
                float pInt = 1.3f, pRange = 430.0f, vInt = 0.25f;
                int vr = 0, vg = 0, vb = 0;
                std::string tid, track;
                if (ls >> br >> bg >> bb >> sr >> sg >> sb >> ar >> ag >> ab >> pr >> pg >> pb >> pInt >> pRange >> vInt >> vr >> vg >> vb)
                {
                    m_theme.BgColor = Color{static_cast<uint8_t>(std::clamp(br, 0, 255)), static_cast<uint8_t>(std::clamp(bg, 0, 255)), static_cast<uint8_t>(std::clamp(bb, 0, 255)), 255};
                    m_theme.BgSecondaryColor = Color{static_cast<uint8_t>(std::clamp(sr, 0, 255)), static_cast<uint8_t>(std::clamp(sg, 0, 255)), static_cast<uint8_t>(std::clamp(sb, 0, 255)), 255};
                    m_theme.AmbientColor = Color{static_cast<uint8_t>(std::clamp(ar, 0, 255)), static_cast<uint8_t>(std::clamp(ag, 0, 255)), static_cast<uint8_t>(std::clamp(ab, 0, 255)), 255};
                    m_theme.PaddleConeColor = Color{static_cast<uint8_t>(std::clamp(pr, 0, 255)), static_cast<uint8_t>(std::clamp(pg, 0, 255)), static_cast<uint8_t>(std::clamp(pb, 0, 255)), 255};
                    m_theme.PaddleLightIntensity = pInt;
                    m_theme.PaddleLightRange = pRange;
                    m_theme.VignetteIntensity = vInt;
                    m_theme.VignetteColor = Color{static_cast<uint8_t>(std::clamp(vr, 0, 255)), static_cast<uint8_t>(std::clamp(vg, 0, 255)), static_cast<uint8_t>(std::clamp(vb, 0, 255)), 255};
                    if (ls >> tid) m_theme.ThemeId = (tid == "-") ? "" : tid;
                    if (ls >> track)
                    {
                        m_theme.MusicTrack = track;
                        m_theme.MusicPlaylist = {track};
                    }
                    m_theme.ClampValues();
                }
            }
            else if (tag == "MUSIC_PLAYLIST")
            {
                std::vector<std::string> pl;
                std::string trk;
                while (ls >> trk)
                {
                    if (!trk.empty()) pl.push_back(trk);
                }
                if (!pl.empty())
                {
                    m_theme.MusicPlaylist = std::move(pl);
                    m_theme.MusicTrack = m_theme.MusicPlaylist.front();
                    m_theme.ClampValues();
                }
            }
            else if (tag == "THEME_NAME")
            {
                std::string rest;
                std::getline(ls, rest);
                while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
                if (!rest.empty()) m_theme.Name = rest;
            }
            else if (tag == "LIGHTING_MODE")
            {
                int lmInt = 1;
                if (ls >> lmInt)
                {
                    m_theme.Lighting = static_cast<LightingMode>(std::clamp(lmInt, 0, 2));
                }
            }
            else if (tag == "INIT_EVENT")
            {
                int evInt = 0;
                if (ls >> evInt)
                {
                    if (evInt > static_cast<int>(ArenaEventType::None) && evInt < static_cast<int>(ArenaEventType::Count))
                    {
                        m_initialEvent = static_cast<ArenaEventType>(evInt);
                    }
                    else
                    {
                        m_initialEvent = ArenaEventType::None;
                    }
                }
            }
            else if (tag == "RULES")
            {
                int hz = 1, ar = 1, inf = 0;
                float spd = 1.0f;
                if (ls >> hz >> ar >> inf)
                {
                    m_rules.HazardZones = (hz != 0);
                    m_rules.ArenaEvents = (ar != 0);
                    m_rules.InfiniteLives = (inf != 0);
                    if (ls >> spd) m_rules.BallSpeedMult = spd;
                }
            }
            else if (tag == "EVENT")
            {
                int evInt = 0;
                float customDelay = -1.0f;
                if (ls >> evInt)
                {
                    if (!(ls >> customDelay)) customDelay = -1.0f;
                    AddEvent(static_cast<EditorEventType>(std::clamp(evInt, 0, static_cast<int>(EditorEventType::Count) - 1)), customDelay);
                }
            }
            else if (tag == "PAGES")
            {
                continue;
            }
            else if (tag == "PAGE")
            {
                size_t pIdx = m_pages.size();
                ls >> pIdx;
                EditorGridPage newPage;
                newPage.Name = "PAGE " + std::to_string(m_pages.size() + 1);
                m_pages.push_back(newPage);
                currentPage = static_cast<int>(m_pages.size()) - 1;
                currentRow = 0;
            }
            else if (tag == "META")
            {
                size_t p = 0;
                int r = 0, c = 0, hp = 1, rndPwr = 1, specPwr = 0;
                float cd = 3.2f, initD = 3.5f, pSpd = 210.0f;
                if (ls >> p >> r >> c >> hp >> rndPwr >> specPwr >> cd >> initD >> pSpd)
                {
                    if (p < m_pages.size() && r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS)
                    {
                        auto& cell = m_pages[p].Cells[r][c];
                        cell.HitPoints = hp;
                        cell.RandomPowerUp = (rndPwr != 0);
                        cell.SpecificPowerUp = static_cast<PowerUpType>(specPwr);
                        cell.TurretCooldown = cd;
                        cell.TurretInitialDelay = initD;
                        cell.TurretProjectileSpeed = pSpd;

                        // Optional extended fields
                        int useCol = 0, cr = 80, cg = 210, cb = 255;
                        int regenEn = 1;
                        float regenCd = 30.0f;
                        int rndEv = 1, specEv = static_cast<int>(ArenaEventType::MeteorShower), evOnHit = 0;
                        int bombTier = 1;
                        float metalBoost = 1.0f;
                        float tDmg = 34.0f;
                        int tAim = 0;
                        int lr = 255, lg = 235, lb = 130;
                        float lRad = 230.0f, lInt = 1.25f;

                        if (ls >> useCol >> cr >> cg >> cb >> regenEn >> regenCd >> rndEv >> specEv >> evOnHit
                               >> bombTier >> metalBoost >> tDmg >> tAim >> lr >> lg >> lb >> lRad >> lInt)
                        {
                            cell.UseCustomColor = (useCol != 0);
                            cell.CustomColor = Color{
                                static_cast<uint8_t>(std::clamp(cr, 0, 255)),
                                static_cast<uint8_t>(std::clamp(cg, 0, 255)),
                                static_cast<uint8_t>(std::clamp(cb, 0, 255)),
                                255
                            };
                            cell.PowerUpRegenEnabled = (regenEn != 0);
                            cell.PowerUpRegenCooldown = regenCd;
                            cell.RandomEvent = (rndEv != 0);
                            cell.SpecificEvent = static_cast<ArenaEventType>(specEv);
                            cell.EventTriggerOnHit = (evOnHit != 0);
                            cell.BombBlastTier = bombTier;
                            cell.MetalBounceBoost = metalBoost;
                            cell.TurretDamage = tDmg;
                            cell.TurretAimed = (tAim != 0);
                            cell.LanternLightColor = Color{
                                static_cast<uint8_t>(std::clamp(lr, 0, 255)),
                                static_cast<uint8_t>(std::clamp(lg, 0, 255)),
                                static_cast<uint8_t>(std::clamp(lb, 0, 255)),
                                255
                            };
                            cell.LanternLightRadius = lRad;
                            cell.LanternLightIntensity = lInt;
                        }
                        cell.ClampValues();
                    }
                }
            }
            else if (currentPage >= 0 && currentRow < GRID_ROWS)
            {
                for (size_t col = 0; col < line.size() && col < static_cast<size_t>(GRID_COLS); ++col)
                {
                    EditorTileType t = EditorTileType::Empty;
                    switch (line[col])
                    {
                    case '1': t = EditorTileType::Light; break;
                    case '2': t = EditorTileType::Medium; break;
                    case '3': t = EditorTileType::Hard; break;
                    case 'S': case 's': t = EditorTileType::Special; break;
                    case 'M': case 'm': case 'U': case 'u': t = EditorTileType::Metal; break;
                    case 'B': case 'b': case 'X': case 'x': t = EditorTileType::Bomb; break;
                    case 'T': case 't': t = EditorTileType::Turret; break;
                    case 'L': case 'l': t = EditorTileType::Lantern; break;
                    case 'E': case 'e': t = EditorTileType::Event; break;
                    default:  t = EditorTileType::Empty; break;
                    }
                    m_pages[currentPage].Cells[currentRow][col] = EditorCellData::CreateDefaultForType(t);
                }
                currentRow++;
            }
        }

        if (m_pages.empty())
        {
            EditorGridPage page1;
            page1.Name = "PAGE 1";
            m_pages.push_back(page1);
        }
    }

    bool SaveToFile(const std::string& path) const
    {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << ExportLevelV2();
        return true;
    }

    bool LoadFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::stringstream ss;
        ss << file.rdbuf();
        ImportLevelV2(ss.str());
        return true;
    }

    // Getters / Setters
    [[nodiscard]] const std::string& GetLevelId() const noexcept { return m_levelId; }
    void SetLevelId(const std::string& id) { m_levelId = id; }

    [[nodiscard]] const std::string& GetLevelName() const noexcept { return m_levelName; }
    void SetLevelName(const std::string& name) { m_levelName = name; }

    [[nodiscard]] EditorThemeData& GetTheme() noexcept { return m_theme; }
    [[nodiscard]] const EditorThemeData& GetTheme() const noexcept { return m_theme; }

    void ApplyLightingMode(LightingMode mode)
    {
        m_theme.Lighting = mode;
        if (mode == LightingMode::Atmospheric)
        {
            m_theme.AmbientColor = GetAmbientLightForLightingMode(LightingMode::Atmospheric);
            m_theme.AmbientIntensity = 0.52f;
        }
        else if (mode == LightingMode::Lit)
        {
            m_theme.AmbientColor = GetAmbientLightForLightingMode(LightingMode::Lit);
            m_theme.AmbientIntensity = 1.0f;
        }
        else if (mode == LightingMode::PitchBlack)
        {
            m_theme.AmbientColor = GetAmbientLightForLightingMode(LightingMode::PitchBlack);
            m_theme.AmbientIntensity = 0.10f;
        }
        m_theme.ClampValues();
    }

    [[nodiscard]] LightingMode GetLightingMode() const noexcept { return m_theme.Lighting; }

    [[nodiscard]] ArenaEventType GetInitialEvent() const noexcept { return m_initialEvent; }
    void SetInitialEvent(ArenaEventType ev) noexcept
    {
        if (ev > ArenaEventType::None && ev < ArenaEventType::Count)
        {
            m_initialEvent = ev;
        }
        else
        {
            m_initialEvent = ArenaEventType::None;
        }
    }

    void CycleInitialEvent(int delta)
    {
        const auto& catalog = GetArenaEventCatalog();
        const int total = static_cast<int>(catalog.size()) + 1; // index 0 = None
        int cur = 0;
        if (m_initialEvent != ArenaEventType::None)
        {
            for (size_t i = 0; i < catalog.size(); ++i)
            {
                if (catalog[i].Type == m_initialEvent)
                {
                    cur = static_cast<int>(i) + 1;
                    break;
                }
            }
        }
        const int next = (cur + delta % total + total) % total;
        m_initialEvent = (next == 0) ? ArenaEventType::None : catalog[static_cast<size_t>(next - 1)].Type;
    }

    [[nodiscard]] EditorGamerulesData& GetGamerules() noexcept { return m_rules; }
    [[nodiscard]] const EditorGamerulesData& GetGamerules() const noexcept { return m_rules; }

    [[nodiscard]] std::vector<EditorEventItem>& GetEventQueue() noexcept { return m_eventQueue; }
    [[nodiscard]] const std::vector<EditorEventItem>& GetEventQueue() const noexcept { return m_eventQueue; }

    [[nodiscard]] EventSortFilter GetEventFilter() const noexcept { return m_sortFilter; }

    [[nodiscard]] Snapshot CreateSnapshot() const
    {
        return Snapshot{m_pages, m_activePageIndex, m_eventQueue, m_initialEvent};
    }

    void RestoreSnapshot(const Snapshot& snap)
    {
        if (snap.Pages.empty()) return;
        m_pages = snap.Pages;
        m_activePageIndex = std::min(snap.ActivePageIndex, m_pages.size() - 1);
        m_eventQueue = snap.EventQueue;
        m_initialEvent = snap.InitialEvent;
    }

    // Compatibility helpers for active page raw grid
    [[nodiscard]] std::array<std::array<EditorTileType, GRID_COLS>, GRID_ROWS> GetRawGrid() const noexcept
    {
        std::array<std::array<EditorTileType, GRID_COLS>, GRID_ROWS> raw{};
        if (m_pages.empty()) return raw;
        for (int r = 0; r < GRID_ROWS; ++r)
        {
            for (int c = 0; c < GRID_COLS; ++c)
            {
                raw[r][c] = m_pages[m_activePageIndex].Cells[r][c].Type;
            }
        }
        return raw;
    }

    void SetRawGrid(const std::array<std::array<EditorTileType, GRID_COLS>, GRID_ROWS>& raw) noexcept
    {
        if (m_pages.empty()) return;
        for (int r = 0; r < GRID_ROWS; ++r)
        {
            for (int c = 0; c < GRID_COLS; ++c)
            {
                SetTile(c, r, raw[r][c]);
            }
        }
    }

private:
    std::string m_levelId{};
    std::string m_levelName{};
    std::vector<EditorGridPage> m_pages{};
    size_t m_activePageIndex = 0;
    std::array<EditorCellData, static_cast<size_t>(EditorTileType::Count)> m_palettePresets{};
    EditorThemeData m_theme{};
    EditorGamerulesData m_rules{};
    std::vector<EditorEventItem> m_eventQueue{};
    EventSortFilter m_sortFilter = EventSortFilter::Clockwise;
    ArenaEventType m_initialEvent = ArenaEventType::None;
};
