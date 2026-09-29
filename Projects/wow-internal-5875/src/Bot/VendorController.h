#pragma once

#include "AutonomousMaintenancePolicy.h"
#include "ClickToMoveController.h"
#include "GameThreadDispatcher.h"
#include "GrindBagMonitor.h"

#include "../Core/Memory.h"
#include "../Debug/Logger.h"
#include "../Navigation/GenericNavMeshPathFollower.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"
#include "../Wow5875/Offsets.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>

namespace Bot
{
    enum class VendorState
    {
        Idle,
        ReturningHomeForSearch,
        SearchingVendor,
        NavigatingVendor,
        NavigatingVendorAnchor,
        DirectVendorApproach,
        WaitingForMerchant,
        Selling,
        Maintaining,
        ReturningToGrind,
        Done,
        Failed
    };

    class VendorController
    {
    private:
        static constexpr std::uintptr_t OnRightClickUnitRva = 0x0020BEA0;
        static constexpr std::uintptr_t LuaDoStringRva = 0x00304CD0;
        static constexpr std::uintptr_t GetTextRva = 0x00303BF0;

        static constexpr std::uint32_t MapId = 1;
        // Phase 14L.1.2: keep the nav handoff comfortably inside the reliable
        // merchant interaction radius. The old 5.5/4.5 pair could report
        // Arrived while the live UnitState was still ~5.4 yd away.
        static constexpr float InteractionDistance = 4.5f;
        static constexpr float VendorNavArrivalDistance = 3.0f;
        static constexpr float HomeArrivalDistance = 12.0f;
        static constexpr float DirectFallbackRadius = 14.0f;
        // Phase 14L.1.3: a live, already-selected merchant may still be safely
        // recoverable when the final NavMesh corridor fails just outside the
        // ordinary direct handoff radius. This wider radius is used only after
        // a NavMesh failure for that same live merchant; normal route startup
        // keeps the tighter DirectFallbackRadius.
        static constexpr float NavFailureDirectFallbackRadius = 24.0f;
        // Phase 14L.1.6: runtime traces showed the bad local vendor corridor can
        // turn away again before reaching the old 12 yd handoff. Transfer
        // ownership earlier while the same selected live merchant is <=16 yd.
        static constexpr float ProactiveVendorHandoffDistance = 16.0f;
        static constexpr float DirectFallbackPrecision = 1.25f;
        static constexpr float DirectProgressEpsilon = 0.75f;
        static constexpr std::uint64_t DirectMoveCooldownTicks = 8;
        static constexpr int MaximumDirectMoves = 8;
        static constexpr int MaximumDirectNoProgressMoves = 3;
        // Phase 14L.1.7: local flank recovery must account for the merchant's
        // vertical offset. A fixed 3.75 yd XY ring can still remain outside
        // the 4.5 yd 3D interaction threshold when the merchant object origin
        // is several yards above the player's current ground plane. Keep the
        // local probe on the player's current Z and shrink the XY ring so the
        // intended 3D separation is 4.0 yd when geometry permits.
        static constexpr float LocalRecoveryTriggerDistance = 8.0f;
        static constexpr float LocalRecoveryTargetDistance3D = 4.0f;
        static constexpr float LocalRecoveryMinimumRadius = 1.0f;
        static constexpr float LocalRecoveryMaximumRadius = 3.0f;
        static constexpr float LocalRecoveryPrecision = 0.75f;
        static constexpr int MaximumLocalRecoveryAttempts = 3;
        static constexpr std::uint64_t VendorSearchWaitTicks = 80; // 20 s
        static constexpr std::uint64_t InteractionRetryTicks = 8;
        static constexpr int MaximumInteractionAttempts = 5;
        static constexpr std::uint64_t MerchantOpenTimeoutTicks = 60; // 15 s
        static constexpr std::uint64_t SaleStepTicks = 2;
        static constexpr std::uint64_t MaintenanceStepTicks = 2;
        static constexpr std::uint64_t ServiceSearchWaitTicks = 48; // 12 s
        static constexpr int MaximumServiceCandidates = 10;
        static constexpr int MinimumFreeSlotsAfterVendor = 2;

        // Phase 14L.2: Wuark in Razor Hill is the primary sell/repair service.
        // Route to a player-proven standable hub point beside him, then use the
        // live ObjectManager position for the final approach/interact.
        static constexpr std::uint32_t PreferredVendorEntry = 3167; // Wuark <Armorer & Shieldcrafter>
        static constexpr Navigation::NavPoint RazorHillServiceHub{
            357.1937f, -4708.1279f, 14.4788f};
        static constexpr float RemoteHubArrivalDistance = 8.0f;
        static constexpr std::uint32_t VendorMemoryVersion = 1;

        static constexpr const char* LuaResultVariable =
            "WOW_INTERNAL_VENDOR_RESULT";

        VendorState state_ = VendorState::Idle;

        Navigation::NavPoint grindHome_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> homeNavigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> vendorNavigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> returnNavigator_{};

        std::uint64_t vendorGuid_ = 0;
        std::uint32_t vendorEntry_ = 0;

        bool knownVendorValid_ = false;
        std::uint32_t knownVendorEntry_ = 0;
        Navigation::NavPoint knownVendorPosition_{};
        bool vendorMemoryLoaded_ = false;
        std::filesystem::path vendorMemoryPath_{};
        bool remoteHubRouting_ = false;
        bool primaryHubReached_ = false;

        std::uint64_t stateStartedTick_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t lastDirectMoveTick_ = 0;
        std::uint64_t lastSaleTick_ = 0;

        float directApproachMaxDistance_ = DirectFallbackRadius;
        float lastDirectDistance_ = 0.0f;
        int directNoProgressMoves_ = 0;
        int localRecoveryAttempts_ = 0;
        int interactionAttempts_ = 0;
        int directMoves_ = 0;
        int saleAttempts_ = 0;
        int salesObserved_ = 0;
        int unsellableSlotsSkipped_ = 0;

        int lastCandidateBag_ = -1;
        int lastCandidateSlot_ = -1;
        std::uint32_t lastCandidateItemId_ = 0;
        bool candidatePendingVerification_ = false;

        std::unordered_set<std::uint32_t> blockedBagSlots_{};

        MaintenanceNeed requestedMaintenance_{};
        MaintenanceSnapshot maintenanceAtStart_{};
        MaintenanceSnapshot lastMaintenanceSnapshot_{};
        bool bagPressureTrigger_ = false;
        bool serviceSearchMode_ = false;
        bool repairSatisfied_ = true;
        bool foodSatisfied_ = true;
        bool drinkSatisfied_ = true;
        bool maintenanceUnmet_ = false;
        bool repairUnavailableHere_ = false;
        bool foodUnavailableHere_ = false;
        bool drinkUnavailableHere_ = false;
        std::uint64_t lastMaintenanceStepTick_ = 0;
        int repairActions_ = 0;
        int foodPurchases_ = 0;
        int drinkPurchases_ = 0;
        int serviceCandidatesTried_ = 0;
        std::unordered_set<std::uint32_t> rejectedServiceEntries_{};

        static const char* StateNameInternal(VendorState state)
        {
            switch (state)
            {
                case VendorState::Idle: return "Idle";
                case VendorState::ReturningHomeForSearch: return "ReturningHomeForSearch";
                case VendorState::SearchingVendor: return "SearchingVendor";
                case VendorState::NavigatingVendor: return "NavigatingVendor";
                case VendorState::NavigatingVendorAnchor: return "NavigatingVendorAnchor";
                case VendorState::DirectVendorApproach: return "DirectVendorApproach";
                case VendorState::WaitingForMerchant: return "WaitingForMerchant";
                case VendorState::Selling: return "Selling";
                case VendorState::Maintaining: return "Maintaining";
                case VendorState::ReturningToGrind: return "ReturningToGrind";
                case VendorState::Done: return "Done";
                case VendorState::Failed: return "Failed";
                default: return "Unknown";
            }
        }

        static std::string Float(float value)
        {
            std::ostringstream stream;
            stream.setf(std::ios::fixed);
            stream.precision(3);
            stream << value;
            return stream.str();
        }

        static float Distance2D(
            float ax,
            float ay,
            float bx,
            float by)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            return std::sqrt(dx * dx + dy * dy);
        }

        static bool IsConfiguredVendorEntry(std::uint32_t entry)
        {
            // Phase 14L.2: Wuark is the only configured primary vendor.
            // Other merchants may still be proven by bounded local discovery.
            return entry == PreferredVendorEntry;
        }

        static void ModuleAnchor() {}

        static std::filesystem::path ResolveProjectRoot()
        {
            HMODULE module = nullptr;
            const BOOL ok = GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&ModuleAnchor),
                &module);
            if (!ok || module == nullptr)
                return {};

            wchar_t buffer[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
                return {};

            std::filesystem::path modulePath(buffer);
            auto moduleDir = modulePath.parent_path();
            if (moduleDir.empty())
                return {};

            if (moduleDir.filename() == L"build")
                return moduleDir.parent_path();

            return moduleDir;
        }

        bool EnsureVendorMemoryPath()
        {
            if (!vendorMemoryPath_.empty())
                return true;

            const auto root = ResolveProjectRoot();
            if (root.empty())
                return false;

            const auto dir = root / "data" / "state" / "vendor_memory";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.1: failed to create vendor-memory directory: " +
                    ec.message());
                return false;
            }

            vendorMemoryPath_ = dir / "map_001.tsv";
            return true;
        }

        void LoadVendorMemory()
        {
            if (vendorMemoryLoaded_)
                return;
            vendorMemoryLoaded_ = true;

            if (!EnsureVendorMemoryPath())
                return;

            std::ifstream input(vendorMemoryPath_);
            if (!input)
                return;

            std::string tag;
            std::uint32_t version = 0;
            std::uint32_t entry = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (!(input >> tag >> version >> entry >> x >> y >> z) ||
                tag != "V" || version != VendorMemoryVersion || entry == 0 ||
                !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            {
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.1: vendor-memory row invalid; ignoring it.");
                return;
            }

            if (entry != PreferredVendorEntry)
            {
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.2: ignoring legacy persistent merchant entry=" +
                    std::to_string(entry) +
                    "; Wuark (3167) is the primary Razor Hill service vendor.");
                return;
            }

            knownVendorValid_ = true;
            knownVendorEntry_ = entry;
            knownVendorPosition_ = Navigation::NavPoint{x, y, z};
            Debug::Logger::Info(
                "VENDOR ROUTING 14L.2: loaded persistent verified Wuark entry=" +
                std::to_string(entry) + " pos=(" + Float(x) + "," +
                Float(y) + "," + Float(z) + ")");
        }

        void SaveVendorMemory()
        {
            if (!knownVendorValid_ || knownVendorEntry_ == 0 ||
                !EnsureVendorMemoryPath())
            {
                return;
            }

            const auto tempPath = vendorMemoryPath_.string() + ".tmp";
            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                    return;
                output << "V\t" << VendorMemoryVersion << '\t'
                       << knownVendorEntry_ << '\t'
                       << std::setprecision(9) << knownVendorPosition_.x << '\t'
                       << knownVendorPosition_.y << '\t'
                       << knownVendorPosition_.z << '\n';
                if (!output)
                    return;
            }

            std::error_code ec;
            std::filesystem::remove(vendorMemoryPath_, ec);
            ec.clear();
            std::filesystem::rename(tempPath, vendorMemoryPath_, ec);
            if (ec)
            {
                std::filesystem::remove(tempPath, ec);
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.1: failed to atomically persist verified merchant.");
            }
        }

        static const char* VendorName(std::uint32_t entry)
        {
            switch (entry)
            {
                case 3167: return "Wuark";
                case 3187: return "Tai'tasi";
                case 5942: return "Zansoa";
                default: return "Discovered merchant";
            }
        }

        static bool IsExecutable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (
                address == 0 ||
                VirtualQuery(
                    reinterpret_cast<LPCVOID>(address),
                    &info,
                    sizeof(info)) == 0 ||
                info.State != MEM_COMMIT ||
                (info.Protect & PAGE_GUARD) != 0 ||
                (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            switch (info.Protect & 0xFF)
            {
                case PAGE_EXECUTE:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        template <typename T>
        static bool ReadValue(std::uintptr_t address, T& value)
        {
            return Core::Memory::Read(address, value);
        }

        static std::uintptr_t FindObjectAddressByGuid(std::uint64_t guid)
        {
            if (guid == 0)
                return 0;

            std::uint32_t manager = 0;
            if (!ReadValue(Wow5875::Offsets::ObjectManager::Root, manager) ||
                manager == 0 || (manager & 1u) != 0)
            {
                return 0;
            }

            std::uint32_t current = 0;
            if (!ReadValue(
                    static_cast<std::uintptr_t>(manager) +
                        Wow5875::Offsets::ObjectManager::FirstObject,
                    current))
            {
                return 0;
            }

            for (int i = 0; i < 4096; ++i)
            {
                if (current == 0 || (current & 1u) != 0)
                    break;

                std::uint64_t currentGuid = 0;
                if (!ReadValue(
                        static_cast<std::uintptr_t>(current) +
                            Wow5875::Offsets::Object::Guid,
                        currentGuid))
                {
                    break;
                }

                if (currentGuid == guid)
                    return static_cast<std::uintptr_t>(current);

                std::uint32_t next = 0;
                if (!ReadValue(
                        static_cast<std::uintptr_t>(current) +
                            Wow5875::Offsets::Object::Next,
                        next))
                {
                    break;
                }

                if (next == current)
                    break;

                current = next;
            }

            return 0;
        }

        static std::uintptr_t OnRightClickUnitAddress()
        {
            return Wow5875::Client::Base() + OnRightClickUnitRva;
        }

        static std::uintptr_t LuaDoStringAddress()
        {
            return Wow5875::Client::Base() + LuaDoStringRva;
        }

        static std::uintptr_t GetTextAddress()
        {
            return Wow5875::Client::Base() + GetTextRva;
        }

        static bool ExecuteLuaReadback(
            const std::string& script,
            const char* scriptName,
            std::string& result)
        {
            const auto doStringAddress = LuaDoStringAddress();
            const auto getTextAddress = GetTextAddress();

            if (!IsExecutable(doStringAddress) ||
                !IsExecutable(getTextAddress))
            {
                return false;
            }

            using DoStringFunction =
                bool (__fastcall*)(const char*, const char*);
            using GetTextFunction =
                const char* (__fastcall*)(char*, std::uint32_t, int);

            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);
            const auto getText =
                reinterpret_cast<GetTextFunction>(getTextAddress);

            char buffer[256]{};
            bool onGameThread = false;
            bool luaExecuted = false;
            bool gotText = false;

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (!onGameThread)
                        return;

                    luaExecuted = doString(script.c_str(), scriptName);
                    if (!luaExecuted)
                        return;

                    const char* raw = getText(
                        const_cast<char*>(LuaResultVariable),
                        0xFFFFFFFFu,
                        0);

                    if (raw != nullptr && *raw != '\0')
                    {
                        std::strncpy(buffer, raw, sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';
                        gotText = true;
                    }
                });

            if (!dispatched || !onGameThread || !luaExecuted || !gotText)
                return false;

            result = buffer;
            return true;
        }

        static bool MerchantOpen()
        {
            std::string result;
            const std::string script =
                "if MerchantFrame and MerchantFrame:IsShown() then "
                "WOW_INTERNAL_VENDOR_RESULT='open'; else "
                "WOW_INTERNAL_VENDOR_RESULT='closed'; end;";

            return ExecuteLuaReadback(
                       script,
                       "wow-internal/VendorMerchantProbe.lua",
                       result) &&
                   result == "open";
        }

        static void CloseMerchant()
        {
            const auto doStringAddress = LuaDoStringAddress();
            if (!IsExecutable(doStringAddress))
                return;

            using DoStringFunction =
                bool (__fastcall*)(const char*, const char*);
            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);

            GameThreadDispatcher::Invoke(
                [&]()
                {
                    if (!GameThreadDispatcher::IsGameThread())
                        return;

                    doString(
                        "if CloseMerchant then CloseMerchant(); elseif MerchantFrame then MerchantFrame:Hide(); end;",
                        "wow-internal/VendorCloseMerchant.lua");
                });
        }

        static void CloseNpcFrames()
        {
            const auto doStringAddress = LuaDoStringAddress();
            if (!IsExecutable(doStringAddress))
                return;

            using DoStringFunction =
                bool (__fastcall*)(const char*, const char*);
            const auto doString =
                reinterpret_cast<DoStringFunction>(doStringAddress);

            GameThreadDispatcher::Invoke(
                [&]()
                {
                    if (!GameThreadDispatcher::IsGameThread())
                        return;

                    doString(
                        "if CloseMerchant then CloseMerchant(); end; "
                        "if CloseGossip then CloseGossip(); end; "
                        "if CloseTrainer then CloseTrainer(); end; "
                        "if CloseQuest then CloseQuest(); end;",
                        "wow-internal/VendorCloseNpcFrames.lua");
                });
        }

        static bool ProbeMaintenanceSnapshotInternal(
            MaintenanceSnapshot& snapshot)
        {
            snapshot = MaintenanceSnapshot{};

            const std::string script =
                "local p=UnitPowerType('player'); if p==nil then p=-1 end; "
                "local money=0; if GetMoney then money=GetMoney() or 0 end; "
                "local mind=100; local dn=0; "
                "if GetInventoryItemDurability then for i=1,19 do "
                "local c,m=GetInventoryItemDurability(i); "
                "if c and m and m>0 then dn=dn+1; local q=(c*100)/m; if q<mind then mind=q end; end; end; end; "
                "local fc=0; local dc=0; "
                "if not WOW_INTERNAL_MAINT_TOOLTIP then "
                "WOW_INTERNAL_MAINT_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_MAINT_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_MAINT_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_MAINT_TOOLTIP; "
                "for b=0,4 do local n=GetContainerNumSlots(b) or 0; for sl=1,n do "
                "local link=GetContainerItemLink(b,sl); if link then "
                "local _,cnt=GetContainerItemInfo(b,sl); cnt=cnt or 1; "
                "tt:ClearLines(); tt:SetBagItem(b,sl); local f=0; local d=0; "
                "for j=1,tt:NumLines() do "
                "local l=getglobal('WOW_INTERNAL_MAINT_TOOLTIPTextLeft'..j); "
                "if l and l:GetText() then local t=string.lower(l:GetText()); "
                "if string.find(t,'health over') and string.find(t,'eating') then f=1 end; "
                "if string.find(t,'mana over') and string.find(t,'drinking') then d=1 end; end; "
                "local r=getglobal('WOW_INTERNAL_MAINT_TOOLTIPTextRight'..j); "
                "if r and r:GetText() then local t=string.lower(r:GetText()); "
                "if string.find(t,'health over') and string.find(t,'eating') then f=1 end; "
                "if string.find(t,'mana over') and string.find(t,'drinking') then d=1 end; end; end; "
                "if f==1 then fc=fc+cnt end; if d==1 then dc=dc+cnt end; "
                "end; end; end; "
                "WOW_INTERNAL_VENDOR_RESULT=p..'|'..money..'|'..mind..'|'..dn..'|'..fc..'|'..dc;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/MaintenanceInventoryProbe.lua",
                    result))
            {
                return false;
            }

            int powerType = -1;
            unsigned money = 0;
            float minimumDurability = 100.0f;
            int durableItems = 0;
            int foodCount = 0;
            int drinkCount = 0;

            if (std::sscanf(
                    result.c_str(),
                    "%d|%u|%f|%d|%d|%d",
                    &powerType,
                    &money,
                    &minimumDurability,
                    &durableItems,
                    &foodCount,
                    &drinkCount) != 6)
            {
                return false;
            }

            snapshot.valid = true;
            snapshot.powerType = powerType;
            snapshot.money = static_cast<std::uint32_t>(money);
            snapshot.minimumDurabilityPercent =
                std::clamp(minimumDurability, 0.0f, 100.0f);
            snapshot.durableItems = std::max(0, durableItems);
            snapshot.foodCount = std::max(0, foodCount);
            snapshot.drinkCount = std::max(0, drinkCount);
            return true;
        }

        enum class MerchantConsumableKind
        {
            Food,
            Drink
        };

        static bool FindMerchantConsumable(
            MerchantConsumableKind kind,
            int& merchantIndex,
            std::uint32_t& price)
        {
            merchantIndex = -1;
            price = 0;

            const std::string script =
                "WOW_INTERNAL_VENDOR_RESULT='none'; "
                "if not (MerchantFrame and MerchantFrame:IsShown()) then WOW_INTERNAL_VENDOR_RESULT='closed'; return; end; "
                "local lvl=UnitLevel('player') or 1; local best=0; local bestscore=-1; local bestprice=0; "
                "if not WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP then "
                "WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP; local n=GetMerchantNumItems and GetMerchantNumItems() or 0; "
                "for i=1,n do "
                "local name,tex,pr,qty,avail,usable=GetMerchantItemInfo(i); pr=pr or 0; qty=qty or 1; "
                "local ok=0; if usable then ok=1 end; "
                "local link=nil; if GetMerchantItemLink then link=GetMerchantItemLink(i) end; "
                "if link and GetItemInfo then local _,_,_,_,req=GetItemInfo(link); req=req or 0; if req<=lvl then ok=1 else ok=0 end; end; "
                "if ok==1 and pr>0 then tt:ClearLines(); tt:SetMerchantItem(i); local match=0; local score=0; "
                "for j=1,tt:NumLines() do local l=getglobal('WOW_INTERNAL_MAINT_MERCHANT_TOOLTIPTextLeft'..j); "
                "if l and l:GetText() then local t=string.lower(l:GetText()); " +
                std::string(kind == MerchantConsumableKind::Food
                    ? "if string.find(t,'health over') and string.find(t,'eating') then match=1; local _,_,v=string.find(t,'(%d+)%s+health%s+over'); score=math.max(score,tonumber(v or '0') or 0) end; "
                    : "if string.find(t,'mana over') and string.find(t,'drinking') then match=1; local _,_,v=string.find(t,'(%d+)%s+mana%s+over'); score=math.max(score,tonumber(v or '0') or 0) end; ") +
                "end; end; if match==1 then if score>bestscore or (score==bestscore and (bestprice==0 or pr<bestprice)) then best=i; bestscore=score; bestprice=pr end; end; end; end; "
                "if best>0 then WOW_INTERNAL_VENDOR_RESULT='candidate|'..best..'|'..bestprice else WOW_INTERNAL_VENDOR_RESULT='none' end;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    kind == MerchantConsumableKind::Food
                        ? "wow-internal/MerchantFoodProbe.lua"
                        : "wow-internal/MerchantDrinkProbe.lua",
                    result))
            {
                return false;
            }

            if (result == "closed")
                return false;
            if (result == "none")
                return true;

            unsigned parsedPrice = 0;
            if (std::sscanf(
                    result.c_str(),
                    "candidate|%d|%u",
                    &merchantIndex,
                    &parsedPrice) != 2)
            {
                merchantIndex = -1;
                return false;
            }

            price = static_cast<std::uint32_t>(parsedPrice);
            return true;
        }

        static bool IssueRepairAll(
            std::uint32_t& repairCost,
            bool& repairAvailable,
            bool& insufficientMoney)
        {
            repairCost = 0;
            repairAvailable = false;
            insufficientMoney = false;

            const std::string script =
                "WOW_INTERNAL_VENDOR_RESULT='unavailable'; "
                "if not (MerchantFrame and MerchantFrame:IsShown()) then WOW_INTERNAL_VENDOR_RESULT='closed'; return; end; "
                "local money=GetMoney and (GetMoney() or 0) or 0; local cost=0; local can=0; "
                "if GetRepairAllCost then local c,ok=GetRepairAllCost(); cost=c or 0; if ok then can=1 end; end; "
                "if can==1 and cost>0 then if money>=cost then if RepairAllItems then RepairAllItems(); WOW_INTERNAL_VENDOR_RESULT='repaired|'..cost; else WOW_INTERNAL_VENDOR_RESULT='unavailable'; end; "
                "else WOW_INTERNAL_VENDOR_RESULT='nomoney|'..cost; end; "
                "elseif can==1 then WOW_INTERNAL_VENDOR_RESULT='repaired|0'; end;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/MerchantRepairAll.lua",
                    result))
            {
                return false;
            }

            if (result == "closed")
                return false;
            if (result == "unavailable")
                return true;

            unsigned parsedCost = 0;
            if (std::sscanf(result.c_str(), "repaired|%u", &parsedCost) == 1)
            {
                repairAvailable = true;
                repairCost = parsedCost;
                return true;
            }

            if (std::sscanf(result.c_str(), "nomoney|%u", &parsedCost) == 1)
            {
                repairAvailable = true;
                insufficientMoney = true;
                repairCost = parsedCost;
                return true;
            }

            return false;
        }

        static bool BuyMerchantItemOnce(int merchantIndex)
        {
            if (merchantIndex <= 0)
                return false;

            const std::string script =
                "WOW_INTERNAL_VENDOR_RESULT='failed'; "
                "if MerchantFrame and MerchantFrame:IsShown() and BuyMerchantItem then "
                "BuyMerchantItem(" + std::to_string(merchantIndex) + ",1); WOW_INTERNAL_VENDOR_RESULT='issued'; end;";

            std::string result;
            return ExecuteLuaReadback(
                       script,
                       "wow-internal/MerchantBuyOne.lua",
                       result) &&
                   result == "issued";
        }

        const Objects::UnitState* FindConfiguredVendor(
            const Objects::WorldState& world) const
        {
            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (
                    !unit.valid ||
                    unit.guid == 0 ||
                    !IsConfiguredVendorEntry(unit.entryId))
                {
                    continue;
                }

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }

            return best;
        }

        const Objects::UnitState* FindServiceCandidate(
            const Objects::WorldState& world) const
        {
            // Keep Wuark preferred even after entering bounded local discovery,
            // unless this trip has already rejected him as a failed candidate.
            if (const auto* preferred = FindConfiguredVendor(world))
            {
                if (rejectedServiceEntries_.find(preferred->entryId) ==
                    rejectedServiceEntries_.end())
                {
                    return preferred;
                }
            }

            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 || unit.entryId == 0 ||
                    unit.health == 0 || unit.npcFlags == 0 ||
                    rejectedServiceEntries_.find(unit.entryId) !=
                        rejectedServiceEntries_.end())
                {
                    continue;
                }

                // UNIT_FLAG_NOT_SELECTABLE.  Other NPC flags are deliberately
                // not decoded here: service discovery proves a merchant by
                // opening MerchantFrame rather than trusting a guessed flag map.
                if ((unit.unitFlags & 0x02000000u) != 0)
                    continue;

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        const Objects::UnitState* FindKnownVendor(
            const Objects::WorldState& world) const
        {
            if (!knownVendorValid_ || knownVendorEntry_ == 0)
                return nullptr;

            const Objects::UnitState* best = nullptr;
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 ||
                    unit.entryId != knownVendorEntry_)
                {
                    continue;
                }

                if (best == nullptr || unit.distance < best->distance)
                    best = &unit;
            }
            return best;
        }

        const Objects::UnitState* FindVendor(
            const Objects::WorldState& world) const
        {
            if (serviceSearchMode_)
                return FindServiceCandidate(world);

            if (const auto* configured = FindConfiguredVendor(world))
                return configured;

            return FindKnownVendor(world);
        }

        const Objects::UnitState* FindVendorByGuid(
            const Objects::WorldState& world) const
        {
            // Phase 14L.1.4: once a vendor trip has selected a concrete GUID,
            // never silently substitute another nearby merchant. Local final
            // approach/recovery must remain owned by the same target.
            if (vendorGuid_ != 0)
            {
                for (const auto& unit : world.units)
                {
                    if (unit.valid && unit.guid == vendorGuid_)
                        return &unit;
                }

                return nullptr;
            }

            return FindVendor(world);
        }

        void LearnVerifiedMerchant(const Objects::UnitState& vendor)
        {
            if (!vendor.valid || vendor.guid == 0 || vendor.entryId == 0)
                return;

            // Phase 14L.2: a fallback merchant may service the current trip,
            // but it must never replace Wuark as the persistent primary route.
            if (vendor.entryId != PreferredVendorEntry)
            {
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.2: MerchantFrame verified fallback merchant entry=" +
                    std::to_string(vendor.entryId) +
                    "; keeping Wuark (3167) as persistent primary.");
                return;
            }

            const bool changed =
                !knownVendorValid_ ||
                knownVendorEntry_ != vendor.entryId ||
                Distance2D(
                    knownVendorPosition_.x,
                    knownVendorPosition_.y,
                    vendor.x,
                    vendor.y) > 1.0f;

            knownVendorValid_ = true;
            knownVendorEntry_ = vendor.entryId;
            knownVendorPosition_ = Navigation::NavPoint{
                vendor.x,
                vendor.y,
                vendor.z
            };

            if (changed)
            {
                Debug::Logger::Info(
                    "VENDOR DISCOVERY 14L.0: MerchantFrame verified merchant entry=" +
                    std::to_string(vendor.entryId) +
                    " pos=(" + Float(vendor.x) + "," +
                    Float(vendor.y) + "," + Float(vendor.z) + ")");
                SaveVendorMemory();
                Debug::Logger::Info(
                    "VENDOR ROUTING 14L.2: persisted MerchantFrame-verified Wuark for future sessions.");
            }
        }

        void SetState(VendorState state, std::uint64_t tick)
        {
            if (state_ == state)
                return;

            Debug::Logger::Info(
                std::string("GRIND 14G.1 VENDOR: state ") +
                StateNameInternal(state_) + " -> " +
                StateNameInternal(state));

            state_ = state;
            stateStartedTick_ = tick;
        }

        void Fail(const std::string& reason, std::uint64_t tick)
        {
            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.1 VENDOR: FAILED");
            Debug::Logger::Info("Reason: " + reason);
            Debug::Logger::Info("================================");
            SetState(VendorState::Failed, tick);
        }

        bool StartHomeNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick,
            bool returningAfterSale)
        {
            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if (!nav->Start(
                    world.player,
                    tick,
                    grindHome_,
                    MapId,
                    HomeArrivalDistance,
                    returningAfterSale
                        ? "grind home after vendor"
                        : "grind home for vendor search"))
            {
                return false;
            }

            if (returningAfterSale)
            {
                returnNavigator_ = std::move(nav);
                SetState(VendorState::ReturningToGrind, tick);
            }
            else
            {
                homeNavigator_ = std::move(nav);
                SetState(VendorState::ReturningHomeForSearch, tick);
            }

            return true;
        }

        bool StartVendorNavigation(
            const Objects::WorldState& world,
            const Objects::UnitState& vendor,
            std::uint64_t tick)
        {
            vendorGuid_ = vendor.guid;
            vendorEntry_ = vendor.entryId;
            localRecoveryAttempts_ = 0;

            if (vendor.distance <= ProactiveVendorHandoffDistance)
            {
                vendorNavigator_.reset();
                directMoves_ = 0;
                directApproachMaxDistance_ = ProactiveVendorHandoffDistance;
                lastDirectDistance_ = vendor.distance;
                directNoProgressMoves_ = 0;
                lastDirectMoveTick_ = 0;
                Debug::Logger::Info(
                    "VENDOR PROACTIVE HANDOFF 14L.1.6: live merchant already inside 16 yd proactive handoff radius at route start; using bounded direct approach. distance=" +
                    Float(vendor.distance) +
                    " max=" + Float(ProactiveVendorHandoffDistance));
                SetState(VendorState::DirectVendorApproach, tick);
                return true;
            }

            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            const Navigation::NavPoint destination{
                vendor.x,
                vendor.y,
                vendor.z
            };

            if (!nav->Start(
                    world.player,
                    tick,
                    destination,
                    MapId,
                    VendorNavArrivalDistance,
                    std::string("merchant entry=") + std::to_string(vendor.entryId) +
                        " " + VendorName(vendor.entryId)))
            {
                if (vendor.distance <= DirectFallbackRadius)
                {
                    vendorNavigator_.reset();
                    directMoves_ = 0;
                    directApproachMaxDistance_ = DirectFallbackRadius;
                    lastDirectDistance_ = vendor.distance;
                    directNoProgressMoves_ = 0;
                    lastDirectMoveTick_ = 0;
                    SetState(VendorState::DirectVendorApproach, tick);
                    return true;
                }

                return false;
            }

            vendorNavigator_ = std::move(nav);
            SetState(VendorState::NavigatingVendor, tick);
            return true;
        }

        bool StartKnownVendorAnchorNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (!knownVendorValid_)
                return false;

            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if (!nav->Start(
                    world.player,
                    tick,
                    RazorHillServiceHub,
                    MapId,
                    RemoteHubArrivalDistance,
                    "Razor Hill Wuark service hub from persistent memory"))
            {
                return false;
            }

            vendorNavigator_ = std::move(nav);
            vendorEntry_ = 0; // hub route has no concrete live merchant selected yet
            vendorGuid_ = 0;
            remoteHubRouting_ = true;
            primaryHubReached_ = false;

            Debug::Logger::Info(
                "VENDOR ROUTING 14L.2: persistent Wuark memory present; routing to standable Razor Hill service hub pos=(" +
                Float(RazorHillServiceHub.x) + "," +
                Float(RazorHillServiceHub.y) + "," +
                Float(RazorHillServiceHub.z) + ")");

            SetState(VendorState::NavigatingVendorAnchor, tick);
            return true;
        }

        bool StartRemoteServiceHubNavigation(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            if (!nav->Start(
                    world.player,
                    tick,
                    RazorHillServiceHub,
                    MapId,
                    RemoteHubArrivalDistance,
                    "Razor Hill Wuark primary service hub"))
            {
                return false;
            }

            vendorNavigator_ = std::move(nav);
            vendorGuid_ = 0;
            vendorEntry_ = 0; // hub route has no concrete live merchant selected yet
            serviceSearchMode_ = false;
            remoteHubRouting_ = true;
            primaryHubReached_ = false;
            Debug::Logger::Info(
                "VENDOR ROUTING 14L.2: Wuark is not currently live; routing to standable Razor Hill service hub pos=(" +
                Float(RazorHillServiceHub.x) + "," + Float(RazorHillServiceHub.y) +
                "," + Float(RazorHillServiceHub.z) + ")");
            SetState(VendorState::NavigatingVendorAnchor, tick);
            return true;
        }

        bool TryLocalVendorApproachRecovery(
            const Objects::WorldState& world,
            const Objects::UnitState& vendor,
            std::uint64_t tick)
        {
            if (
                vendor.distance > LocalRecoveryTriggerDistance ||
                localRecoveryAttempts_ >= MaximumLocalRecoveryAttempts ||
                directMoves_ >= MaximumDirectMoves)
            {
                return false;
            }

            const float dx = world.player.x - vendor.x;
            const float dy = world.player.y - vendor.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length < 0.01f)
                return false;

            const float ux = dx / length;
            const float uy = dy / length;

            // Probe alternating points around the merchant rather than aiming
            // at the same blocked center line. Size the XY ring from the live
            // vertical separation so the intended 3D separation is inside the
            // reliable interaction threshold when the current ground plane
            // makes that geometrically possible.
            const float verticalSeparation = std::fabs(vendor.z - world.player.z);
            const float horizontalBudgetSquared = std::max(
                0.0f,
                LocalRecoveryTargetDistance3D * LocalRecoveryTargetDistance3D -
                    verticalSeparation * verticalSeparation);
            const float geometryAwareRadius = std::clamp(
                std::sqrt(horizontalBudgetSquared),
                LocalRecoveryMinimumRadius,
                LocalRecoveryMaximumRadius);

            float cosAngle = 0.5f;
            float sinAngle = 0.8660254f;
            const char* side = "left-60";
            if (localRecoveryAttempts_ == 1)
            {
                sinAngle = -0.8660254f;
                side = "right-60";
            }
            else if (localRecoveryAttempts_ >= 2)
            {
                cosAngle = -0.5f;
                sinAngle = 0.8660254f;
                side = "left-120";
            }

            const float rx = ux * cosAngle - uy * sinAngle;
            const float ry = ux * sinAngle + uy * cosAngle;
            const float targetX = vendor.x + rx * geometryAwareRadius;
            const float targetY = vendor.y + ry * geometryAwareRadius;
            // Keep a local lateral probe on the player's current ground plane.
            // The merchant's object-origin Z is not a safe CTM ground target.
            const float targetZ = world.player.z;

            if (!ClickToMoveController::MoveTo(
                    world.player,
                    targetX,
                    targetY,
                    targetZ,
                    LocalRecoveryPrecision))
            {
                Debug::Logger::Info(
                    "VENDOR LOCAL APPROACH 14L.1.4: flank recovery CTM dispatch failed attempt=" +
                    std::to_string(localRecoveryAttempts_ + 1) +
                    "/" + std::to_string(MaximumLocalRecoveryAttempts));
                return false;
            }

            ++localRecoveryAttempts_;
            ++directMoves_;
            directNoProgressMoves_ = 0;
            // A lateral probe intentionally may not reduce radial distance.
            // Suppress the old radial-progress comparison until the following
            // direct-to-merchant move has established a new baseline.
            lastDirectDistance_ = 0.0f;
            lastDirectMoveTick_ = tick;

            Debug::Logger::Info(
                "VENDOR LOCAL APPROACH 14L.1.4: flank recovery issued side=" +
                std::string(side) +
                " attempt=" + std::to_string(localRecoveryAttempts_) +
                "/" + std::to_string(MaximumLocalRecoveryAttempts) +
                " vendorDistance=" + Float(vendor.distance) +
                " vertical=" + Float(verticalSeparation) +
                " ringRadius=" + Float(geometryAwareRadius) +
                " target=(" + Float(targetX) + "," +
                Float(targetY) + "," + Float(targetZ) + ")" +
                " move=" + std::to_string(directMoves_) +
                "/" + std::to_string(MaximumDirectMoves));
            return true;
        }

        bool IssueInteraction(
            const Objects::UnitState& vendor,
            std::uint64_t tick)
        {
            if (interactionAttempts_ >= MaximumInteractionAttempts)
                return false;

            const auto functionAddress = OnRightClickUnitAddress();
            if (!IsExecutable(functionAddress))
                return false;

            const std::uintptr_t objectAddress =
                FindObjectAddressByGuid(vendor.guid);
            if (objectAddress == 0)
                return false;

            using OnRightClickUnitFunction =
                void (__thiscall*)(std::uint32_t, int);
            const auto onRightClickUnit =
                reinterpret_cast<OnRightClickUnitFunction>(functionAddress);

            bool onGameThread = false;
            const std::uint32_t npcThis =
                static_cast<std::uint32_t>(objectAddress);

            const bool dispatched = GameThreadDispatcher::Invoke(
                [&]()
                {
                    onGameThread = GameThreadDispatcher::IsGameThread();
                    if (onGameThread)
                        onRightClickUnit(npcThis, 0);
                });

            if (!dispatched || !onGameThread)
                return false;

            vendorGuid_ = vendor.guid;
            vendorEntry_ = vendor.entryId;
            ++interactionAttempts_;
            lastInteractionTick_ = tick;

            Debug::Logger::Info(
                "GRIND 14G.1 VENDOR: interaction issued entry=" +
                std::to_string(vendor.entryId) +
                " name=" + VendorName(vendor.entryId) +
                " distance=" + Float(vendor.distance) +
                " attempt=" + std::to_string(interactionAttempts_) +
                "/" + std::to_string(MaximumInteractionAttempts));

            SetState(VendorState::WaitingForMerchant, tick);
            return true;
        }

        std::string BlockedSlotLuaExpression() const
        {
            if (blockedBagSlots_.empty())
                return "false";

            std::string expression;
            bool first = true;
            for (const std::uint32_t key : blockedBagSlots_)
            {
                const int bag = static_cast<int>((key >> 16) & 0xFFFFu);
                const int slot = static_cast<int>(key & 0xFFFFu);
                if (!first)
                    expression += " or ";
                expression += "(b==" + std::to_string(bag) +
                    " and s==" + std::to_string(slot) + ")";
                first = false;
            }
            return expression;
        }

        enum class SellStepResult
        {
            Failed,
            MerchantClosed,
            CandidateIssued,
            Done
        };

        SellStepResult SellOneUnprotectedItem(
            int& bag,
            int& slot,
            std::uint32_t& itemId)
        {
            bag = -1;
            slot = -1;
            itemId = 0;

            const std::string blocked = BlockedSlotLuaExpression();

            const std::string script =
                "WOW_INTERNAL_VENDOR_RESULT='none'; "
                "if not (MerchantFrame and MerchantFrame:IsShown()) then "
                "WOW_INTERNAL_VENDOR_RESULT='merchant_closed'; return; end; "
                "if not WOW_INTERNAL_VENDOR_TOOLTIP then "
                "WOW_INTERNAL_VENDOR_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_VENDOR_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_VENDOR_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_VENDOR_TOOLTIP; local found=0; "
                "for b=0,4 do if found==0 then local n=GetContainerNumSlots(b) or 0; "
                "for s=1,n do if found==0 and not (" + blocked + ") then "
                "local link=GetContainerItemLink(b,s); if link then "
                "local _,_,id=string.find(link,'item:(%d+)'); id=tonumber(id or '0') or 0; "
                "local texture,count,locked=GetContainerItemInfo(b,s); "
                "local name,ilink,quality,ilvl,req,itype=GetItemInfo(link); "
                "local protect=0; "
                "if locked then protect=1 end; "
                "if id==6948 then protect=1 end; "
                "if itype=='Quest' or itype=='Key' then protect=1 end; "
                "tt:ClearLines(); tt:SetBagItem(b,s); "
                "for i=1,tt:NumLines() do "
                "local l=getglobal('WOW_INTERNAL_VENDOR_TOOLTIPTextLeft'..i); "
                "if l and l:GetText() then local t=string.lower(l:GetText()); "
                "if string.find(t,'quest item') then protect=1 end; "
                "if string.find(t,'health over') and string.find(t,'eating') then protect=1 end; "
                "if string.find(t,'mana over') and string.find(t,'drinking') then protect=1 end; end; "
                "local r=getglobal('WOW_INTERNAL_VENDOR_TOOLTIPTextRight'..i); "
                "if r and r:GetText() then local t=string.lower(r:GetText()); "
                "if string.find(t,'quest item') then protect=1 end; "
                "if string.find(t,'health over') and string.find(t,'eating') then protect=1 end; "
                "if string.find(t,'mana over') and string.find(t,'drinking') then protect=1 end; end; end; "
                "if protect==0 then "
                "WOW_INTERNAL_VENDOR_RESULT='try|'..b..'|'..s..'|'..id; "
                "UseContainerItem(b,s); found=1; end; "
                "end; end; end; end; end; "
                "if found==0 then WOW_INTERNAL_VENDOR_RESULT='done'; end;";

            std::string result;
            if (!ExecuteLuaReadback(
                    script,
                    "wow-internal/VendorSellOne.lua",
                    result))
            {
                return SellStepResult::Failed;
            }

            if (result == "merchant_closed")
                return SellStepResult::MerchantClosed;

            if (result == "done")
                return SellStepResult::Done;

            unsigned parsedId = 0;
            if (std::sscanf(
                    result.c_str(),
                    "try|%d|%d|%u",
                    &bag,
                    &slot,
                    &parsedId) != 3)
            {
                return SellStepResult::Failed;
            }

            itemId = static_cast<std::uint32_t>(parsedId);
            return SellStepResult::CandidateIssued;
        }

        static std::uint32_t SlotKey(int bag, int slot)
        {
            return
                (static_cast<std::uint32_t>(bag & 0xFFFF) << 16) |
                static_cast<std::uint32_t>(slot & 0xFFFF);
        }

        bool StartAlternateServiceSearch(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (vendorEntry_ != 0)
                rejectedServiceEntries_.insert(vendorEntry_);

            CloseNpcFrames();
            vendorNavigator_.reset();
            vendorGuid_ = 0;
            interactionAttempts_ = 0;
            directMoves_ = 0;
            directApproachMaxDistance_ = DirectFallbackRadius;
            lastDirectDistance_ = 0.0f;
            directNoProgressMoves_ = 0;
            localRecoveryAttempts_ = 0;
            lastInteractionTick_ = 0;
            lastDirectMoveTick_ = 0;
            repairUnavailableHere_ = false;
            foodUnavailableHere_ = false;
            drinkUnavailableHere_ = false;
            serviceSearchMode_ = true;
            ++serviceCandidatesTried_;

            if (serviceCandidatesTried_ >= MaximumServiceCandidates)
            {
                maintenanceUnmet_ = true;
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: service candidate budget exhausted; returning to grind with bounded retry backoff.");
                return false;
            }

            const auto* candidate = FindServiceCandidate(world);
            if (candidate != nullptr)
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: trying alternate service NPC entry=" +
                    std::to_string(candidate->entryId) +
                    " distance=" + Float(candidate->distance) +
                    " attempt=" + std::to_string(serviceCandidatesTried_) +
                    "/" + std::to_string(MaximumServiceCandidates));

                if (candidate->distance <= InteractionDistance)
                    return IssueInteraction(*candidate, tick);

                return StartVendorNavigation(world, *candidate, tick);
            }

            SetState(VendorState::SearchingVendor, tick);
            return true;
        }

        bool RunMaintenanceStep(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (lastMaintenanceStepTick_ != 0 &&
                tick < lastMaintenanceStepTick_ + MaintenanceStepTicks)
            {
                return true;
            }
            lastMaintenanceStepTick_ = tick;

            MaintenanceSnapshot snapshot{};
            if (!ProbeMaintenanceSnapshotInternal(snapshot))
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: inventory/durability probe failed; keeping merchant open for bounded retry.");
                return true;
            }
            lastMaintenanceSnapshot_ = snapshot;

            if (requestedMaintenance_.repair && !repairSatisfied_)
            {
                if (snapshot.durableItems == 0 ||
                    snapshot.minimumDurabilityPercent >
                        AutonomousMaintenancePolicy::RepairTripThresholdPercent)
                {
                    repairSatisfied_ = true;
                }
                else if (!repairUnavailableHere_)
                {
                    std::uint32_t cost = 0;
                    bool available = false;
                    bool noMoney = false;
                    if (!IssueRepairAll(cost, available, noMoney))
                        return true;

                    if (!available)
                    {
                        repairUnavailableHere_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: current merchant cannot repair; alternate service search required.");
                    }
                    else if (noMoney)
                    {
                        maintenanceUnmet_ = true;
                        repairSatisfied_ = true; // terminal for this trip; another vendor cannot fix the purse.
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: repair needed but unaffordable cost=" +
                            std::to_string(cost) +
                            " money=" + std::to_string(snapshot.money));
                    }
                    else
                    {
                        ++repairActions_;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: RepairAllItems issued cost=" +
                            std::to_string(cost) +
                            " minDurabilityBefore=" +
                            Float(snapshot.minimumDurabilityPercent));
                        return true;
                    }
                }
            }

            if (requestedMaintenance_.food && !foodSatisfied_)
            {
                if (!AutonomousMaintenancePolicy::WantsFoodTopUp(snapshot))
                {
                    foodSatisfied_ = true;
                }
                else if (!foodUnavailableHere_)
                {
                    int index = -1;
                    std::uint32_t price = 0;
                    if (!FindMerchantConsumable(
                            MerchantConsumableKind::Food,
                            index,
                            price))
                    {
                        return true;
                    }

                    if (index <= 0)
                    {
                        foodUnavailableHere_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: current merchant has no usable food; alternate service search required.");
                    }
                    else if (!AutonomousMaintenancePolicy::CanBuyConsumable(
                                 snapshot.money,
                                 price))
                    {
                        maintenanceUnmet_ = true;
                        foodSatisfied_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: food restock stopped by cash reserve money=" +
                            std::to_string(snapshot.money) +
                            " price=" + std::to_string(price));
                    }
                    else if (BuyMerchantItemOnce(index))
                    {
                        ++foodPurchases_;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: bought one food merchant unit index=" +
                            std::to_string(index) +
                            " price=" + std::to_string(price) +
                            " inventoryBefore=" +
                            std::to_string(snapshot.foodCount) +
                            " target=" +
                            std::to_string(AutonomousMaintenancePolicy::FoodTarget));
                        return true;
                    }
                }
            }

            if (requestedMaintenance_.drink && !drinkSatisfied_)
            {
                if (!AutonomousMaintenancePolicy::WantsDrinkTopUp(snapshot))
                {
                    drinkSatisfied_ = true;
                }
                else if (!drinkUnavailableHere_)
                {
                    int index = -1;
                    std::uint32_t price = 0;
                    if (!FindMerchantConsumable(
                            MerchantConsumableKind::Drink,
                            index,
                            price))
                    {
                        return true;
                    }

                    if (index <= 0)
                    {
                        drinkUnavailableHere_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: current merchant has no usable drink; alternate service search required.");
                    }
                    else if (!AutonomousMaintenancePolicy::CanBuyConsumable(
                                 snapshot.money,
                                 price))
                    {
                        maintenanceUnmet_ = true;
                        drinkSatisfied_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: drink restock stopped by cash reserve money=" +
                            std::to_string(snapshot.money) +
                            " price=" + std::to_string(price));
                    }
                    else if (BuyMerchantItemOnce(index))
                    {
                        ++drinkPurchases_;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: bought one drink merchant unit index=" +
                            std::to_string(index) +
                            " price=" + std::to_string(price) +
                            " inventoryBefore=" +
                            std::to_string(snapshot.drinkCount) +
                            " target=" +
                            std::to_string(AutonomousMaintenancePolicy::DrinkTarget));
                        return true;
                    }
                }
            }

            const bool needsAlternate =
                (requestedMaintenance_.repair && !repairSatisfied_ && repairUnavailableHere_) ||
                (requestedMaintenance_.food && !foodSatisfied_ && foodUnavailableHere_) ||
                (requestedMaintenance_.drink && !drinkSatisfied_ && drinkUnavailableHere_);

            if (needsAlternate)
            {
                if (StartAlternateServiceSearch(world, tick))
                    return true;
                maintenanceUnmet_ = true;
            }

            if ((!requestedMaintenance_.repair || repairSatisfied_) &&
                (!requestedMaintenance_.food || foodSatisfied_) &&
                (!requestedMaintenance_.drink || drinkSatisfied_))
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: maintenance pass complete repairs=" +
                    std::to_string(repairActions_) +
                    " foodPurchases=" + std::to_string(foodPurchases_) +
                    " drinkPurchases=" + std::to_string(drinkPurchases_) +
                    " unmet=" + (maintenanceUnmet_ ? std::string("yes") : std::string("no")));
                return false;
            }

            if (maintenanceUnmet_ && !needsAlternate)
                return false;

            return true;
        }

        bool FinishVendorAndReturn(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            GrindBagMonitor::Snapshot bags{};
            if (!GrindBagMonitor::Read(bags))
            {
                Fail("bag verification failed after vendor pass.", tick);
                return false;
            }

            Debug::Logger::Info(
                "GRIND 14G.1 VENDOR: sell pass complete freeSlots=" +
                std::to_string(bags.freeSlots) +
                "/" + std::to_string(bags.totalSlots) +
                " salesObserved=" + std::to_string(salesObserved_) +
                " unsellableSlotsSkipped=" +
                std::to_string(unsellableSlotsSkipped_));

            if (bagPressureTrigger_ &&
                bags.freeSlots < MinimumFreeSlotsAfterVendor)
            {
                Fail(
                    "vendor pass completed but fewer than two free bag slots remain; protected/unsellable items fill the bags.",
                    tick);
                return false;
            }

            CloseNpcFrames();

            if (
                Distance2D(
                    world.player.x,
                    world.player.y,
                    grindHome_.x,
                    grindHome_.y) <= HomeArrivalDistance)
            {
                SetState(VendorState::Done, tick);
                return true;
            }

            if (!StartHomeNavigation(world, tick, true))
            {
                Fail("failed to start return route to grind home after selling.", tick);
                return false;
            }

            return true;
        }

    public:
        void ObserveWorld(const Objects::WorldState& world)
        {
            const auto* vendor = FindConfiguredVendor(world);
            if (vendor == nullptr)
                return;

            const bool changed =
                !knownVendorValid_ ||
                knownVendorEntry_ != vendor->entryId ||
                Distance2D(
                    knownVendorPosition_.x,
                    knownVendorPosition_.y,
                    vendor->x,
                    vendor->y) > 1.0f;

            knownVendorValid_ = true;
            knownVendorEntry_ = vendor->entryId;
            knownVendorPosition_ = Navigation::NavPoint{
                vendor->x,
                vendor->y,
                vendor->z
            };

            if (changed)
            {
                Debug::Logger::Info(
                    "GRIND 14G.1 VENDOR: cached live vendor entry=" +
                    std::to_string(knownVendorEntry_) +
                    " name=" + VendorName(knownVendorEntry_) +
                    " pos=(" + Float(knownVendorPosition_.x) + "," +
                    Float(knownVendorPosition_.y) + "," +
                    Float(knownVendorPosition_.z) + ")");
            }
        }

        bool Start(
            const Objects::WorldState& world,
            const Navigation::NavPoint& grindHome,
            std::uint64_t tick,
            MaintenanceNeed maintenanceNeed = {},
            bool bagPressureTrigger = false)
        {
            if (state_ != VendorState::Idle)
                return false;

            grindHome_ = grindHome;
            LoadVendorMemory();
            remoteHubRouting_ = false;
            primaryHubReached_ = false;
            vendorGuid_ = 0;
            vendorEntry_ = 0;
            stateStartedTick_ = tick;
            lastInteractionTick_ = 0;
            lastDirectMoveTick_ = 0;
            lastSaleTick_ = 0;
            interactionAttempts_ = 0;
            directMoves_ = 0;
            directApproachMaxDistance_ = DirectFallbackRadius;
            lastDirectDistance_ = 0.0f;
            directNoProgressMoves_ = 0;
            localRecoveryAttempts_ = 0;
            saleAttempts_ = 0;
            salesObserved_ = 0;
            unsellableSlotsSkipped_ = 0;
            lastCandidateBag_ = -1;
            lastCandidateSlot_ = -1;
            lastCandidateItemId_ = 0;
            candidatePendingVerification_ = false;
            blockedBagSlots_.clear();
            requestedMaintenance_ = maintenanceNeed;
            bagPressureTrigger_ = bagPressureTrigger;
            serviceSearchMode_ = false;
            repairSatisfied_ = !requestedMaintenance_.repair;
            foodSatisfied_ = !requestedMaintenance_.food;
            drinkSatisfied_ = !requestedMaintenance_.drink;
            maintenanceUnmet_ = false;
            repairUnavailableHere_ = false;
            foodUnavailableHere_ = false;
            drinkUnavailableHere_ = false;
            lastMaintenanceStepTick_ = 0;
            repairActions_ = 0;
            foodPurchases_ = 0;
            drinkPurchases_ = 0;
            serviceCandidatesTried_ = 0;
            rejectedServiceEntries_.clear();
            maintenanceAtStart_ = MaintenanceSnapshot{};
            lastMaintenanceSnapshot_ = MaintenanceSnapshot{};
            ProbeMaintenanceSnapshotInternal(maintenanceAtStart_);

            ObserveWorld(world);

            Debug::Logger::Info("================================");
            Debug::Logger::Info("GRIND 14G.1 VENDOR: START");
            Debug::Logger::Info(
                "Grind home=(" + Float(grindHome_.x) + "," +
                Float(grindHome_.y) + "," + Float(grindHome_.z) + ")");
            Debug::Logger::Info(
                "VENDOR DISCOVERY 14L.0: configured/learned merchant first; otherwise bounded local NPC service discovery proves candidates by MerchantFrame.");
            Debug::Logger::Info(
                "VENDOR ROUTING 14L.2: Wuark (3167) is primary for sell/repair; route to the player-proven Razor Hill hub, wait for Wuark, then use bounded local MerchantFrame discovery only as fallback.");
            Debug::Logger::Info(
                "VENDOR FINAL APPROACH 14L.1.3: a failed NavMesh route may hand off to bounded direct CTM only while the same live merchant remains within 24 yd; progress is checked before each retry.");
            Debug::Logger::Info(
                "VENDOR LOCAL APPROACH 14L.1.4: selected vendor GUID stays locked; direct stalls within 8 yd may use up to three bounded flank probes on a 3.75 yd interaction ring.");
            Debug::Logger::Info(
                "VENDOR PROACTIVE HANDOFF 14L.1.6: selected live merchants at <=16 yd bypass/leave NavMesh and use the existing bounded direct/local final-approach path.");
            Debug::Logger::Info(
                "VENDOR INTERACTION GEOMETRY 14L.1.7: local flank probes keep the player's current Z and use a geometry-aware 1-3 yd XY ring targeting 4.0 yd 3D separation inside the unchanged 4.5 yd interaction threshold.");
            Debug::Logger::Info(
                "Sell policy: all bag items attempted except Hearthstone, quest/key items, locked items, and food/drink used by recovery.");
            Debug::Logger::Info(
                "MAINTENANCE 14G.5.1: requested repair=" +
                std::string(requestedMaintenance_.repair ? "yes" : "no") +
                " food=" + (requestedMaintenance_.food ? std::string("yes") : std::string("no")) +
                " drink=" + (requestedMaintenance_.drink ? std::string("yes") : std::string("no")) +
                " bagPressure=" + (bagPressureTrigger_ ? std::string("yes") : std::string("no")));
            if (maintenanceAtStart_.valid)
            {
                Debug::Logger::Info(
                    "MAINTENANCE 14G.5.1: start minDurability=" +
                    Float(maintenanceAtStart_.minimumDurabilityPercent) +
                    " food=" + std::to_string(maintenanceAtStart_.foodCount) +
                    " drink=" + std::to_string(maintenanceAtStart_.drinkCount) +
                    " money=" + std::to_string(maintenanceAtStart_.money));
            }
            Debug::Logger::Info("================================");

            const auto* vendor = FindVendor(world);
            if (vendor != nullptr)
            {
                if (vendor->distance <= InteractionDistance)
                    return IssueInteraction(*vendor, tick);

                if (StartVendorNavigation(world, *vendor, tick))
                    return true;
            }

            if (knownVendorValid_ && StartKnownVendorAnchorNavigation(world, tick))
                return true;

            // Phase 14L.2: when Wuark is not currently in ObjectManager and no
            // persisted Wuark route is available, navigate to the player-proven
            // Razor Hill service hub. There we wait briefly for Wuark before
            // falling back to bounded local MerchantFrame discovery.
            if (StartRemoteServiceHubNavigation(world, tick))
                return true;

            // Phase 14L.0: grindHome_ is the captured pre-vendor resume point, not
            // a vendor-search hub. Returning to it before SearchingVendor simply
            // searches the same grind pocket forever. When no preferred/learned
            // merchant is live, immediately switch to bounded local service
            // discovery and prove a candidate by MerchantFrame.
            Debug::Logger::Info(
                "VENDOR DISCOVERY 14L.0: no preferred or learned merchant is currently live; starting bounded local service discovery from the current area.");
            if (!StartAlternateServiceSearch(world, tick))
            {
                Fail("dynamic local merchant discovery could not start.", tick);
                return false;
            }

            return true;
        }

        void Update(
            const Objects::WorldState& world,
            std::uint64_t tick)
        {
            if (
                state_ == VendorState::Idle ||
                state_ == VendorState::Done ||
                state_ == VendorState::Failed)
            {
                return;
            }

            if (state_ == VendorState::ReturningHomeForSearch)
            {
                if (!homeNavigator_)
                {
                    Fail("home navigator missing.", tick);
                    return;
                }

                homeNavigator_->Update(world.player, tick);
                if (homeNavigator_->Arrived())
                {
                    homeNavigator_.reset();
                    SetState(VendorState::SearchingVendor, tick);
                }
                else if (homeNavigator_->Failed())
                {
                    Fail("NavMesh return to vendor-search home failed.", tick);
                }
                return;
            }

            if (state_ == VendorState::SearchingVendor)
            {
                const auto* vendor = FindVendor(world);
                if (vendor != nullptr)
                {
                    primaryHubReached_ = false;
                    vendorGuid_ = vendor->guid;
                    vendorEntry_ = vendor->entryId;

                    if (vendor->distance <= InteractionDistance)
                    {
                        if (!IssueInteraction(*vendor, tick))
                        {
                            if (serviceSearchMode_)
                            {
                                if (!StartAlternateServiceSearch(world, tick))
                                    FinishVendorAndReturn(world, tick);
                            }
                            else
                            {
                                Fail("native vendor interaction failed.", tick);
                            }
                        }
                        return;
                    }

                    if (!StartVendorNavigation(world, *vendor, tick))
                    {
                        if (serviceSearchMode_)
                        {
                            if (!StartAlternateServiceSearch(world, tick))
                                FinishVendorAndReturn(world, tick);
                        }
                        else
                        {
                            Fail("failed to route to live configured vendor.", tick);
                        }
                    }
                    return;
                }

                const std::uint64_t waitTicks =
                    serviceSearchMode_ ? ServiceSearchWaitTicks : VendorSearchWaitTicks;
                if (tick >= stateStartedTick_ + waitTicks)
                {
                    if (serviceSearchMode_)
                    {
                        maintenanceUnmet_ = true;
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: no additional service NPC became visible within bounded search window; returning to grind.");
                        FinishVendorAndReturn(world, tick);
                    }
                    else if (primaryHubReached_)
                    {
                        primaryHubReached_ = false;
                        Debug::Logger::Info(
                            "VENDOR ROUTING 14L.2: Wuark was not visible within the bounded Razor Hill wait window; starting local MerchantFrame-proven fallback discovery.");
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail(
                            "preferred Wuark merchant was not visible within the bounded search window and no dynamic service search was active.",
                            tick);
                    }
                }
                return;
            }

            if (state_ == VendorState::NavigatingVendorAnchor)
            {
                const auto* liveVendor = FindVendor(world);
                if (liveVendor != nullptr)
                {
                    vendorNavigator_.reset();
                    remoteHubRouting_ = false;
                    primaryHubReached_ = false;
                    if (liveVendor->distance <= InteractionDistance)
                    {
                        if (!IssueInteraction(*liveVendor, tick))
                            Fail("vendor interaction failed after cached-anchor reacquisition.", tick);
                        return;
                    }

                    if (!StartVendorNavigation(world, *liveVendor, tick))
                        Fail("failed to route from cached vendor anchor to live vendor.", tick);
                    return;
                }

                if (!vendorNavigator_)
                {
                    Fail("cached vendor anchor navigator missing.", tick);
                    return;
                }

                vendorNavigator_->Update(world.player, tick);
                if (vendorNavigator_->Arrived())
                {
                    vendorNavigator_.reset();
                    if (remoteHubRouting_)
                    {
                        remoteHubRouting_ = false;
                        primaryHubReached_ = true;
                        serviceSearchMode_ = false;
                        Debug::Logger::Info(
                            "VENDOR ROUTING 14L.2: Razor Hill service hub reached; waiting for preferred Wuark before any fallback merchant discovery.");
                        SetState(VendorState::SearchingVendor, tick);
                    }
                    else
                    {
                        SetState(VendorState::SearchingVendor, tick);
                    }
                }
                else if (vendorNavigator_->Failed())
                {
                    vendorNavigator_.reset();
                    if (remoteHubRouting_)
                    {
                        remoteHubRouting_ = false;
                        primaryHubReached_ = false;
                        Fail("NavMesh route to Razor Hill Wuark service hub failed.", tick);
                    }
                    else if (
                        Distance2D(
                            world.player.x,
                            world.player.y,
                            grindHome_.x,
                            grindHome_.y) <= HomeArrivalDistance)
                    {
                        SetState(VendorState::SearchingVendor, tick);
                    }
                    else if (!StartHomeNavigation(world, tick, false))
                    {
                        Fail("cached vendor anchor route failed and fallback home navigation could not start.", tick);
                    }
                }
                return;
            }

            if (state_ == VendorState::NavigatingVendor)
            {
                if (!vendorNavigator_)
                {
                    Fail("vendor navigator missing.", tick);
                    return;
                }

                const auto* proactiveVendor = FindVendorByGuid(world);
                if (
                    proactiveVendor != nullptr &&
                    proactiveVendor->distance <= ProactiveVendorHandoffDistance)
                {
                    vendorNavigator_.reset();
                    directMoves_ = 0;
                    directApproachMaxDistance_ = ProactiveVendorHandoffDistance;
                    lastDirectDistance_ = proactiveVendor->distance;
                    directNoProgressMoves_ = 0;
                    localRecoveryAttempts_ = 0;
                    lastDirectMoveTick_ = 0;
                    Debug::Logger::Info(
                        "VENDOR PROACTIVE HANDOFF 14L.1.6: selected live merchant entered 16 yd proactive handoff radius; leaving NavMesh before the final corridor. distance=" +
                        Float(proactiveVendor->distance) +
                        " max=" + Float(ProactiveVendorHandoffDistance));
                    SetState(VendorState::DirectVendorApproach, tick);
                    return;
                }

                vendorNavigator_->Update(world.player, tick);

                if (vendorNavigator_->Failed())
                {
                    const auto* vendor = FindVendorByGuid(world);
                    vendorNavigator_.reset();
                    if (vendor != nullptr && vendor->distance <= NavFailureDirectFallbackRadius)
                    {
                        directMoves_ = 0;
                        directApproachMaxDistance_ = NavFailureDirectFallbackRadius;
                        lastDirectDistance_ = vendor->distance;
                        directNoProgressMoves_ = 0;
                        localRecoveryAttempts_ = 0;
                        lastDirectMoveTick_ = 0;
                        Debug::Logger::Info(
                            "VENDOR FINAL APPROACH 14L.1.3: NavMesh failed with the selected live merchant still within bounded final-approach range; switching to direct CTM. distance=" +
                            Float(vendor->distance) +
                            " max=" + Float(NavFailureDirectFallbackRadius));
                        SetState(VendorState::DirectVendorApproach, tick);
                        return;
                    }

                    if (serviceSearchMode_)
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("NavMesh route to configured vendor failed outside direct fallback radius.", tick);
                    }
                    return;
                }

                if (vendorNavigator_->Arrived())
                {
                    vendorNavigator_.reset();
                    const auto* vendor = FindVendorByGuid(world);
                    if (vendor == nullptr)
                    {
                        SetState(VendorState::SearchingVendor, tick);
                        return;
                    }

                    if (vendor->distance <= InteractionDistance)
                    {
                        if (!IssueInteraction(*vendor, tick))
                        {
                            if (serviceSearchMode_)
                            {
                                if (!StartAlternateServiceSearch(world, tick))
                                    FinishVendorAndReturn(world, tick);
                            }
                            else
                            {
                                Fail("vendor interaction failed after NavMesh arrival.", tick);
                            }
                        }
                        return;
                    }

                    if (vendor->distance <= DirectFallbackRadius)
                    {
                        directMoves_ = 0;
                        directApproachMaxDistance_ = DirectFallbackRadius;
                        lastDirectDistance_ = vendor->distance;
                        directNoProgressMoves_ = 0;
                        localRecoveryAttempts_ = 0;
                        lastDirectMoveTick_ = 0;
                        SetState(VendorState::DirectVendorApproach, tick);
                        return;
                    }

                    if (serviceSearchMode_)
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("vendor still outside interaction/direct range after NavMesh arrival.", tick);
                    }
                }
                return;
            }

            if (state_ == VendorState::DirectVendorApproach)
            {
                const auto* vendor = FindVendorByGuid(world);
                if (vendor == nullptr)
                {
                    if (serviceSearchMode_)
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("live vendor disappeared during direct final approach.", tick);
                    }
                    return;
                }

                if (vendor->distance <= InteractionDistance)
                {
                    if (!IssueInteraction(*vendor, tick))
                    {
                        if (serviceSearchMode_)
                        {
                            if (!StartAlternateServiceSearch(world, tick))
                                FinishVendorAndReturn(world, tick);
                        }
                        else
                        {
                            Fail("vendor interaction failed after direct approach.", tick);
                        }
                    }
                    return;
                }

                if (
                    directMoves_ >= MaximumDirectMoves ||
                    directNoProgressMoves_ >= MaximumDirectNoProgressMoves ||
                    vendor->distance > directApproachMaxDistance_)
                {
                    if (serviceSearchMode_)
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("bounded direct vendor approach exhausted.", tick);
                    }
                    return;
                }

                if (
                    lastDirectMoveTick_ == 0 ||
                    tick >= lastDirectMoveTick_ + DirectMoveCooldownTicks)
                {
                    if (directMoves_ > 0 && lastDirectDistance_ > 0.0f)
                    {
                        if (vendor->distance <= lastDirectDistance_ - DirectProgressEpsilon)
                        {
                            directNoProgressMoves_ = 0;
                        }
                        else
                        {
                            ++directNoProgressMoves_;
                            Debug::Logger::Info(
                                "VENDOR FINAL APPROACH 14L.1.3: no measurable direct-approach progress; distance=" +
                                Float(vendor->distance) +
                                " previous=" + Float(lastDirectDistance_) +
                                " noProgress=" + std::to_string(directNoProgressMoves_) +
                                "/" + std::to_string(MaximumDirectNoProgressMoves));

                            if (
                                vendor->distance <= LocalRecoveryTriggerDistance &&
                                localRecoveryAttempts_ < MaximumLocalRecoveryAttempts &&
                                TryLocalVendorApproachRecovery(world, *vendor, tick))
                            {
                                return;
                            }

                            if (directNoProgressMoves_ >= MaximumDirectNoProgressMoves)
                            {
                                if (serviceSearchMode_)
                                {
                                    if (!StartAlternateServiceSearch(world, tick))
                                        FinishVendorAndReturn(world, tick);
                                }
                                else
                                {
                                    Fail("bounded direct vendor approach made no measurable progress after local flank recovery budget.", tick);
                                }
                                return;
                            }
                        }
                    }

                    if (!ClickToMoveController::MoveTo(
                            world.player,
                            vendor->x,
                            vendor->y,
                            vendor->z,
                            DirectFallbackPrecision))
                    {
                        if (serviceSearchMode_)
                        {
                            if (!StartAlternateServiceSearch(world, tick))
                                FinishVendorAndReturn(world, tick);
                        }
                        else
                        {
                            Fail("CTM command failed during direct vendor approach.", tick);
                        }
                        return;
                    }

                    ++directMoves_;
                    lastDirectDistance_ = vendor->distance;
                    lastDirectMoveTick_ = tick;
                    Debug::Logger::Info(
                        "GRIND 14G.1 VENDOR: direct approach distance=" +
                        Float(vendor->distance) +
                        " move=" + std::to_string(directMoves_) +
                        "/" + std::to_string(MaximumDirectMoves) +
                        " maxDistance=" + Float(directApproachMaxDistance_));
                }
                return;
            }

            if (state_ == VendorState::WaitingForMerchant)
            {
                if (MerchantOpen())
                {
                    const auto* verifiedMerchant = FindVendorByGuid(world);
                    if (verifiedMerchant != nullptr)
                        LearnVerifiedMerchant(*verifiedMerchant);

                    Debug::Logger::Info("GRIND 14G.1 VENDOR: MerchantFrame open.");
                    lastSaleTick_ = 0;
                    SetState(VendorState::Selling, tick);
                    return;
                }

                if (
                    interactionAttempts_ < MaximumInteractionAttempts &&
                    tick >= lastInteractionTick_ + InteractionRetryTicks)
                {
                    const auto* vendor = FindVendorByGuid(world);
                    if (vendor == nullptr)
                    {
                        Debug::Logger::Info(
                            "VENDOR INTERACTION 14L.1.2: MerchantFrame still closed and the selected merchant is absent from the live snapshot; restarting bounded vendor acquisition.");
                        vendorGuid_ = 0;
                        vendorEntry_ = 0;
                        SetState(VendorState::SearchingVendor, tick);
                        return;
                    }

                    if (vendor->distance > InteractionDistance)
                    {
                        if (vendor->distance <= DirectFallbackRadius)
                        {
                            Debug::Logger::Info(
                                "VENDOR INTERACTION 14L.1.2: MerchantFrame still closed and merchant distance=" +
                                Float(vendor->distance) +
                                " is outside the reliable interaction radius; resuming bounded direct approach.");
                            directMoves_ = 0;
                            directApproachMaxDistance_ = DirectFallbackRadius;
                            lastDirectDistance_ = vendor->distance;
                            directNoProgressMoves_ = 0;
                            localRecoveryAttempts_ = 0;
                            lastDirectMoveTick_ = 0;
                            SetState(VendorState::DirectVendorApproach, tick);
                            return;
                        }

                        Debug::Logger::Info(
                            "VENDOR INTERACTION 14L.1.2: MerchantFrame still closed and merchant moved outside direct fallback radius; restarting bounded vendor acquisition.");
                        vendorGuid_ = 0;
                        vendorEntry_ = 0;
                        SetState(VendorState::SearchingVendor, tick);
                        return;
                    }

                    Debug::Logger::Info(
                        "VENDOR INTERACTION 14L.1.2: MerchantFrame still closed; retrying merchant interaction at distance=" +
                        Float(vendor->distance) +
                        " attempt=" + std::to_string(interactionAttempts_ + 1) +
                        "/" + std::to_string(MaximumInteractionAttempts));
                    if (!IssueInteraction(*vendor, tick))
                    {
                        Debug::Logger::Info(
                            "VENDOR INTERACTION 14L.1.2: retry dispatch failed; restarting bounded vendor acquisition.");
                        vendorGuid_ = 0;
                        vendorEntry_ = 0;
                        SetState(VendorState::SearchingVendor, tick);
                    }
                    return;
                }

                if (tick >= stateStartedTick_ + MerchantOpenTimeoutTicks)
                {
                    if (serviceSearchMode_)
                    {
                        Debug::Logger::Info(
                            "MAINTENANCE 14G.5.1: candidate did not open MerchantFrame; rejecting and trying another service NPC.");
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("MerchantFrame did not open after bounded interaction retries.", tick);
                    }
                }
                return;
            }

            if (state_ == VendorState::Selling)
            {
                if (lastSaleTick_ != 0 && tick < lastSaleTick_ + SaleStepTicks)
                    return;

                int bag = -1;
                int slot = -1;
                std::uint32_t itemId = 0;
                const SellStepResult result =
                    SellOneUnprotectedItem(bag, slot, itemId);
                lastSaleTick_ = tick;

                if (result == SellStepResult::Failed)
                {
                    Fail("Lua vendor sell step failed.", tick);
                    return;
                }

                if (result == SellStepResult::MerchantClosed)
                {
                    Fail("MerchantFrame closed during vendor sell pass.", tick);
                    return;
                }

                if (result == SellStepResult::Done)
                {
                    if (candidatePendingVerification_)
                        ++salesObserved_;
                    candidatePendingVerification_ = false;

                    if (requestedMaintenance_.Any())
                    {
                        repairUnavailableHere_ = false;
                        foodUnavailableHere_ = false;
                        drinkUnavailableHere_ = false;
                        lastMaintenanceStepTick_ = 0;
                        SetState(VendorState::Maintaining, tick);
                    }
                    else
                    {
                        FinishVendorAndReturn(world, tick);
                    }
                    return;
                }

                ++saleAttempts_;

                if (
                    candidatePendingVerification_ &&
                    bag == lastCandidateBag_ &&
                    slot == lastCandidateSlot_ &&
                    itemId == lastCandidateItemId_)
                {
                    const std::uint32_t key = SlotKey(bag, slot);
                    blockedBagSlots_.insert(key);
                    ++unsellableSlotsSkipped_;
                    candidatePendingVerification_ = false;

                    Debug::Logger::Info(
                        "GRIND 14G.1 VENDOR: item did not leave bag; marking slot unsellable for this pass bag=" +
                        std::to_string(bag) +
                        " slot=" + std::to_string(slot) +
                        " itemId=" + std::to_string(itemId));
                    return;
                }

                if (candidatePendingVerification_)
                    ++salesObserved_;

                lastCandidateBag_ = bag;
                lastCandidateSlot_ = slot;
                lastCandidateItemId_ = itemId;
                candidatePendingVerification_ = true;

                Debug::Logger::Info(
                    "GRIND 14G.1 VENDOR: sell attempt bag=" +
                    std::to_string(bag) +
                    " slot=" + std::to_string(slot) +
                    " itemId=" + std::to_string(itemId) +
                    " attempts=" + std::to_string(saleAttempts_));
                return;
            }

            if (state_ == VendorState::Maintaining)
            {
                if (!MerchantOpen())
                {
                    if (requestedMaintenance_.Any())
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            FinishVendorAndReturn(world, tick);
                    }
                    else
                    {
                        Fail("MerchantFrame closed during maintenance pass.", tick);
                    }
                    return;
                }

                if (!RunMaintenanceStep(world, tick))
                    FinishVendorAndReturn(world, tick);
                return;
            }

            if (state_ == VendorState::ReturningToGrind)
            {
                if (!returnNavigator_)
                {
                    Fail("return-to-grind navigator missing.", tick);
                    return;
                }

                returnNavigator_->Update(world.player, tick);
                if (returnNavigator_->Arrived())
                {
                    returnNavigator_.reset();
                    SetState(VendorState::Done, tick);
                }
                else if (returnNavigator_->Failed())
                {
                    Fail("NavMesh return from vendor to grind home failed.", tick);
                }
            }
        }

        void Reset()
        {
            homeNavigator_.reset();
            vendorNavigator_.reset();
            returnNavigator_.reset();
            CloseNpcFrames();
            state_ = VendorState::Idle;
            vendorGuid_ = 0;
            vendorEntry_ = 0;
            directApproachMaxDistance_ = DirectFallbackRadius;
            lastDirectDistance_ = 0.0f;
            directNoProgressMoves_ = 0;
            localRecoveryAttempts_ = 0;
            directMoves_ = 0;
            lastDirectMoveTick_ = 0;
            blockedBagSlots_.clear();
            candidatePendingVerification_ = false;
            requestedMaintenance_ = MaintenanceNeed{};
            maintenanceAtStart_ = MaintenanceSnapshot{};
            lastMaintenanceSnapshot_ = MaintenanceSnapshot{};
            bagPressureTrigger_ = false;
            serviceSearchMode_ = false;
            remoteHubRouting_ = false;
            repairSatisfied_ = true;
            foodSatisfied_ = true;
            drinkSatisfied_ = true;
            repairUnavailableHere_ = false;
            foodUnavailableHere_ = false;
            drinkUnavailableHere_ = false;
            rejectedServiceEntries_.clear();
            maintenanceUnmet_ = false;
            lastMaintenanceStepTick_ = 0;
            repairActions_ = 0;
            foodPurchases_ = 0;
            drinkPurchases_ = 0;
            serviceCandidatesTried_ = 0;
        }

        VendorState State() const { return state_; }
        const char* StateName() const { return StateNameInternal(state_); }
        bool IsActive() const
        {
            return state_ != VendorState::Idle &&
                   state_ != VendorState::Done &&
                   state_ != VendorState::Failed;
        }
        bool IsDone() const { return state_ == VendorState::Done; }
        bool Failed() const { return state_ == VendorState::Failed; }
        int SaleAttempts() const { return saleAttempts_; }
        int SalesObserved() const { return salesObserved_; }
        int UnsellableSlotsSkipped() const { return unsellableSlotsSkipped_; }
        std::uint32_t VendorEntry() const { return vendorEntry_; }
        bool MaintenanceUnmet() const { return maintenanceUnmet_; }
        int RepairActions() const { return repairActions_; }
        int FoodPurchases() const { return foodPurchases_; }
        int DrinkPurchases() const { return drinkPurchases_; }
        const MaintenanceSnapshot& LastMaintenanceSnapshot() const
        {
            return lastMaintenanceSnapshot_;
        }

        static bool ProbeMaintenance(MaintenanceSnapshot& snapshot)
        {
            return ProbeMaintenanceSnapshotInternal(snapshot);
        }
    };
}
