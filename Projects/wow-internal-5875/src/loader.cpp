#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
    using NtQueryInformationProcessFn =
        LONG (WINAPI*)(
            HANDLE,
            ULONG,
            PVOID,
            ULONG,
            PULONG
        );

    struct ProcessBasicInformation32
    {
        LONG exitStatus;
        std::uint32_t pebBaseAddress;
        std::uint32_t affinityMask;
        LONG basePriority;
        std::uint32_t uniqueProcessId;
        std::uint32_t inheritedFromUniqueProcessId;
    };

    struct ListEntry32
    {
        std::uint32_t flink;
        std::uint32_t blink;
    };

    struct UnicodeString32
    {
        std::uint16_t length;
        std::uint16_t maximumLength;
        std::uint32_t buffer;
    };

    template<typename T>
    bool ReadRemote(
        HANDLE process,
        std::uint32_t address,
        T& value)
    {
        SIZE_T bytesRead = 0;

        if (!ReadProcessMemory(
                process,
                reinterpret_cast<LPCVOID>(
                    static_cast<std::uintptr_t>(address)
                ),
                &value,
                sizeof(T),
                &bytesRead))
        {
            return false;
        }

        return bytesRead == sizeof(T);
    }

    bool GetRemotePeb(
        HANDLE process,
        std::uint32_t& pebAddress)
    {
        HMODULE ntdll =
            GetModuleHandleA("ntdll.dll");

        if (!ntdll)
        {
            std::printf(
                "GetModuleHandleA(ntdll.dll) failed: %lu\n",
                static_cast<unsigned long>(
                    GetLastError()
                )
            );

            return false;
        }

        auto ntQuery =
            reinterpret_cast<NtQueryInformationProcessFn>(
                GetProcAddress(
                    ntdll,
                    "NtQueryInformationProcess"
                )
            );

        if (!ntQuery)
        {
            std::printf(
                "GetProcAddress(NtQueryInformationProcess) failed.\n"
            );

            return false;
        }

        ProcessBasicInformation32 info{};

        ULONG returnedLength = 0;

        const LONG status =
            ntQuery(
                process,
                0, // ProcessBasicInformation
                &info,
                sizeof(info),
                &returnedLength
            );

        if (status < 0)
        {
            std::printf(
                "NtQueryInformationProcess failed: NTSTATUS=0x%08lX\n",
                static_cast<unsigned long>(status)
            );

            return false;
        }

        pebAddress =
            info.pebBaseAddress;

        std::printf(
            "Remote PEB: 0x%08lX\n",
            static_cast<unsigned long>(
                pebAddress
            )
        );

        return pebAddress != 0;
    }

    bool ReadRemoteUnicodeString(
        HANDLE process,
        const UnicodeString32& value,
        std::wstring& result)
    {
        result.clear();

        if (value.length == 0 ||
            value.buffer == 0)
        {
            return true;
        }

        if ((value.length % 2) != 0)
        {
            return false;
        }

        const SIZE_T characterCount =
            value.length / sizeof(wchar_t);

        if (characterCount > 32768)
        {
            return false;
        }

        std::vector<wchar_t> buffer(
            characterCount + 1,
            L'\0'
        );

        SIZE_T bytesRead = 0;

        if (!ReadProcessMemory(
                process,
                reinterpret_cast<LPCVOID>(
                    static_cast<std::uintptr_t>(
                        value.buffer
                    )
                ),
                buffer.data(),
                value.length,
                &bytesRead))
        {
            return false;
        }

        if (bytesRead != value.length)
        {
            return false;
        }

        result.assign(
            buffer.data(),
            characterCount
        );

        return true;
    }

    const wchar_t* BaseName(
        const std::wstring& path)
    {
        const wchar_t* result =
            path.c_str();

        const wchar_t* slash =
            std::wcsrchr(
                path.c_str(),
                L'\\'
            );

        const wchar_t* forwardSlash =
            std::wcsrchr(
                path.c_str(),
                L'/'
            );

        if (slash)
        {
            result = slash + 1;
        }

        if (forwardSlash &&
            forwardSlash + 1 > result)
        {
            result =
                forwardSlash + 1;
        }

        return result;
    }

    std::uint32_t FindRemoteModuleFromPeb(
        HANDLE process,
        const wchar_t* wantedModule)
    {
        std::uint32_t pebAddress = 0;

        if (!GetRemotePeb(
                process,
                pebAddress))
        {
            return 0;
        }

        /*
         * 32-bit PEB:
         *
         * +0x0C -> PEB_LDR_DATA*
         */
        std::uint32_t ldrAddress = 0;

        if (!ReadRemote(
                process,
                pebAddress + 0x0C,
                ldrAddress))
        {
            std::printf(
                "Unable to read PEB.Ldr.\n"
            );

            return 0;
        }

        if (ldrAddress == 0)
        {
            std::printf(
                "PEB.Ldr is NULL.\n"
            );

            return 0;
        }

        std::printf(
            "Remote PEB_LDR_DATA: 0x%08lX\n",
            static_cast<unsigned long>(
                ldrAddress
            )
        );

        /*
         * 32-bit PEB_LDR_DATA:
         *
         * +0x14 -> InMemoryOrderModuleList
         */
        const std::uint32_t listHead =
            ldrAddress + 0x14;

        ListEntry32 head{};

        if (!ReadRemote(
                process,
                listHead,
                head))
        {
            std::printf(
                "Unable to read module list head.\n"
            );

            return 0;
        }

        std::uint32_t current =
            head.flink;

        std::printf(
            "\n=== Modules from remote PEB ===\n"
        );

        /*
         * InMemoryOrderLinks ligger på +0x08
         * inne i 32-bitars LDR_DATA_TABLE_ENTRY.
         */
        for (int i = 0;
             i < 256 &&
             current != 0 &&
             current != listHead;
             ++i)
        {
            if (current < 0x08)
            {
                break;
            }

            const std::uint32_t entryAddress =
                current - 0x08;

            std::uint32_t dllBase = 0;

            /*
             * LDR_DATA_TABLE_ENTRY:
             *
             * +0x18 -> DllBase
             */
            if (!ReadRemote(
                    process,
                    entryAddress + 0x18,
                    dllBase))
            {
                std::printf(
                    "Failed reading DllBase at 0x%08lX\n",
                    static_cast<unsigned long>(
                        entryAddress
                    )
                );

                break;
            }

            UnicodeString32 fullDllName{};

            /*
             * +0x24 -> FullDllName
             */
            if (!ReadRemote(
                    process,
                    entryAddress + 0x24,
                    fullDllName))
            {
                std::printf(
                    "Failed reading FullDllName.\n"
                );

                break;
            }

            std::wstring modulePath;

            if (ReadRemoteUnicodeString(
                    process,
                    fullDllName,
                    modulePath))
            {
                const wchar_t* moduleName =
                    BaseName(modulePath);

                std::wprintf(
                    L"0x%08lX  %ls\n",
                    static_cast<unsigned long>(
                        dllBase
                    ),
                    moduleName
                );

                if (_wcsicmp(
                        moduleName,
                        wantedModule) == 0)
                {
                    std::printf(
                        "================================\n\n"
                    );

                    return dllBase;
                }
            }

            ListEntry32 links{};

            if (!ReadRemote(
                    process,
                    current,
                    links))
            {
                std::printf(
                    "Failed reading module list link.\n"
                );

                break;
            }

            current =
                links.flink;
        }

        std::printf(
            "================================\n\n"
        );

        return 0;
    }

    std::uint32_t WaitForRemoteModule(
        HANDLE process,
        const wchar_t* moduleName)
    {
        for (int attempt = 0;
             attempt < 100;
             ++attempt)
        {
            const std::uint32_t module =
                FindRemoteModuleFromPeb(
                    process,
                    moduleName
                );

            if (module != 0)
            {
                return module;
            }

            Sleep(100);
        }

        return 0;
    }

    const char* BaseNameA(
        const std::string& path)
    {
        const char* result = path.c_str();

        const char* slash = std::strrchr(path.c_str(), '\\');
        const char* forwardSlash = std::strrchr(path.c_str(), '/');

        if (slash)
            result = slash + 1;
        if (forwardSlash && forwardSlash + 1 > result)
            result = forwardSlash + 1;

        return result;
    }

    std::wstring ToWide(
        const std::string& value)
    {
        if (value.empty())
            return {};

        const int required = MultiByteToWideChar(
            CP_ACP,
            0,
            value.c_str(),
            -1,
            nullptr,
            0);

        if (required <= 1)
            return {};

        std::wstring result(
            static_cast<std::size_t>(required),
            L'\0');

        MultiByteToWideChar(
            CP_ACP,
            0,
            value.c_str(),
            -1,
            result.data(),
            required);

        if (!result.empty() && result.back() == L'\0')
            result.pop_back();

        return result;
    }

    bool FindSingleProcessId(
        const char* executableName,
        DWORD& processId)
    {
        processId = 0;

        HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

        if (snapshot == INVALID_HANDLE_VALUE)
        {
            std::printf(
                "CreateToolhelp32Snapshot failed: %lu\n",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }

        const std::wstring executableNameWide =
            ToWide(executableName);

        if (executableNameWide.empty())
        {
            CloseHandle(snapshot);
            std::printf(
                "Could not convert process name to UTF-16.\n");
            return false;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        int matches = 0;

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(
                        entry.szExeFile,
                        executableNameWide.c_str()) == 0)
                {
                    processId = entry.th32ProcessID;
                    ++matches;
                }
            }
            while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);

        if (matches == 0)
        {
            std::printf(
                "No running %s process was found.\n",
                executableName);
            processId = 0;
            return false;
        }

        if (matches > 1)
        {
            std::printf(
                "More than one %s process is running. Close extra clients before injecting.\n",
                executableName);
            processId = 0;
            return false;
        }

        return true;
    }

    bool InjectDll(
        HANDLE process,
        const std::string& dllPath)
    {
        const SIZE_T pathSize =
            dllPath.size() + 1;

        void* remotePath =
            VirtualAllocEx(
                process,
                nullptr,
                pathSize,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE
            );

        if (!remotePath)
        {
            std::printf(
                "VirtualAllocEx failed: %lu\n",
                static_cast<unsigned long>(
                    GetLastError()
                )
            );

            return false;
        }

        SIZE_T written = 0;

        if (!WriteProcessMemory(
                process,
                remotePath,
                dllPath.c_str(),
                pathSize,
                &written))
        {
            std::printf(
                "WriteProcessMemory failed: %lu\n",
                static_cast<unsigned long>(
                    GetLastError()
                )
            );

            VirtualFreeEx(
                process,
                remotePath,
                0,
                MEM_RELEASE
            );

            return false;
        }

        std::printf(
            "DLL path written: %llu bytes\n",
            static_cast<unsigned long long>(
                written
            )
        );

        const std::uint32_t remoteKernel32 =
            WaitForRemoteModule(
                process,
                L"kernel32.dll"
            );

        if (remoteKernel32 == 0)
        {
            std::printf(
                "kernel32.dll was not found through PEB.\n"
            );

            VirtualFreeEx(
                process,
                remotePath,
                0,
                MEM_RELEASE
            );

            return false;
        }

        std::printf(
            "Remote kernel32.dll: 0x%08lX\n",
            static_cast<unsigned long>(
                remoteKernel32
            )
        );

        HMODULE localKernel32 =
            GetModuleHandleA(
                "kernel32.dll"
            );

        if (!localKernel32)
        {
            std::printf(
                "Local kernel32.dll not found.\n"
            );

            return false;
        }

        FARPROC localLoadLibrary =
            GetProcAddress(
                localKernel32,
                "LoadLibraryA"
            );

        if (!localLoadLibrary)
        {
            std::printf(
                "Local LoadLibraryA not found.\n"
            );

            return false;
        }

        const std::uintptr_t localKernelBase =
            reinterpret_cast<std::uintptr_t>(
                localKernel32
            );

        const std::uintptr_t localLoadLibraryAddress =
            reinterpret_cast<std::uintptr_t>(
                localLoadLibrary
            );

        const std::uintptr_t loadLibraryRva =
            localLoadLibraryAddress -
            localKernelBase;

        const std::uintptr_t remoteLoadLibrary =
            static_cast<std::uintptr_t>(
                remoteKernel32
            ) +
            loadLibraryRva;

        std::printf(
            "Local kernel32:       0x%08lX\n",
            static_cast<unsigned long>(
                localKernelBase
            )
        );

        std::printf(
            "Local LoadLibraryA:   0x%08lX\n",
            static_cast<unsigned long>(
                localLoadLibraryAddress
            )
        );

        std::printf(
            "LoadLibraryA RVA:     0x%08lX\n",
            static_cast<unsigned long>(
                loadLibraryRva
            )
        );

        std::printf(
            "Remote LoadLibraryA:  0x%08lX\n",
            static_cast<unsigned long>(
                remoteLoadLibrary
            )
        );

        HANDLE remoteThread =
            CreateRemoteThread(
                process,
                nullptr,
                0,
                reinterpret_cast<
                    LPTHREAD_START_ROUTINE>(
                        remoteLoadLibrary
                    ),
                remotePath,
                0,
                nullptr
            );

        if (!remoteThread)
        {
            std::printf(
                "CreateRemoteThread failed: %lu\n",
                static_cast<unsigned long>(
                    GetLastError()
                )
            );

            VirtualFreeEx(
                process,
                remotePath,
                0,
                MEM_RELEASE
            );

            return false;
        }

        std::printf(
            "Remote LoadLibraryA thread started.\n"
        );

        const DWORD waitResult =
            WaitForSingleObject(
                remoteThread,
                15000
            );

        if (waitResult != WAIT_OBJECT_0)
        {
            std::printf(
                "Remote thread timeout/error: %lu\n",
                static_cast<unsigned long>(
                    waitResult
                )
            );

            CloseHandle(
                remoteThread
            );

            return false;
        }

        DWORD loadedModule = 0;

        if (!GetExitCodeThread(
                remoteThread,
                &loadedModule))
        {
            std::printf(
                "GetExitCodeThread failed: %lu\n",
                static_cast<unsigned long>(
                    GetLastError()
                )
            );

            CloseHandle(
                remoteThread
            );

            return false;
        }

        CloseHandle(
            remoteThread
        );

        VirtualFreeEx(
            process,
            remotePath,
            0,
            MEM_RELEASE
        );

        if (loadedModule == 0)
        {
            std::printf(
                "Remote LoadLibraryA returned NULL.\n"
            );

            return false;
        }

        std::printf(
            "wow_internal.dll loaded at 0x%08lX\n",
            static_cast<unsigned long>(
                loadedModule
            )
        );

        return true;
    }

    bool InjectExistingProcess(
        const std::string& wowPath,
        const std::string& dllPath)
    {
        const char* executableName = BaseNameA(wowPath);

        DWORD processId = 0;
        if (!FindSingleProcessId(executableName, processId))
            return false;

        std::printf(
            "Attaching to existing %s PID %lu (0x%lX)\n",
            executableName,
            static_cast<unsigned long>(processId),
            static_cast<unsigned long>(processId));

        HANDLE process = OpenProcess(
            PROCESS_CREATE_THREAD |
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE |
            PROCESS_VM_READ |
            SYNCHRONIZE,
            FALSE,
            processId);

        if (!process)
        {
            std::printf(
                "OpenProcess failed: %lu\n",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }

        const std::wstring dllName = ToWide(BaseNameA(dllPath));
        if (!dllName.empty())
        {
            const std::uint32_t existingModule =
                FindRemoteModuleFromPeb(process, dllName.c_str());

            if (existingModule != 0)
            {
                std::printf(
                    "wow_internal.dll is already loaded at 0x%08lX; refusing duplicate injection.\n",
                    static_cast<unsigned long>(existingModule));
                CloseHandle(process);
                return true;
            }
        }

        const bool injected = InjectDll(process, dllPath);
        CloseHandle(process);
        return injected;
    }

    bool InjectInheritedProcessHandle(
        HANDLE process,
        const std::string& dllPath)
    {
        if (!process || process == INVALID_HANDLE_VALUE)
        {
            std::printf("Inherited WoW process handle is invalid.\n");
            return false;
        }

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(process, &exitCode))
        {
            std::printf(
                "GetExitCodeProcess on inherited WoW handle failed: %lu\n",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }

        if (exitCode != STILL_ACTIVE)
        {
            std::printf("Inherited WoW process handle refers to an exited process.\n");
            return false;
        }

        std::printf(
            "Using inherited WoW process handle 0x%llX; no OpenProcess call is required.\n",
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(process)));

        const std::wstring dllName = ToWide(BaseNameA(dllPath));
        if (!dllName.empty())
        {
            const std::uint32_t existingModule =
                FindRemoteModuleFromPeb(process, dllName.c_str());

            if (existingModule != 0)
            {
                std::printf(
                    "wow_internal.dll is already loaded at 0x%08lX; refusing duplicate injection.\n",
                    static_cast<unsigned long>(existingModule));
                return true;
            }
        }

        return InjectDll(process, dllPath);
    }
}

int main(int argc, char** argv)
{
    std::printf(
        "wow-internal-5875 launcher\n"
    );

    std::printf(
        "==========================\n\n"
    );

    if (argc == 4 &&
        std::strcmp(argv[1], "--inject-handle") == 0)
    {
        char* end = nullptr;
        const unsigned long long rawHandle = std::strtoull(argv[2], &end, 0);
        const std::string dllPath = argv[3];

        if (!end || *end != '\0' || rawHandle == 0)
        {
            std::printf("Invalid inherited process handle value.\n");
            return 1;
        }

        if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::printf("wow_internal.dll was not found.\n");
            return 1;
        }

        HANDLE process = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(rawHandle));

        std::printf(
            "Mode: inject through inherited WoW process handle.\n\n");

        if (!InjectInheritedProcessHandle(process, dllPath))
        {
            std::printf("\nDLL injection through inherited handle failed.\n");
            return 1;
        }

        std::printf("\nDLL injection through inherited handle completed successfully.\n");
        return 0;
    }

    if (argc == 4 &&
        std::strcmp(argv[1], "--inject-existing") == 0)
    {
        const std::string wowPath = argv[2];
        const std::string dllPath = argv[3];

        if (GetFileAttributesA(wowPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::printf("WoW.exe was not found.\n");
            return 1;
        }

        if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::printf("wow_internal.dll was not found.\n");
            return 1;
        }

        std::printf(
            "Mode: inject into the already-running WoW process.\n\n");

        if (!InjectExistingProcess(wowPath, dllPath))
        {
            std::printf("\nDLL injection into existing WoW failed.\n");
            return 1;
        }

        std::printf("\nDLL injection into existing WoW completed successfully.\n");
        return 0;
    }

    if (argc != 3)
    {
        std::printf(
            "Usage:\n"
            "  wow_loader.exe --inject-handle <inherited-handle> <wow_internal.dll>\n"
            "  wow_loader.exe --inject-existing <WoW.exe> <wow_internal.dll>\n"
            "  wow_loader.exe <WoW.exe> <wow_internal.dll>   (legacy launch + inject)\n"
        );

        return 1;
    }

    const std::string wowPath = argv[1];
    const std::string dllPath = argv[2];

    std::printf(
        "WoW:\n%s\n\n",
        wowPath.c_str()
    );

    std::printf(
        "DLL:\n%s\n\n",
        dllPath.c_str()
    );

    if (GetFileAttributesA(
            wowPath.c_str()) ==
        INVALID_FILE_ATTRIBUTES)
    {
        std::printf(
            "WoW.exe was not found.\n"
        );

        return 1;
    }

    if (GetFileAttributesA(
            dllPath.c_str()) ==
        INVALID_FILE_ATTRIBUTES)
    {
        std::printf(
            "wow_internal.dll was not found.\n"
        );

        return 1;
    }

    std::string wowDirectory =
        wowPath;

    const auto slash =
        wowDirectory.find_last_of("\\/");

    if (slash != std::string::npos)
    {
        wowDirectory.resize(slash);
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION processInfo{};

    const BOOL created =
        CreateProcessA(
            wowPath.c_str(),
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            wowDirectory.c_str(),
            &startup,
            &processInfo
        );

    if (!created)
    {
        std::printf(
            "CreateProcessA failed: %lu\n",
            static_cast<unsigned long>(
                GetLastError()
            )
        );

        return 1;
    }

    std::printf(
        "WoW started successfully.\n"
    );

    std::printf(
        "Windows PID: %lu (0x%lX)\n",
        static_cast<unsigned long>(
            processInfo.dwProcessId
        ),
        static_cast<unsigned long>(
            processInfo.dwProcessId
        )
    );

    const DWORD idleResult =
        WaitForInputIdle(
            processInfo.hProcess,
            15000
        );

    std::printf(
        "WaitForInputIdle result: %lu\n",
        static_cast<unsigned long>(
            idleResult
        )
    );

    Sleep(3000);

    const bool injected =
        InjectDll(
            processInfo.hProcess,
            dllPath
        );

    CloseHandle(
        processInfo.hThread
    );

    CloseHandle(
        processInfo.hProcess
    );

    if (!injected)
    {
        std::printf(
            "\nDLL load failed.\n"
        );

        return 1;
    }

    std::printf(
        "\nDLL load completed successfully.\n"
    );

    return 0;
}
