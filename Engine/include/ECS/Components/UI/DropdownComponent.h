#pragma once
#include "ECS/Entity.h"
#include <vector>
#include <string>
#include <functional>

struct DropdownComponent
{
    int     ScrollOffset = 0;
    int     MaxVisible = 5;
    std::vector<std::string>    Options;
    std::vector<Entity>     OptionButtons;
    Entity  HeadButton = NULL_ENTITY;
    std::function<void(int, std::string)>   OnSelect = nullptr;
    Entity  ScrollbarFill = NULL_ENTITY;
};
