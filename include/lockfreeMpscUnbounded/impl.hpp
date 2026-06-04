#ifndef LOCKFREE_MPSC_UNBOUNDED_IMPL
#define LOCKFREE_MPSC_UNBOUNDED_IMPL

#include "defs.hpp"
#include <thread>
#include <utility>

namespace tsfqueue::impl {
    template<typename T, typename Allocator, bool TrackMetrics>
    lockfreeMpscUnbounded<T,Allocator,TrackMetrics>::lockfreeMpscUnbounded(){
        node* stub = allocateNode();
        head = stub;
        tail.store(stub, std::memory_order_relaxed);
        
        if constexpr (TrackMetrics) {
            size.store(0, std::memory_order_relaxed);
            maxSizeVal.store(0, std::memory_order_relaxed);
        }
    }

    template<typename T, typename Allocator, bool TrackMetrics>
    lockfreeMpscUnbounded<T,Allocator,TrackMetrics>::lockfreeMpscUnbounded(
        lockfreeMpscUnbounded &&other) noexcept
        : lockfreeMpscUnbounded(){
            swap(other);
        }

    template <typename T, typename Allocator, bool TrackMetrics>
    lockfreeMpscUnbounded<T, Allocator, TrackMetrics> &
    lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::operator=(
    lockfreeMpscUnbounded &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        lockfreeMpscUnbounded tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    template<typename T, typename Allocator, bool TrackMetrics>
    lockfreeMpscUnbounded<T,Allocator,TrackMetrics>::~lockfreeMpscUnbounded(){
        node* curr = head;
        while(curr != nullptr){
            node* nxt = curr->next.load(std::memory_order_relaxed);
            nodeAllocTraits::destroy(alloc, curr);
            nodeAllocTraits::deallocate(alloc, curr, 1);
            curr = nxt;
        }
    }

    template<typename T, typename Allocator, bool TrackMetrics>
    void lockfreeMpscUnbounded<T,Allocator,TrackMetrics>::swap(
        lockfreeMpscUnbounded &other) noexcept {
            using std::swap;
            swap(alloc, other.alloc);
            swap(head, other.head);
            
            node* thisTail = tail.load(std::memory_order_relaxed);
            node* otherTail = other.tail.load(std::memory_order_relaxed);
            tail.store(otherTail, std::memory_order_relaxed);
            other.tail.store(thisTail, std::memory_order_relaxed);

            if constexpr (TrackMetrics) {
                size_t thisSize = size.load(std::memory_order_relaxed);
                size_t otherSize = other.size.load(std::memory_order_relaxed);
                size.store(otherSize, std::memory_order_relaxed);
                other.size.store(thisSize, std::memory_order_relaxed);

                size_t thisMax = maxSizeVal.load(std::memory_order_relaxed);
                size_t otherMax = other.maxSizeVal.load(std::memory_order_relaxed);
                maxSizeVal.store(otherMax, std::memory_order_relaxed);
                other.maxSizeVal.store(thisMax, std::memory_order_relaxed);
            }
        }
    
    template <typename T, typename Allocator, bool TrackMetrics>
    typename lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::node *
    lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::allocateNode() {
        node *p = nodeAllocTraits::allocate(alloc, 1);
        nodeAllocTraits::construct(alloc, p);
        p->next.store(nullptr, std::memory_order_relaxed);
        return p;
   }

   template <typename T, typename Allocator, bool TrackMetrics>
   void lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::deallocateNode(node *p) noexcept {
        nodeAllocTraits::destroy(alloc, p);
        nodeAllocTraits::deallocate(alloc, p, 1);
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  void lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::push(T value) {
    emplace(std::move(value));
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  template <typename... Args>
  void lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::emplace(Args &&...args) {
    node *newStub = allocateNode();
    node *oldTail = tail.exchange(newStub, std::memory_order_acq_rel);
    oldTail->data = T(std::forward<Args>(args)...);
    oldTail->next.store(newStub, std::memory_order_release);
    
    if constexpr (TrackMetrics) {
        size.fetch_add(1, std::memory_order_relaxed);
    }
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  bool lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::tryPop(T& value){
    node *oldHead = head;
    node *nxt = oldHead->next.load(std::memory_order_acquire);
    if(nxt == nullptr) return false;
    value = std::move(oldHead->data);
    head = nxt;
    deallocateNode(oldHead);
    
    if constexpr (TrackMetrics) {
        size_t cSize = size.fetch_sub(1, std::memory_order_relaxed) - 1;
        size_t mSize = maxSizeVal.load(std::memory_order_relaxed);
        while (cSize > mSize && !maxSizeVal.compare_exchange_weak(mSize, cSize, 
               std::memory_order_relaxed, std::memory_order_relaxed));
    }
    return true;
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  void lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::waitAndPop(T &value) {
    while (!tryPop(value)) {
        std::this_thread::yield();
    }
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  bool lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::peek(T &value) const { 
    static_assert(std::is_copy_assignable_v<T>, "peek() requires T to be copy assignable");
    node *currentHead = head;
    node *next = currentHead->next.load(std::memory_order_acquire);
    if (next == nullptr) {
        return false;
    }
    value = currentHead->data;
    return true;
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  bool lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::empty() const {
    return head->next.load(std::memory_order_acquire) == nullptr;
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  size_t lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::getSize() const noexcept {
    if constexpr (TrackMetrics) {
        return size.load(std::memory_order_relaxed);
    }
    return 0;
  }

  template <typename T, typename Allocator, bool TrackMetrics>
  size_t lockfreeMpscUnbounded<T, Allocator, TrackMetrics>::maxSize() const noexcept {
    if constexpr (TrackMetrics) {
        return maxSizeVal.load(std::memory_order_relaxed);
    }
    return 0;
  }

}

#endif