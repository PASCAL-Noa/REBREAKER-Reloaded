#include "Core/PlayerPrefs.h"
#include "Core/Debug.h"
#include "Application.h"

#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <exception>

int main()
{
    std::set_terminate([]() {
        try
        {
            auto ex = std::current_exception();
            if (ex) std::rethrow_exception(ex);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[FATAL] Unknown unhandled exception caught in terminate handler." << std::endl;
        }
        std::abort();
    });

    try
    {
        Debug::Init();
        PlayerPrefs::Load();
        Debug::Info("Engine initialized successfully.");

        WindowConfig config{};
        config.Width = 2560;
        config.Height = 1600;
        config.Mode = WindowMode::Fullscreen;

        Application app(config);
        app.Run();

        return 0;
    }
    catch (const std::exception& e)
    {
        Debug::Error("Unhandled exception in main: {}", e.what());
        return -1;
    }
    catch (...)
    {
        Debug::Error("Unhandled unknown exception caught in main.");
        return -1;
    }
}