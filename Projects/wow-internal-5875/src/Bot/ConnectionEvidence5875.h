#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Bot
{
    // Diagnostic evidence only. Neither field authorizes reconnect or gameplay.
    // IsConnectedToServer is also used at character select; lastGlueScreen is
    // historical storage, NOT proof that a glue frame is currently visible.
    struct ConnectionEvidence5875
    {
        bool signaturesKnown = false;
        bool serverConnectionKnown = false;
        bool serverConnected = false;
        const char* lastGlueScreen = "unknown";
        const char* reason = "signature_mismatch";

        template<class Read, std::size_t N>
        static bool Match(Read& read, std::uintptr_t address,
                          const std::array<unsigned char, N>& expected)
        {
            std::array<unsigned char, N> actual{};
            return read(address, actual) && actual == expected;
        }

        // read(address, value) must be a bounded, fault-safe read; no native
        // calls, Lua, UI methods, credentials, hooks or memory writes here.
        template<class Read>
        static ConnectionEvidence5875 Observe(std::uintptr_t base, Read read)
        {
            ConnectionEvidence5875 e;
            // Absolute operands below belong to this non-relocated 5875 image.
            if (base != 0x400000) return e;
            // Registration pair and complete IsConnectedToServer Lua callback.
            if (!Match(read, 0x8374a0, std::array<unsigned char,8>{
                    0xb0,0x77,0x83,0,0x80,0xd3,0x46,0}) ||
                !Match(read, 0x46d380, std::array<unsigned char,51>{
                    0x56,0x8b,0xf1,0xe8,0x08,0xe1,0x13,0,0x8b,0x88,0,0x1b,0,0,
                    0x85,0xc9,0x8b,0xce,0x74,0x13,0x68,0,0,0xf0,0x3f,0x6a,0,
                    0xe8,0x70,0x64,0x28,0,0xb8,1,0,0,0,0x5e,0xc3,
                    0xe8,0x44,0x64,0x28,0,0xb8,1,0,0,0,0x5e,0xc3}) ||
                !Match(read, 0x5ab490, std::array<unsigned char,6>{
                    0xa1,0x28,0x81,0xc2,0,0xc3}) ||
                // SetCurrentScreen -> bounded copy of name to this buffer.
                !Match(read, 0x46ce8f, std::array<unsigned char,7>{
                    0x8b,0xc8,0xe8,0xca,0xe9,0xff,0xff}) ||
                !Match(read, 0x46b860, std::array<unsigned char,14>{
                    0x6a,0x40,0x51,0x68,0x78,0x14,0xb4,0,
                    0xe8,0x33,0xed,0x1d,0,0xc3})) return e;
            e.signaturesKnown = true;
            e.reason = "read_unavailable";

            std::uint32_t owner = 0, ownerAfter = 0, connection = 0, connectionAfter = 0;
            if (read(0xc28128, owner) && owner &&
                owner <= std::numeric_limits<std::uint32_t>::max() - 0x1b04 &&
                read(owner + 0x1b00, connection) && read(owner + 0x1b00, connectionAfter) &&
                read(0xc28128, ownerAfter) && ownerAfter == owner && connectionAfter == connection)
            {
                e.serverConnectionKnown = true;
                e.serverConnected = connection != 0; // exact Lua API predicate
                e.reason = "read_only_api_predicate";
            }
            std::array<char,64> screen{}, screenAfter{};
            if (read(0xb41478, screen) && read(0xb41478, screenAfter) && screen == screenAfter)
            {
                const auto end = std::find(screen.begin(), screen.end(), '\0');
                if (end != screen.end())
                {
                    const std::string_view name(screen.data(), static_cast<std::size_t>(end-screen.begin()));
                    // Never print arbitrary memory/string contents.
                    for (const auto* known : {"login", "charselect", "realmwizard", "charcreate",
                                               "patchdownload", "movie", "credits"})
                        if (name == known) e.lastGlueScreen = known;
                }
            }
            return e;
        }
    };
}
