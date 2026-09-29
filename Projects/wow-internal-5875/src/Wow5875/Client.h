#pragma once

#include "../Core/Module.h"

#include <cstdint>

namespace Wow5875
{
    class Client
    {
    public:
        static std::uintptr_t Base()
        {
            return Core::Module::Base();
        }

        template<typename T>
        static T* At(std::uintptr_t rva)
        {
            return reinterpret_cast<T*>(
                Base() + rva
            );
        }

        template<typename T>
        static T Read(std::uintptr_t rva)
        {
            return *At<T>(rva);
        }
    };
}
