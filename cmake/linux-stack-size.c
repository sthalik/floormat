#include <sys/resource.h>
#include <stdio.h>

#ifdef __SANITIZE_ADDRESS__
#define fm_ASAN 1
#elif defined __has_feature
#if __has_feature(address_sanitizer)
#define fm_ASAN 1
#endif
#endif
#ifndef fm_ASAN
#define fm_ASAN 0
#endif

static void set_stack_size(void)
{
    struct rlimit limit;
    if (getrlimit(RLIMIT_STACK, &limit) != 0)
    {
#if !fm_ASAN
        perror("floormat: getrlimit(RLIMIT_STACK)");
#endif
        return;
    }
    if (limit.rlim_cur >= FLOORMAT_STACK_SIZE)
        return;

    limit.rlim_cur = FLOORMAT_STACK_SIZE;
    if (setrlimit(RLIMIT_STACK, &limit) != 0)
    {
#if !fm_ASAN
        perror("floormat: setrlimit(RLIMIT_STACK)");
#endif
    }
}

__attribute__((section(".preinit_array"), used))
static void (*const stack_size_at_startup)(void) = set_stack_size;
