#include "Generators/FileLevelGenerator.h"
#include "Factories/BrickFactory.h"
#include <fstream>

FileLevelGenerator::FileLevelGenerator(const std::string& filepath) : m_filepath(filepath) {}

int FileLevelGenerator::Generate(Registry& registry, GameContext& context, uint32_t brickTexId)
{
    (void)context;
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
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        {
            line.pop_back();
        }

        for (size_t col = 0; col < line.length(); ++col)
        {
            char c = line[col];
            if (c == '0' || c == ' ') continue;

            BrickType type;
            bool isSpecial = false;

            if (c == '1')
            {
                type = BrickType::Light;
            }
            else if (c == '2')
            {
                type = BrickType::Medium;
            }
            else if (c == '3')
            {
                type = BrickType::Hard;
            }
            else if (c == 'S')
            {
                type = BrickType::Special;
                isSpecial = true;
            }
            else
            {
                // Ignore any character that does not strictly match an allowed tile type
                continue;
            }

            float x = startX + static_cast<float>(col) * (bWidth + pad);
            float y = startY + static_cast<float>(row) * (bHeight + pad);

            BrickFactory::Create(registry, x, y, type, isSpecial, brickTexId);
            brickCount++;
        }
        row++;
    }
    return brickCount;
}

void FileLevelGenerator::Update(float dt, Registry& registry, GameContext& context)
{
    (void)dt;
    (void)registry;
    (void)context;
}