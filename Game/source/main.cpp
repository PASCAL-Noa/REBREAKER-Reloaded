#include "Core/PlayerPrefs.h"
#include "Core/Debug.h"
#include "Application.h"
#include <iostream>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#if defined(_DEBUG)
#include <crtdbg.h>
#endif
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static LONG WINAPI CustomCrashHandler(EXCEPTION_POINTERS* pEx)
{
    std::ofstream file("crash.log", std::ios::app);
    file << "\n================ CRASH DETECTED ================\n";
    file << "Exception Code: 0x" << std::hex << pEx->ExceptionRecord->ExceptionCode << "\n";
    file << "Exception Address: 0x" << std::hex << (uintptr_t)pEx->ExceptionRecord->ExceptionAddress << "\n";

    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        pEx->ExceptionRecord->NumberParameters >= 2)
    {
        file << "Access Violation: tried to "
             << (pEx->ExceptionRecord->ExceptionInformation[0] ? "write" : "read")
             << " address 0x" << std::hex << pEx->ExceptionRecord->ExceptionInformation[1] << "\n";
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymInitialize(process, NULL, TRUE);

    STACKFRAME64 stackFrame{};
    CONTEXT contextRecord = *pEx->ContextRecord;
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

    stackFrame.AddrPC.Offset = contextRecord.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = contextRecord.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = contextRecord.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    file << "--- STACK TRACE ---\n";
    for (int frame = 0; frame < 64; ++frame)
    {
        if (!StackWalk64(machineType, process, thread, &stackFrame, &contextRecord,
                         NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
        {
            break;
        }

        DWORD64 address = stackFrame.AddrPC.Offset;
        if (address == 0) break;

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]{};
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol))
        {
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line))
            {
                file << "#" << frame << " " << symbol->Name << " at " << line.FileName << ":" << line.LineNumber << "\n";
            }
            else
            {
                file << "#" << frame << " " << symbol->Name << " + 0x" << std::hex << displacement << "\n";
            }
        }
        else
        {
            file << "#" << frame << " 0x" << std::hex << address << "\n";
        }
    }
    file << "================================================\n" << std::flush;
    file.close();

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#include <exception>
#include <fstream>

int main()
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(CustomCrashHandler);
#endif
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
        std::ofstream errLog("crash.log", std::ios::app);
        errLog << "[FATAL] Unhandled exception in main: " << e.what() << std::endl;
        Debug::Error("Unhandled exception in main: {}", e.what());
        return -1;
    }
    catch (...)
    {
        std::ofstream errLog("crash.log", std::ios::app);
        errLog << "[FATAL] Unhandled unknown exception caught in main." << std::endl;
        Debug::Error("Unhandled unknown exception caught in main.");
        return -1;
    }
}