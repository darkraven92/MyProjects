#pragma once
#include "EquipmentDurabilityProbe.h"
#include "QuestMaintenancePolicy.h"

#include "AutonomousMaintenancePolicy.h"
#include "AutoSellItemPolicy.h"
#include "ClickToMoveController.h"
#include "ConsumableClassificationPolicy.h"
#include "GameThreadDispatcher.h"
#include "GrindBagMonitor.h"
#include "ServiceHubSelectionPolicy.h"
#include "ServiceHubRegistryPolicy.h"
#include "ServiceHubBackoffPolicy.h"
#include "MaintenanceOutcomePolicy.h"
#include "ServiceHubCatalogue.h"

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
#include <vector>

namespace Bot
{
    enum class VendorState
    {
        Idle,
        PreparingHubSelection,
        SelectingHub,
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
        static constexpr std::uint64_t MetadataRetryTicks = 16; // 4 s
        static constexpr std::uint64_t MetadataResolutionTimeoutTicks = 240; // 60 s
        static constexpr std::uint64_t MaintenanceStepTicks = 2;
        static constexpr std::uint64_t ServiceSearchWaitTicks = 48; // 12 s
        static constexpr int MaximumServiceCandidates = 10;
        static constexpr int MinimumFreeSlotsAfterVendor = 2;

        // Project history identifies Tai'tasi and Zansoa as Sen'jin merchant
        // candidates, but provides no verified position for either one.
        // They become routable only when a live WorldState supplies a position.
        static constexpr std::uint32_t WuarkEntry = 3167;
        static constexpr std::uint32_t TaiTasiEntry = 3187;
        static constexpr std::uint32_t ZansoaEntry = 5942;
        static constexpr Navigation::NavPoint RazorHillServiceHub{
            357.1937f, -4708.1279f, 14.4788f};
        static constexpr float RemoteHubArrivalDistance = 8.0f;
        static constexpr float HubMerchantMatchRadius = 80.0f;
        static constexpr std::size_t MaximumHubShortlist = 4;
        static constexpr std::size_t InvalidHubIndex = static_cast<std::size_t>(-1);

        static constexpr const char* LuaResultVariable =
            "WOW_INTERNAL_VENDOR_RESULT";

        VendorState state_ = VendorState::Idle;

        Navigation::NavPoint grindHome_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> homeNavigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> vendorNavigator_{};
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> returnNavigator_{};

        std::uint64_t vendorGuid_ = 0;
        std::uint32_t vendorEntry_ = 0;

        std::vector<ServiceHubCandidate> knownHubs_{};
        std::unordered_set<std::uint32_t> verifiedHubEntries_{};
        bool vendorMemoryLoaded_ = false;
        std::filesystem::path vendorMemoryPath_{};
        bool remoteHubRouting_ = false;
        std::vector<ServiceHubCandidate> tripCandidates_{};
        Objects::PlayerState selectionPlayer_{};
        ServiceSelectionOrigin selectionOrigin_{};
        std::vector<std::size_t> hubShortlist_{};
        std::size_t hubProbeIndex_ = 0;
        std::size_t selectedHubIndex_ = InvalidHubIndex;
        std::uint32_t failoverFromEntry_ = 0;
        std::unique_ptr<Navigation::GenericNavMeshPathFollower> hubProbeNavigator_{};

        std::uint64_t stateStartedTick_ = 0;
        std::uint64_t lastInteractionTick_ = 0;
        std::uint64_t lastDirectMoveTick_ = 0;
        std::uint64_t lastSaleTick_ = 0;
        std::uint64_t metadataWaitStartedTick_ = 0;
        std::uint64_t nextMetadataRetryTick_ = 0;
        int lastMetadataPendingCount_ = -1;

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
        std::unordered_set<std::string> loggedSaleDecisions_{};

        MaintenanceNeed requestedMaintenance_{};
        MaintenanceSnapshot maintenanceAtStart_{};
        MaintenanceSnapshot lastMaintenanceSnapshot_{};
        bool bagPressureTrigger_ = false;
        bool bagPressureSatisfied_ = true;
        ServiceHubBackoffPolicy localBackoff_{};
        ServiceHubBackoffPolicy* sharedBackoff_ = nullptr;
        ServiceHubBackoffPolicy& CandidateBackoff()
        { return sharedBackoff_ ? *sharedBackoff_ : localBackoff_; }
        void BackOffCandidate(std::uint32_t entry, std::uint64_t tick, const char* reason)
        {
            if (!entry) return;
            rejectedServiceEntries_.insert(entry);
            CandidateBackoff().Reject(entry, tick, QuestMaintenancePolicy::RetryTicks);
            Debug::Logger::Info("VENDOR CANDIDATE BACKOFF entry=" + std::to_string(entry) +
                " reason=" + reason + " untilTick=" +
                std::to_string(tick + QuestMaintenancePolicy::RetryTicks));
        }
        bool conservativeQuestSales_ = false;
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
                case VendorState::PreparingHubSelection: return "PreparingHubSelection";
                case VendorState::SelectingHub: return "SelectingHub";
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

        static const char* ServiceName(ServiceKnowledge value)
        {
            switch (value)
            {
                case ServiceKnowledge::Available: return "yes";
                case ServiceKnowledge::Unavailable: return "no";
                default: return "unknown";
            }
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

            // The seed is intentionally unverified: MerchantFrame must prove
            // its services, exactly like any other candidate.
            ServiceHubCandidate wuark{};
            wuark.entry = WuarkEntry;
            wuark.source = ServiceHubSource::Seeded;
            wuark.x = RazorHillServiceHub.x;
            wuark.y = RazorHillServiceHub.y;
            wuark.z = RazorHillServiceHub.z;
            wuark.positionKnown = true;
            knownHubs_.push_back(wuark);

            for (const std::uint32_t entry : {TaiTasiEntry, ZansoaEntry})
            {
                ServiceHubCandidate localMerchant{};
                localMerchant.entry = entry;
                localMerchant.source = ServiceHubSource::Seeded;
                knownHubs_.push_back(localMerchant);
            }

            if (!EnsureVendorMemoryPath())
                return;

            std::ifstream input(vendorMemoryPath_);
            if (!input)
                return;

            ServiceHubRegistryPolicy::Load(input, knownHubs_, verifiedHubEntries_);
        }

        void SaveVendorMemory()
        {
            if (!EnsureVendorMemoryPath())
            {
                return;
            }

            const auto tempPath = vendorMemoryPath_.string() + ".tmp";
            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                    return;
                ServiceHubRegistryPolicy::Write(
                    output, knownHubs_, verifiedHubEntries_);
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

            // The manual-vendor inventory diagnostic is bounded to one bag
            // snapshot and needs more than the ordinary scalar readbacks.
            char buffer[4096]{};
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
                + std::string(EquipmentDurabilityProbe::Lua()) +
                "local fc=0; local dc=0; local unresolved=0; "
                "local metadataPending=0; local tooltipPending=0; local inventoryPending=0; "
                "if not WOW_INTERNAL_MAINT_TOOLTIP then "
                "WOW_INTERNAL_MAINT_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_MAINT_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_MAINT_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_MAINT_TOOLTIP; " +
                std::string(ConsumableClassificationPolicy::LuaDefinition()) +
                "for b=0,4 do local n=GetContainerNumSlots(b); "
                "if b==0 and (not n or n==0) then inventoryPending=inventoryPending+1; unresolved=unresolved+1 end; "
                "n=n or 0; for sl=1,n do "
                "local texture,cnt=GetContainerItemInfo(b,sl); "
                "local link=GetContainerItemLink(b,sl); "
                "if link then "
                "local pending=0; "
                "tt:ClearLines(); tt:SetBagItem(b,sl); "
                "local _,_,_,_,_,itemType=GetItemInfo(link); "
                "if not itemType then "
                "local _,_,idText=string.find(link,'item:(%d+)'); "
                "local id=tonumber(idText or '0') or 0; "
                "if id>0 then local _,_,_,_,_,byIdType=GetItemInfo(id); "
                "itemType=byIdType; end; end; "
                "local f,d,h,e,m,dr=classifyConsumable(tt); "
                "if cnt and cnt>0 then "
                "if f then fc=fc+cnt end; if d then dc=dc+cnt end; "
                "else inventoryPending=inventoryPending+1; pending=1 end; "
                "if not f and not d then "
                "if not itemType then metadataPending=metadataPending+1; pending=1 "
                "elseif itemType=='Consumable' and (tt:NumLines()<2 or h or e or m or dr) then "
                "tooltipPending=tooltipPending+1; pending=1 end; end; "
                "if pending==1 then unresolved=unresolved+1 end; "
                "elseif texture then inventoryPending=inventoryPending+1; unresolved=unresolved+1 end; "
                "end; end; "
                "WOW_INTERNAL_VENDOR_RESULT=p..'|'..money..'|'..mind..'|'..dn..'|'..fc..'|'..dc..'|'..unresolved..'|'..metadataPending..'|'..tooltipPending..'|'..inventoryPending..'|'..durabilityKnown;";

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
            int unresolvedItems = 0;
            int metadataPendingItems = 0;
            int tooltipPendingItems = 0;
            int inventoryPendingItems = 0;
            int durabilityKnown = 0;

            if (std::sscanf(
                    result.c_str(),
                    "%d|%u|%f|%d|%d|%d|%d|%d|%d|%d|%d",
                    &powerType,
                    &money,
                    &minimumDurability,
                    &durableItems,
                    &foodCount,
                    &drinkCount,
                    &unresolvedItems,
                    &metadataPendingItems,
                    &tooltipPendingItems,
                    &inventoryPendingItems,
                    &durabilityKnown) != 11)
            {
                return false;
            }

            snapshot.valid = true;
            snapshot.durabilityKnown = durabilityKnown == 1;
            snapshot.powerType = powerType;
            snapshot.money = static_cast<std::uint32_t>(money);
            snapshot.minimumDurabilityPercent =
                std::clamp(minimumDurability, 0.0f, 100.0f);
            snapshot.durableItems = std::max(0, durableItems);
            snapshot.foodCount = std::max(0, foodCount);
            snapshot.drinkCount = std::max(0, drinkCount);
            snapshot.unresolvedItems = std::max(0, unresolvedItems);
            snapshot.metadataPendingItems = std::max(0, metadataPendingItems);
            snapshot.tooltipPendingItems = std::max(0, tooltipPendingItems);
            snapshot.inventoryPendingItems = std::max(0, inventoryPendingItems);
            snapshot.foodCountKnown = unresolvedItems == 0 && inventoryPendingItems == 0;
            snapshot.drinkCountKnown = snapshot.foodCountKnown;
            return true;
        }

        static bool ProbeFoodInventoryDiagnosticInternal(std::string& result)
        {
            // Manual wait only, at the existing maintenance cadence. The
            // compact record is compared by the owner before logging, so
            // unchanged bags do not emit repeated lines.
            const std::string script =
                "if not WOW_INTERNAL_FOOD_DIAG_TOOLTIP then "
                "WOW_INTERNAL_FOOD_DIAG_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_FOOD_DIAG_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_FOOD_DIAG_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_FOOD_DIAG_TOOLTIP; " +
                std::string(ConsumableClassificationPolicy::LuaDefinition()) +
                "local out=''; local truncated=0; "
                "for b=0,4 do local n=GetContainerNumSlots(b) or 0; "
                "for s=1,n do local link=GetContainerItemLink(b,s); "
                "if link and truncated==0 then "
                "local _,cnt=GetContainerItemInfo(b,s); cnt=cnt or 1; "
                "local _,_,idText=string.find(link,'item:(%d+)'); "
                "local id=tonumber(idText or '0') or 0; "
                "local _,_,q,_,_,itype,subtype=GetItemInfo(link); "
                "tt:ClearLines(); tt:SetBagItem(b,s); "
                "local f,d,h,e,m,dr=classifyConsumable(tt); "
                "local reason='not_food'; "
                "if f then reason='food' elseif h or e then reason='incomplete_food_tooltip' "
                "elseif not itype then reason='metadata_missing' end; "
                "itype=string.gsub(itype or 'unknown','[|;:]','_'); "
                "subtype=string.gsub(subtype or 'unknown','[|;:]','_'); "
                "local candidate=f or itype=='Consumable'; "
                "local row='slot='..b..','..s..' itemId='..id..' count='..cnt.. "
                "' quality='..(q or -1)..' type='..itype..' subtype='..subtype.. "
                "' foodCandidate='..(candidate and 'yes' or 'no').. "
                "' healthOver='..(h and 'yes' or 'no').. "
                "' eating='..(e and 'yes' or 'no').. "
                "' drink='..(d and 'yes' or 'no')..' reason='..reason..';'; "
                "if string.len(out)+string.len(row)>3700 then truncated=1 else out=out..row end; "
                "end; end; end; "
                "if truncated==1 then out=out..'truncated;' end; "
                "WOW_INTERNAL_VENDOR_RESULT=out~='' and out or 'empty';";
            return ExecuteLuaReadback(
                script, "wow-internal/ManualFoodInventoryDiagnostic.lua", result);
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
                "local tt=WOW_INTERNAL_MAINT_MERCHANT_TOOLTIP; " +
                std::string(ConsumableClassificationPolicy::LuaDefinition()) +
                "local n=GetMerchantNumItems and GetMerchantNumItems() or 0; "
                "for i=1,n do "
                "local name,tex,pr,qty,avail,usable=GetMerchantItemInfo(i); pr=pr or 0; qty=qty or 1; "
                "local ok=0; if usable then ok=1 end; "
                "local link=nil; if GetMerchantItemLink then link=GetMerchantItemLink(i) end; "
                "if link and GetItemInfo then local _,_,_,_,req=GetItemInfo(link); req=req or 0; if req<=lvl then ok=1 else ok=0 end; end; "
                "if ok==1 and pr>0 then tt:ClearLines(); tt:SetMerchantItem(i); "
                "local food,drink,_,_,_,_,_,text=classifyConsumable(tt); "
                "local match=0; local score=0; " +
                std::string(kind == MerchantConsumableKind::Food
                    ? "if food then match=1; local _,_,v=string.find(text,'(%d+)%s+health%s+over'); score=tonumber(v or '0') or 0 end; "
                    : "if drink then match=1; local _,_,v=string.find(text,'(%d+)%s+mana%s+over'); score=tonumber(v or '0') or 0 end; ") +
                "if match==1 then if score>bestscore or (score==bestscore and (bestprice==0 or pr<bestprice)) then best=i; bestscore=score; bestprice=pr end; end; end; end; "
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

        const Objects::UnitState* FindVendor(
            const Objects::WorldState& world) const
        {
            if (selectedHubIndex_ < tripCandidates_.size())
            {
                const auto& selected = tripCandidates_[selectedHubIndex_];
                const Objects::UnitState* best = nullptr;
                for (const auto& unit : world.units)
                {
                    if (!unit.valid || unit.guid == 0 ||
                        unit.entryId != selected.entry ||
                        Distance2D(unit.x, unit.y, selected.x, selected.y) >
                            HubMerchantMatchRadius)
                        continue;
                    if (selected.guid != 0 && unit.guid == selected.guid)
                        return &unit;
                    if (best == nullptr || unit.distance < best->distance)
                        best = &unit;
                }
                return best;
            }
            return nullptr;
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
            if (!MerchantOpen() || !vendor.valid || vendor.guid == 0 || vendor.entryId == 0)
                return;
            auto existing = std::find_if(knownHubs_.begin(), knownHubs_.end(),
                [&](const auto& item) { return item.entry == vendor.entryId; });
            const bool changed = existing == knownHubs_.end() ||
                Distance2D(existing->x, existing->y, vendor.x, vendor.y) > 1.0f ||
                existing->sell != ServiceKnowledge::Available;
            if (existing == knownHubs_.end())
            {
                knownHubs_.push_back(ServiceHubCandidate{});
                existing = knownHubs_.end() - 1;
            }
            existing->entry = vendor.entryId;
            existing->source = ServiceHubSource::MerchantFrameVerified;
            existing->x = vendor.x;
            existing->y = vendor.y;
            existing->z = vendor.z;
            existing->positionKnown = true;
            existing->sell = ServiceKnowledge::Available;
            verifiedHubEntries_.insert(vendor.entryId);
            if (changed)
            {
                Debug::Logger::Info(
                    "VENDOR 14V.1: MerchantFrame verified entry=" +
                    std::to_string(vendor.entryId) + " sell=yes");
                SaveVendorMemory();
            }
        }

        void LearnRepairCapability(bool available)
        {
            if (vendorEntry_ == 0 ||
                verifiedHubEntries_.find(vendorEntry_) ==
                    verifiedHubEntries_.end())
                return;
            auto known = std::find_if(knownHubs_.begin(), knownHubs_.end(),
                [&](const auto& hub) { return hub.entry == vendorEntry_; });
            if (known == knownHubs_.end())
                return;
            const auto observed = available
                ? ServiceKnowledge::Available : ServiceKnowledge::Unavailable;
            if (known->repair != observed)
            {
                known->repair = observed;
                SaveVendorMemory();
                Debug::Logger::Info(
                    "VENDOR 14V.1: learned repair entry=" +
                    std::to_string(vendorEntry_) +
                    " available=" + (available ? std::string("yes") : "no"));
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
            maintenanceUnmet_ = MaintenanceOutcomePolicy::Unmet(maintenanceUnmet_,
                bagPressureTrigger_, bagPressureSatisfied_, requestedMaintenance_.repair, repairSatisfied_,
                requestedMaintenance_.food, foodSatisfied_, requestedMaintenance_.drink, drinkSatisfied_);
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

        bool PrepareHubSelection(const Objects::WorldState& world,
                                 std::uint64_t tick)
        {
            // This is an ownership handoff, before candidate evaluation.
            // A destroyed roam/vendor follower may leave its prior CTM active.
            if (!ClickToMoveController::MoveTo(
                    world.player, world.player.x, world.player.y,
                    world.player.z, 0.25f))
            {
                Fail("could not hold position before service-hub selection", tick);
                return false;
            }
            Debug::Logger::Info(
                "VENDOR 14V.1 OWNER HOLD before_selection; route probes remain movement-free.");
            SetState(VendorState::PreparingHubSelection, tick);
            return true;
        }

        bool BeginHubSelection(const Objects::WorldState& world,
                               std::uint64_t tick)
        {
            vendorNavigator_.reset();
            hubProbeNavigator_.reset();
            tripCandidates_.clear();
            hubShortlist_.clear();
            selectedHubIndex_ = InvalidHubIndex;
            hubProbeIndex_ = 0;
            vendorGuid_ = 0;
            vendorEntry_ = 0;
            remoteHubRouting_ = false;
            serviceSearchMode_ = true;
            selectionPlayer_ = world.player;
            selectionOrigin_ = ServiceSelectionOrigin{
                world.player.x, world.player.y, world.player.z};

            CandidateBackoff().Prune(tick);

            for (const auto& remembered : knownHubs_)
            {
                if (rejectedServiceEntries_.find(remembered.entry) !=
                    rejectedServiceEntries_.end() || CandidateBackoff().Blocked(remembered.entry, tick))
                    continue;
                ServiceHubCandidate candidate = remembered;
                if (candidate.positionKnown)
                    candidate.euclideanDistance =
                        ServiceHubSelectionPolicy::DistanceFrom(
                            selectionOrigin_, candidate);
                tripCandidates_.push_back(candidate);
            }

            // Source NPC flags are audit authority, NOT proof of live services.
            unsigned playerFaction = 0;
            if (world.player.descriptors)
                Core::Memory::Read(world.player.descriptors + 0x8Cu, playerFaction);
            const auto sourceHubs = ServiceHubCatalogue::Instance().Candidates(
                MapId, playerFaction, selectionOrigin_, requestedMaintenance_.repair && !repairSatisfied_);
            for (const auto& source : sourceHubs)
            {
                if (rejectedServiceEntries_.contains(source.entry) ||
                    CandidateBackoff().Blocked(source.entry, tick)) continue;
                auto existing = std::find_if(tripCandidates_.begin(), tripCandidates_.end(),
                    [&](const auto& hub) { return hub.entry == source.entry; });
                if (existing == tripCandidates_.end()) tripCandidates_.push_back(source);
                else if (!existing->positionKnown) *existing = source;
            }

            // Only registry-backed entries are eligible. A nonzero raw
            // npcFlags value does not establish merchant capability in 5875.
            for (const auto& unit : world.units)
            {
                if (!unit.valid || unit.guid == 0 || unit.health == 0)
                    continue;
                auto existing = std::find_if(tripCandidates_.begin(),
                    tripCandidates_.end(), [&](const auto& candidate) {
                        return candidate.entry == unit.entryId;
                    });
                if (existing == tripCandidates_.end())
                    continue;
                existing->x = unit.x;
                existing->y = unit.y;
                existing->z = unit.z;
                existing->positionKnown = true;
                existing->guid = unit.guid;
                existing->live = true;
                existing->euclideanDistance =
                    ServiceHubSelectionPolicy::DistanceFrom(
                        selectionOrigin_, *existing);
            }

            hubShortlist_ = ServiceHubSelectionPolicy::Shortlist(
                tripCandidates_, true, requestedMaintenance_.repair &&
                    !repairSatisfied_, std::min<std::size_t>(MaximumHubShortlist,
                        MaximumServiceCandidates - std::min(serviceCandidatesTried_, MaximumServiceCandidates)));
            const auto seededCount = std::count_if(knownHubs_.begin(), knownHubs_.end(),
                [](const auto& hub) { return hub.source == ServiceHubSource::Seeded; });
            const auto persistedCount = std::count_if(knownHubs_.begin(), knownHubs_.end(),
                [](const auto& hub) { return hub.source == ServiceHubSource::Persisted; });
            Debug::Logger::Info(
                "VENDOR 14V.2 REGISTRY registryCount=" +
                std::to_string(knownHubs_.size()) +
                " seededCount=" + std::to_string(seededCount) +
                " persistedCount=" + std::to_string(persistedCount) +
                " verifiedCount=" + std::to_string(verifiedHubEntries_.size()) +
                " playerFaction=" + std::to_string(playerFaction) +
                " sourceCatalogueActors=" + std::to_string(ServiceHubCatalogue::Instance().ActorCount()) +
                " sourceCatalogueSpawns=" + std::to_string(ServiceHubCatalogue::Instance().SpawnCount()) +
                " sourceCandidateCount=" + std::to_string(sourceHubs.size()) +
                " registeredPositionedCount=" + std::to_string(std::count_if(
                    knownHubs_.begin(), knownHubs_.end(), [](const auto& hub) { return hub.positionKnown; })) +
                " positionedCount=" + std::to_string(std::count_if(
                    tripCandidates_.begin(), tripCandidates_.end(),
                    [](const auto& hub) { return hub.positionKnown; })));
            Debug::Logger::Info(
                "VENDOR 14V.1 SELECTION BEGIN need=" +
                std::string(requestedMaintenance_.repair && !repairSatisfied_
                    ? "sell+repair" : "sell") +
                " candidateCount=" + std::to_string(hubShortlist_.size()) +
                " selectionOrigin=(" + Float(selectionOrigin_.x) + "," +
                Float(selectionOrigin_.y) + "," +
                Float(selectionOrigin_.z) + ")");
            if (hubShortlist_.empty())
            {
                Fail("no suitable service hub candidates", tick);
                return false;
            }
            SetState(VendorState::SelectingHub, tick);
            return true;
        }

        bool StartSelectedHubNavigation(const Objects::WorldState& world,
                                        std::uint64_t tick)
        {
            if (selectedHubIndex_ >= tripCandidates_.size())
                return false;
            const auto& candidate = tripCandidates_[selectedHubIndex_];
            vendorEntry_ = candidate.entry;
            vendorGuid_ = candidate.live ? candidate.guid : 0;
            if (candidate.live &&
                candidate.euclideanDistance <= ProactiveVendorHandoffDistance)
            {
                for (const auto& unit : world.units)
                    if (unit.valid && unit.guid == candidate.guid)
                        return StartVendorNavigation(world, unit, tick);
            }

            // The shortlist probe disables full-map fallback to bound its
            // cost. Start the selected route with ordinary follower options
            // so vendor navigation retains its existing fallback semantics.
            auto nav = std::make_unique<Navigation::GenericNavMeshPathFollower>();
            Debug::Logger::Info("VENDOR ROUTE REVALIDATE entry=" + std::to_string(candidate.entry) +
                " probeOrigin=(" + Float(selectionOrigin_.x) + "," + Float(selectionOrigin_.y) +
                "," + Float(selectionOrigin_.z) + ") executionOrigin=(" + Float(world.player.x) +
                "," + Float(world.player.y) + "," + Float(world.player.z) + ") destination=(" +
                Float(candidate.x) + "," + Float(candidate.y) + "," + Float(candidate.z) +
                ") reason=live_origin_and_current_hazards proof=initial_route_only");
            if (!nav->Start(world.player, tick,
                    Navigation::NavPoint{candidate.x, candidate.y, candidate.z},
                    MapId, candidate.live ? VendorNavArrivalDistance
                                          : RemoteHubArrivalDistance,
                    std::string("service hub entry=") +
                        std::to_string(candidate.entry)))
                return false;
            vendorNavigator_ = std::move(nav);
            remoteHubRouting_ = !candidate.live;
            SetState(candidate.live ? VendorState::NavigatingVendor
                                    : VendorState::NavigatingVendorAnchor, tick);
            return true;
        }

        void UpdateHubSelection(const Objects::WorldState& world,
                                std::uint64_t tick)
        {
            if (hubProbeIndex_ >= hubShortlist_.size())
            {
                selectedHubIndex_ = ServiceHubSelectionPolicy::BestReachable(
                    tripCandidates_, hubShortlist_);
                if (selectedHubIndex_ == tripCandidates_.size())
                {
                    if (!ServiceHubSelectionPolicy::MayTryAnotherCandidate(
                            static_cast<std::size_t>(serviceCandidatesTried_),
                            MaximumServiceCandidates) ||
                        !BeginHubSelection(world, tick))
                        Fail("no suitable reachable service hub within bounded candidate budget", tick);
                    return;
                }
                const auto& chosen = tripCandidates_[selectedHubIndex_];
                Debug::Logger::Info(
                    "VENDOR 14V.1 SELECTED name=" +
                    std::string(VendorName(chosen.entry)) +
                    " entry=" + std::to_string(chosen.entry) +
                    " navCost=" + Float(chosen.navigationCost) +
                    " reason=lowest_reachable_nav_cost");
                if (failoverFromEntry_ != 0)
                {
                    Debug::Logger::Info(
                        "VENDOR 14V.1 FAILOVER from=" +
                        std::to_string(failoverFromEntry_) +
                        " to=" + std::to_string(chosen.entry) +
                        " reason=next_suitable_reachable");
                    failoverFromEntry_ = 0;
                }
                if (!StartSelectedHubNavigation(world, tick) &&
                    !StartAlternateServiceSearch(world, tick))
                    Fail("selected service hub navigation could not start", tick);
                return;
            }

            auto& candidate = tripCandidates_[hubShortlist_[hubProbeIndex_]];
            if (!hubProbeNavigator_ && !candidate.evaluated)
            {
                Debug::Logger::Info(
                    "VENDOR 14V.1a PROBE BEGIN entry=" +
                    std::to_string(candidate.entry) +
                    " selectionOrigin=(" + Float(selectionOrigin_.x) + "," +
                    Float(selectionOrigin_.y) + "," +
                    Float(selectionOrigin_.z) + ") mode=route_or_expanded");
                hubProbeNavigator_ = std::make_unique<
                    Navigation::GenericNavMeshPathFollower>();
                const Navigation::GenericNavMeshStartOptions options{false, true};
                if (!hubProbeNavigator_->Start(selectionPlayer_, tick,
                        Navigation::NavPoint{candidate.x, candidate.y, candidate.z},
                        MapId, candidate.live ? VendorNavArrivalDistance
                                              : RemoteHubArrivalDistance,
                        std::string("service hub probe entry=") +
                            std::to_string(candidate.entry), true, options))
                {
                    candidate.evaluated = true;
                    candidate.probeFailureReason =
                        Navigation::NavigationInitTelemetryPolicy::ReasonName(
                            hubProbeNavigator_->PlanningOnlyResult().failure);
                    hubProbeNavigator_.reset();
                }
            }
            if (hubProbeNavigator_ && !candidate.evaluated)
            {
                hubProbeNavigator_->Update(selectionPlayer_, tick);
                const auto probe = hubProbeNavigator_->PlanningOnlyResult();
                if (probe.status == Navigation::RouteCostProbeStatus::Reachable)
                {
                    candidate.evaluated = true;
                    candidate.reachable = true;
                    candidate.plannedPathLength = probe.pathLength;
                    // A staged path's active prefix can be shorter than the
                    // final route. The direct distance is a conservative
                    // lower bound for comparing first-leg route estimates.
                    candidate.navigationCost = std::max(
                        candidate.euclideanDistance,
                        probe.pathLength);
                }
                else if (probe.status == Navigation::RouteCostProbeStatus::Unreachable)
                {
                    candidate.evaluated = true;
                    candidate.probeFailureReason =
                        Navigation::NavigationInitTelemetryPolicy::ReasonName(
                            probe.failure);
                }
            }
            if (!candidate.evaluated)
                return;

            if (!candidate.reachable && candidate.live &&
                candidate.euclideanDistance <= ProactiveVendorHandoffDistance)
            {
                // Preserve the existing bounded direct approach for nearby
                // live merchants, but never call it a navmesh-verified route.
                candidate.reachable = true;
                candidate.directFallbackOnly = true;
                candidate.navigationCost = candidate.euclideanDistance;
            }
            if (!candidate.reachable)
            {
                BackOffCandidate(candidate.entry, tick, "route_probe_failed");
                ++serviceCandidatesTried_;
            }

            Debug::Logger::Info(
                "VENDOR 14V.1a PROBE END entry=" +
                std::to_string(candidate.entry) +
                " result=" + (candidate.directFallbackOnly
                    ? "bounded_direct_fallback" :
                    (candidate.reachable ? "planned" : "unreachable")) +
                " movementCommands=" + std::to_string(
                    hubProbeNavigator_ ? hubProbeNavigator_->Commands() : 0) +
                " plannedPathLength=" + (candidate.reachable &&
                    !candidate.directFallbackOnly
                    ? Float(candidate.plannedPathLength) : std::string("unknown")) +
                " navCost=" + (candidate.reachable
                    ? Float(candidate.navigationCost) : std::string("unknown")) +
                " navCostBasis=" + (candidate.directFallbackOnly
                    ? "bounded_direct_fallback" :
                    (!candidate.reachable ? "unknown" :
                    (candidate.plannedPathLength >= candidate.euclideanDistance
                        ? "validated_path_length" : "direct_lower_bound_for_staged_prefix"))) +
                " reason=" + candidate.probeFailureReason);

            Debug::Logger::Info(
                "VENDOR 14V.1 CANDIDATE name=" +
                std::string(VendorName(candidate.entry)) +
                " entry=" + std::to_string(candidate.entry) +
                " source=" + ServiceHubSelectionPolicy::SourceName(candidate.source) +
                " euclideanDistance=" + Float(candidate.euclideanDistance) +
                " selectionOrigin=(" + Float(selectionOrigin_.x) + "," +
                Float(selectionOrigin_.y) + "," +
                Float(selectionOrigin_.z) + ")" +
                " knownServices=sell:" + ServiceName(candidate.sell) +
                ",repair:" + ServiceName(candidate.repair) +
                " plannedPathLength=" + (candidate.reachable &&
                    !candidate.directFallbackOnly
                    ? Float(candidate.plannedPathLength) : std::string("unknown")) +
                " navCost=" + (candidate.reachable
                    ? Float(candidate.navigationCost) : std::string("unknown")) +
                " navCostBasis=" + (candidate.directFallbackOnly
                    ? "bounded_direct_fallback" :
                    (!candidate.reachable ? "unknown" :
                    (candidate.plannedPathLength >= candidate.euclideanDistance
                        ? "validated_path_length" : "direct_lower_bound_for_staged_prefix"))) +
                " reachable=" + (candidate.reachable ? "yes" : "no") +
                " reachability=" + (candidate.directFallbackOnly
                    ? "bounded_direct_fallback" :
                    (candidate.reachable ? "navmesh" : "unproven")) +
                " probeFailure=" + candidate.probeFailureReason +
                " decision=" + (candidate.reachable ? "ranked" : "rejected"));
            hubProbeNavigator_.reset();
            ++hubProbeIndex_;
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
            MetadataPending,
            Done
        };

        SellStepResult SellOneUnprotectedItem(
            int& bag,
            int& slot,
            std::uint32_t& itemId,
            int& metadataPending)
        {
            bag = -1;
            slot = -1;
            itemId = 0;
            metadataPending = 0;

            const std::string blocked = BlockedSlotLuaExpression();

            const std::string script =
                "WOW_INTERNAL_VENDOR_RESULT='none'; "
                "if not (MerchantFrame and MerchantFrame:IsShown()) then "
                "WOW_INTERNAL_VENDOR_RESULT='merchant_closed'; return; end; "
                "if not WOW_INTERNAL_VENDOR_TOOLTIP then "
                "WOW_INTERNAL_VENDOR_TOOLTIP=CreateFrame('GameTooltip','WOW_INTERNAL_VENDOR_TOOLTIP',UIParent,'GameTooltipTemplate'); "
                "WOW_INTERNAL_VENDOR_TOOLTIP:SetOwner(UIParent,'ANCHOR_NONE'); end; "
                "local tt=WOW_INTERNAL_VENDOR_TOOLTIP; local found=0; local trace=''; local metadataPending=0; " +
                std::string(ConsumableClassificationPolicy::LuaDefinition()) +
                AutoSellItemPolicy::LuaDefinition() +
                (conservativeQuestSales_ ? std::string(QuestMaintenancePolicy::RestrictSaleLua()) : std::string{}) +
                "for b=0,4 do if found==0 then local n=GetContainerNumSlots(b) or 0; "
                "for s=1,n do if found==0 and not (" + blocked + ") then "
                "local link=GetContainerItemLink(b,s); if link then "
                "local _,count,locked=GetContainerItemInfo(b,s); "
                "local reason,id,quality,itype=saleReason(b,s,link,count,locked); "
                "if reason=='classification_unknown' and "
                "(quality<0 or itype=='unknown') then "
                "metadataPending=metadataPending+1; end; "
                "if reason=='sell' then "
                "local currentLink=GetContainerItemLink(b,s); "
                "local _,currentCount,currentLocked=GetContainerItemInfo(b,s); "
                "if currentLink==link and currentCount==count and "
                "not currentLocked and not locked then "
                "local currentReason,currentId=saleReason(b,s,currentLink,currentCount,currentLocked); "
                "if currentReason=='sell' and currentId==id then "
                "WOW_INTERNAL_VENDOR_RESULT='try|'..b..'|'..s..'|'..id; "
                "UseContainerItem(b,s); found=1; "
                "else reason='revalidation_failed' end; "
                "else reason='slot_changed' end; end; "
                "local decision=reason=='sell' and 'sell' or 'keep'; "
                "local row='bag='..b..' slot='..s..' itemId='..id.. "
                "' quality='..quality..' count='..(count or 0).. "
                "' classification='..itype..' decision='..decision..' reason='..reason..';'; "
                "if string.len(trace)+string.len(row)<3000 then trace=trace..row end; "
                "end; end; end; end; end; "
                "if found==0 then "
                "if metadataPending>0 then "
                "WOW_INTERNAL_VENDOR_RESULT='metadata_pending|'..metadataPending "
                "else WOW_INTERNAL_VENDOR_RESULT='done' end; end; "
                "WOW_INTERNAL_VENDOR_RESULT=WOW_INTERNAL_VENDOR_RESULT..'#'..trace;";

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

            const auto separator = result.find('#');
            if (separator != std::string::npos)
            {
                const std::string trace = result.substr(separator + 1);
                std::size_t begin = 0;
                while (begin < trace.size())
                {
                    const auto end = trace.find(';', begin);
                    if (end == std::string::npos)
                        break;
                    const std::string decision = trace.substr(begin, end - begin);
                    if (!decision.empty() &&
                        loggedSaleDecisions_.insert(decision).second)
                    {
                        Debug::Logger::Info("VENDOR SELL DECISION " + decision);
                    }
                    begin = end + 1;
                }
            }

            if (result.rfind("done#", 0) == 0)
                return SellStepResult::Done;

            if (result.rfind("metadata_pending|", 0) == 0)
            {
                if (std::sscanf(result.c_str(), "metadata_pending|%d",
                        &metadataPending) != 1 || metadataPending <= 0)
                    return SellStepResult::Failed;
                return SellStepResult::MetadataPending;
            }

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
            std::uint64_t tick,
            const char* reason = "candidate_failed")
        {
            const std::uint32_t previousEntry = vendorEntry_;
            failoverFromEntry_ = previousEntry;
            BackOffCandidate(vendorEntry_, tick, reason);

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
            ++serviceCandidatesTried_;

            if (!ServiceHubSelectionPolicy::MayTryAnotherCandidate(
                    static_cast<std::size_t>(serviceCandidatesTried_),
                    MaximumServiceCandidates))
            {
                maintenanceUnmet_ = true;
                Debug::Logger::Info(
                    "VENDOR 14V.1: service candidate failover budget exhausted.");
                Fail("service candidate failover budget exhausted", tick);
                return false;
            }
            Debug::Logger::Info(
                "VENDOR 14V.1 FAILOVER from=" +
                std::to_string(previousEntry) +
                " reason=" + reason + " attempt=" +
                std::to_string(serviceCandidatesTried_) + "/" +
                std::to_string(MaximumServiceCandidates) +
                " remainingCandidates=" + std::to_string(std::count_if(
                    tripCandidates_.begin(), tripCandidates_.end(),
                    [&](const auto& hub) {
                        return rejectedServiceEntries_.find(hub.entry) ==
                            rejectedServiceEntries_.end() &&
                            ServiceHubSelectionPolicy::Suitable(
                                hub, true, requestedMaintenance_.repair &&
                                    !repairSatisfied_);
                    })));
            return PrepareHubSelection(world, tick);
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
                if (!snapshot.durabilityKnown) return true; // bounded by merchant maintenance deadline
                if (snapshot.durableItems == 0 ||
                    snapshot.minimumDurabilityPercent >
                        AutonomousMaintenancePolicy::RepairTripThresholdPercent)
                {
                    repairSatisfied_ = true;
                    if (repairActions_ > 0 && snapshot.durableItems > 0)
                        Debug::Logger::Info(
                            "QUEST REPAIR VERIFY result=confirmed reason=repair_threshold_resolved source=live_durability_probe minDurability=" +
                            Float(snapshot.minimumDurabilityPercent));
                }
                else if (!repairUnavailableHere_)
                {
                    // Allow one retry, then observe only until the existing
                    // merchant wait expires. Issuing a command is not proof.
                    if (repairActions_ >= 2) return true;
                    std::uint32_t cost = 0;
                    bool available = false;
                    bool noMoney = false;
                    if (!IssueRepairAll(cost, available, noMoney))
                        return true;

                    LearnRepairCapability(available);

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
                if (!snapshot.foodCountKnown &&
                    snapshot.foodCount < AutonomousMaintenancePolicy::FoodTarget)
                    return true; // Unknown lower bound cannot prove a shortage is resolved.
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
                if (!snapshot.drinkCountKnown &&
                    snapshot.drinkCount < AutonomousMaintenancePolicy::DrinkTarget)
                    return true;
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
            if (state_ == VendorState::Failed)
                return false;
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
            bagPressureSatisfied_ = true; // authoritative post-pass free-slot read

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
        void SetCandidateBackoff(ServiceHubBackoffPolicy* backoff) { sharedBackoff_ = backoff; }
        void ObserveWorld(const Objects::WorldState& world)
        {
            for (const auto& vendor : world.units)
            {
                if (!vendor.valid || vendor.guid == 0)
                    continue;
                auto known = std::find_if(knownHubs_.begin(), knownHubs_.end(),
                    [&](const auto& hub) { return hub.entry == vendor.entryId; });
                if (known != knownHubs_.end())
                {
                    known->x = vendor.x;
                    known->y = vendor.y;
                    known->z = vendor.z;
                }
            }
        }

        bool Start(
            const Objects::WorldState& world,
            const Navigation::NavPoint& grindHome,
            std::uint64_t tick,
            MaintenanceNeed maintenanceNeed = {},
            bool bagPressureTrigger = false,
            bool conservativeQuestSales = false)
        {
            if (state_ != VendorState::Idle)
                return false;

            grindHome_ = grindHome;
            conservativeQuestSales_ = conservativeQuestSales;
            LoadVendorMemory();
            remoteHubRouting_ = false;
            vendorGuid_ = 0;
            vendorEntry_ = 0;
            failoverFromEntry_ = 0;
            stateStartedTick_ = tick;
            lastInteractionTick_ = 0;
            lastDirectMoveTick_ = 0;
            lastSaleTick_ = 0;
            metadataWaitStartedTick_ = 0;
            nextMetadataRetryTick_ = 0;
            lastMetadataPendingCount_ = -1;
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
            loggedSaleDecisions_.clear();
            requestedMaintenance_ = maintenanceNeed;
            bagPressureTrigger_ = bagPressureTrigger;
            bagPressureSatisfied_ = !bagPressureTrigger;
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
            CandidateBackoff().Prune(tick);
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
                "VENDOR 14V.1: seeded and MerchantFrame-verified service hubs compete by suitability and bounded route cost; raw visible NPC flags alone do not establish merchant eligibility.");
            Debug::Logger::Info(
                "VENDOR FINAL APPROACH 14L.1.3: a failed NavMesh route may hand off to bounded direct CTM only while the same live merchant remains within 24 yd; progress is checked before each retry.");
            Debug::Logger::Info(
                "VENDOR LOCAL APPROACH 14L.1.4: selected vendor GUID stays locked; direct stalls within 8 yd may use up to three bounded flank probes on a 3.75 yd interaction ring.");
            Debug::Logger::Info(
                "VENDOR PROACTIVE HANDOFF 14L.1.6: selected live merchants at <=16 yd bypass/leave NavMesh and use the existing bounded direct/local final-approach path.");
            Debug::Logger::Info(
                "VENDOR INTERACTION GEOMETRY 14L.1.7: local flank probes keep the player's current Z and use a geometry-aware 1-3 yd XY ring targeting 4.0 yd 3D separation inside the unchanged 4.5 yd interaction threshold.");
            Debug::Logger::Info(
                conservativeQuestSales_
                    ? "Sell policy: questing protected poor miscellaneous trash only; all equipment retained."
                    : "Sell policy: existing AutoSellItemPolicy protected-item classification.");
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

            if (!PrepareHubSelection(world, tick))
            {
                Reset();
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

            if (state_ == VendorState::SelectingHub)
            {
                UpdateHubSelection(world, tick);
                return;
            }

            if (state_ == VendorState::PreparingHubSelection)
            {
                BeginHubSelection(world, tick);
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
                    if (!StartAlternateServiceSearch(world, tick))
                        Fail("selected service hub merchant not visible; failover exhausted", tick);
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
                    if (liveVendor->distance <= InteractionDistance)
                    {
                        if (!IssueInteraction(*liveVendor, tick))
                        {
                            if (!StartAlternateServiceSearch(world, tick))
                                Fail("selected merchant interaction failed", tick);
                        }
                        return;
                    }

                    if (!StartVendorNavigation(world, *liveVendor, tick))
                    {
                        if (!StartAlternateServiceSearch(world, tick))
                            Fail("selected merchant final route failed", tick);
                    }
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
                        Debug::Logger::Info(
                            "VENDOR 14V.1: selected service hub reached entry=" +
                            std::to_string(vendorEntry_) +
                            "; waiting for its live merchant.");
                        SetState(VendorState::SearchingVendor, tick);
                    }
                    else
                    {
                        SetState(VendorState::SearchingVendor, tick);
                    }
                }
                else if (vendorNavigator_->Failed())
                {
                    const auto failure = vendorNavigator_->LastPlanFailure();
                    vendorNavigator_.reset();
                    Debug::Logger::Info(
                        "VENDOR 14V.1: selected hub navigation failed reason=" +
                        std::string(Navigation::NavigationInitTelemetryPolicy::ReasonName(failure)));
                    if (!StartAlternateServiceSearch(
                            world, tick,
                            Navigation::NavigationInitTelemetryPolicy::ReasonName(failure)))
                        Fail("selected service hub route failed and failover exhausted", tick);
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
                    const auto failure = vendorNavigator_->LastPlanFailure();
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
                        Debug::Logger::Info(
                            "VENDOR 14V.1: merchant route failed reason=" +
                            std::string(Navigation::NavigationInitTelemetryPolicy::ReasonName(failure)));
                        if (!StartAlternateServiceSearch(
                                world, tick,
                                Navigation::NavigationInitTelemetryPolicy::ReasonName(failure)))
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
                    if (verifiedMerchant == nullptr || verifiedMerchant->entryId != vendorEntry_ ||
                        verifiedMerchant->guid != vendorGuid_ || verifiedMerchant->distance > InteractionDistance)
                    {
                        if (!StartAlternateServiceSearch(world, tick, "merchant_actor_unverified"))
                            Fail("MerchantFrame opened without matching live selected actor", tick);
                        return;
                    }
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
                if (tick < nextMetadataRetryTick_)
                    return;

                int bag = -1;
                int slot = -1;
                std::uint32_t itemId = 0;
                int metadataPending = 0;
                SellStepResult result =
                    SellOneUnprotectedItem(bag, slot, itemId, metadataPending);
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

                if (result == SellStepResult::MetadataPending)
                {
                    // Unknown metadata need not delay independent repair or
                    // supplies once bag pressure is already proven relieved.
                    GrindBagMonitor::Snapshot currentBags{};
                    const bool slotsVerified = bagPressureTrigger_ &&
                        GrindBagMonitor::Read(currentBags) &&
                        currentBags.freeSlots >= MinimumFreeSlotsAfterVendor;
                    if (!bagPressureTrigger_ || slotsVerified)
                    {
                        Debug::Logger::Info(
                            "VENDOR ITEM METADATA state=deferred unresolved=" +
                            std::to_string(metadataPending) +
                            " reason=bag_requirement_satisfied_or_not_requested");
                        result = SellStepResult::Done;
                    }
                    else
                    {
                        if (metadataWaitStartedTick_ == 0)
                            metadataWaitStartedTick_ = tick;
                        if (metadataPending != lastMetadataPendingCount_)
                        {
                            lastMetadataPendingCount_ = metadataPending;
                            Debug::Logger::Info(
                                "VENDOR ITEM METADATA state=pending unresolved=" +
                                std::to_string(metadataPending) +
                                " reason=GetItemInfo_not_ready");
                        }
                        if (tick - metadataWaitStartedTick_ >=
                            MetadataResolutionTimeoutTicks)
                        {
                            Fail("item metadata remained unavailable after bounded merchant wait; no unknown item was sold.", tick);
                            return;
                        }
                        nextMetadataRetryTick_ = tick + MetadataRetryTicks;
                        return;
                    }
                }
                nextMetadataRetryTick_ = 0;
                if (lastMetadataPendingCount_ > 0)
                {
                    Debug::Logger::Info(
                        "VENDOR ITEM METADATA state=progress previousUnresolved=" +
                        std::to_string(lastMetadataPendingCount_) +
                        " decision=" +
                        std::string(result == SellStepResult::CandidateIssued
                            ? "sale_candidate_ready" : "classification_complete"));
                    lastMetadataPendingCount_ = -1;
                    metadataWaitStartedTick_ = 0;
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

                if (tick >= stateStartedTick_ + MerchantOpenTimeoutTicks)
                {
                    maintenanceUnmet_ = true;
                    Fail("maintenance evidence deadline expired; no further purchase/repair commands",tick);
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
            hubProbeNavigator_.reset();
            tripCandidates_.clear();
            hubShortlist_.clear();
            selectedHubIndex_ = InvalidHubIndex;
            failoverFromEntry_ = 0;
            hubProbeIndex_ = 0;
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
            loggedSaleDecisions_.clear();
            candidatePendingVerification_ = false;
            requestedMaintenance_ = MaintenanceNeed{};
            maintenanceAtStart_ = MaintenanceSnapshot{};
            lastMaintenanceSnapshot_ = MaintenanceSnapshot{};
            bagPressureTrigger_ = false;
            bagPressureSatisfied_ = true;
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
        bool NavigationInitializationPending() const
        {
            return (homeNavigator_ && homeNavigator_->InitializationProgressing()) ||
                (vendorNavigator_ && vendorNavigator_->InitializationProgressing()) ||
                (returnNavigator_ && returnNavigator_->InitializationProgressing()) ||
                (hubProbeNavigator_ && hubProbeNavigator_->InitializationProgressing());
        }
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
        MaintenanceOutcome Outcome() const
        {
            if (state_ == VendorState::Failed)
                return maintenanceUnmet_ ? MaintenanceOutcome::TemporarilyUnavailable
                                         : MaintenanceOutcome::TerminalFailure;
            if (state_ == VendorState::Done)
                return maintenanceUnmet_ ? MaintenanceOutcome::StillRequired : MaintenanceOutcome::Satisfied;
            return MaintenanceOutcome::InProgress;
        }
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

        static bool ProbeFoodInventoryDiagnostic(std::string& result)
        {
            return ProbeFoodInventoryDiagnosticInternal(result);
        }
    };
}
