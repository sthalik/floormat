#include "game-mode.hpp"

#ifdef _WIN32
#include <bit>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cr/Array.h>
#include <cr/ScopeGuard.h>
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#endif

namespace floormat {

#ifdef _WIN32

namespace {

// Undocumented, same values as Special K's wgc_registrar.cpp: Parents\<SHA-1 hex of the lowercase
// exe filename as UTF-16LE> has a Children list of GUIDs naming Children\<guid> keys, one per exe
// path and argument string.

using protect_fn = decltype(&CryptProtectData);

protect_fn crypt_protect_data() noexcept
{
    static const protect_fn fn = [] {
        auto* dll = LoadLibraryA("crypt32.dll");
        return dll ? std::bit_cast<protect_fn>(GetProcAddress(dll, "CryptProtectData")) : nullptr;
    }();
    return fn;
}

wchar_t* put_hex(wchar_t* out, const BYTE* data, uint32_t size) noexcept
{
    constexpr wchar_t digits[] = L"0123456789abcdef";
    for (uint32_t i = 0; i < size; i++)
    {
        *out++ = digits[data[i] >> 4];
        *out++ = digits[data[i] & 15];
    }
    return out;
}

struct sha1_digest
{
    BYTE bytes[20];
    bool ok;
};

sha1_digest sha1(HCRYPTPROV prov, const void* data, uint32_t size) noexcept
{
    sha1_digest d{};
    HCRYPTHASH hash;
    if (!CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash))
        return d;
    ScopeGuard hash_guard{hash, CryptDestroyHash};
    DWORD digest_size = sizeof d.bytes;
    d.ok = CryptHashData(hash, (const BYTE*)data, size, 0) &&
           CryptGetHashParam(hash, HP_HASHVAL, d.bytes, &digest_size, 0) && digest_size == sizeof d.bytes;
    return d;
}

bool set_value(HKEY key, const wchar_t* name, DWORD type, const void* data, uint32_t size) noexcept
{
    return RegSetValueExW(key, name, 0, type, (const BYTE*)data, size) == ERROR_SUCCESS;
}

bool delete_value(HKEY key, const wchar_t* name) noexcept
{
    auto err = RegDeleteValueW(key, name);
    return err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND;
}

enum class child_state : uint8_t { missing, other, match };

child_state check_child(HKEY children, const wchar_t* guid, ArrayView<wchar_t> buf,
                        const wchar_t* path, uint32_t len) noexcept
{
    HKEY key;
    if (auto err = RegOpenKeyExW(children, guid, 0, KEY_QUERY_VALUE, &key); err != ERROR_SUCCESS)
        return err == ERROR_FILE_NOT_FOUND ? child_state::missing : child_state::other;
    ScopeGuard key_guard{key, RegCloseKey};
    if (RegGetValueW(key, nullptr, L"Arguments", RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
        return child_state::other;
    // buf holds our path and its NUL, so a longer path fails with ERROR_MORE_DATA.
    auto bytes = DWORD(buf.size() * sizeof(wchar_t));
    if (RegGetValueW(key, nullptr, L"MatchedExeFullPath", RRF_RT_REG_SZ, nullptr, buf.data(), &bytes) != ERROR_SUCCESS)
        return child_state::other;
    if (CompareStringOrdinal(buf.data(), -1, path, (int)len, TRUE) == CSTR_EQUAL)
        return child_state::match;
    return child_state::other;
}

} // namespace

with_game_mode::with_game_mode() noexcept
{
    if (designate() == entry::added)
        if (auto status = relaunch())
            std::exit(*status);
}

auto with_game_mode::designate() noexcept -> entry
{
    Array<wchar_t> path{ValueInit, 32768};
    const uint32_t len = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    if (!len || len == path.size())
        return entry::failed;
    const wchar_t* sep = std::wcsrchr(path.data(), L'\\');
    if (!sep || !sep[1])
        return entry::failed;
    const auto dir_len = uint32_t(sep - path.data()), name_len = len - dir_len - 1;
    const auto name_bytes = DWORD(name_len * sizeof(wchar_t));

    Array<wchar_t> lower{ValueInit, len};
    if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, path.data(), (int)len,
                      lower.data(), (int)len, nullptr, nullptr, 0) != (int)len)
        return entry::failed;
    wchar_t* const name = lower.data() + dir_len + 1;

    HCRYPTPROV prov;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return entry::failed;
    ScopeGuard prov_guard{prov, [](HCRYPTPROV p) { CryptReleaseContext(p, 0); }};
    const auto name_digest = sha1(prov, name, name_bytes);
    // Same path, same GUID, so two first launches at once write one entry. Version 8, because
    // version 5 also hashes a namespace.
    auto guid_digest = sha1(prov, lower.data(), len * (uint32_t)sizeof(wchar_t));
    if (!name_digest.ok || !guid_digest.ok)
        return entry::failed;
    guid_digest.bytes[6] = BYTE((guid_digest.bytes[6] & 0x0f) | 0x80);
    guid_digest.bytes[8] = BYTE((guid_digest.bytes[8] & 0x3f) | 0x80);
    wchar_t parent_name[2 * sizeof name_digest.bytes + 1] = {};
    put_hex(parent_name, name_digest.bytes, sizeof name_digest.bytes);

    HKEY children, parents;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore\\Children", 0, nullptr, 0,
                        KEY_CREATE_SUB_KEY, nullptr, &children, nullptr) != ERROR_SUCCESS)
        return entry::failed;
    ScopeGuard children_guard{children, RegCloseKey};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore\\Parents", 0, nullptr, 0,
                        KEY_CREATE_SUB_KEY, nullptr, &parents, nullptr) != ERROR_SUCCESS)
        return entry::failed;
    ScopeGuard parents_guard{parents, RegCloseKey};

    // Two spare NULs keep the list terminated whatever the stored value holds.
    Array<wchar_t> list;
    if (HKEY parent; RegOpenKeyExW(parents, parent_name, 0, KEY_QUERY_VALUE, &parent) == ERROR_SUCCESS)
    {
        ScopeGuard parent_guard{parent, RegCloseKey};
        DWORD bytes = 0;
        auto err = RegGetValueW(parent, nullptr, L"Children", RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &bytes);
        if (err == ERROR_SUCCESS)
        {
            list = Array<wchar_t>{ValueInit, bytes / sizeof(wchar_t) + 2};
            err = RegGetValueW(parent, nullptr, L"Children", RRF_RT_REG_MULTI_SZ, nullptr, list.data(), &bytes);
        }
        if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
            return entry::failed;
    }

    constexpr uint32_t guid_len = 36;
    Array<wchar_t> new_list{ValueInit, guid_len + 1 + list.size() + 1};
    wchar_t* const guid = new_list.data();
    {
        const BYTE* id = guid_digest.bytes;
        wchar_t* p = guid;
        p = put_hex(p, id, 4);
        *p++ = L'-';
        p = put_hex(p, id + 4, 2);
        *p++ = L'-';
        p = put_hex(p, id + 6, 2);
        *p++ = L'-';
        p = put_hex(p, id + 8, 2);
        *p++ = L'-';
        put_hex(p, id + 10, 6);
    }
    uint32_t pos = guid_len + 1;
    bool listed = false;
    Array<wchar_t> buf{ValueInit, len + 1};
    for (const wchar_t* g = list.data(); g && *g; )
    {
        const auto n = uint32_t(std::wcslen(g) + 1);
        const auto state = check_child(children, g, buf, path.data(), len);
        if (state == child_state::match)
            return entry::already_set;
        if (CompareStringOrdinal(g, -1, guid, (int)guid_len, TRUE) == CSTR_EQUAL)
            listed = true;
        else if (state == child_state::other)
        {
            std::memcpy(new_list.data() + pos, g, n * sizeof(wchar_t));
            pos += n;
        }
        g += n;
    }

    // Entries sealed with the user's key instead were ignored.
    auto* const protect = crypt_protect_data();
    DATA_BLOB in{name_bytes, (BYTE*)name}, blob{};
    if (!protect || !protect(&in, nullptr, nullptr, nullptr, nullptr,
                             CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN, &blob))
        return entry::failed;
    ScopeGuard blob_guard{blob.pbData, LocalFree};

    Array<wchar_t> dir{ValueInit, dir_len + 1};
    std::memcpy(dir.data(), path.data(), dir_len * sizeof(wchar_t));
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const uint64_t accessed = uint64_t{now.dwHighDateTime} << 32 | now.dwLowDateTime;
    constexpr DWORD type = 1, revision = 1, flags = 0x11;

    HKEY child;
    if (RegCreateKeyExW(children, guid, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &child, nullptr) != ERROR_SUCCESS)
        return entry::failed;
    bool ok = delete_value(child, L"Arguments") &&
              set_value(child, L"Type", REG_DWORD, &type, sizeof type) &&
              set_value(child, L"Revision", REG_DWORD, &revision, sizeof revision) &&
              set_value(child, L"Flags", REG_DWORD, &flags, sizeof flags) &&
              set_value(child, L"Parent", REG_BINARY, blob.pbData, blob.cbData) &&
              set_value(child, L"ExeParentDirectory", REG_SZ, dir.data(), (dir_len + 1) * sizeof(wchar_t)) &&
              set_value(child, L"MatchedExeFullPath", REG_SZ, path.data(), (len + 1) * sizeof(wchar_t)) &&
              set_value(child, L"LastAccessed", REG_QWORD, &accessed, sizeof accessed);
    RegCloseKey(child);

    // Listed only once complete: a listed GUID without its Children key reportedly makes Windows
    // ignore every child of that parent.
    if (HKEY parent; ok && RegCreateKeyExW(parents, parent_name, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &parent, nullptr) == ERROR_SUCCESS)
    {
        ok = set_value(parent, L"Children", REG_MULTI_SZ, new_list.data(), (pos + 1) * sizeof(wchar_t));
        RegCloseKey(parent);
    }
    else
        ok = false;
    if (!ok)
    {
        // Still listed, so deleting it would hide every child, as above.
        if (!listed)
            RegDeleteKeyW(children, guid);
        return entry::failed;
    }
    return entry::added;
}

Optional<int> with_game_mode::relaunch() noexcept
{
    constexpr const wchar_t* guard = L"_FLOORMAT_ALREADY_SET_AS_GAME";
    // Counts the NUL, so 1 is an empty value.
    if (GetEnvironmentVariableW(guard, nullptr, 0) > 1)
        return {};
    // The debugger would stay on the parent, which only waits.
    if (IsDebuggerPresent())
        return {};

    Array<wchar_t> path{ValueInit, 32768};
    const uint32_t len = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    if (!len || len == path.size())
        return {};
    // CreateProcessW may write to the command line it's given.
    const wchar_t* const cmdline = GetCommandLineW();
    const auto cmd_size = uint32_t(std::wcslen(cmdline) + 1);
    Array<wchar_t> cmd{NoInit, cmd_size};
    std::memcpy(cmd.data(), cmdline, cmd_size * sizeof(wchar_t));

    STARTUPINFOW si;
    GetStartupInfoW(&si);
    si.lpReserved = nullptr;
    // The CRT's table of inherited file descriptors, whose handles the parent may have closed.
    si.cbReserved2 = 0;
    si.lpReserved2 = nullptr;

    // Killing the parent kills the child, as with execve() there'd be one process to kill.
    // The handle stays open until the parent exits.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits))
        {
            CloseHandle(job);
            job = nullptr;
        }
    }

    PROCESS_INFORMATION pi;
    SetEnvironmentVariableW(guard, L"1");
    // Suspended until it's in the job.
    if (!CreateProcessW(path.data(), cmd.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED,
                        nullptr, nullptr, &si, &pi))
    {
        SetEnvironmentVariableW(guard, nullptr);
        if (job)
            CloseHandle(job);
        return {};
    }
    if (job)
        AssignProcessToJobObject(job, pi.hProcess);
    // Only now: the child would inherit it. Ctrl+C reaches both, and the child decides.
    SetConsoleCtrlHandler(nullptr, TRUE);
    DBG << "game-mode: relaunched as pid" << pi.dwProcessId;
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD status = 1;
    GetExitCodeProcess(pi.hProcess, &status);
    CloseHandle(pi.hProcess);
    return (int)status;
}

#else

with_game_mode::with_game_mode() noexcept {}
auto with_game_mode::designate() noexcept -> entry { return entry::failed; }
Optional<int> with_game_mode::relaunch() noexcept { return {}; }

#endif

} // namespace floormat
