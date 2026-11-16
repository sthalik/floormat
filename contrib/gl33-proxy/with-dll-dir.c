// with-dll-dir DIR PROGRAM [ARGS...] runs PROGRAM after SetDllDirectory(DIR), which the loader also applies to
// PROGRAM's load-time imports, so they find DIR's DLLs before System32's. Rebuild it with build.sh.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static BOOL WINAPI ignore_ctrl(DWORD type)
{
    (void)type;
    return TRUE;
}

static wchar_t* split_arg(wchar_t* s, wchar_t** arg)
{
    if (*s == L'"')
    {
        *arg = ++s;
        while (*s && *s != L'"')
            s++;
    }
    else
    {
        *arg = s;
        while (*s && *s != L' ' && *s != L'\t')
            s++;
    }
    if (*s)
        *s++ = L'\0';
    while (*s == L' ' || *s == L'\t')
        s++;
    return s;
}

int main(void)
{
    wchar_t *self, *dir;
    wchar_t* cmdline = split_arg(split_arg(GetCommandLineW(), &self), &dir);
    if (!*dir || !*cmdline)
    {
        fputs("usage: with-dll-dir DIR PROGRAM [ARGS...]\n", stderr);
        return 2;
    }
    const DWORD attributes = GetFileAttributesW(dir);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        fprintf(stderr, "with-dll-dir: %ls isn't a directory\n", dir);
        return 2;
    }
    if (!SetDllDirectoryW(dir))
    {
        fprintf(stderr, "with-dll-dir: SetDllDirectory failed, error %lu\n", GetLastError());
        return 127;
    }

    SetConsoleCtrlHandler(ignore_ctrl, TRUE);
    STARTUPINFOW si = { .cb = sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi))
    {
        fprintf(stderr, "with-dll-dir: CreateProcess failed, error %lu\n", GetLastError());
        return 127;
    }
    DWORD exit_code = 127;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    return (int)exit_code;
}
