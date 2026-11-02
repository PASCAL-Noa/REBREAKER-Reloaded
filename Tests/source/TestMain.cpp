#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <cstdlib>

int main(int argc, char** argv)
{
#if defined(__linux__)
    setenv("ALSOFT_DRIVERS", "null", 0);
#endif

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}

