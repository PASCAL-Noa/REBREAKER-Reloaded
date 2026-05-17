#pragma once

template <>
inline sf::Texture* ResourceManager::Get<sf::Texture>(uint32_t id) const
{
    auto it = m_textures.find(id);
    return (it != m_textures.end()) ? it->second : nullptr;
}

template <>
inline sf::Shader* ResourceManager::Get<sf::Shader>(uint32_t id) const
{
    auto it = m_shaders.find(id);
    return (it != m_shaders.end()) ? it->second : nullptr;
}

template <>
inline sf::Font* ResourceManager::Get<sf::Font>(uint32_t id) const
{
    auto it = m_fonts.find(id);
    return (it != m_fonts.end()) ? it->second : nullptr;
}

template <>
inline sf::SoundBuffer* ResourceManager::Get<sf::SoundBuffer>(uint32_t id) const
{
    auto it = m_soundBuffers.find(id);
    return (it != m_soundBuffers.end()) ? it->second : nullptr;
}