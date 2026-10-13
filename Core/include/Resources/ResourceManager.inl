#pragma once
#include <shared_mutex>

template <>
inline sf::Texture* ResourceManager::Get<sf::Texture>(uint32_t id) const
{
    if (id == 0) return nullptr;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);
    auto it = m_textures.find(id);
    if (it != m_textures.end() && it->second != nullptr)
        return it->second.get();
    return m_fallbackTexture ? m_fallbackTexture.get() : nullptr;
}

template <>
inline sf::Shader* ResourceManager::Get<sf::Shader>(uint32_t id) const
{
    if (id == 0) return nullptr;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);
    auto it = m_shaders.find(id);
    return (it != m_shaders.end() && it->second != nullptr) ? it->second.get() : nullptr;
}

template <>
inline sf::Font* ResourceManager::Get<sf::Font>(uint32_t id) const
{
    if (id == 0) return nullptr;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);
    auto it = m_fonts.find(id);
    return (it != m_fonts.end() && it->second != nullptr) ? it->second.get() : nullptr;
}

template <>
inline sf::SoundBuffer* ResourceManager::Get<sf::SoundBuffer>(uint32_t id) const
{
    if (id == 0) return nullptr;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);
    auto it = m_soundBuffers.find(id);
    if (it != m_soundBuffers.end() && it->second != nullptr)
        return it->second.get();
    return m_fallbackSoundBuffer ? m_fallbackSoundBuffer.get() : nullptr;
}

template <>
inline std::string* ResourceManager::Get<std::string>(uint32_t id) const
{
    if (id == 0) return nullptr;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);
    auto it = m_textFiles.find(id);
    return (it != m_textFiles.end()) ? const_cast<std::string*>(&it->second) : nullptr;
}