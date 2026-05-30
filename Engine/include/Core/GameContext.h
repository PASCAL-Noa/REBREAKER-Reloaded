#pragma once

class   InputManager;
class   Renderer;
class   ResourceManager;
struct  GameData;

struct GameContext
{
    InputManager&       Input;
    Renderer&           Render;
    ResourceManager&    Resources;
    GameData&           Data;
};