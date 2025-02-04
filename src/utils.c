/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <stdarg.h>

#include "utils.h"
#include "iodev.h"
#include "smp.h"
#include "types.h"
#include "vsprintf.h"

static char ascii(char s)
{
    if (s < 0x20)
        return '.';
    if (s > 0x7E)
        return '.';
    return s;
}

void hexdump(const void *d, size_t len)
{
    u8 *data;
    size_t i, off;
    data = (u8 *)d;
    for (off = 0; off < len; off += 16) {
        printf("%08lx  ", off);
        for (i = 0; i < 16; i++) {
            if ((i + off) >= len)
                printf("   ");
            else
                printf("%02x ", data[off + i]);
        }

        printf(" ");
        for (i = 0; i < 16; i++) {
            if ((i + off) >= len)
                printf(" ");
            else
                printf("%c", ascii(data[off + i]));
        }
        printf("\n");
    }
}

void regdump(u64 addr, size_t len)
{
    u64 i, off;
    for (off = 0; off < len; off += 32) {
        printf("%016lx  ", addr + off);
        for (i = 0; i < 32; i += 4) {
            printf("%08x ", read32(addr + off + i));
        }
        printf("\n");
    }
}

int sprintf(char *buffer, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buffer, fmt, args);
    va_end(args);
    return i;
}

int debug_printf(const char *fmt, ...)
{
    va_list args;
    char buffer[512];
    int i;

    va_start(args, fmt);
    i = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    iodev_console_write(buffer, min(i, (int)(sizeof(buffer) - 1)));

    return i;
}

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function)
{
    printf("Assertion failed: '%s' on %s:%d:%s\n", assertion, file, line, function);
    flush_and_reboot();
}

#define AIC_TIMER 0x23b108020

void udelay(u32 d)
{
    u32 delay = d * 24;
    u32 val = read32(AIC_TIMER);
    while ((read32(AIC_TIMER) - val) < delay)
        ;
}

void flush_and_reboot(void)
{
    iodev_console_flush();
    reboot();
}

void spin_lock(spinlock_t *lock)
{
    s64 me = smp_id();
    if (__atomic_load_n(&lock->lock, __ATOMIC_ACQUIRE) == me) {
        lock->count++;
        return;
    }

    s64 free = -1;

    while (!__atomic_compare_exchange_n(&lock->lock, &free, me, false, __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED))
        free = -1;

    assert(__atomic_load_n(&lock->lock, __ATOMIC_RELAXED) == me);
    lock->count++;
}

void spin_unlock(spinlock_t *lock)
{
    s64 me = smp_id();
    assert(__atomic_load_n(&lock->lock, __ATOMIC_RELAXED) == me);
    assert(lock->count > 0);
    if (!--lock->count)
        __atomic_store_n(&lock->lock, -1L, __ATOMIC_RELEASE);
}
