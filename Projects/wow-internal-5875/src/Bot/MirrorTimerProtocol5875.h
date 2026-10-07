#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Bot
{
    enum class MirrorTimerKind5875 { Unknown, Fatigue, Breath, FeignDeath };
    enum class MirrorTimerEvent5875 { Unknown, Start, Pause, Stop };
    enum class MirrorTimerDirection5875 { Unknown, Draining, Refilling, Stationary, Paused };

    struct MirrorTimerPacket5875
    {
        bool valid=false;
        MirrorTimerKind5875 kind=MirrorTimerKind5875::Unknown;
        MirrorTimerEvent5875 event=MirrorTimerEvent5875::Unknown;
        std::uint32_t current=0;
        std::uint32_t maximum=0;
        std::int32_t scale=0;
        bool paused=false;
        std::uint32_t spell=0;
        MirrorTimerDirection5875 direction=MirrorTimerDirection5875::Unknown;
    };

    // Mirrors 0x5E7990 packet parsing, for offline/source tests only. No packet
    // hook is installed. Decoding an old event is NOT a fresh live timer read.
    inline MirrorTimerPacket5875 DecodeMirrorTimer5875(
        std::uint16_t opcode, std::span<const std::uint8_t> payload)
    {
        MirrorTimerPacket5875 result;
        if (opcode!=0x1d9 && opcode!=0x1da && opcode!=0x1db) return result;
        const std::size_t size=opcode==0x1d9 ? 21u : opcode==0x1da ? 5u : 4u;
        if (payload.size()!=size) return result;
        auto word=[&](std::size_t offset)
        {
            return std::uint32_t(payload[offset]) |
                (std::uint32_t(payload[offset+1])<<8) |
                (std::uint32_t(payload[offset+2])<<16) |
                (std::uint32_t(payload[offset+3])<<24);
        };
        const auto rawKind=word(0);
        result.kind=rawKind==0 ? MirrorTimerKind5875::Fatigue :
            rawKind==1 ? MirrorTimerKind5875::Breath :
            rawKind==2 ? MirrorTimerKind5875::FeignDeath :
                         MirrorTimerKind5875::Unknown;
        if (result.kind==MirrorTimerKind5875::Unknown) return result;
        result.event=opcode==0x1d9 ? MirrorTimerEvent5875::Start :
            opcode==0x1da ? MirrorTimerEvent5875::Pause :
                            MirrorTimerEvent5875::Stop;
        result.valid=true;
        if (result.event==MirrorTimerEvent5875::Start)
        {
            result.current=word(4);
            result.maximum=word(8);
            result.scale=std::bit_cast<std::int32_t>(word(12));
            result.paused=payload[16]!=0;
            result.spell=word(17);
            result.direction=result.paused ? MirrorTimerDirection5875::Paused :
                result.scale<0 ? MirrorTimerDirection5875::Draining :
                result.scale>0 ? MirrorTimerDirection5875::Refilling :
                                 MirrorTimerDirection5875::Stationary;
        }
        else if (result.event==MirrorTimerEvent5875::Pause)
        {
            result.paused=payload[4]!=0;
            result.direction=result.paused ? MirrorTimerDirection5875::Paused :
                                             MirrorTimerDirection5875::Unknown;
        }
        return result;
    }
}
