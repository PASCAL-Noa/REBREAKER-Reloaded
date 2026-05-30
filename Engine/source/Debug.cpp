#include "Core/Debug.h"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace Debug
{
    void Init()
    {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwOutMode = 0;
        if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwOutMode))
        {
            SetConsoleMode(hOut, dwOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD dwErrMode = 0;
        if (hErr != INVALID_HANDLE_VALUE && GetConsoleMode(hErr, &dwErrMode))
        {
            SetConsoleMode(hErr, dwErrMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#endif
    }
}