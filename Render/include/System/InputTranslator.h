#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include "Core/InputManager.h"

namespace InputTranslator
{
    KeyCode     TranslateKey(sf::Keyboard::Key key);
    MouseButton TranslateMouseButton(sf::Mouse::Button button);
}