#include "../src/Bot/ConnectionEvidence5875.h"
#include "../src/Bot/DisconnectDiagnosticPolicy.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct MemoryFixture
{
    std::map<std::uintptr_t, std::vector<unsigned char>> bytes;
    int ownerReads = 0;
    bool changeOwner = false;
    int connectionReads = 0;
    bool changeConnection = false;
    int screenReads = 0;
    bool changeScreen = false;
    template<class T> void Put(std::uintptr_t address, const T& value)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&value);
        bytes[address] = std::vector<unsigned char>(p, p+sizeof(value));
    }
    template<class T> bool Read(std::uintptr_t address, T& value)
    {
        if (address == 0xc28128 && changeOwner && ++ownerReads > 1) return false;
        const auto it = bytes.find(address);
        if (it == bytes.end() || it->second.size() != sizeof(value)) return false;
        std::memcpy(&value, it->second.data(), sizeof(value));
        if ((address == 0x101b00 && changeConnection && ++connectionReads > 1) ||
            (address == 0xb41478 && changeScreen && ++screenReads > 1))
            reinterpret_cast<unsigned char*>(&value)[0] ^= 1;
        return true;
    }
    MemoryFixture()
    {
        bytes[0x8374a0] = {0xb0,0x77,0x83,0,0x80,0xd3,0x46,0};
        bytes[0x46d380] = {0x56,0x8b,0xf1,0xe8,0x08,0xe1,0x13,0,0x8b,0x88,0,0x1b,0,0,
            0x85,0xc9,0x8b,0xce,0x74,0x13,0x68,0,0,0xf0,0x3f,0x6a,0,
            0xe8,0x70,0x64,0x28,0,0xb8,1,0,0,0,0x5e,0xc3,
            0xe8,0x44,0x64,0x28,0,0xb8,1,0,0,0,0x5e,0xc3};
        bytes[0x5ab490] = {0xa1,0x28,0x81,0xc2,0,0xc3};
        bytes[0x46ce8f] = {0x8b,0xc8,0xe8,0xca,0xe9,0xff,0xff};
        bytes[0x46b860] = {0x6a,0x40,0x51,0x68,0x78,0x14,0xb4,0,0xe8,0x33,0xed,0x1d,0,0xc3};
        Put(0xc28128, std::uint32_t{0x100000});
        Put(0x101b00, std::uint32_t{1});
        std::array<char,64> screen{};
        std::memcpy(screen.data(), "charselect", 10);
        Put(0xb41478, screen);
    }
    auto Observe(std::uintptr_t base = 0x400000)
    { return Bot::ConnectionEvidence5875::Observe(base, [this](auto a, auto& v){return Read(a,v);}); }
};

int main()
{
    MemoryFixture m;
    auto e = m.Observe();
    assert(e.signaturesKnown && e.serverConnectionKnown && e.serverConnected);
    assert(std::string(e.lastGlueScreen) == "charselect");
    m.Put(0x101b00, std::uint32_t{0});
    e = m.Observe();
    assert(e.serverConnectionKnown && !e.serverConnected);
    // Character-select cached data can remain when connection is absent.
    assert(std::string(e.lastGlueScreen) == "charselect");
    m.changeOwner = true;
    assert(!m.Observe().serverConnectionKnown);
    m.changeOwner = false;
    m.Put(0xc28128, std::uint32_t{0});
    assert(!m.Observe().serverConnectionKnown); // null owner != disconnected
    m.Put(0xc28128, std::uint32_t{0xffffff00});
    assert(!m.Observe().serverConnectionKnown); // no overflow / stale pointer
    assert(!m.Observe(0x500000).signaturesKnown);
    m = MemoryFixture{};
    for (auto address : {0x8374a0,0x46d380,0x5ab490,0x46ce8f,0x46b860})
    {
        m.bytes[address][0] ^= 1;
        assert(!m.Observe().signaturesKnown);
        m.bytes[address][0] ^= 1;
    }
    std::array<char,64> invalid{}; invalid.fill('x');
    m.Put(0xb41478, invalid);
    assert(std::string(m.Observe().lastGlueScreen) == "unknown");
    invalid.fill(0); std::memcpy(invalid.data(), "secret", 6);
    m.Put(0xb41478, invalid);
    assert(std::string(m.Observe().lastGlueScreen) == "unknown");
    m.bytes.erase(0x101b00);
    assert(!m.Observe().serverConnectionKnown); // cannot reuse prior success

    for (auto address : {0x8374a0,0x46d380,0x5ab490,0x46ce8f,0x46b860})
    {
        m = MemoryFixture{};
        m.bytes.erase(address);
        e = m.Observe();
        assert(!e.signaturesKnown && !e.serverConnectionKnown);
        assert(std::string(e.lastGlueScreen) == "unknown");
    }
    m = MemoryFixture{};
    m.changeConnection = true;
    e = m.Observe();
    assert(!e.serverConnectionKnown); // torn predicate fails closed
    assert(std::string(e.lastGlueScreen) == "charselect");
    m = MemoryFixture{};
    m.changeScreen = true;
    e = m.Observe();
    assert(e.serverConnectionKnown && e.serverConnected);
    assert(std::string(e.lastGlueScreen) == "unknown");
    m = MemoryFixture{};
    m.bytes.erase(0xb41478);
    e = m.Observe();
    assert(e.serverConnectionKnown && e.serverConnected);
    assert(std::string(e.lastGlueScreen) == "unknown");
    m.bytes.erase(0xc28128);
    e = m.Observe();
    assert(!e.serverConnectionKnown && std::string(e.lastGlueScreen) == "unknown");

    // Existing world-gap diagnostics remain observational, not reconnect.
    Bot::DisconnectDiagnosticPolicy d;
    using D = Bot::DisconnectDiagnosticEvent;
    assert(d.Update(true,0) == D::Healthy);
    assert(d.Update(false,1) == D::SnapshotLost);
    assert(d.Update(true,100) == D::SnapshotRecovered); // short loading gap
    assert(d.Update(false,101) == D::SnapshotLost);
    assert(d.Update(false,1000000) == D::SnapshotStillUnavailable);
    assert(d.Update(true,1000001) == D::SnapshotRecovered); // passive, not success
}
