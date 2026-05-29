#pragma once
#include <iostream>

#include "builtins.h"
#ifndef LIST_H
    #define LIST_H

namespace kw
{
template <class T>
class lockfree_list
{
   public:
    struct node
    {
        T val;
        volatile node* next;
    };

    lockfree_list() : head(new node), tail(new node) { _store_release(&head->next, tail); }

    void push(T _val)
    {
        node* new_next = new node(_val);

        for (;;)
        {
            node* old_next = _load_acquire(&head->next);
            new_next->next = old_next;

            if (!_CAS_strong_release(&head->next, &old_next, new_next))
            {
            }
        }
    }
    void push_front();

    void pop_front();
    void pop_back();

   private:
    volatile node* head;
    char pad1[64 - sizeof(head)];
    volatile node* tail;
};
}  // namespace kw

#endif