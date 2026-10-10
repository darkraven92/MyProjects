#pragma once

#include "ConnectionEvidence5875.h"

#include <optional>

namespace Bot
{
    // Raw storage only. Neither equal reads nor matching signatures establish
    // current-world lifetime, current-player ownership, or location authority.
    struct LocationCandidates5875
    {
        bool signaturesKnown = false;
        std::optional<std::uint32_t> owner;
        std::optional<std::uint32_t> mapCandidate;
        std::optional<std::uint32_t> zoneCandidate;
        std::optional<std::uint32_t> areaCandidate;

        // exactFile must come from the audited executable SHA256 check. Every
        // sample also checks live instruction bytes; version/name alone cannot
        // enable this reader. Read must copy exactly sizeof(value), fault-safely.
        template<class Read>
        static LocationCandidates5875 Observe(std::uintptr_t base, bool exactFile, Read read)
        {
            LocationCandidates5875 result;
            if (!exactFile || base != 0x400000) return result;
            if (!ConnectionEvidence5875::Match(read, 0x468580, std::array<unsigned char,19>{
                    0xa1,0x14,0x14,0xb4,0,0x85,0xc0,0x74,7,0x8b,0x80,0xcc,0,0,0,0xc3,0x33,0xc0,0xc3}) ||
                !ConnectionEvidence5875::Match(read, 0x4685a0, std::array<unsigned char,12>{
                    0xa1,0x14,0x14,0xb4,0,0x89,0x88,0xcc,0,0,0,0xc3}) ||
                !ConnectionEvidence5875::Match(read, 0x494787, std::array<unsigned char,31>{
                    0x8b,0x35,0x14,0xe3,0xb4,0,0x85,0xf6,0x0f,0x94,0xc0,0x33,0xdb,
                    0x3b,0xf1,0x57,0x0f,0x95,0xc3,0x89,0x0d,0x14,0xe3,0xb4,0,
                    0x89,0x15,0x18,0xe3,0xb4,0}) ||
                !ConnectionEvidence5875::Match(read, 0x491266, std::array<unsigned char,12>{
                    0x89,0x3d,0x14,0xe3,0xb4,0,0x89,0x3d,0x18,0xe3,0xb4,0})) return result;
            result.signaturesKnown = true;

            std::uint32_t owner = 0, after = 0, map = 0, mapAfter = 0;
            const bool ownerRead = read(0xb41414, owner);
            const bool mapRead = ownerRead && owner != 0 &&
                owner <= std::numeric_limits<std::uint32_t>::max() - 0xd0 &&
                read(std::uintptr_t(owner) + 0xcc, map) &&
                read(std::uintptr_t(owner) + 0xcc, mapAfter) && map == mapAfter;
            if (ownerRead && read(0xb41414, after) && owner == after)
            {
                result.owner = owner; // a successfully read null is observable
                if (mapRead) result.mapCandidate = map;
            }

            // Separate DWORD stores publish these globals. Pair equality only
            // rejects detected tearing, not a writer paused between its stores.
            // Read independently of OM availability to expose teardown staleness.
            std::array<std::uint32_t,2> pair{}, pairAfter{};
            if (read(0xb4e314, pair) && read(0xb4e314, pairAfter) && pair == pairAfter)
            {
                result.zoneCandidate = pair[0];
                result.areaCandidate = pair[1];
            }
            return result;
        }

        // The observer brackets WorldStateReader with fresh raw acquisitions.
        // Reject detected changes independently for owner/map and zone/area;
        // this is still sequential correlation, never atomic world evidence.
        static LocationCandidates5875 AcrossWorldRead(const LocationCandidates5875& before,
                                                       const LocationCandidates5875& after)
        {
            LocationCandidates5875 result;
            if (!before.signaturesKnown || !after.signaturesKnown) return result;
            result.signaturesKnown = true;
            if (before.owner == after.owner)
            {
                result.owner = after.owner;
                if (after.owner && *after.owner != 0 && before.mapCandidate == after.mapCandidate)
                    result.mapCandidate = after.mapCandidate;
            }
            if (before.zoneCandidate == after.zoneCandidate && before.areaCandidate == after.areaCandidate)
            {
                result.zoneCandidate = after.zoneCandidate;
                result.areaCandidate = after.areaCandidate;
            }
            return result;
        }
    };
}
