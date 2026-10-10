#include "../src/Bot/LocationCandidates5875.h"

#include <cassert>
#include <cstring>
#include <map>
#include <vector>

struct Memory
{
    std::map<std::uintptr_t, std::vector<unsigned char>> bytes;
    std::map<std::uintptr_t, unsigned> reads;
    std::uintptr_t change = 0, failRepeat = 0;
    template<class T> void Put(std::uintptr_t address, const T& value)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&value);
        bytes[address] = {p, p + sizeof(value)};
    }
    Memory()
    {
        bytes[0x468580] = {0xa1,0x14,0x14,0xb4,0,0x85,0xc0,0x74,7,0x8b,0x80,0xcc,0,0,0,0xc3,0x33,0xc0,0xc3};
        bytes[0x4685a0] = {0xa1,0x14,0x14,0xb4,0,0x89,0x88,0xcc,0,0,0,0xc3};
        bytes[0x494787] = {0x8b,0x35,0x14,0xe3,0xb4,0,0x85,0xf6,0x0f,0x94,0xc0,0x33,0xdb,0x3b,0xf1,0x57,0x0f,0x95,0xc3,0x89,0x0d,0x14,0xe3,0xb4,0,0x89,0x15,0x18,0xe3,0xb4,0};
        bytes[0x491266] = {0x89,0x3d,0x14,0xe3,0xb4,0,0x89,0x3d,0x18,0xe3,0xb4,0};
        Put(0xb41414, std::uint32_t{0x100000});
        Put(0x1000cc, std::uint32_t{1});
        Put(0xb4e314, std::array<std::uint32_t,2>{14,363});
    }
    template<class T> bool Read(std::uintptr_t address, T& value)
    {
        const auto count = ++reads[address];
        if (address == failRepeat && count > 1) return false;
        const auto it = bytes.find(address);
        if (it == bytes.end() || it->second.size() != sizeof(value)) return false;
        std::memcpy(&value, it->second.data(), sizeof(value));
        if (address == change && count > 1)
            reinterpret_cast<unsigned char*>(&value)[sizeof(value)-1] ^= 1;
        return true;
    }
    auto Observe(bool exact = true, std::uintptr_t base = 0x400000)
    {
        return Bot::LocationCandidates5875::Observe(base, exact,
            [this](auto address, auto& value) { return Read(address, value); });
    }
};

static void Unknown(const Bot::LocationCandidates5875& r)
{
    assert(!r.signaturesKnown && !r.owner && !r.mapCandidate && !r.zoneCandidate && !r.areaCandidate);
}

int main()
{
    using L = Bot::LocationCandidates5875;
    Memory m;
    const auto good = m.Observe();
    assert(good.signaturesKnown && good.owner == 0x100000 && good.mapCandidate == 1);
    assert(good.zoneCandidate == 14 && good.areaCandidate == 363);
    assert(m.reads.size() == 7);
    for (auto a : {0xb41414, 0x1000cc, 0xb4e314}) assert(m.reads[a] == 2);
    m = Memory{};
    Unknown(m.Observe(false));
    Unknown(m.Observe(true, 0x500000));
    assert(m.reads.empty()); // wrong client never interprets candidate addresses

    for (auto address : {0x468580,0x4685a0,0x494787,0x491266})
    {
        const auto original = Memory{}.bytes[address];
        for (std::size_t index = 0; index < original.size(); ++index)
        {
            m = Memory{};
            m.bytes[address][index] ^= 1;
            Unknown(m.Observe());
            assert(!m.reads.contains(0xb41414) && !m.reads.contains(0xb4e314));
        }
        for (bool missing : {false,true})
        {
            m = Memory{};
            if (missing) m.bytes.erase(address); else m.bytes[address].pop_back();
            Unknown(m.Observe());
        }
    }
    for (auto owner : {0u,0xffffff30u,0xffffffffu})
    {
        m = Memory{};
        m.Put(0xb41414, owner);
        const auto r = m.Observe();
        assert(r.owner == owner && !r.mapCandidate);
        assert(r.zoneCandidate == 14 && r.areaCandidate == 363);
        assert(!m.reads.contains(0x1000cc));
    }
    for (auto address : {0xb41414,0x1000cc,0xb4e314})
    {
        for (int failure = 0; failure < 4; ++failure)
        {
            m = Memory{};
            if (failure == 0) m.bytes.erase(address);
            if (failure == 1) m.bytes[address].pop_back();
            if (failure == 2) m.change = address;
            if (failure == 3) m.failRepeat = address;
            const auto r = m.Observe();
            assert(r.signaturesKnown);
            if (address == 0xb4e314)
                assert(!r.zoneCandidate && !r.areaCandidate && r.mapCandidate == 1);
            else
                assert(!r.mapCandidate && r.zoneCandidate == 14 && r.areaCandidate == 363);
        }
    }
    for (auto value : {0u,0xffffffffu})
    {
        m = Memory{};
        m.Put(0x1000cc, value);
        m.Put(0xb4e314, std::array<std::uint32_t,2>{value,value});
        const auto r = m.Observe();
        assert(r.mapCandidate == value && r.zoneCandidate == value && r.areaCandidate == value);
        m.bytes.erase(0x1000cc);
        m.bytes.erase(0xb4e314);
        const auto failed = m.Observe();
        assert(!failed.mapCandidate && !failed.zoneCandidate && !failed.areaCandidate);
    }

    // Changes across the world-read bracket reject only their raw group.
    assert(L::AcrossWorldRead(good, good).mapCandidate == 1);
    Unknown(L::AcrossWorldRead(good, {}));
    Unknown(L::AcrossWorldRead({}, good));
    for (int field = 0; field < 4; ++field)
    {
        auto after = good;
        if (field == 0) after.owner = 0x200000;
        if (field == 1) after.mapCandidate = 2;
        if (field == 2) after.zoneCandidate = 15;
        if (field == 3) after.areaCandidate = 364;
        const auto r = L::AcrossWorldRead(good, after);
        if (field < 2) assert(!r.mapCandidate && r.zoneCandidate == 14 && r.areaCandidate == 363);
        else assert(r.mapCandidate == 1 && !r.zoneCandidate && !r.areaCandidate);
    }
}
