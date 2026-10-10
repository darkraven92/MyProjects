#pragma once

#include <windows.h>
#include <wincrypt.h>

#include <array>

namespace Bot
{
    // Observer-only file provenance. No client calls or client memory writes.
    // Fail closed on path truncation, file/crypto errors or a different digest.
    inline bool LocationClientFingerprint5875()
    {
        std::array<char,32768> path{};
        const DWORD length = GetModuleFileNameA(nullptr, path.data(), path.size());
        if (!length || length >= path.size()) return false;
        const HANDLE file = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        HCRYPTPROV provider = 0;
        HCRYPTHASH hash = 0;
        LARGE_INTEGER size{};
        bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
            size.QuadPart <= 64 * 1024 * 1024 &&
            CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
            CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
        std::array<BYTE,16384> buffer{};
        LONGLONG total = 0;
        while (ok)
        {
            DWORD copied = 0;
            if (!ReadFile(file, buffer.data(), buffer.size(), &copied, nullptr))
            {
                ok = false;
                break;
            }
            if (!copied) break;
            total += copied;
            ok = total <= size.QuadPart && CryptHashData(hash, buffer.data(), copied, 0);
        }
        constexpr std::array<BYTE,32> expected{
            0xb4,0x75,0x6d,0x38,0xef,0x20,0x7c,0x02,0xed,0x65,0x1f,0x49,0x52,0xbd,0x89,0xa7,
            0x0b,0x48,0x57,0xb7,0x3a,0x33,0x41,0x33,0x39,0xe1,0xb2,0x85,0xb2,0x8d,0x2d,0xc7};
        std::array<BYTE,32> digest{};
        DWORD digestSize = digest.size();
        ok = ok && total == size.QuadPart &&
            CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize, 0) &&
            digestSize == digest.size() && digest == expected;
        if (hash) CryptDestroyHash(hash);
        if (provider) CryptReleaseContext(provider, 0);
        CloseHandle(file);
        return ok;
    }
}
