#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

namespace Core
{
    class Module
    {
    public:
        static std::uintptr_t Base()
        {
            return reinterpret_cast<std::uintptr_t>(
                GetModuleHandleA(nullptr)
            );
        }

        static std::string ExecutablePath()
        {
            char path[MAX_PATH] = {};

            GetModuleFileNameA(
                nullptr,
                path,
                MAX_PATH
            );

            return path;
        }

        static bool Is32Bit()
        {
            return sizeof(void*) == 4;
        }
    };
}
