#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Core
{
    class Memory
    {
    public:
        static bool IsReadable(
            std::uintptr_t address,
            std::size_t size)
        {
            if (address == 0 || size == 0)
                return false;

            MEMORY_BASIC_INFORMATION mbi{};

            const SIZE_T result =
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
                    &mbi,
                    sizeof(mbi)
                );

            if (result == 0)
                return false;

            if (mbi.State != MEM_COMMIT)
                return false;

            if ((mbi.Protect & PAGE_GUARD) != 0)
                return false;

            if ((mbi.Protect & PAGE_NOACCESS) != 0)
                return false;

            const DWORD protection =
                mbi.Protect & 0xFF;

            const bool readable =
                protection == PAGE_READONLY ||
                protection == PAGE_READWRITE ||
                protection == PAGE_WRITECOPY ||
                protection == PAGE_EXECUTE_READ ||
                protection == PAGE_EXECUTE_READWRITE ||
                protection == PAGE_EXECUTE_WRITECOPY;

            if (!readable)
                return false;

            const auto regionStart =
                reinterpret_cast<std::uintptr_t>(
                    mbi.BaseAddress
                );

            const auto regionEnd =
                regionStart +
                static_cast<std::uintptr_t>(
                    mbi.RegionSize
                );

            if (address < regionStart)
                return false;

            if (address + size > regionEnd)
                return false;

            return true;
        }

        template<typename T>
        static bool Read(
            std::uintptr_t address,
            T& value)
        {
            if (!IsReadable(address, sizeof(T)))
                return false;

            std::memcpy(
                &value,
                reinterpret_cast<const void*>(address),
                sizeof(T)
            );

            return true;
        }
    };
}
