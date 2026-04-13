#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::impl::lockfree_spsc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {
    node* temp = tail.load(std::memory_order_relaxed);
    temp->data = std::move(value);
    node* new_tail = new node();
    new_tail->next.store(nullptr,std::memory_order_relaxed);
    temp->next.store(new_tail,std::memory_order_release);
    tail.store(new_tail,std::memory_order_relaxed);
    sz.fetch_add(1,std::memory_order_relaxed);
}
// head -> stub <-tail

template <typename T> bool queue<T>::try_pop(T &value) {
    node* old=head;
    node* nxt = old->next.load(std::memory_order_acquire);
    if(nxt==nullptr) return false;
    value = std::move(old->data);
    head = nxt;
    delete old;
    sz.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {
    while(!try_pop(value)){
        std::this_thread::yield(); //Yield execution to allow other threads to run 
    } 
}

template <typename T> bool queue<T>::peek(T &value) {//Not atomic as a whole, so it is not a proper peek operation in general except spsc. 
    // Lets' say the value of the first element was read by a thread and then the first element got popped from the
    // queue by some other thread. So then again the initial thread which did the peek will return the value of the new front element.
    // But it is safe in SPSC as only consumer thread has access to the front element, so the implementation is correct.
    if (empty()) {
        return false;
    }
    value=head->data;
    return true;

}

template <typename T> bool queue<T>::empty(void) {
    node* next = head->next.load(std::memory_order_acquire);
    if(next==nullptr){
        return true;
    }
    return false;
}

template <typename T> size_t queue<T>::size() const {
    return sz.load(std::memory_order_relaxed);
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??
