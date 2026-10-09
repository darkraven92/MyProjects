#pragma once

#include <windows.h>

#include <cstdint>

namespace Control
{
    enum class BotMode : LONG
    {
        Unknown = 0,
        Grind = 1,
        Questing = 2
    };

    enum class BotRunState : LONG
    {
        Unknown = 0,
        Stopped = 1,
        Running = 2,
        Unloading = 3
    };

    struct RuntimeControlState
    {
        DWORD magic = 0;
        DWORD version = 0;
        volatile LONG runRequested = 0;
        volatile LONG requestedMode = static_cast<LONG>(BotMode::Grind);
        volatile LONG unloadRequested = 0;
        volatile LONG runtimeAttached = 0;
        volatile LONG activeMode = static_cast<LONG>(BotMode::Unknown);
        volatile LONG runtimeState = static_cast<LONG>(BotRunState::Unknown);
        volatile LONG heartbeat = 0;
        volatile LONG vendorAutomationEnabled = 0;
    };

    class RuntimeControlChannel
    {
    private:
        static constexpr DWORD Magic = 0x32484357; // WCH2
        static constexpr DWORD Version = 3;
        static constexpr wchar_t MappingName[] = L"WowInternal5875Control14H2V3";

        HANDLE mapping_ = nullptr;
        RuntimeControlState* state_ = nullptr;

        void CloseInternal()
        {
            if (state_)
            {
                UnmapViewOfFile(state_);
                state_ = nullptr;
            }

            if (mapping_)
            {
                CloseHandle(mapping_);
                mapping_ = nullptr;
            }
        }

        bool MapHandle(HANDLE mapping)
        {
            if (!mapping)
                return false;

            auto* state = static_cast<RuntimeControlState*>(
                MapViewOfFile(
                    mapping,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    sizeof(RuntimeControlState)));

            if (!state)
            {
                CloseHandle(mapping);
                return false;
            }

            mapping_ = mapping;
            state_ = state;
            return true;
        }

    public:
        RuntimeControlChannel() = default;

        RuntimeControlChannel(const RuntimeControlChannel&) = delete;
        RuntimeControlChannel& operator=(const RuntimeControlChannel&) = delete;

        ~RuntimeControlChannel()
        {
            CloseInternal();
        }

        bool CreateOwner(
            BotMode initialMode = BotMode::Grind,
            bool initialRunRequested = false)
        {
            if (state_)
                return true;

            HANDLE mapping = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                static_cast<DWORD>(sizeof(RuntimeControlState)),
                MappingName);

            if (!mapping)
                return false;

            const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;
            if (!MapHandle(mapping))
                return false;

            if (!existed || state_->magic != Magic || state_->version != Version)
            {
                state_->magic = Magic;
                state_->version = Version;
                InterlockedExchange(&state_->runRequested, initialRunRequested ? 1 : 0);
                InterlockedExchange(&state_->requestedMode, static_cast<LONG>(initialMode));
                InterlockedExchange(&state_->unloadRequested, 0);
                InterlockedExchange(&state_->runtimeAttached, 0);
                InterlockedExchange(&state_->activeMode, static_cast<LONG>(BotMode::Unknown));
                InterlockedExchange(&state_->runtimeState, static_cast<LONG>(BotRunState::Unknown));
                InterlockedExchange(&state_->heartbeat, 0);
                InterlockedExchange(&state_->vendorAutomationEnabled, 0);
            }

            return true;
        }

        bool OpenExisting()
        {
            if (state_)
                return true;

            HANDLE mapping = OpenFileMappingW(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                MappingName);

            if (!mapping)
                return false;

            if (!MapHandle(mapping))
                return false;

            if (state_->magic != Magic || state_->version != Version)
            {
                CloseInternal();
                return false;
            }

            return true;
        }

        bool IsOpen() const
        {
            return state_ != nullptr;
        }

        void RequestRun(bool run)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->runRequested, run ? 1 : 0);
        }

        bool RunRequested(bool fallback = false) const
        {
            if (!state_)
                return fallback;
            return state_->runRequested != 0;
        }

        void RequestVendorAutomation(bool enabled)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->vendorAutomationEnabled, enabled ? 1 : 0);
        }

        bool VendorAutomationEnabled(bool fallback = false) const
        {
            return state_ ? state_->vendorAutomationEnabled != 0 : fallback;
        }

        void RequestMode(BotMode mode)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->requestedMode, static_cast<LONG>(mode));
        }

        BotMode RequestedMode(BotMode fallback = BotMode::Grind) const
        {
            if (!state_)
                return fallback;

            const LONG raw = state_->requestedMode;
            if (raw == static_cast<LONG>(BotMode::Questing))
                return BotMode::Questing;
            if (raw == static_cast<LONG>(BotMode::Grind))
                return BotMode::Grind;
            return fallback;
        }

        void RequestUnload(bool unload)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->unloadRequested, unload ? 1 : 0);
        }

        bool UnloadRequested(bool fallback = false) const
        {
            if (!state_)
                return fallback;
            return state_->unloadRequested != 0;
        }

        void PrepareForInjection(BotMode mode)
        {
            if (!state_)
                return;

            InterlockedExchange(&state_->requestedMode, static_cast<LONG>(mode));
            InterlockedExchange(&state_->unloadRequested, 0);
            InterlockedExchange(&state_->runRequested, 1);
            InterlockedExchange(&state_->runtimeAttached, 0);
            InterlockedExchange(&state_->activeMode, static_cast<LONG>(BotMode::Unknown));
            InterlockedExchange(&state_->runtimeState, static_cast<LONG>(BotRunState::Unknown));
        }

        void MarkRuntimeAttached(BotMode mode)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->runtimeAttached, 1);
            InterlockedExchange(&state_->activeMode, static_cast<LONG>(mode));
        }

        void MarkRuntimeDetached()
        {
            if (!state_)
                return;

            InterlockedExchange(&state_->runtimeState, static_cast<LONG>(BotRunState::Stopped));
            InterlockedExchange(&state_->runtimeAttached, 0);
            InterlockedExchange(&state_->activeMode, static_cast<LONG>(BotMode::Unknown));
            InterlockedExchange(&state_->runRequested, 0);
            InterlockedExchange(&state_->unloadRequested, 0);
        }

        bool RuntimeAttached() const
        {
            return state_ && state_->runtimeAttached != 0;
        }

        BotMode ActiveMode() const
        {
            if (!state_)
                return BotMode::Unknown;

            const LONG raw = state_->activeMode;
            if (raw == static_cast<LONG>(BotMode::Questing))
                return BotMode::Questing;
            if (raw == static_cast<LONG>(BotMode::Grind))
                return BotMode::Grind;
            return BotMode::Unknown;
        }

        void MarkRuntimeState(BotRunState runState, LONG heartbeat)
        {
            if (!state_)
                return;
            InterlockedExchange(&state_->runtimeState, static_cast<LONG>(runState));
            InterlockedExchange(&state_->heartbeat, heartbeat);
        }

        BotRunState RuntimeState() const
        {
            if (!state_)
                return BotRunState::Unknown;

            const LONG raw = state_->runtimeState;
            if (raw == static_cast<LONG>(BotRunState::Stopped))
                return BotRunState::Stopped;
            if (raw == static_cast<LONG>(BotRunState::Running))
                return BotRunState::Running;
            if (raw == static_cast<LONG>(BotRunState::Unloading))
                return BotRunState::Unloading;
            return BotRunState::Unknown;
        }

        LONG Heartbeat() const
        {
            return state_ ? state_->heartbeat : 0;
        }
    };
}
