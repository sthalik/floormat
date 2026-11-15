// run-gl33.sh puts this in place of Mesa's opengl32.dll, renamed opengl32-mesa.dll. Rebuild it with build.sh.
// GL functions the FLOORMAT_GL33_ALLOWED_FUNCTIONS file lacks resolve to a stub that prints their name and exits.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined __x86_64__ && !defined _M_X64
#error "the stubs are x86-64 machine code"
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef unsigned char GLubyte;
typedef short GLshort;
typedef unsigned short GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef long long GLint64;
typedef unsigned long long GLuint64;
typedef struct __GLsync* GLsync;

typedef PROC (WINAPI *get_proc_address_fn)(LPCSTR);

struct wrapper
{
    const char* name;
    PROC wrapped;
    void* real;
};

static get_proc_address_fn mesa_get_proc_address;
static const char* allowed_functions_path;
static const char** allowed_functions;
static size_t allowed_function_count;
static const char* allowed_enums_path;
static GLenum* allowed_enums;
static size_t allowed_enum_count;
static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;

enum { stub_slot_size = 32, stub_block_size = 65536 };
static SRWLOCK stub_lock = SRWLOCK_INIT;
static unsigned char* stub_block;
static size_t stub_block_used = stub_block_size;

enum { max_reported = 256 };
static SRWLOCK report_lock = SRWLOCK_INIT;
static struct { const char* function; const char* param; GLenum value; } reported[max_reported];
static size_t reported_count;

static void fail(const char* format, const char* arg)
{
    fputs("gl33-proxy: ", stderr);
    fprintf(stderr, format, arg);
    fputc('\n', stderr);
    fflush(stderr);
    ExitProcess(3);
}

static int compare_names(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static int compare_enums(const void* a, const void* b)
{
    const GLenum x = *(const GLenum*)a, y = *(const GLenum*)b;
    return (x > y) - (x < y);
}

static char** read_lines(const char* variable, const char** path_out, size_t* count_out)
{
    const char* path = getenv(variable);
    if (!path || !*path)
        fail("%s isn't set", variable);
    *path_out = _strdup(path);
    FILE* f = fopen(path, "rb");
    if (!*path_out || !f)
        fail("can't open %s", path);
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size)
        fail("can't read %s", path);
    fclose(f);
    text[size] = '\0';

    size_t max_count = 1;
    for (long i = 0; i < size; i++)
        if (text[i] == '\n')
            max_count++;
    char** lines = malloc(max_count * sizeof *lines);
    if (!lines)
        fail("out of memory reading %s", path);
    size_t count = 0;
    for (char* line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n"))
        lines[count++] = line;
    *count_out = count;
    return lines;
}

static BOOL CALLBACK init(PINIT_ONCE once, PVOID param, PVOID* context);
static void ensure_init(void);
static void check_enum(const char* function, const char* param, GLenum value);

#include "wrappers.h"

static BOOL CALLBACK init(PINIT_ONCE once, PVOID param, PVOID* context)
{
    (void)once; (void)param; (void)context;

    HMODULE mesa = LoadLibraryA("opengl32-mesa.dll");
    if (!mesa)
        fail("can't load %s", "opengl32-mesa.dll");
    mesa_get_proc_address = (get_proc_address_fn)(void*)GetProcAddress(mesa, "wglGetProcAddress");
    if (!mesa_get_proc_address)
        fail("%s has no wglGetProcAddress", "opengl32-mesa.dll");

    allowed_functions = (const char**)read_lines("FLOORMAT_GL33_ALLOWED_FUNCTIONS", &allowed_functions_path,
                                                 &allowed_function_count);
    qsort(allowed_functions, allowed_function_count, sizeof *allowed_functions, compare_names);

    size_t line_count;
    char** lines = read_lines("FLOORMAT_GL33_ALLOWED_ENUMS", &allowed_enums_path, &line_count);
    allowed_enums = malloc((line_count + 1) * sizeof *allowed_enums);
    if (!allowed_enums)
        fail("out of memory reading %s", allowed_enums_path);
    for (size_t i = 0; i < line_count; i++)
    {
        const char* value = strchr(lines[i], ' ');
        char* end;
        const unsigned long long x = value ? strtoull(value + 1, &end, 0) : 0;
        if (!value || end == value + 1 || *end)
            fail("can't read the line \"%s\", expected a name and a value", lines[i]);
        if (x <= 0xffffffffu)
            allowed_enums[allowed_enum_count++] = (GLenum)x;
    }
    qsort(allowed_enums, allowed_enum_count, sizeof *allowed_enums, compare_enums);

    qsort(wrappers, sizeof wrappers / sizeof *wrappers, sizeof *wrappers, compare_names);
    for (size_t i = 0; i < sizeof wrappers / sizeof *wrappers; i++)
    {
        PROC real = GetProcAddress(mesa, wrappers[i].name);
        memcpy(wrappers[i].real, &real, sizeof real);
    }

    fprintf(stderr, "gl33-proxy: %zu GL functions allowed by %s\n", allowed_function_count,
            allowed_functions_path);
    fprintf(stderr, "gl33-proxy: %zu enum values allowed by %s\n", allowed_enum_count, allowed_enums_path);
    fflush(stderr);
    return TRUE;
}

static void ensure_init(void)
{
    InitOnceExecuteOnce(&init_once, init, NULL, NULL);
}

static void check_enum(const char* function, const char* param, GLenum value)
{
    if (bsearch(&value, allowed_enums, allowed_enum_count, sizeof *allowed_enums, compare_enums))
        return;

    AcquireSRWLockExclusive(&report_lock);
    size_t i = 0;
    while (i < reported_count && (reported[i].value != value || strcmp(reported[i].function, function) ||
                                  strcmp(reported[i].param, param)))
        i++;
    if (i == reported_count)
    {
        if (reported_count < max_reported)
        {
            reported[reported_count].function = function;
            reported[reported_count].param = param;
            reported[reported_count++].value = value;
        }
        fprintf(stderr, "gl33-proxy: %s got %s = 0x%04X, but %s doesn't allow it\n", function, param, value,
                allowed_enums_path);
        fflush(stderr);
    }
    ReleaseSRWLockExclusive(&report_lock);
}

static void report_hidden_call(const char* name)
{
    fprintf(stderr, "gl33-proxy: %s was called, but %s doesn't allow it\n", name, allowed_functions_path);
    fflush(stderr);
    ExitProcess(3);
}

static PROC make_stub(LPCSTR name)
{
    // mov rcx, name_copy / mov rax, report_hidden_call / jmp rax
    static const unsigned char code[22] = { 0x48, 0xb9, [10] = 0x48, 0xb8, [20] = 0xff, 0xe0 };
    void (*report)(const char*) = report_hidden_call;
    char* name_copy = _strdup(name);
    if (!name_copy)
        fail("out of memory making the stub of %s", name);

    AcquireSRWLockExclusive(&stub_lock);
    if (stub_block_used + stub_slot_size > stub_block_size)
    {
        stub_block = VirtualAlloc(NULL, stub_block_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!stub_block)
            fail("can't allocate executable memory for the stub of %s", name);
        stub_block_used = 0;
    }
    // -fsanitize=function reads the 8 bytes before the target of every instrumented indirect call.
    unsigned char* stub = stub_block + stub_block_used + 8;
    stub_block_used += stub_slot_size;
    ReleaseSRWLockExclusive(&stub_lock);

    memcpy(stub, code, sizeof code);
    memcpy(stub + 2, &name_copy, sizeof name_copy);
    memcpy(stub + 12, &report, sizeof report);
    FlushInstructionCache(GetCurrentProcess(), stub, sizeof code);
    return (PROC)(void*)stub;
}

PROC WINAPI proxy_wglGetProcAddress(LPCSTR name)
{
    ensure_init();
    if (name[0] != 'g' || name[1] != 'l')
        return mesa_get_proc_address(name);
    if (!bsearch(&name, allowed_functions, allowed_function_count, sizeof *allowed_functions, compare_names))
        return make_stub(name);

    PROC real = mesa_get_proc_address(name);
    const struct wrapper* w = bsearch(&name, wrappers, sizeof wrappers / sizeof *wrappers, sizeof *wrappers,
                                      compare_names);
    if (!real || !w)
        return real;
    memcpy(w->real, &real, sizeof real);
    return w->wrapped;
}
