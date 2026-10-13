#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

class ThreadPool;

class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept;
    ResourceManager& operator=(ResourceManager&&) noexcept;

    // Synchronous resource loading
    uint32_t                                        LoadResource(const std::string& filepath);

    // Asynchronous resource loading (returns future and invokes optional callback)
    std::future<uint32_t>                           LoadResourceAsync(
        const std::string& filepath, 
        std::function<void(uint32_t)> onLoaded = nullptr
    );

    // Check if resource is completely ready in memory/GPU
    [[nodiscard]] bool                              IsResourceLoaded(uint32_t id) const;

    // Process pending GPU texture / shader uploads on the main thread (called each frame)
    void                                            ProcessPendingUploads(size_t maxUploadsPerFrame = 0);

    // Access to worker thread pool
    [[nodiscard]] ThreadPool&                       GetThreadPool() noexcept;

    template <typename T> T*                        Get(uint32_t id) const;

    [[nodiscard]] sf::Texture*                      GetFallbackTexture() const;
    [[nodiscard]] sf::SoundBuffer*                  GetFallbackSoundBuffer() const;

    void                                            Clear();

    [[nodiscard]] static uint32_t                   GetResourceId(const std::string& filepath);
    [[nodiscard]] static std::string                GetExtension(const std::string& filepath);

private:
    void                                            InitFallbacks();

    bool                                            LoadTexture(uint32_t id, const std::string& filepath);
    bool                                            LoadShader(uint32_t id, const std::string& filepath);
    bool                                            LoadFont(uint32_t id, const std::string& filepath);
    bool                                            LoadSoundBuffer(uint32_t id, const std::string& filepath);
    bool                                            LoadText(uint32_t id, const std::string& filepath);

    struct PendingTexture
    {
        uint32_t id = 0;
        std::string filepath;
        std::unique_ptr<sf::Image> image;
        bool success = false;
    };

    struct PendingShader
    {
        uint32_t id = 0;
        std::string filepath;
        std::string source;
        bool isFragment = true;
        bool success = false;
    };

    std::unique_ptr<ThreadPool>                     m_threadPool;

    mutable std::unique_ptr<std::shared_mutex>      m_mutex;
    mutable std::unique_ptr<std::mutex>             m_pendingMutex;

    std::unordered_map<uint32_t, std::unique_ptr<sf::Texture>>      m_textures;
    std::unordered_map<uint32_t, std::unique_ptr<sf::Shader>>       m_shaders;
    std::unordered_map<uint32_t, std::unique_ptr<sf::Font>>         m_fonts;
    std::unordered_map<uint32_t, std::unique_ptr<sf::SoundBuffer>>  m_soundBuffers;
    std::unordered_map<uint32_t, std::string>                       m_textFiles;

    std::vector<PendingTexture>                     m_pendingTextures;
    std::vector<PendingShader>                      m_pendingShaders;

    std::unordered_set<uint32_t>                    m_loadingIds;
    std::unordered_map<uint32_t, std::vector<std::function<void(uint32_t)>>>            m_inFlightCallbacks;
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<std::promise<uint32_t>>>> m_inFlightPromises;

    std::unique_ptr<sf::Texture>                    m_fallbackTexture;
    std::unique_ptr<sf::SoundBuffer>                m_fallbackSoundBuffer;
};

#include "ResourceManager.inl"
