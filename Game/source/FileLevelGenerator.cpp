#include "Generators/FileLevelGenerator.h"
#include "Factories/BrickFactory.h"
#include <fstream>

FileLevelGenerator::FileLevelGenerator(const std::string& filepath) : m_filepath(filepath) {}

int FileLevelGenerator::Generate(Registry& registry, GameContext& context, uint32_t brickTexId)
{
    std::ifstream file(m_filepath);
    if (!file.is_open()) return 0;

    int brickCount = 0;
    std::string line;
    
    const float bWidth = 100.0f;
    const float bHeight = 30.0f;
    const float startX = -605.0f;
    const float startY = -350.0f;
    const float pad = 10.0f;

    int row = 0;
    while (std::getline(file, line))
    {
        for (int col = 0; col < line.length(); ++col)
        {
            char c = line[col];
            if (c == '0' || c == ' ') continue;

            float x = startX + col * (bWidth + pad);
            float y = startY + row * (bHeight + pad);

            BrickType type = BrickType::Light;
            bool isSpecial = false;

            if (c == '1') type = BrickType::Light;
            else if (c == '2') type = BrickType::Medium;
            else if (c == '3') type = BrickType::Hard;
            else if (c == 'S') { type = BrickType::Special; isSpecial = true; }

            BrickFactory::Create(registry, x, y, type, isSpecial, brickTexId);
            brickCount++;
        }
        row++;
    }
    return brickCount;
}

void FileLevelGenerator::Update(float dt, Registry& registry, GameContext& context)
{
}