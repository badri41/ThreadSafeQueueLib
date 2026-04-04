#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::lockfree_spsc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {
    node* temp = tail.load(std::memory_order_relaxed);
    temp->data = std::move(value);
    new_tail = new node();
    new_tail->next.store(nullptr,std::memory_order_relaxed);
    temp->next.store(new_tail,std::memory_order_release);
    tail.store(new_tail,std::memory_order_relaxed);
}
// head -> stub <-tail

template <typename T> bool queue<T>::try_pop(T &value) {
    node* old=head;
    node* nxt = old->next.load(std::memory_order_acquire);
    if(nxt==nullptr) return false;
    value = std::move(old->data);
    head = nxt;
    delete old;
    return true;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {}

template <typename T> bool queue<T>::peek(T &value) {}

template <typename T> bool queue<T>::empty(void) {}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??