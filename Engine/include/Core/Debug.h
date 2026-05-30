#pragma once
#include <iostream>
#include <format>
#include <source_location>
#include <type_traits>

namespace Debug
{
    void Init();

    constexpr const char* GetFileName(const char* path)
    {
        const char* file = path;
        while (*path)
        {
            if (*path == '/' || *path == '\\') file = path + 1;
            path++;
        }
        return file;
    }

    template <typename... Args>
    struct LogContext
    {
        std::format_string<Args...> fmt;
        std::source_location loc;

        template <typename String>
        consteval LogContext(const String& s, std::source_location location = std::source_location::current())
            : fmt(s), loc(location) {}
    };

    template <typename... Args>
    void Info(LogContext<std::type_identity_t<Args>...> ctx, Args&&... args)
    {
#ifndef NDEBUG
        std::cout << "\033[32m[INFO]\033[0m "
                  << std::format(ctx.fmt, std::forward<Args>(args)...)
                  << " \033[90m(" << GetFileName(ctx.loc.file_name()) << ":" << ctx.loc.line() << ")\033[0m\n";
#endif
    }

    template <typename... Args>
    void Warning(LogContext<std::type_identity_t<Args>...> ctx, Args&&... args)
    {
#ifndef NDEBUG
        std::cout << "\033[33m[WARN]\033[0m "
                  << std::format(ctx.fmt, std::forward<Args>(args)...)
                  << " \033[90m(" << GetFileName(ctx.loc.file_name()) << ":" << ctx.loc.line() << ")\033[0m\n";
#endif
    }

    template <typename... Args>
    void Error(LogContext<std::type_identity_t<Args>...> ctx, Args&&... args)
    {
#ifndef NDEBUG
        std::cerr << "\033[31m[ERROR]\033[0m "
                  << std::format(ctx.fmt, std::forward<Args>(args)...)
                  << " \033[90m(" << GetFileName(ctx.loc.file_name()) << ":" << ctx.loc.line() << ")\033[0m\n";
#endif
    }
}