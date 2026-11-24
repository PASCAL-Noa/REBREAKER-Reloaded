#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <optional>
#include "Editor/LevelData.h"

struct CustomPreviewBrick
{
    int Col = 0;
    int Row = 0;
    Color Tint{255, 255, 255, 255};
};

struct CustomLevelEntry
{
    std::string Id;
    std::string Name;
    std::string FilePath;
    int PageCount = 1;
    int BrickCount = 0;
    Color BgColor{10, 14, 26, 255};
    std::vector<CustomPreviewBrick> PreviewBricks;
};

struct CustomPlaylist
{
    static constexpr size_t MAX_SLOTS = 10;

    std::string Id;
    std::string Name;
    std::vector<std::string> LevelIds;
};

class CustomLevelLibrary
{
public:
    explicit CustomLevelLibrary(std::string baseDir = "Resources/levels/custom", bool autoImportLegacy = true)
        : m_baseDir(std::move(baseDir))
        , m_autoImportLegacy(autoImportLegacy)
    {
        Load();
    }

    static std::string SanitizeName(const std::string& raw, const std::string& fallback = "NIVEAU")
    {
        std::string out;
        out.reserve(raw.size());
        for (char ch : raw)
        {
            if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t')
            {
                out.push_back(' ');
            }
            else if (static_cast<unsigned char>(ch) >= 32)
            {
                out.push_back(ch);
            }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back() == ' ') out.pop_back();
        if (out.size() > 22) out.resize(22);
        return out.empty() ? fallback : out;
    }

    [[nodiscard]] std::string PeekNextLevelId() const
    {
        return FormatId("RBK-", m_nextLevelCounter, 4);
    }

    [[nodiscard]] std::string PeekNextPlaylistId() const
    {
        return FormatId("LST-", m_nextPlaylistCounter, 2);
    }

    void Load()
    {
        m_levels.clear();
        m_playlists.clear();
        m_nextLevelCounter = 1;
        m_nextPlaylistCounter = 1;

        std::error_code ec;
        std::filesystem::create_directories(m_baseDir, ec);

        const std::string manifestPath = GetManifestPath();
        const bool manifestExists = std::filesystem::exists(manifestPath, ec);

        if (manifestExists)
        {
            std::ifstream in(manifestPath);
            std::string line;
            while (std::getline(in, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (line.empty() || line[0] == '#') continue;

                const auto parts = SplitPipe(line);
                if (parts.empty()) continue;

                if (parts[0] == "COUNTERS" && parts.size() >= 3)
                {
                    try
                    {
                        m_nextLevelCounter = std::max(1, std::stoi(parts[1]));
                        m_nextPlaylistCounter = std::max(1, std::stoi(parts[2]));
                    }
                    catch (...) {}
                }
                else if (parts[0] == "LEVEL" && parts.size() >= 4)
                {
                    CustomLevelEntry entry;
                    entry.Id = parts[1];
                    entry.Name = SanitizeName(parts[2], entry.Id);
                    entry.FilePath = parts[3];

                    LevelData data;
                    if (data.LoadFromFile(entry.FilePath))
                    {
                        RefreshEntryPreview(entry, data);
                        m_levels.push_back(std::move(entry));
                    }
                }
                else if (parts[0] == "PLAYLIST" && parts.size() >= 3)
                {
                    CustomPlaylist pl;
                    pl.Id = parts[1];
                    pl.Name = SanitizeName(parts[2], pl.Id);
                    if (parts.size() >= 4 && !parts[3].empty())
                    {
                        std::istringstream ss(parts[3]);
                        std::string lvlId;
                        while (std::getline(ss, lvlId, ','))
                        {
                            if (!lvlId.empty() && pl.LevelIds.size() < CustomPlaylist::MAX_SLOTS)
                            {
                                pl.LevelIds.push_back(lvlId);
                            }
                        }
                    }
                    m_playlists.push_back(std::move(pl));
                }
            }

            PruneOrphanedPlaylistSlots();
            EnsureCountersAboveExisting();
        }
        else if (m_autoImportLegacy)
        {
            ImportLegacyCustomLevels();
            Save();
        }
    }

    bool Save() const
    {
        std::error_code ec;
        std::filesystem::create_directories(m_baseDir, ec);

        std::ofstream out(GetManifestPath());
        if (!out.is_open()) return false;

        out << "#REBREAKER_ROBOSTOCK_V1\n";
        out << "COUNTERS|" << m_nextLevelCounter << "|" << m_nextPlaylistCounter << "\n";
        for (const auto& lvl : m_levels)
        {
            out << "LEVEL|" << lvl.Id << "|" << SanitizeName(lvl.Name, lvl.Id) << "|" << lvl.FilePath << "\n";
        }
        for (const auto& pl : m_playlists)
        {
            out << "PLAYLIST|" << pl.Id << "|" << SanitizeName(pl.Name, pl.Id) << "|";
            for (size_t i = 0; i < pl.LevelIds.size(); ++i)
            {
                if (i > 0) out << ",";
                out << pl.LevelIds[i];
            }
            out << "\n";
        }
        return true;
    }

    std::string SaveNewLevel(const std::string& rawName, LevelData& data)
    {
        EnsureCountersAboveExisting();
        const std::string id = FormatId("RBK-", m_nextLevelCounter++, 4);
        const std::string cleanName = SanitizeName(rawName, "NIVEAU " + std::to_string(m_levels.size() + 1));
        const std::string filePath = m_baseDir + "/" + id + ".txt";

        data.SetLevelId(id);
        data.SetLevelName(cleanName);

        std::error_code ec;
        std::filesystem::create_directories(m_baseDir, ec);
        if (!data.SaveToFile(filePath))
        {
            return "";
        }

        CustomLevelEntry entry;
        entry.Id = id;
        entry.Name = cleanName;
        entry.FilePath = filePath;
        RefreshEntryPreview(entry, data);
        m_levels.push_back(std::move(entry));
        Save();
        return id;
    }

    bool UpdateExistingLevel(const std::string& id, LevelData& data)
    {
        auto it = FindLevelIter(id);
        if (it == m_levels.end()) return false;

        if (data.GetLevelName().empty())
        {
            data.SetLevelName(it->Name);
        }
        else
        {
            it->Name = SanitizeName(data.GetLevelName(), it->Name);
            data.SetLevelName(it->Name);
        }
        data.SetLevelId(it->Id);

        if (!data.SaveToFile(it->FilePath)) return false;
        RefreshEntryPreview(*it, data);
        return Save();
    }

    bool RenameLevel(const std::string& id, const std::string& newName)
    {
        auto it = FindLevelIter(id);
        if (it == m_levels.end()) return false;

        it->Name = SanitizeName(newName, it->Name);
        LevelData data;
        if (data.LoadFromFile(it->FilePath))
        {
            data.SetLevelId(it->Id);
            data.SetLevelName(it->Name);
            data.SaveToFile(it->FilePath);
        }
        return Save();
    }

    bool DeleteLevel(const std::string& id)
    {
        auto it = FindLevelIter(id);
        if (it == m_levels.end()) return false;

        std::error_code ec;
        std::filesystem::remove(it->FilePath, ec);
        m_levels.erase(it);

        PruneOrphanedPlaylistSlots();
        return Save();
    }

    bool ReorderLevel(size_t fromIdx, size_t toIdx)
    {
        if (fromIdx >= m_levels.size() || toIdx >= m_levels.size() || fromIdx == toIdx)
        {
            return false;
        }
        CustomLevelEntry moved = std::move(m_levels[fromIdx]);
        m_levels.erase(m_levels.begin() + static_cast<std::ptrdiff_t>(fromIdx));
        m_levels.insert(m_levels.begin() + static_cast<std::ptrdiff_t>(toIdx), std::move(moved));
        return Save();
    }

    bool LoadLevelData(const std::string& id, LevelData& outData) const
    {
        const auto* entry = FindLevel(id);
        if (!entry) return false;
        if (!outData.LoadFromFile(entry->FilePath)) return false;
        outData.SetLevelId(entry->Id);
        outData.SetLevelName(entry->Name);
        return true;
    }

    [[nodiscard]] const CustomLevelEntry* FindLevel(const std::string& id) const
    {
        for (const auto& lvl : m_levels)
        {
            if (lvl.Id == id) return &lvl;
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<size_t> FindLevelIndex(const std::string& id) const
    {
        for (size_t i = 0; i < m_levels.size(); ++i)
        {
            if (m_levels[i].Id == id) return i;
        }
        return std::nullopt;
    }

    // Special Playlists (max 10 slots per list)
    std::string CreatePlaylist(const std::string& rawName = "")
    {
        EnsureCountersAboveExisting();
        CustomPlaylist pl;
        pl.Id = FormatId("LST-", m_nextPlaylistCounter++, 2);
        pl.Name = SanitizeName(rawName, "LISTE " + FormatNumber(static_cast<int>(m_playlists.size()) + 1, 2));
        m_playlists.push_back(pl);
        Save();
        return pl.Id;
    }

    bool RenamePlaylist(const std::string& playlistId, const std::string& newName)
    {
        auto it = FindPlaylistIter(playlistId);
        if (it == m_playlists.end()) return false;
        it->Name = SanitizeName(newName, it->Name);
        return Save();
    }

    bool DeletePlaylist(const std::string& playlistId)
    {
        auto it = FindPlaylistIter(playlistId);
        if (it == m_playlists.end()) return false;
        m_playlists.erase(it);
        return Save();
    }

    bool AddLevelToPlaylist(const std::string& playlistId, const std::string& levelId, std::optional<size_t> targetSlot = std::nullopt)
    {
        auto it = FindPlaylistIter(playlistId);
        if (it == m_playlists.end() || !FindLevel(levelId)) return false;

        if (targetSlot.has_value() && *targetSlot < it->LevelIds.size())
        {
            // Replace existing slot if dropped directly onto an occupied slot when full, or insert if space remains
            if (it->LevelIds.size() >= CustomPlaylist::MAX_SLOTS)
            {
                it->LevelIds[*targetSlot] = levelId;
                return Save();
            }
            it->LevelIds.insert(it->LevelIds.begin() + static_cast<std::ptrdiff_t>(*targetSlot), levelId);
            return Save();
        }

        if (it->LevelIds.size() >= CustomPlaylist::MAX_SLOTS)
        {
            return false;
        }

        it->LevelIds.push_back(levelId);
        return Save();
    }

    bool RemoveSlotFromPlaylist(const std::string& playlistId, size_t slotIdx)
    {
        auto it = FindPlaylistIter(playlistId);
        if (it == m_playlists.end() || slotIdx >= it->LevelIds.size()) return false;
        it->LevelIds.erase(it->LevelIds.begin() + static_cast<std::ptrdiff_t>(slotIdx));
        return Save();
    }

    bool ReorderPlaylistSlot(const std::string& playlistId, size_t fromSlot, size_t toSlot)
    {
        auto it = FindPlaylistIter(playlistId);
        if (it == m_playlists.end() || fromSlot >= it->LevelIds.size() || toSlot >= it->LevelIds.size() || fromSlot == toSlot)
        {
            return false;
        }
        std::string moved = std::move(it->LevelIds[fromSlot]);
        it->LevelIds.erase(it->LevelIds.begin() + static_cast<std::ptrdiff_t>(fromSlot));
        it->LevelIds.insert(it->LevelIds.begin() + static_cast<std::ptrdiff_t>(toSlot), std::move(moved));
        return Save();
    }

    [[nodiscard]] const CustomPlaylist* FindPlaylist(const std::string& id) const
    {
        for (const auto& pl : m_playlists)
        {
            if (pl.Id == id) return &pl;
        }
        return nullptr;
    }

    [[nodiscard]] const std::vector<CustomLevelEntry>& GetLevels() const noexcept { return m_levels; }
    [[nodiscard]] const std::vector<CustomPlaylist>& GetPlaylists() const noexcept { return m_playlists; }

    static void RefreshEntryPreview(CustomLevelEntry& entry, const LevelData& data)
    {
        entry.PageCount = static_cast<int>(data.GetPageCount());
        entry.BrickCount = data.GetTotalBrickCount();
        entry.BgColor = data.GetTheme().BgColor;
        entry.PreviewBricks.clear();

        size_t previewPage = 0;
        for (size_t p = 0; p < data.GetPageCount(); ++p)
        {
            if (data.GetBrickCount(p) > 0)
            {
                previewPage = p;
                break;
            }
        }

        for (int r = 0; r < LevelData::GRID_ROWS; ++r)
        {
            for (int c = 0; c < LevelData::GRID_COLS; ++c)
            {
                const EditorCellData cell = data.GetCell(c, r, previewPage);
                if (cell.Type != EditorTileType::Empty)
                {
                    CustomPreviewBrick pb;
                    pb.Col = c;
                    pb.Row = r;
                    pb.Tint = LevelData::GetDisplayColorForCell(cell);
                    entry.PreviewBricks.push_back(pb);
                }
            }
        }
    }

private:
    [[nodiscard]] std::string GetManifestPath() const
    {
        return m_baseDir + "/robostock.manifest";
    }

    static std::string FormatNumber(int value, int width)
    {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill('0') << std::max(0, value);
        return oss.str();
    }

    static std::string FormatId(const char* prefix, int value, int width)
    {
        return std::string(prefix) + FormatNumber(value, width);
    }

    static std::vector<std::string> SplitPipe(const std::string& str)
    {
        std::vector<std::string> parts;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, '|'))
        {
            parts.push_back(item);
        }
        if (!str.empty() && str.back() == '|')
        {
            parts.emplace_back();
        }
        return parts;
    }

    std::vector<CustomLevelEntry>::iterator FindLevelIter(const std::string& id)
    {
        return std::find_if(m_levels.begin(), m_levels.end(), [&](const CustomLevelEntry& e) {
            return e.Id == id;
        });
    }

    std::vector<CustomPlaylist>::iterator FindPlaylistIter(const std::string& id)
    {
        return std::find_if(m_playlists.begin(), m_playlists.end(), [&](const CustomPlaylist& p) {
            return p.Id == id;
        });
    }

    void PruneOrphanedPlaylistSlots()
    {
        for (auto& pl : m_playlists)
        {
            pl.LevelIds.erase(
                std::remove_if(pl.LevelIds.begin(), pl.LevelIds.end(), [&](const std::string& lid) {
                    return FindLevel(lid) == nullptr;
                }),
                pl.LevelIds.end());
        }
    }

    void EnsureCountersAboveExisting()
    {
        for (const auto& lvl : m_levels)
        {
            if (lvl.Id.rfind("RBK-", 0) == 0)
            {
                try
                {
                    int n = std::stoi(lvl.Id.substr(4));
                    m_nextLevelCounter = std::max(m_nextLevelCounter, n + 1);
                }
                catch (...) {}
            }
        }
        for (const auto& pl : m_playlists)
        {
            if (pl.Id.rfind("LST-", 0) == 0)
            {
                try
                {
                    int n = std::stoi(pl.Id.substr(4));
                    m_nextPlaylistCounter = std::max(m_nextPlaylistCounter, n + 1);
                }
                catch (...) {}
            }
        }
    }

    void ImportLegacyCustomLevels()
    {
        std::error_code ec;
        const std::filesystem::path legacyDir("Resources/levels");
        if (!std::filesystem::exists(legacyDir, ec)) return;

        std::vector<std::filesystem::path> found;
        for (const auto& dirEntry : std::filesystem::directory_iterator(legacyDir, ec))
        {
            if (!dirEntry.is_regular_file()) continue;
            const std::string fn = dirEntry.path().filename().string();
            if (fn.rfind("custom_level_", 0) == 0 && dirEntry.path().extension() == ".txt")
            {
                found.push_back(dirEntry.path());
            }
        }
        std::sort(found.begin(), found.end());

        for (const auto& p : found)
        {
            LevelData data;
            if (data.LoadFromFile(p.string()) && data.GetTotalBrickCount() > 0)
            {
                const std::string name = data.GetLevelName().empty()
                    ? ("NIVEAU " + FormatNumber(m_nextLevelCounter, 2))
                    : data.GetLevelName();
                SaveNewLevel(name, data);
            }
        }
    }

    std::string m_baseDir;
    bool m_autoImportLegacy = true;
    int m_nextLevelCounter = 1;
    int m_nextPlaylistCounter = 1;
    std::vector<CustomLevelEntry> m_levels;
    std::vector<CustomPlaylist> m_playlists;
};
