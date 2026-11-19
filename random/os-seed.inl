#pragma once
#include "os-seed.hpp"
#include "compat/assert.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#if __has_include(<sys/random.h>)
#include <sys/random.h>
#endif
#endif

namespace floormat::Random {

namespace {

#ifndef _WIN32
bool syscall_random(u8* out, u32 size)
{
#if defined __linux__ || defined __APPLE__ || defined __FreeBSD__ || defined __OpenBSD__ \
    || defined __NetBSD__ || defined __DragonFly__
    return getentropy(out, size) == 0;
#else
    (void)out;
    (void)size;
    return false;
#endif
}

bool read_urandom(u8* out, u32 size)
{
    const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    else
    {
        u32 done = 0;
        while (done < size)
        {
            const auto n = ::read(fd, out + done, size - done);
            if (n > 0)
                done += (u32)n;
            else if (n < 0 && errno == EINTR)
                continue;
            else
                break;
        }
        ::close(fd);
        return done == size;
    }
}
#endif

} // namespace

template<u32 N> void os_random_bytes(u8 (&out)[N])
{
    static_assert(N <= 256);
#ifdef _WIN32
    fm_assert(BCRYPT_SUCCESS(BCryptGenRandom(nullptr, out, N, BCRYPT_USE_SYSTEM_PREFERRED_RNG)));
#else
    fm_assert(syscall_random(out, N) || read_urandom(out, N));
#endif
}

} // namespace floormat::Random
