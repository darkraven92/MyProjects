#pragma once
#include "AfkInputPulse.h"
#include "AfkProtectionPolicy.h"
#include "AfkQualificationHold.h"
#include "AfkClientFlag5875Policy.h"
#include "AfkSafeInputScript.h"
#include "GameThreadDispatcher.h"
#include "../Core/Memory.h"
#include "../Objects/PlayerSnapshot.h"
#include "../Wow5875/Client.h"
#include <array>
#include <cstring>
#include <string>
#include <windows.h>

namespace Bot
{
    struct AfkDispatchResult
    {
        bool issued=false, releaseDelivered=false, blocked=true;
        std::string reason="dispatch_unavailable";
        AfkObservation before{};
        AfkCandidateScene sceneBefore{};
    };

    // Read-only client observations, plus normal game-thread input/API calls.
    // No writes to AFK flags, clocks, movement flags or key-state memory.
    // Exact binary provenance and addresses are documented in docs/AFK_5875_AUDIT.md.
    class AfkClient5875
    {
        template<std::size_t N> static bool Match(std::uintptr_t rva,
            const std::array<unsigned char,N>& expected)
        {
            std::array<unsigned char,N> actual{};
            return Core::Memory::Read(Wow5875::Client::Base()+rva,actual) && actual==expected;
        }
        static bool Supported()
        {
            return Match(0x82ec3,std::array<unsigned char,18>{
                0x8b,0x3d,0xc8,0x0b,0xcf,0x00,0x8b,0xd8,0x2b,0xc7,
                0x8d,0x88,0x20,0x6c,0xfb,0xff,0x85,0xc9}) &&
                Match(0x1eb836,std::array<unsigned char,5>{0xa1,0xcc,0xe5,0xb6,0}) &&
                Match(0x1ee9ef,std::array<unsigned char,9>{0x83,0xe1,0x02,0x89,0x0d,0xcc,0xe5,0xb6,0}) &&
                Match(0x365f34,std::array<unsigned char,6>{0x89,0x0d,0xc8,0x0b,0xcf,0}) &&
                Match(0x2c010,std::array<unsigned char,5>{0xe9,0x7b,0xf7,0xff,0xff});
        }
        struct WindowSearch { DWORD thread=0; HWND window=nullptr; };
        static BOOL CALLBACK FindWindow(HWND hwnd, LPARAM parameter)
        {
            auto& search=*reinterpret_cast<WindowSearch*>(parameter);
            DWORD pid=0;
            const auto thread=GetWindowThreadProcessId(hwnd,&pid);
            char name[80]{};
            GetClassNameA(hwnd,name,sizeof(name));
            if (pid==GetCurrentProcessId() && thread==search.thread && IsWindowVisible(hwnd) &&
                (std::strcmp(name,"GxWindowClassD3d")==0 ||
                 std::strcmp(name,"GxWindowClassOpenGl")==0))
            { search.window=hwnd; return FALSE; }
            return TRUE;
        }
        struct KeyDriver
        {
            HWND window=nullptr;
            bool released=false;
            bool Send(bool up)
            {
                DWORD pid=0;
                if (!IsWindow(window) || GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId() ||
                    pid!=GetCurrentProcessId()) return false;
                const auto scan=MapVirtualKeyA(VK_F12,MAPVK_VK_TO_VSC);
                const LPARAM data=static_cast<LPARAM>(1u | (scan<<16) |
                    (up ? 0xc0000000u : 0u));
                // Same thread: synchronous delivery, no queued held input,
                // no foreground activation, no global SendInput/mouse takeover.
                SendMessageA(window,up ? WM_KEYUP : WM_KEYDOWN,VK_F12,data);
                return true;
            }
            bool Down()
            {
                Debug::Logger::Info("AFK CANDIDATE TEST action=F12 phase=press");
                return Send(false);
            }
            bool Up()
            {
                released=Send(true);
                Debug::Logger::Info(std::string("AFK CANDIDATE TEST action=F12 phase=release result=")+
                    (released ? "message_delivered" : "failed"));
                return released;
            }
        };
    public:
        static AfkCandidateScene ReadScene(const Objects::PlayerState& player)
        {
            Objects::PlayerState fresh;
            AfkCandidateScene scene;
            if (!player.valid || !Objects::PlayerSnapshot::Read(player.address,fresh)) return scene;
            scene.known=Core::Memory::Read(fresh.movement+0x40,scene.movementFlags);
            scene.x=fresh.x; scene.y=fresh.y; scene.z=fresh.z;
            scene.facing=fresh.rotation; scene.target=fresh.targetGuid;
            return scene;
        }
        // A read-only UI probe for qualification acquisition/continued safety.
        // It sends no key, movement, AFK packet or input-clock write.
        static std::string SafeInputReason()
        {
            std::string reason="guard_dispatch_unavailable";
            if (!Supported()) return "client_signature_mismatch";
            const bool invoked=GameThreadDispatcher::Invoke([&]
            {
                using DoString=bool (__fastcall*)(const char*,const char*);
                using GetText=const char* (__fastcall*)(char*,std::uint32_t,int);
                const auto base=Wow5875::Client::Base();
                const auto run=reinterpret_cast<DoString>(base+0x304cd0);
                const auto text=reinterpret_cast<GetText>(base+0x303bf0);
                if (!run(AfkSafeInputScript,"wow-internal/AfkQualificationGuard.lua")) return;
                const char* value=text(const_cast<char*>("WOW_INTERNAL_AFK_INPUT"),0xffffffffu,0);
                reason=value ? value : "guard_unavailable";
            });
            return invoked ? reason : "guard_dispatch_failed";
        }
        static const char* StationarySafetyReason(const Objects::PlayerState& player)
        {
            std::uint32_t movement=0,unitFlags=0,playerFlags=0,health=0;
            if (!player.valid || !player.movement || !player.descriptors) return "player_unavailable";
            if (!Core::Memory::Read(player.movement+0x40,movement) ||
                !Core::Memory::Read(player.descriptors+0xb8,unitFlags) ||
                !Core::Memory::Read(player.descriptors+0x2f8,playerFlags) ||
                !Core::Memory::Read(player.descriptors+0x58,health)) return "native_safety_unknown";
            if (health<=1 || (playerFlags&0x10u)) return "death_or_ghost";
            if (unitFlags&0x80000u) return "native_combat_flag";
            if (movement&0x00200000u) return "swimming";
            if (movement&~0x100u) return "movement_or_transport_flags";
            return nullptr;
        }
        static bool StationaryAliveLand(const Objects::PlayerState& player)
        {
            return StationarySafetyReason(player)==nullptr;
        }
        static AfkObservation Read(const Objects::PlayerState& player)
        {
            AfkObservation o;
            if (!player.valid || !player.descriptors) { o.evidenceReason="player_unavailable"; return o; }
            if (!Supported()) { o.evidenceReason="client_signature_mismatch"; return o; }
            const auto base=Wow5875::Client::Base();
            std::uint32_t client=0,flags=0;
            std::int32_t negativeThreshold=0;
            if (!Core::Memory::Read(base+0x76e5cc,client)) { o.evidenceReason="local_flag_unreadable"; return o; }
            // Local AFK is B6E5CC; server state is PLAYER_FLAGS index BE * 4.
            if (!AfkClientFlag5875Policy::Known(client)) { o.evidenceReason="local_flag_invalid"; return o; }
            if (!Core::Memory::Read(player.descriptors+0x2f8,flags) ||
                !Core::Memory::Read(base+0x8f0bc8,o.lastInput) ||
                !Core::Memory::Read(base+0x82ecf,negativeThreshold) || negativeThreshold>=0)
            { o.evidenceReason="flags_clock_or_threshold_unreadable"; return o; }
            using ClientClock=std::uint32_t (__cdecl*)();
            o.clientNow=reinterpret_cast<ClientClock>(base+0x2c010)();
            o.thresholdMs=static_cast<std::uint32_t>(-negativeThreshold);
            o.serverAfk=(flags&2u)!=0; o.clientAfk=AfkClientFlag5875Policy::Active(client); o.known=true;
            return o;
        }
        static AfkDispatchResult Dispatch(const Objects::PlayerState& player,
            AfkAction action, const AfkObservation& expected)
        {
            AfkDispatchResult result;
            if (!Supported()) { result.reason="client_signature_mismatch"; return result; }
            const bool invoked=GameThreadDispatcher::Invoke([&]
            {
                if (!GameThreadDispatcher::IsGameThread()) return;
                result.before=Read(player);
                if (!AfkProtectionPolicy::Valid(result.before) ||
                    result.before.lastInput!=expected.lastInput)
                { result.reason="input_or_world_changed"; return; }
                // Reject all movement/transport/swim/fall/unknown flags; only
                // the stationary walk-mode preference is permitted.
                if (!StationaryAliveLand(player))
                { result.reason="movement_water_or_unknown"; return; }
                for (int key : {VK_F12,VK_SHIFT,VK_CONTROL,VK_MENU})
                    if ((GetAsyncKeyState(key)&0x8000)!=0)
                    { result.reason="physical_key_held"; return; }
                using DoString=bool (__fastcall*)(const char*,const char*);
                using GetText=const char* (__fastcall*)(char*,std::uint32_t,int);
                const auto base=Wow5875::Client::Base();
                const auto run=reinterpret_cast<DoString>(base+0x304cd0);
                const auto text=reinterpret_cast<GetText>(base+0x303bf0);
                if (!run(AfkSafeInputScript,"wow-internal/AfkSafeInput.lua")) return;
                const char* guard=text(const_cast<char*>("WOW_INTERNAL_AFK_INPUT"),0xffffffffu,0);
                if (!guard || std::strcmp(guard,"ready")!=0)
                { result.reason=guard ? guard : "guard_unavailable"; return; }
                if (action==AfkAction::ClearFlag)
                {
                    const auto live=Read(player);
                    if (!live.known || !live.clientAfk || !live.serverAfk)
                    { result.reason="afk_changed_before_clear"; return; }
                    result.blocked=false;
                    result.issued=run("SendChatMessage('', 'AFK')","wow-internal/AfkClear.lua");
                    result.reason=result.issued ? "clear_requested" : "clear_dispatch_failed";
                    return;
                }
                WindowSearch search{GetCurrentThreadId(),nullptr};
                EnumWindows(FindWindow,reinterpret_cast<LPARAM>(&search));
                if (!search.window) { result.reason="wow_window_unavailable"; return; }
                KeyDriver driver{search.window,false};
                result.sceneBefore=ReadScene(player);
                if (!result.sceneBefore.known) { result.reason="candidate_scene_unavailable"; return; }
                result.blocked=false;
                result.issued=AfkInputPulse(driver);
                result.releaseDelivered=driver.released;
                result.reason=result.issued ? "paired_messages_delivered" : "input_dispatch_failed";
            });
            if (!invoked) { result.blocked=false; result.reason="game_thread_dispatch_failed"; }
            return result;
        }
    };
}
