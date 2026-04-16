#pragma once
#include <cstdint>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();

    uint32_t                                        LoadResource(const std::string& filepath);

    template <typename T> T*                        Get(uint32_t id) const;

    void                                            Clear();

private:
    [[nodiscard]] static uint32_t                   GetResourceId(const std::string& filepath);
    [[nodiscard]] static std::string                GetExtension(const std::string& filepath);

    bool                                            LoadTexture(uint32_t id, const std::string& filepath);
    bool                                            LoadShader(uint32_t id, const std::string& filepath);
    bool                                            LoadFont(uint32_t id, const std::string& filepath);
    bool                                            LoadSoundBuffer(uint32_t id, const std::string& filepath);

    std::unordered_map<uint32_t, sf::Texture*>      m_textures;
    std::unordered_map<uint32_t, sf::Shader*>       m_shaders;
    std::unordered_map<uint32_t, sf::Font*>         m_fonts;
    std::unordered_map<uint32_t, sf::SoundBuffer*>  m_soundBuffers;
};

#include "ResourceManager.inl"