#pragma once

#include "../Core/Module.h"

#include <windows.h>
#include <winver.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace Wow5875
{
    struct ClientVersion
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t patch = 0;
        std::uint16_t build = 0;

        bool valid = false;
    };

    class ClientIdentity
    {
    public:
        static std::string ExecutableName()
        {
            std::string path = Core::Module::ExecutablePath();

            const auto position = path.find_last_of("\\/");

            if (position == std::string::npos)
                return path;

            return path.substr(position + 1);
        }

        static ClientVersion Version()
        {
            ClientVersion result{};

            const std::string path =
                Core::Module::ExecutablePath();

            DWORD handle = 0;

            const DWORD size =
                GetFileVersionInfoSizeA(
                    path.c_str(),
                    &handle
                );

            if (size == 0)
                return result;

            std::vector<unsigned char> data(size);

            if (!GetFileVersionInfoA(
                    path.c_str(),
                    0,
                    size,
                    data.data()))
            {
                return result;
            }

            VS_FIXEDFILEINFO* info = nullptr;
            UINT infoSize = 0;

            if (!VerQueryValueA(
                    data.data(),
                    "\\",
                    reinterpret_cast<void**>(&info),
                    &infoSize))
            {
                return result;
            }

            if (!info ||
                infoSize < sizeof(VS_FIXEDFILEINFO))
            {
                return result;
            }

            result.major =
                HIWORD(info->dwFileVersionMS);

            result.minor =
                LOWORD(info->dwFileVersionMS);

            result.patch =
                HIWORD(info->dwFileVersionLS);

            result.build =
                LOWORD(info->dwFileVersionLS);

            result.valid = true;

            return result;
        }

        static bool IsWowExecutable()
        {
            std::string name = ExecutableName();

            std::transform(
                name.begin(),
                name.end(),
                name.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(
                        std::tolower(c)
                    );
                }
            );

            return name == "wow.exe";
        }

        static bool IsBuild5875()
        {
            const ClientVersion version = Version();

            return
                version.valid &&
                version.major == 1 &&
                version.minor == 12 &&
                version.patch == 1 &&
                version.build == 5875;
        }

        static bool IsExpectedClient()
        {
            return
                IsWowExecutable() &&
                IsBuild5875();
        }
    };
}
