#include "Generators/FileLevelGenerator.h"
#include "Factories/BrickFactory.h"
#include "Resources/ResourceManager.h"
#include "Core/GameContext.h"
#include <fstream>
#include <sstream>

FileLevelGenerator::FileLevelGenerator(const std::string& filepath) : m_filepath(filepath) {}

int FileLevelGenerator::Generate(Registry& registry, GameContext& context, uint32_t brickTexId)
{
    std::string levelContent;
    uint32_t resId = ResourceManager::GetResourceId(m_filepath);
    std::string* cached = context.Resources.Get<std::string>(resId);
    if (cached && !cached->empty())
    {
        levelContent = *cached;
    }
    else
    {
        std::ifstream file(m_filepath);
        if (!file.is_open()) return 0;
        std::stringstream ss;
        ss << file.rdbuf();
        levelContent = ss.str();
    }

    std::istringstream stream(levelContent);
    int brickCount = 0;
    std::string line;
    
    const float bWidth = 100.0f;
    const float bHeight = 30.0f;
    const float startX = -605.0f;
    const float startY = -350.0f;
    const float pad = 10.0f;

    int row = 0;
    while (std::getline(stream, line))
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
            else if (c == 'S' || c == 's')
            {
                type = BrickType::Special;
                isSpecial = true;
            }
            else if (c == 'M' || c == 'm' || c == 'U' || c == 'u')
            {
                type = BrickType::Metal;
            }
            else if (c == 'B' || c == 'b' || c == 'X' || c == 'x')
            {
                type = BrickType::Bomb;
            }
            else if (c == 'L' || c == 'l')
            {
                type = BrickType::Lantern;
            }
            else
            {
                // Ignore any character that does not strictly match an allowed tile type
                continue;
            }

            float x = startX + static_cast<float>(col) * (bWidth + pad);
            float y = startY + static_cast<float>(row) * (bHeight + pad);

            BrickFactory::Create(registry, x, y, type, isSpecial, brickTexId);
            if (type != BrickType::Metal && type != BrickType::Special)
            {
                brickCount++;
            }
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