#include "Core/PlayerPrefs.h"
#include "Core/Debug.h"
#include "Application.h"

#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main()
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