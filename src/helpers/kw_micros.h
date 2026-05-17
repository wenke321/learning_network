#pragma once

#include <linux/futex.h>
#include <sys/syscall.h>

#define kw_debug 1;

#define kw_likely(expr)   __builtin_expect(!!expr, 1)
#define kw_unlikely(expr) __builtin_expect(!!expr, 0)

// both uint32_t
#define futex_wait(futex_word, expected) syscall(SYS_futex, futex_word, FUTEX_WAIT, expected, NULL)
// uint32_t
#define futex_wake(futex_word) syscall(SYS_futex, futex_word, FUTEX_WAKE, INT_MAX)