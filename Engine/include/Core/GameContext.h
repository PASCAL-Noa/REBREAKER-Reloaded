#pragma once

class   InputManager;
class   Renderer;
class   ResourceManager;
class   SceneManager;
class   AudioMixer;
class   EventDispatcher;
struct  GameData;


struct GameContext
{
    InputManager&       Input;
    Renderer&           Render;
    ResourceManager&    Resources;
    GameData&           Data;
    SceneManager&       Scenes;
    AudioMixer&         Audio;
    EventDispatcher&    Events;
};