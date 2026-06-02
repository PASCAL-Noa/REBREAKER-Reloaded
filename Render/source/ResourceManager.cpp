#include "Resources/ResourceManager.h"
#include <functional>
#include <ranges>

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager()
{
    Clear();
}

uint32_t ResourceManager::GetResourceId(const std::string& filepath)
{
    return static_cast<uint32_t>(std::hash<std::string>{}(filepath));
}

std::string ResourceManager::GetExtension(const std::string& filepath)
{
    const size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) return "";
    return filepath.substr(dotPos + 1);
}

uint32_t ResourceManager::LoadResource(const std::string& filepath)
{
    const uint32_t id = GetResourceId(filepath);
    const std::string ext = GetExtension(filepath);

    if (ext == "png" || ext == "jpg" || ext == "bmp")
        LoadTexture(id, filepath);
    else if (ext == "frag" || ext == "vert")
        LoadShader(id, filepath);
    else if (ext == "ttf" || ext == "otf")
        LoadFont(id, filepath);
    else if (ext == "wav" || ext == "ogg" || ext == "flac" || ext == "mp3")
        LoadSoundBuffer(id, filepath);

    return id;
}

bool ResourceManager::LoadTexture(const uint32_t id, const std::string& filepath)
{
    if (m_textures.contains(id)) return true;
    sf::Texture* tex = new sf::Texture();
    if (tex->loadFromFile(filepath)) { m_textures[id] = tex; return true; }
    delete tex; return false;
}

bool ResourceManager::LoadShader(const uint32_t id, const std::string& filepath)
{
    if (m_shaders.contains(id)) return true;
    sf::Shader* shader = new sf::Shader();
    sf::Shader::Type type = (GetExtension(filepath) == "frag") ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
    if (shader->loadFromFile(filepath, type)) { m_shaders[id] = shader; return true; }
    delete shader; return false;
}

bool ResourceManager::LoadFont(uint32_t id, const std::string& filepath)
{
    if (m_fonts.contains(id)) return true;
    sf::Font* font = new sf::Font();
    if (font->openFromFile(filepath)) { m_fonts[id] = font; return true; }
    delete font; return false;
}

bool ResourceManager::LoadSoundBuffer(uint32_t id, const std::string& filepath)
{
    if (m_soundBuffers.contains(id)) return true;
    sf::SoundBuffer* buf = new sf::SoundBuffer();
    if (buf->loadFromFile(filepath)) { m_soundBuffers[id] = buf; return true; }
    delete buf; return false;
}

void ResourceManager::Clear()
{
    for (auto &val: m_textures | std::views::values) delete val;
    m_textures.clear();

    for (auto &val: m_shaders | std::views::values) delete val;
    m_shaders.clear();

    for (auto &val: m_fonts | std::views::values) delete val;
    m_fonts.clear();

    for (auto &val: m_soundBuffers | std::views::values) delete val;
    m_soundBuffers.clear();
}