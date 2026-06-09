#pragma once
#include "ILevelGenerator.h"
#include <string>

class FileLevelGenerator : public ILevelGenerator
{
public:
    explicit FileLevelGenerator(const std::string& filepath);
    
    int     Generate(Registry& registry, GameContext& context, uint32_t brickTexId) override;
    void    Update(float dt, Registry& registry, GameContext& context) override;

private:
    std::string     m_filepath;
};