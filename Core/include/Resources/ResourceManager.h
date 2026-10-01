#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept;
    ResourceManager& operator=(ResourceManager&&) noexcept;

    uint32_t                                        LoadResource(const std::string& filepath);
    template <typename T> T*                        Get(uint32_t id) const;

    [[nodiscard]] sf::Texture*                      GetFallbackTexture() const;
    [[nodiscard]] sf::SoundBuffer*                  GetFallbackSoundBuffer() const;

    void                                            Clear();

private:
    [[nodiscard]] static uint32_t                   GetResourceId(const std::string& filepath);
    [[nodiscard]] static std::string                GetExtension(const std::string& filepath);

    void                                            InitFallbacks();

    bool                                            LoadTexture(uint32_t id, const std::string& filepath);
    bool                                            LoadShader(uint32_t id, const std::string& filepath);
    bool                                            LoadFont(uint32_t id, const std::string& filepath);
    bool                                            LoadSoundBuffer(uint32_t id, const std::string& filepath);

    std::unordered_map<uint32_t, std::unique_ptr<sf::Texture>>      m_textures;
    std::unordered_map<uint32_t, std::unique_ptr<sf::Shader>>       m_shaders;
    std::unordered_map<uint32_t, std::unique_ptr<sf::Font>>         m_fonts;
    std::unordered_map<uint32_t, std::unique_ptr<sf::SoundBuffer>>  m_soundBuffers;

    std::unique_ptr<sf::Texture>                    m_fallbackTexture;
    std::unique_ptr<sf::SoundBuffer>                m_fallbackSoundBuffer;
};

#include "ResourceManager.inl"
