#pragma once

// init file
#define cache_line_size 64

#define atomic_int int

template <class T>
struct wrap
{
    T val;
};