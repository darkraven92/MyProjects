#pragma once

#include "ConnectionObservationPolicy.h"
#include "../Control/RuntimeControl.h"
#include "../Debug/Logger.h"
#include "../Objects/WorldState.h"
#include "../Wow5875/Client.h"

#include <windows.h>

namespace Bot
{
    // Separate lifetime from all gameplay controllers, including their stop
    // paths. Only client memory reads, diagnostic logging and GUI IPC occur.
    class ConnectionLifecycleObserver5875
    {
    public:
        static void Run(ConnectionMode mode)
        {
            Control::RuntimeControlChannel control;
            const bool controlAvailable = control.OpenExisting();
            if (mode != ConnectionMode::Observe || !controlAvailable)
            {
                Debug::Logger::Info(std::string("CONNECTION OBSERVE CONFIG result=blocked reason=") +
                    (mode != ConnectionMode::Observe ? "invalid_mode" : "control_unavailable") +
                    " inputOwner=none commands=none");
                if (controlAvailable) control.MarkRuntimeDetached();
                return;
            }

            Debug::Logger::Event("BOT SESSION START mode=connection_observe");
            Debug::Logger::Info("CONNECTION OBSERVE CONFIG mode=observe pollMs=250 inputOwner=none commands=none");
            ConnectionObservationTracker tracker;
            std::uint64_t heartbeat = 0;
            while (control.RunRequested(false) && !control.UnloadRequested(false))
            {
                // Unknown is deliberate: neither Grind nor Quest owns this
                // diagnostic session, regardless of the GUI's requested mode.
                control.MarkRuntimeAttached(Control::BotMode::Unknown);
                control.MarkRuntimeState(Control::BotRunState::Running,
                    static_cast<LONG>((++heartbeat) & 0x7fffffffULL));

                const auto connection = ConnectionEvidence5875::Observe(
                    Wow5875::Client::Base(), [](std::uintptr_t address, auto& value)
                    {
                        SIZE_T copied = 0;
                        return ReadProcessMemory(GetCurrentProcess(),
                            reinterpret_cast<const void*>(address), &value,
                            sizeof(value), &copied) && copied == sizeof(value);
                    });
                Objects::WorldState world{};
                Objects::WorldReadStage stage = Objects::WorldReadStage::Complete;
                const bool valid = Objects::WorldStateReader::Read(world, &stage);
                std::string fields;
                if (tracker.Observe(connection,
                    {valid, Objects::WorldReadStageName(stage), world.manager,
                     world.activePlayerGuid, world.localPlayer}, fields))
                {
                    Debug::Logger::Event("CONNECTION OBSERVE sampleMs=" +
                        std::to_string(GetTickCount64()) + " " + fields);
                }
                Sleep(250);
            }

            // Never call the normal attack/movement stop path: no input owner
            // was constructed or started in this session.
            control.MarkRuntimeState(Control::BotRunState::Unloading,
                static_cast<LONG>(heartbeat & 0x7fffffffULL));
            Debug::Logger::Event("BOT SESSION STOP mode=connection_observe reason=stop_or_unload inputOwner=none commands=none");
            control.MarkRuntimeDetached();
            Debug::Logger::Info("GUI CONTROL 14H.2: RUNTIME DETACHED; connection observer ended; bootstrap thread will unload wow_internal.dll now.");
        }
    };
}
