#pragma once

#include <cstdint>

namespace Wow5875::Offsets
{
    inline constexpr std::uintptr_t ImageBase =
        0x00400000;

    namespace ObjectManager
    {
        inline constexpr std::uintptr_t Root =
            0x00B41414;

        inline constexpr std::uintptr_t FirstObject =
            0xAC;

        inline constexpr std::uintptr_t ActivePlayerGuid =
            0xC0;
    }

    namespace Object
    {
        inline constexpr std::uintptr_t Descriptor =
            0x08;

        inline constexpr std::uintptr_t Type =
            0x14;

        inline constexpr std::uintptr_t Guid =
            0x30;

        inline constexpr std::uintptr_t Next =
            0x3C;
    }

    namespace Functions
    {
        /*
         * CGGameUI_Target / SetTarget
         *
         * Absolute:
         *     0x00493540
         *
         * RVA:
         *     0x00093540
         */
        inline constexpr std::uintptr_t SetTargetRva =
            0x00093540;

        /*
         * CGPlayer_C__ClickToMove
         *
         * Absolute:
         *     0x00611130
         *
         * RVA:
         *     0x00211130
         */
        inline constexpr std::uintptr_t ClickToMoveRva =
            0x00211130;
    }
}
