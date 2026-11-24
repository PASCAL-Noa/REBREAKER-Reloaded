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

enum class EditorTileType : uint8_t
{
    Empty = 0,
    Light = 1,     // 1+ HP
    Medium = 2,    // 2+ HP
    Hard = 3,      // 3+ HP
    Special = 4,   // Capsule drop
    Metal = 5,     // Indestructible
    Bomb = 6,      // Explosive
    Turret = 7,    // Shoots downward triangles
    Lantern = 8,   // Light emitter
    Count = 9
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
    Count = 5
};

struct EditorCellData
{
    EditorTileType Type = EditorTileType::Empty;
    int HitPoints = 1;                 // 1..9 PV for destructible bricks
    bool RandomPowerUp = true;         // Special brick: random vs specific capsule
    PowerUpType SpecificPowerUp = PowerUpType::MultiBall;
    float TurretCooldown = 3.2f;       // Firing cadence in seconds (Campaign default: 3.2s)
    float TurretInitialDelay = 3.5f;   // Initial delay before first shot (Campaign default: 3.5s)
    float TurretProjectileSpeed = 210.0f; // Triangle missile speed px/s (Campaign default: 210)

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
        case EditorTileType::Special: cell.HitPoints = 1; cell.RandomPowerUp = true; cell.SpecificPowerUp = PowerUpType::MultiBall; break;
        case EditorTileType::Metal:   cell.HitPoints = 999999; break;
        case EditorTileType::Bomb:    cell.HitPoints = 1; break;
        case EditorTileType::Turret:
            cell.HitPoints = 3;
            cell.TurretCooldown = 3.2f;
            cell.TurretInitialDelay = 3.5f;
            cell.TurretProjectileSpeed = 210.0f;
            break;
        case EditorTileType::Lantern: cell.HitPoints = 1; break;
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
            HitPoints = 1;
        }
        else
        {
            HitPoints = std::clamp(HitPoints, 1, 9);
        }
        TurretCooldown = std::clamp(TurretCooldown, 0.5f, 10.0f);
        TurretInitialDelay = std::clamp(TurretInitialDelay, 0.5f, 12.0f);
        TurretProjectileSpeed = std::clamp(TurretProjectileSpeed, 100.0f, 450.0f);
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
    std::string Name = "HAZARD SURGE";
    float TriggerDelay = 15.0f;
    Color ThemeColor{255, 75, 75, 255};
};

struct EditorThemeData
{
    EditorThemePreset Preset = EditorThemePreset::DeepSpace;
    std::string Name = "DEEP SPACE";
    Color BgColor{10, 14, 26, 255};
    Color AmbientColor{45, 75, 140, 255};
    float AmbientIntensity = 0.38f;
    Color PaddleConeColor{80, 200, 255, 255};
    float PaddleConeAngle = 48.0f;
    std::string MusicTrack = "Resources/audio/music/Game-1.ogg";
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
            item.Name = "METEORES";
            item.ThemeColor = Color{255, 75, 75, 255};
            break;
        case EditorEventType::LaserSweep:
            item.Name = "GRAVITE LOURDE";
            item.ThemeColor = Color{255, 160, 40, 255};
            break;
        case EditorEventType::DarknessEclipse:
            item.Name = "ECLIPSE";
            item.ThemeColor = Color{150, 75, 255, 255};
            break;
        case EditorEventType::TurretOvercharge:
            item.Name = "EMP LOCKDOWN";
            item.ThemeColor = Color{255, 45, 95, 255};
            break;
        case EditorEventType::BossAlert:
            item.Name = "SEISME";
            item.ThemeColor = Color{255, 215, 0, 255};
            break;
        case EditorEventType::SpeedBoost:
            item.Name = "FLUX TEMPOREL";
            item.ThemeColor = Color{65, 225, 255, 255};
            break;
        default:
            break;
        }
        return item;
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
        case PowerUpType::MultiBall:     return "MULTI-BALL";
        case PowerUpType::ExpandPaddle:  return "AGRANDIR PADDLE";
        case PowerUpType::ShrinkPaddle:  return "RETRECIR (MALUS)";
        case PowerUpType::Storm:         return "TEMPETE / LASER";
        case PowerUpType::TempoBall:     return "TEMPO BALL";
        case PowerUpType::ExtraLife:     return "VIE SUPP.";
        case PowerUpType::BigBall:       return "GROSSE BALLE";
        case PowerUpType::FireBall:      return "BALLE DE FEU";
        case PowerUpType::StickyPaddle:  return "PADDLE COLLANT";
        case PowerUpType::BottomBarrier: return "BARRIERE SOL";
        case PowerUpType::MagnetPaddle:  return "AIMANT";
        case PowerUpType::GhostBall:     return "BALLE FANTOME";
        case PowerUpType::ItemRegen:     return "REGEN ITEM";
        case PowerUpType::GlitchMalus:   return "GLITCH (MALUS)";
        default:                         return "POWER-UP";
        }
    }

    static Color GetColorForHitPoints(int hitPoints)
    {
        const int clamped = std::clamp(hitPoints, 1, 9);
        switch (clamped)
        {
        case 1:  return Color{65, 225, 95, 255};   // 1 PV : Emerald Green
        case 2:  return Color{250, 215, 45, 255};  // 2 PV : Solar Yellow
        case 3:  return Color{245, 65, 65, 255};   // 3 PV : Crimson Red
        case 4:  return Color{255, 135, 35, 255};  // 4 PV : Blaze Orange
        case 5:  return Color{40, 225, 255, 255};  // 5 PV : Electric Cyan
        case 6:  return Color{55, 125, 255, 255};  // 6 PV : Royal Azure
        case 7:  return Color{175, 70, 255, 255};  // 7 PV : Plasma Violet
        case 8:  return Color{255, 65, 195, 255};  // 8 PV : Neon Magenta
        case 9:  return Color{255, 235, 140, 255}; // 9 PV : Radiant Diamond Gold
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
            return GetColorForHitPoints(cell.HitPoints);
        case EditorTileType::Special:
            return Color{230, 50, 240, 255};
        case EditorTileType::Metal:
            return Color{160, 175, 195, 255};
        case EditorTileType::Bomb:
            return Color{255, 100, 30, 255};
        case EditorTileType::Turret:
            return (cell.HitPoints > 3) ? GetColorForHitPoints(cell.HitPoints) : Color{255, 45, 55, 255};
        case EditorTileType::Lantern:
            return Color{255, 235, 120, 255};
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

    void ApplyThemePreset(EditorThemePreset preset)
    {
        m_theme.Preset = preset;
        switch (preset)
        {
        case EditorThemePreset::DeepSpace:
            m_theme.Name = "DEEP SPACE";
            m_theme.BgColor = Color{10, 14, 26, 255};
            m_theme.AmbientColor = Color{45, 75, 140, 255};
            m_theme.AmbientIntensity = 0.38f;
            m_theme.PaddleConeColor = Color{80, 200, 255, 255};
            m_theme.MusicTrack = "Resources/audio/music/Game-1.ogg";
            break;
        case EditorThemePreset::CyberNeon:
            m_theme.Name = "CYBER NEON";
            m_theme.BgColor = Color{18, 10, 26, 255};
            m_theme.AmbientColor = Color{130, 40, 150, 255};
            m_theme.AmbientIntensity = 0.42f;
            m_theme.PaddleConeColor = Color{255, 65, 190, 255};
            m_theme.MusicTrack = "Resources/audio/music/Game-2.ogg";
            break;
        case EditorThemePreset::SolarFlare:
            m_theme.Name = "SOLAR FLARE";
            m_theme.BgColor = Color{26, 14, 10, 255};
            m_theme.AmbientColor = Color{150, 85, 30, 255};
            m_theme.AmbientIntensity = 0.45f;
            m_theme.PaddleConeColor = Color{255, 185, 45, 255};
            m_theme.MusicTrack = "Resources/audio/music/Game-3.ogg";
            break;
        case EditorThemePreset::ToxicMatrix:
            m_theme.Name = "TOXIC MATRIX";
            m_theme.BgColor = Color{8, 22, 14, 255};
            m_theme.AmbientColor = Color{35, 130, 65, 255};
            m_theme.AmbientIntensity = 0.40f;
            m_theme.PaddleConeColor = Color{65, 255, 135, 255};
            m_theme.MusicTrack = "Resources/audio/music/Game-4.ogg";
            break;
        case EditorThemePreset::VoidAbyss:
            m_theme.Name = "VOID ABYSS";
            m_theme.BgColor = Color{6, 6, 10, 255};
            m_theme.AmbientColor = Color{22, 22, 35, 255};
            m_theme.AmbientIntensity = 0.20f;
            m_theme.PaddleConeColor = Color{220, 235, 255, 255};
            m_theme.MusicTrack = "Resources/audio/music/Game-1.ogg";
            break;
        default:
            break;
        }
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
            else if (roll < 0.88f)
            {
                type = EditorTileType::Hard;
            }
            else if (roll < 0.94f)
            {
                type = (difficulty > 0.4f) ? EditorTileType::Turret : EditorTileType::Special;
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
                default:  t = EditorTileType::Empty; break;
                }
                SetTile(static_cast<int>(col), row, t);
            }
            row++;
        }
    }

    // Full V2 Multi-Page + Cell Metadata Serialization
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
                            << cell.TurretProjectileSpeed << "\n";
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
            ClearAllPages();
            ImportFromAscii(text);
            return;
        }

        m_levelId.clear();
        m_levelName.clear();
        m_pages.clear();
        m_eventQueue.clear();
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

    [[nodiscard]] EditorGamerulesData& GetGamerules() noexcept { return m_rules; }
    [[nodiscard]] const EditorGamerulesData& GetGamerules() const noexcept { return m_rules; }

    [[nodiscard]] std::vector<EditorEventItem>& GetEventQueue() noexcept { return m_eventQueue; }
    [[nodiscard]] const std::vector<EditorEventItem>& GetEventQueue() const noexcept { return m_eventQueue; }

    [[nodiscard]] EventSortFilter GetEventFilter() const noexcept { return m_sortFilter; }

    [[nodiscard]] Snapshot CreateSnapshot() const
    {
        return Snapshot{m_pages, m_activePageIndex, m_eventQueue};
    }

    void RestoreSnapshot(const Snapshot& snap)
    {
        if (snap.Pages.empty()) return;
        m_pages = snap.Pages;
        m_activePageIndex = std::min(snap.ActivePageIndex, m_pages.size() - 1);
        m_eventQueue = snap.EventQueue;
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
};
