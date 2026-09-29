#include <windows.h>
#include <cstdio>

int main()
{
    std::printf("Test host started.\n");

    HMODULE module = LoadLibraryA("wow_internal.dll");

    if (!module)
    {
        std::printf(
            "LoadLibrary failed. Error: %lu\n",
            GetLastError()
        );

        return 1;
    }

    std::printf("DLL loaded successfully.\n");
    std::printf("Sleeping for 120 seconds...\n");
    std::printf("PID: %lu\n", GetCurrentProcessId());

    Sleep(120000);

    FreeLibrary(module);

    std::printf("Done.\n");

    return 0;
}
