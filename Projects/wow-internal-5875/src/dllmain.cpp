#include <windows.h>

#include "Bot/WorldMonitor.h"
#include "Control/RuntimeControl.h"

#include "Core/Memory.h"
#include "Core/Module.h"

#include "Debug/Logger.h"

#include "Objects/ObjectManagerProbe.h"

#include "Wow5875/Client.h"
#include "Wow5875/ClientIdentity.h"
#include "Wow5875/Offsets.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    std::string Hex32(
        std::uint32_t value)
    {
        std::ostringstream stream;

        stream
            << "0x"
            << std::hex
            << std::uppercase
            << std::setw(8)
            << std::setfill('0')
            << value;

        return stream.str();
    }

    std::string Hex64(
        std::uint64_t value)
    {
        std::ostringstream stream;

        stream
            << "0x"
            << std::hex
            << std::uppercase
            << std::setw(16)
            << std::setfill('0')
            << value;

        return stream.str();
    }

    std::string VersionString(
        const Wow5875::ClientVersion& version)
    {
        if (!version.valid)
            return "unknown";

        std::ostringstream stream;

        stream
            << version.major
            << '.'
            << version.minor
            << '.'
            << version.patch
            << '.'
            << version.build;

        return stream.str();
    }

    [[noreturn]] void UnloadSelf(
        HMODULE module,
        const char* reason)
    {
        Control::RuntimeControlChannel runtimeControl;
        if (runtimeControl.OpenExisting())
            runtimeControl.MarkRuntimeDetached();

        if (reason && *reason)
            Debug::Logger::Info(reason);

        Debug::Logger::Event(std::string("DLL LIFECYCLE event=unload_requested reason=\"") +
            (reason ? reason : "unspecified") + "\"");

        FreeLibraryAndExitThread(
            module,
            0
        );

        for (;;)
            Sleep(INFINITE);
    }

    bool WaitForWorld()
    {
        Debug::Logger::Info(
            "Waiting for initialized "
            "ObjectManager/world..."
        );

        Control::RuntimeControlChannel runtimeControl;

        for (int attempt = 0;
             attempt < 300;
             ++attempt)
        {
            if (!runtimeControl.IsOpen())
                runtimeControl.OpenExisting();

            if (runtimeControl.IsOpen() &&
                (runtimeControl.UnloadRequested(false) ||
                 !runtimeControl.RunRequested(true)))
            {
                Debug::Logger::Info(
                    "GUI CONTROL 14H.2: stop requested while waiting for the WoW world; bootstrap will unload.");
                return false;
            }

            std::uint32_t manager = 0;

            std::uint64_t guid = 0;

            std::uint32_t firstObject = 0;

            const bool managerRead =
                Core::Memory::Read(
                    Wow5875::Offsets::
                        ObjectManager::Root,
                    manager
                );

            bool guidRead = false;
            bool firstRead = false;

            if (managerRead &&
                manager != 0 &&
                Core::Memory::IsReadable(
                    manager,
                    0xC8))
            {
                guidRead =
                    Core::Memory::Read(
                        manager +
                            Wow5875::Offsets::
                                ObjectManager::
                                ActivePlayerGuid,
                        guid
                    );

                firstRead =
                    Core::Memory::Read(
                        manager +
                            Wow5875::Offsets::
                                ObjectManager::
                                FirstObject,
                        firstObject
                    );
            }

            if (managerRead &&
                guidRead &&
                firstRead &&
                manager != 0 &&
                guid != 0 &&
                firstObject != 0 &&
                (firstObject & 1) == 0)
            {
                Debug::Logger::Info(
                    "World/ObjectManager initialized."
                );

                Debug::Logger::Info(
                    "ObjectManager: " +
                    Hex32(manager)
                );

                Debug::Logger::Info(
                    "ActivePlayerGuid: " +
                    Hex64(guid)
                );

                Debug::Logger::Info(
                    "FirstObject: " +
                    Hex32(firstObject)
                );

                return true;
            }

            if (attempt == 0 ||
                (attempt % 10) == 0)
            {
                Debug::Logger::Info(
                    "Still waiting..."
                );

                Debug::Logger::Info(
                    "Manager: " +
                    Hex32(manager)
                );

                Debug::Logger::Info(
                    "GUID: " +
                    Hex64(guid)
                );

                Debug::Logger::Info(
                    "FirstObject: " +
                    Hex32(firstObject)
                );
            }

            Sleep(1000);
        }

        Debug::Logger::Info(
            "Timed out waiting for world."
        );

        return false;
    }
}

DWORD WINAPI BootstrapThread(
    LPVOID parameter)
{
    HMODULE module =
        reinterpret_cast<HMODULE>(
            parameter
        );

    Debug::Logger::SetModule(
        module
    );

    // Outside DllMain/loader lock. Unexpected process termination may not
    // produce an unload marker; do not claim that it did.
    Debug::Logger::Event("DLL LIFECYCLE event=attach source=DLL_PROCESS_ATTACH");

    Sleep(500);

    Debug::Logger::Info(
        "=== wow-internal bootstrap ==="
    );

    Debug::Logger::Info(
        "Executable: " +
        Core::Module::ExecutablePath()
    );

    Debug::Logger::Info(
        "Executable name: " +
        Wow5875::ClientIdentity::
            ExecutableName()
    );

    Debug::Logger::Info(
        "Module base: " +
        Hex32(
            static_cast<std::uint32_t>(
                Core::Module::Base()
            )
        )
    );

    Debug::Logger::Info(
        std::string(
            "Architecture: "
        ) +
        (
            Core::Module::Is32Bit()
                ? "32-bit"
                : "64-bit"
        )
    );

    const auto version =
        Wow5875::ClientIdentity::
            Version();

    Debug::Logger::Info(
        "File version: " +
        VersionString(version)
    );

    if (!Wow5875::ClientIdentity::
            IsExpectedClient())
    {
        Debug::Logger::Info(
            "Host is not "
            "WoW 1.12.1 build 5875."
        );

        UnloadSelf(
            module,
            "GUI CONTROL 14H.2: bootstrap validation failed; unloading wow_internal.dll."
        );
    }

    Debug::Logger::Info(
        "WoW 1.12.1 build 5875 verified."
    );

    {
        Control::RuntimeControlChannel startupControl;
        if (startupControl.OpenExisting())
        {
            const Control::BotMode requestedMode =
                startupControl.RequestedMode(Control::BotMode::Grind);
            startupControl.MarkRuntimeAttached(requestedMode);
            startupControl.MarkRuntimeState(Control::BotRunState::Running, 0);
        }
    }

    Debug::Logger::Info(
        "GUI CONTROL 14H.2: DLL attached; waiting for the WoW world to initialize."
    );

    Debug::Logger::Info(
        "Wow5875::Client base: " +
        Hex32(
            static_cast<std::uint32_t>(
                Wow5875::Client::Base()
            )
        )
    );

    if (!WaitForWorld())
    {
        Debug::Logger::Info(
            "Bootstrap stopped."
        );

        UnloadSelf(
            module,
            "GUI CONTROL 14H.2: bootstrap stopped; unloading wow_internal.dll."
        );
    }

    /*
     * One-time detailed diagnostic.
     */
    Objects::ObjectManagerProbe::Run();

    /*
     * Continuous read-only world monitor.
     *
     * This function intentionally keeps the
     * bootstrap worker thread alive.
     */
    Bot::WorldMonitor::Run();

    UnloadSelf(
        module,
        "GUI CONTROL 14H.2: bootstrap exiting; unloading wow_internal.dll."
    );
}

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID reserved)
{
    (void)reserved;

    if (reason ==
        DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(
            module
        );

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                BootstrapThread,
                module,
                0,
                nullptr
            );

        if (thread)
        {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
