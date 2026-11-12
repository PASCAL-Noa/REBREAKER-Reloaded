#include "Core/PlayerPrefs.h"
#include "Core/Debug.h"
#include "Application.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <exception>
#include <fstream>

int main()
{
#if defined(_WIN32) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#ifdef _WIN32
    // Protect against defective/dead 8th logical core (index 7, 0-based)
    {
        HANDLE hProcess = GetCurrentProcess();
        DWORD_PTR procMask = 0;
        DWORD_PTR sysMask = 0;
        if (GetProcessAffinityMask(hProcess, &procMask, &sysMask))
        {
            DWORD_PTR safeMask = procMask & ~(static_cast<DWORD_PTR>(1) << 7);
            if (safeMask != 0)
            {
                SetProcessAffinityMask(hProcess, safeMask);
            }
        }
    }
#endif

    std::set_terminate([]() {
        try
        {
            auto ex = std::current_exception();
            if (ex) std::rethrow_exception(ex);
        }
        catch (const std::exception& e)
        {
            std::ofstream errLog("crash.log", std::ios::app);
            errLog << "[FATAL] Unhandled exception: " << e.what() << std::endl;
            std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::ofstream errLog("crash.log", std::ios::app);
            errLog << "[FATAL] Unknown unhandled exception caught in terminate handler." << std::endl;
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
        int resW = PlayerPrefs::GetInt("ResolutionWidth", 0);
        if (resW <= 0) resW = PlayerPrefs::GetInt("WindowWidth", 1920);
        if (resW <= 0) resW = 1920;
        config.Width = resW;

        int resH = PlayerPrefs::GetInt("ResolutionHeight", 0);
        if (resH <= 0) resH = PlayerPrefs::GetInt("WindowHeight", 1080);
        if (resH <= 0) resH = 1080;
        config.Height = resH;

        const bool fs = PlayerPrefs::GetBool("Fullscreen", false);
        config.Mode = fs ? WindowMode::Fullscreen : WindowMode::Windowed;

        Application app(config);
        app.Run();

        PlayerPrefs::Save();
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