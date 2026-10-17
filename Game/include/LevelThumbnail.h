#pragma once

#include "Data/Color.h"
#include "LevelManager.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>

struct LevelBrickPreview
{
    float RelX = 0.0f;
    float RelY = 0.0f;
    float W = 0.0f;
    float H = 0.0f;
    Color Tint = Colors::White;
};

struct LevelThumbnailData
{
    std::vector<LevelBrickPreview> Bricks;
    int BrickCount = 0;
};

inline std::vector<LevelThumbnailData> LoadAllLevelThumbnails(const LevelManager& levelManager)
{
    std::vector<LevelThumbnailData> thumbnails;
    const int count = levelManager.GetLevelCount();
    thumbnails.resize(static_cast<size_t>(count));

    for (int lvlIdx = 0; lvlIdx < count; ++lvlIdx)
    {
        const std::string& path = levelManager.GetLevelPath(lvlIdx);
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        size_t maxCols = 0;
        while (std::getline(file, line))
        {
            if (line.empty()) continue;
            lines.push_back(line);
            maxCols = std::max(maxCols, line.length());
        }

        if (lines.empty() || maxCols == 0) continue;

        const size_t totalRows = lines.size();
        constexpr float maxThumbW = 180.0f;
        constexpr float maxThumbH = 75.0f;
        constexpr float gap = 2.0f;

        float brickW = (maxThumbW - static_cast<float>(maxCols - 1) * gap) / static_cast<float>(maxCols);
        float brickH = (maxThumbH - static_cast<float>(totalRows - 1) * gap) / static_cast<float>(totalRows);

        brickW = std::clamp(brickW, 4.0f, 18.0f);
        brickH = std::clamp(brickH, 3.0f, 10.0f);

        const float actualW = static_cast<float>(maxCols) * brickW + static_cast<float>(maxCols - 1) * gap;
        const float actualH = static_cast<float>(totalRows) * brickH + static_cast<float>(totalRows - 1) * gap;

        const float startRelX = -actualW * 0.5f + brickW * 0.5f;
        const float startRelY = -actualH * 0.5f + brickH * 0.5f;

        auto& thumb = thumbnails[static_cast<size_t>(lvlIdx)];

        for (size_t r = 0; r < totalRows; ++r)
        {
            const auto& rowStr = lines[r];
            for (size_t c = 0; c < rowStr.length(); ++c)
            {
                const char ch = rowStr[c];
                if (ch == '0' || ch == ' ') continue;

                Color brickColor = Colors::Green;
                if (ch == '1') brickColor = Colors::Green;
                else if (ch == '2') brickColor = Colors::Yellow;
                else if (ch == '3') brickColor = Colors::Red;
                else if (ch == 'S' || ch == 's') brickColor = Color{255, 0, 255, 255};
                else if (ch == 'M' || ch == 'm' || ch == 'U' || ch == 'u') brickColor = Color{180, 195, 210, 255};
                else if (ch == 'B' || ch == 'b' || ch == 'X' || ch == 'x') brickColor = Color{255, 80, 20, 255};
                else continue;

                LevelBrickPreview brick;
                brick.RelX = startRelX + static_cast<float>(c) * (brickW + gap);
                brick.RelY = startRelY + static_cast<float>(r) * (brickH + gap);
                brick.W = brickW;
                brick.H = brickH;
                brick.Tint = brickColor;

                thumb.Bricks.push_back(brick);
                if (ch != 'M' && ch != 'm' && ch != 'U' && ch != 'u')
                {
                    thumb.BrickCount++;
                }
            }
        }
    }
    return thumbnails;
}
