#pragma once

// init file
#define cache_line_size 64

// atomics
#define atomic_ulong unsigned long long

template <class T>
struct wrap
{
    T val;
};