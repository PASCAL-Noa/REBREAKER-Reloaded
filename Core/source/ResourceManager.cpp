#include "Resources/ResourceManager.h"
#include "Threading/ThreadPool.h"
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

ResourceManager::ResourceManager()
    : m_threadPool(std::make_unique<ThreadPool>()),
      m_mutex(std::make_unique<std::shared_mutex>()),
      m_pendingMutex(std::make_unique<std::mutex>())
{
    InitFallbacks();
}

ResourceManager::~ResourceManager()
{
    if (m_threadPool)
    {
        m_threadPool->Stop();
    }
}

ResourceManager::ResourceManager(ResourceManager&&) noexcept = default;
ResourceManager& ResourceManager::operator=(ResourceManager&&) noexcept = default;

ThreadPool& ResourceManager::GetThreadPool() noexcept
{
    return *m_threadPool;
}

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

bool ResourceManager::IsResourceLoaded(uint32_t id) const
{
    if (id == 0) return false;
    std::shared_lock<std::shared_mutex> lock(*m_mutex);

    if (auto it = m_textures.find(id); it != m_textures.end() && it->second != nullptr) return true;
    if (auto it = m_shaders.find(id); it != m_shaders.end() && it->second != nullptr) return true;
    if (auto it = m_fonts.find(id); it != m_fonts.end() && it->second != nullptr) return true;
    if (auto it = m_soundBuffers.find(id); it != m_soundBuffers.end() && it->second != nullptr) return true;
    if (auto it = m_textFiles.find(id); it != m_textFiles.end()) return true;

    return false;
}

uint32_t ResourceManager::LoadResource(const std::string& filepath)
{
    if (filepath.empty())
    {
        std::cerr << "[ResourceManager] Warning: Attempted to load resource with empty filepath.\n";
        return 0;
    }

    const uint32_t id = GetResourceId(filepath);

    // Process any textures/shaders waiting for GPU upload
    ProcessPendingUploads();

    // Check if already loaded
    if (IsResourceLoaded(id))
    {
        return id;
    }

    // If currently loading in background, wait for it
    std::shared_ptr<std::promise<uint32_t>> waitPromise;
    {
        std::lock_guard<std::mutex> lock(*m_pendingMutex);
        if (m_loadingIds.contains(id))
        {
            waitPromise = std::make_shared<std::promise<uint32_t>>();
            m_inFlightPromises[id].push_back(waitPromise);
        }
    }

    if (waitPromise)
    {
        std::future<uint32_t> fut = waitPromise->get_future();
        // Periodically process pending uploads while waiting so texture uploads don't deadlock
        while (fut.wait_for(std::chrono::milliseconds(5)) != std::future_status::ready)
        {
            ProcessPendingUploads();
        }
        ProcessPendingUploads();
        return fut.get();
    }

    // Synchronous fallback
    const std::string ext = GetExtension(filepath);

    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
        LoadTexture(id, filepath);
    else if (ext == "frag" || ext == "vert")
        LoadShader(id, filepath);
    else if (ext == "ttf" || ext == "otf")
        LoadFont(id, filepath);
    else if (ext == "wav" || ext == "ogg" || ext == "flac" || ext == "mp3")
        LoadSoundBuffer(id, filepath);
    else if (ext == "txt" || ext == "lvl" || ext == "json" || ext == "csv")
        LoadText(id, filepath);
    else
        std::cerr << "[ResourceManager] Warning: Unsupported resource extension for \"" << filepath << "\".\n";

    return id;
}

std::future<uint32_t> ResourceManager::LoadResourceAsync(
    const std::string& filepath, 
    std::function<void(uint32_t)> onLoaded)
{
    if (filepath.empty())
    {
        std::cerr << "[ResourceManager] Warning: Attempted to load async resource with empty filepath.\n";
        auto p = std::make_shared<std::promise<uint32_t>>();
        p->set_value(0);
        return p->get_future();
    }

    const uint32_t id = GetResourceId(filepath);

    // Check if already loaded
    if (IsResourceLoaded(id))
    {
        if (onLoaded)
        {
            onLoaded(id);
        }
        auto p = std::make_shared<std::promise<uint32_t>>();
        p->set_value(id);
        return p->get_future();
    }

    auto promise = std::make_shared<std::promise<uint32_t>>();
    std::future<uint32_t> future = promise->get_future();

    {
        std::lock_guard<std::mutex> lock(*m_pendingMutex);
        if (onLoaded)
        {
            m_inFlightCallbacks[id].push_back(std::move(onLoaded));
        }
        m_inFlightPromises[id].push_back(promise);

        if (m_loadingIds.contains(id))
        {
            // Already in flight; future and callback registered above
            return future;
        }

        m_loadingIds.insert(id);
    }

    // Dispatch background loading task to ThreadPool
    m_threadPool->EnqueueDetached([this, id, filepath]() {
        const std::string ext = GetExtension(filepath);

        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
        {
            auto image = std::make_unique<sf::Image>();
            bool success = image->loadFromFile(filepath);

            std::lock_guard<std::mutex> lock(*m_pendingMutex);
            m_pendingTextures.push_back({ id, filepath, std::move(image), success });
        }
        else if (ext == "frag" || ext == "vert")
        {
            std::ifstream file(filepath);
            std::string source;
            bool success = false;
            if (file.is_open())
            {
                std::stringstream ss;
                ss << file.rdbuf();
                source = ss.str();
                success = true;
            }
            bool isFragment = (ext == "frag");

            std::lock_guard<std::mutex> lock(*m_pendingMutex);
            m_pendingShaders.push_back({ id, filepath, std::move(source), isFragment, success });
        }
        else if (ext == "ttf" || ext == "otf")
        {
            auto font = std::make_unique<sf::Font>();
            bool success = font->openFromFile(filepath);

            std::vector<std::function<void(uint32_t)>> callbacks;
            std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
            {
                std::unique_lock<std::shared_mutex> rLock(*m_mutex);
                if (success)
                    m_fonts[id] = std::move(font);
                else
                    m_fonts[id] = nullptr;
            }
            {
                std::lock_guard<std::mutex> pLock(*m_pendingMutex);
                m_loadingIds.erase(id);
                if (auto it = m_inFlightCallbacks.find(id); it != m_inFlightCallbacks.end())
                {
                    callbacks = std::move(it->second);
                    m_inFlightCallbacks.erase(it);
                }
                if (auto it = m_inFlightPromises.find(id); it != m_inFlightPromises.end())
                {
                    promises = std::move(it->second);
                    m_inFlightPromises.erase(it);
                }
            }
            for (auto& prom : promises) if (prom) prom->set_value(id);
            for (auto& cb : callbacks) if (cb) cb(id);
        }
        else if (ext == "wav" || ext == "ogg" || ext == "flac" || ext == "mp3")
        {
            auto buf = std::make_unique<sf::SoundBuffer>();
            bool success = buf->loadFromFile(filepath);

            std::vector<std::function<void(uint32_t)>> callbacks;
            std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
            {
                std::unique_lock<std::shared_mutex> rLock(*m_mutex);
                if (success)
                    m_soundBuffers[id] = std::move(buf);
                else
                    m_soundBuffers[id] = nullptr;
            }
            {
                std::lock_guard<std::mutex> pLock(*m_pendingMutex);
                m_loadingIds.erase(id);
                if (auto it = m_inFlightCallbacks.find(id); it != m_inFlightCallbacks.end())
                {
                    callbacks = std::move(it->second);
                    m_inFlightCallbacks.erase(it);
                }
                if (auto it = m_inFlightPromises.find(id); it != m_inFlightPromises.end())
                {
                    promises = std::move(it->second);
                    m_inFlightPromises.erase(it);
                }
            }
            for (auto& prom : promises) if (prom) prom->set_value(id);
            for (auto& cb : callbacks) if (cb) cb(id);
        }
        else if (ext == "txt" || ext == "lvl" || ext == "json" || ext == "csv")
        {
            std::ifstream file(filepath);
            std::string content;
            bool success = false;
            if (file.is_open())
            {
                std::stringstream ss;
                ss << file.rdbuf();
                content = ss.str();
                success = true;
            }

            std::vector<std::function<void(uint32_t)>> callbacks;
            std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
            {
                std::unique_lock<std::shared_mutex> rLock(*m_mutex);
                if (success)
                    m_textFiles[id] = std::move(content);
                else
                    m_textFiles[id] = "";
            }
            {
                std::lock_guard<std::mutex> pLock(*m_pendingMutex);
                m_loadingIds.erase(id);
                if (auto it = m_inFlightCallbacks.find(id); it != m_inFlightCallbacks.end())
                {
                    callbacks = std::move(it->second);
                    m_inFlightCallbacks.erase(it);
                }
                if (auto it = m_inFlightPromises.find(id); it != m_inFlightPromises.end())
                {
                    promises = std::move(it->second);
                    m_inFlightPromises.erase(it);
                }
            }
            for (auto& prom : promises) if (prom) prom->set_value(id);
            for (auto& cb : callbacks) if (cb) cb(id);
        }
        else
        {
            std::cerr << "[ResourceManager] Warning: Unsupported resource extension for \"" << filepath << "\".\n";
            std::vector<std::function<void(uint32_t)>> callbacks;
            std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
            {
                std::lock_guard<std::mutex> pLock(*m_pendingMutex);
                m_loadingIds.erase(id);
                if (auto it = m_inFlightCallbacks.find(id); it != m_inFlightCallbacks.end())
                {
                    callbacks = std::move(it->second);
                    m_inFlightCallbacks.erase(it);
                }
                if (auto it = m_inFlightPromises.find(id); it != m_inFlightPromises.end())
                {
                    promises = std::move(it->second);
                    m_inFlightPromises.erase(it);
                }
            }
            for (auto& prom : promises) if (prom) prom->set_value(0);
            for (auto& cb : callbacks) if (cb) cb(0);
        }
    });

    return future;
}

void ResourceManager::ProcessPendingUploads(size_t maxUploadsPerFrame)
{
    std::vector<PendingTexture> textures;
    std::vector<PendingShader> shaders;

    {
        std::lock_guard<std::mutex> lock(*m_pendingMutex);
        if (m_pendingTextures.empty() && m_pendingShaders.empty())
        {
            return;
        }

        if (maxUploadsPerFrame == 0)
        {
            textures = std::move(m_pendingTextures);
            shaders = std::move(m_pendingShaders);
            m_pendingTextures.clear();
            m_pendingShaders.clear();
        }
        else
        {
            size_t texCount = std::min(m_pendingTextures.size(), maxUploadsPerFrame);
            textures.insert(textures.end(),
                            std::make_move_iterator(m_pendingTextures.begin()),
                            std::make_move_iterator(m_pendingTextures.begin() + static_cast<std::ptrdiff_t>(texCount)));
            m_pendingTextures.erase(m_pendingTextures.begin(), m_pendingTextures.begin() + static_cast<std::ptrdiff_t>(texCount));

            size_t rem = maxUploadsPerFrame - texCount;
            if (rem > 0 && !m_pendingShaders.empty())
            {
                size_t shCount = std::min(m_pendingShaders.size(), rem);
                shaders.insert(shaders.end(),
                               std::make_move_iterator(m_pendingShaders.begin()),
                               std::make_move_iterator(m_pendingShaders.begin() + static_cast<std::ptrdiff_t>(shCount)));
                m_pendingShaders.erase(m_pendingShaders.begin(), m_pendingShaders.begin() + static_cast<std::ptrdiff_t>(shCount));
            }
        }
    }

    for (auto& texItem : textures)
    {
        auto tex = std::make_unique<sf::Texture>();
        bool uploaded = false;
        if (texItem.success && texItem.image && tex->loadFromImage(*texItem.image))
        {
            tex->setSmooth(true);
            uploaded = true;
        }

        std::vector<std::function<void(uint32_t)>> callbacks;
        std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
        {
            std::unique_lock<std::shared_mutex> rLock(*m_mutex);
            if (uploaded)
                m_textures[texItem.id] = std::move(tex);
            else
                m_textures[texItem.id] = nullptr;
        }
        {
            std::lock_guard<std::mutex> pLock(*m_pendingMutex);
            m_loadingIds.erase(texItem.id);
            if (auto it = m_inFlightCallbacks.find(texItem.id); it != m_inFlightCallbacks.end())
            {
                callbacks = std::move(it->second);
                m_inFlightCallbacks.erase(it);
            }
            if (auto it = m_inFlightPromises.find(texItem.id); it != m_inFlightPromises.end())
            {
                promises = std::move(it->second);
                m_inFlightPromises.erase(it);
            }
        }
        for (auto& prom : promises) if (prom) prom->set_value(texItem.id);
        for (auto& cb : callbacks) if (cb) cb(texItem.id);
    }

    for (auto& shItem : shaders)
    {
        auto shader = std::make_unique<sf::Shader>();
        sf::Shader::Type type = shItem.isFragment ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
        bool uploaded = false;
        if (shItem.success && shader->loadFromMemory(shItem.source, type))
        {
            uploaded = true;
        }

        std::vector<std::function<void(uint32_t)>> callbacks;
        std::vector<std::shared_ptr<std::promise<uint32_t>>> promises;
        {
            std::unique_lock<std::shared_mutex> rLock(*m_mutex);
            if (uploaded)
                m_shaders[shItem.id] = std::move(shader);
            else
                m_shaders[shItem.id] = nullptr;
        }
        {
            std::lock_guard<std::mutex> pLock(*m_pendingMutex);
            m_loadingIds.erase(shItem.id);
            if (auto it = m_inFlightCallbacks.find(shItem.id); it != m_inFlightCallbacks.end())
            {
                callbacks = std::move(it->second);
                m_inFlightCallbacks.erase(it);
            }
            if (auto it = m_inFlightPromises.find(shItem.id); it != m_inFlightPromises.end())
            {
                promises = std::move(it->second);
                m_inFlightPromises.erase(it);
            }
        }
        for (auto& prom : promises) if (prom) prom->set_value(shItem.id);
        for (auto& cb : callbacks) if (cb) cb(shItem.id);
    }
}

bool ResourceManager::LoadTexture(const uint32_t id, const std::string& filepath)
{
    {
        std::shared_lock<std::shared_mutex> lock(*m_mutex);
        if (m_textures.contains(id)) return m_textures.at(id) != nullptr;
    }

    auto tex = std::make_unique<sf::Texture>();
    if (tex->loadFromFile(filepath))
    {
        std::unique_lock<std::shared_mutex> lock(*m_mutex);
        m_textures[id] = std::move(tex);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load texture \"" << filepath << "\". Using fallback checkerboard.\n";
    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_textures[id] = nullptr;
    return false;
}

bool ResourceManager::LoadShader(const uint32_t id, const std::string& filepath)
{
    {
        std::shared_lock<std::shared_mutex> lock(*m_mutex);
        if (m_shaders.contains(id)) return m_shaders.at(id) != nullptr;
    }

    auto shader = std::make_unique<sf::Shader>();
    sf::Shader::Type type = (GetExtension(filepath) == "frag") ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
    if (shader->loadFromFile(filepath, type))
    {
        std::unique_lock<std::shared_mutex> lock(*m_mutex);
        m_shaders[id] = std::move(shader);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load shader \"" << filepath << "\".\n";
    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_shaders[id] = nullptr;
    return false;
}

bool ResourceManager::LoadFont(uint32_t id, const std::string& filepath)
{
    {
        std::shared_lock<std::shared_mutex> lock(*m_mutex);
        if (m_fonts.contains(id)) return m_fonts.at(id) != nullptr;
    }

    auto font = std::make_unique<sf::Font>();
    if (font->openFromFile(filepath))
    {
        std::unique_lock<std::shared_mutex> lock(*m_mutex);
        m_fonts[id] = std::move(font);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load font \"" << filepath << "\".\n";
    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_fonts[id] = nullptr;
    return false;
}

bool ResourceManager::LoadSoundBuffer(uint32_t id, const std::string& filepath)
{
    {
        std::shared_lock<std::shared_mutex> lock(*m_mutex);
        if (m_soundBuffers.contains(id)) return m_soundBuffers.at(id) != nullptr;
    }

    auto buf = std::make_unique<sf::SoundBuffer>();
    if (buf->loadFromFile(filepath))
    {
        std::unique_lock<std::shared_mutex> lock(*m_mutex);
        m_soundBuffers[id] = std::move(buf);
        return true;
    }

    std::cerr << "[ResourceManager] Warning: Failed to load sound buffer \"" << filepath << "\". Using silent fallback.\n";
    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_soundBuffers[id] = nullptr;
    return false;
}

bool ResourceManager::LoadText(uint32_t id, const std::string& filepath)
{
    {
        std::shared_lock<std::shared_mutex> lock(*m_mutex);
        if (m_textFiles.contains(id)) return true;
    }

    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "[ResourceManager] Warning: Failed to open text file \"" << filepath << "\".\n";
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();

    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_textFiles[id] = ss.str();
    return true;
}

void ResourceManager::Clear()
{
    if (m_threadPool)
    {
        m_threadPool->WaitAll();
    }
    ProcessPendingUploads();

    std::unique_lock<std::shared_mutex> lock(*m_mutex);
    m_textures.clear();
    m_shaders.clear();
    m_fonts.clear();
    m_soundBuffers.clear();
    m_textFiles.clear();
}