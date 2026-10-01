#include "Resources/ResourceManager.h"
#include <functional>
#include <iostream>
#include <algorithm>
#include <cctype>

ResourceManager::ResourceManager()
{
    InitFallbacks();
}

ResourceManager::~ResourceManager() = default;

ResourceManager::ResourceManager(ResourceManager&&) noexcept = default;
ResourceManager& ResourceManager::operator=(ResourceManager&&) noexcept = default;

void ResourceManager::InitFallbacks()
{
    // 2x2 magenta/black checkerboard
    // Row 0: Magenta, Black
    // Row 1: Black, Magenta
    const std::uint8_t checkerboardPixels[16] = {
        255,   0, 255, 255,      0,   0,   0, 255,
          0,   0,   0, 255,    255,   0, 255, 255
    };
    sf::Image checkerImage(sf::Vector2u(2, 2), checkerboardPixels);
    m_fallbackTexture = std::make_unique<sf::Texture>();
    if (m_fallbackTexture->loadFromImage(checkerImage))
    {
        m_fallbackTexture->setRepeated(true);
        m_fallbackTexture->setSmooth(false);
    }

    // Silent audio buffer (mono, 44100Hz, 64 silent samples)
    const std::int16_t silentSamples[64] = {0};
    const std::vector<sf::SoundChannel> channelMap = { sf::SoundChannel::Mono };
    m_fallbackSoundBuffer = std::make_unique<sf::SoundBuffer>();
    (void)m_fallbackSoundBuffer->loadFromSamples(silentSamples, 64, 1, 44100, channelMap);
}

sf::Texture* ResourceManager::GetFallbackTexture() const
{
    return m_fallbackTexture ? m_fallbackTexture.get() : nullptr;
}

sf::SoundBuffer* ResourceManager::GetFallbackSoundBuffer() const
{
    return m_fallbackSoundBuffer ? m_fallbackSoundBuffer.get() : nullptr;
}

uint32_t ResourceManager::GetResourceId(const std::string& filepath)
{
    return static_cast<uint32_t>(std::hash<std::string>{}(filepath));
}

std::string ResourceManager::GetExtension(const std::string& filepath)
{
    const size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) return "";
    std::string ext = filepath.substr(dotPos + 1);
    for (char& c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

uint32_t ResourceManager::LoadResource(const std::string& filepath)
{
    if (filepath.empty())
    {
        std::cerr << "[ResourceManager] Warning: Attempted to load resource with empty filepath.\n";
        return 0;
    }

    const uint32_t id = GetResourceId(filepath);
    const std::string ext = GetExtension(filepath);

    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
        LoadTexture(id, filepath);
    else if (ext == "frag" || ext == "vert")
        LoadShader(id, filepath);
    else if (ext == "ttf" || ext == "otf")
        LoadFont(id, filepath);
    else if (ext == "wav" || ext == "ogg" || ext == "flac" || ext == "mp3")
        LoadSoundBuffer(id, filepath);
    else
        std::cerr << "[ResourceManager] Warning: Unsupported resource extension for \"" << filepath << "\".\n";

    return id;
}

bool ResourceManager::LoadTexture(const uint32_t id, const std::string& filepath)
{
    if (m_textures.contains(id)) return m_textures[id] != nullptr;

    auto tex = std::make_unique<sf::Texture>();
    if (tex->loadFromFile(filepath))
    {
        m_textures[id] = std::move(tex);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load texture \"" << filepath << "\". Using fallback checkerboard.\n";
    m_textures[id] = nullptr;
    return false;
}

bool ResourceManager::LoadShader(const uint32_t id, const std::string& filepath)
{
    if (m_shaders.contains(id)) return m_shaders[id] != nullptr;

    auto shader = std::make_unique<sf::Shader>();
    sf::Shader::Type type = (GetExtension(filepath) == "frag") ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
    if (shader->loadFromFile(filepath, type))
    {
        m_shaders[id] = std::move(shader);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load shader \"" << filepath << "\".\n";
    m_shaders[id] = nullptr;
    return false;
}

bool ResourceManager::LoadFont(uint32_t id, const std::string& filepath)
{
    if (m_fonts.contains(id)) return m_fonts[id] != nullptr;

    auto font = std::make_unique<sf::Font>();
    if (font->openFromFile(filepath))
    {
        m_fonts[id] = std::move(font);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load font \"" << filepath << "\".\n";
    m_fonts[id] = nullptr;
    return false;
}

bool ResourceManager::LoadSoundBuffer(uint32_t id, const std::string& filepath)
{
    if (m_soundBuffers.contains(id)) return m_soundBuffers[id] != nullptr;

    auto buf = std::make_unique<sf::SoundBuffer>();
    if (buf->loadFromFile(filepath))
    {
        m_soundBuffers[id] = std::move(buf);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load sound buffer \"" << filepath << "\". Using silent fallback.\n";
    m_soundBuffers[id] = nullptr;
    return false;
}

void ResourceManager::Clear()
{
    m_textures.clear();
    m_shaders.clear();
    m_fonts.clear();
    m_soundBuffers.clear();
}