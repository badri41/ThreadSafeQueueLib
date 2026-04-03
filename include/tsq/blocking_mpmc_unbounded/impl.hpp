#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {
    std::shared_ptr<T> temp = std::make_shared<T>(std::move(value));
    std::unique_ptr<node> t = std::make_unique<node>();
    std::lock_guard<std::mutex> lock(tail_mutex);
    tail->data = temp;
    tail->next = std::move(t);
    tail = (tail->next).get();
    ++sz;
    //lock.unlock();
    cond.notify_one();
}

template <typename T> queue<T>::node *queue<T>::get_tail() {
    std::lock_guard<std::mutex> lock(tail_mutex);
    return tail;
    // This is a private function because we cant allow user to use this function 
    // because it is returning a pointer. The user might modify the pointer and also
    // This might lead to race conditons, since after the function runs the user
    // has the access to the pointer but the tail_mutex is not locked
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get() {
     std::unique_lock<std::mutex> lock(head_mutex);
     cond.wait(lock,[this]{ return head.get()!= get_tail();});
     //We use unique lock because we want extra control over the lock to unlock and relock it again.
     std::unique_ptr<node> old = std::move(head);
     head = std::move(old->next);
     return old;
     // head -> node -> node -> node ->node ----
     // pop : old -> node -> node -> node ---
     //                  head-|
}

template <typename T> std::unique_ptr<typename queue<T>::node> queue<T>::try_get() {
    std::lock_guard<std::mutex> lock(head_mutex);
    if(head.get()==get_tail()){
        return std::unique_ptr<node>();
    }
    std::unique_ptr<node> old = std::move(head);
    head = std::move(old->next);
    return old;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {
    std::unique_ptr<node> old = wait_and_get();
    value = std::move(*(old->data));
    --sz;
}

template <typename T> std::shared_ptr<T> queue<T>::wait_and_pop() {
    std::unique_ptr<node> old = wait_and_get();
    --sz;
    return old->data;
}

template <typename T> bool queue<T>::try_pop(T &value) {
    std::unique_ptr<node> old = std::move(try_get());
    if(!old) return false;
    value = std::move(*(old->data));
    --sz;
    return true;
}

template <typename T> std::shared_ptr<T> queue<T>::try_pop() {
    std::unique_ptr<node> old = std::move(try_get());
    if(!old) return nullptr;
    --sz;
    return old->data;
}

template <typename T> bool queue<T>::empty() {
    std::lock_guard<std::mutex> lock(head_mutex);
    return (head.get()==get_tail());
}         
// head ->  dummy <- tail
template <typename T> size_t queue<T>::size() const{
    return sz.load(std::memory_order_relaxed);
} 


#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ?? 


