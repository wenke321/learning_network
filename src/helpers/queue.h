#pragma once

#include <sched.h>

#include <cassert>
#include <cstdint>

#include "builtins.h"

template <typename T>
class mpmc_bounded_Queue
{
   public:
    mpmc_bounded_Queue(uint64_t buffer_size_) : buffer(new node[buffer_size_]), buffer_size(buffer_size_), buffer_mask(buffer_size_ - 1)
    {
        assert(buffer_size_ >= 2 && ((buffer_size_ & buffer_mask) == 0));

        for (uint64_t i = 0; i < buffer_size_; i++)
        {
            buffer[i].sequence = i;
        }
        _store_relaxed(&head_pos, 0);
        _store_relaxed(&tail_pos, 0);
    }
    ~mpmc_bounded_Queue() { delete[] buffer; }

    bool empty()
    {
        uint64_t h = _load_acquire(&head_pos), t = _load_acquire(&tail_pos);
        return h == t;
    }

    bool size()
    {
        uint64_t h = _load_acquire(&head_pos), t = _load_acquire(&tail_pos);
        return t - h;
    }

    void push(T& _data)
    {
        for (int i = 1, r = 0; i < 65; ++i)
        {
            if (try_push(_data)) return;
            if (i & 64)
            {
                ++r;
                i = 1 << r;
                sched_yield();
            }
        }
    }

    void pop(T& _data)
    {
        for (int i = 1, r = 0; i < 65; ++i)
        {
            if (try_pop(_data)) return;
            if (i & 64)
            {
                ++r;
                i = 1 << r;
                sched_yield();
            }
        }
    }

    bool try_push(T& data)
    {
        node* new_data;
        uint64_t pos = _load_relaxed(&tail_pos);
        uint64_t seq;

        for (;;)
        {
            new_data = &buffer[pos & buffer_mask];
            seq      = _load_acquire(&new_data->sequence);

            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                if (_CAS_weak_relaxed(&tail_pos, &pos, pos + 1)) break;
            }
            else if (dif < 0)
                return false;
            else
                pos = _load_relaxed(&tail_pos);
        }

        new_data->data = data;
        _store_release(&new_data->sequence, pos + 1);
        return true;
    }

    bool try_pop(T& data)
    {
        node* now_data;
        uint64_t pos = _load_relaxed(&head_pos);
        uint64_t seq;

        for (;;)
        {
            now_data = &buffer[pos & buffer_mask];
            seq      = _load_acquire(&now_data->sequence);

            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                if (_CAS_weak_relaxed(&head_pos, &pos, pos + 1)) break;
            }
            else if (dif < 0)
                return false;
            else
                pos = _load_relaxed(&head_pos);
        }

        data = now_data->data;
        _store_release(&now_data->sequence, pos + buffer_mask + 1);
        return true;
    }

   private:
    struct node
    {
        volatile uint64_t sequence;
        T data;
    };

    static constexpr uint8_t cachelinesize =
#ifdef __cpp_lib_hardware_interference_size
        std::hardware_constructive_interference_size;
#else
        64;
#endif
    char pad0[cachelinesize];
    node* const buffer;
    uint64_t buffer_size;
    uint64_t buffer_mask;
    char pad1[cachelinesize];
    volatile uint64_t head_pos;
    char pad2[cachelinesize - sizeof(head_pos)];
    volatile uint64_t tail_pos;
    char pad3[cachelinesize - sizeof(tail_pos)];
};
