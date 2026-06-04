#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"
#include <thread>
#include <utility>

namespace tsfqueue::impl {

    template <typename T, typename Allocator, bool TrackMetrics>
    lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::lockfreeSpscUnbounded() {
        node* stub = allocateNode_();
        head_ = stub;
        tail_ = stub;

        if constexpr (TrackMetrics) {
            size_.store(0, std::memory_order_relaxed);
            maxSizeVal_.store(0, std::memory_order_relaxed);
        }
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::lockfreeSpscUnbounded(
        lockfreeSpscUnbounded &&other) noexcept 
        : lockfreeSpscUnbounded() {
        swap(other);
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    lockfreeSpscUnbounded<T, Allocator, TrackMetrics>& 
    lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::operator=(
        lockfreeSpscUnbounded &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        lockfreeSpscUnbounded tmp(std::move(other));
        swap(tmp);
        return *this;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::~lockfreeSpscUnbounded() {
        node* curr = head_;
        while (curr != nullptr) {
            node* nxt = curr->next.load(std::memory_order_relaxed);
            deallocateNode_(curr);
            curr = nxt;
        }
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    void lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::swap(
        lockfreeSpscUnbounded &other) noexcept {
        using std::swap;
        swap(alloc_, other.alloc_);
        swap(head_, other.head_);
        swap(tail_, other.tail_);

        if constexpr (TrackMetrics) {
            size_t thisSize = size_.load(std::memory_order_relaxed);
            size_t otherSize = other.size_.load(std::memory_order_relaxed);
            size_.store(otherSize, std::memory_order_relaxed);
            other.size_.store(thisSize, std::memory_order_relaxed);

            size_t thisMax = maxSizeVal_.load(std::memory_order_relaxed);
            size_t otherMax = other.maxSizeVal_.load(std::memory_order_relaxed);
            maxSizeVal_.store(otherMax, std::memory_order_relaxed);
            other.maxSizeVal_.store(thisMax, std::memory_order_relaxed);
        }
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    typename lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::node* lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::allocateNode_() {
        node* p = nodeAllocTraits::allocate(alloc_, 1);
        nodeAllocTraits::construct(alloc_, p);
        p->next.store(nullptr, std::memory_order_relaxed);
        return p;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    void lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::deallocateNode_(node *p) noexcept {
        nodeAllocTraits::destroy(alloc_, p);
        nodeAllocTraits::deallocate(alloc_, p, 1);
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    void lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::push(T value) {
        emplace(std::move(value));
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    template <typename... Args>
    void lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::emplace(Args &&...args) {
        node* newStub = allocateNode_();
        
        tail_->data = T(std::forward<Args>(args)...);
        tail_->next.store(newStub, std::memory_order_release);
        tail_ = newStub;

        if constexpr (TrackMetrics) {
            size_t currentSz = size_.fetch_add(1, std::memory_order_relaxed) + 1;
            size_t maxSz = maxSizeVal_.load(std::memory_order_relaxed);
            while (currentSz > maxSz && !maxSizeVal_.compare_exchange_weak(maxSz, currentSz, 
                   std::memory_order_relaxed, std::memory_order_relaxed));
        }
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    bool lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::tryPop(T& value) {
        node* oldHead = head_;
        node* nxt = oldHead->next.load(std::memory_order_acquire);
        
        if (nxt == nullptr) {
            return false; 
        }

        value = std::move(oldHead->data);
        head_ = nxt;
        deallocateNode_(oldHead);

        if constexpr (TrackMetrics) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }

        return true;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    void lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::waitAndPop(T &value) {
        while (!tryPop(value)) {
            std::this_thread::yield();
        }
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    bool lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::peek(T &value) const {
        static_assert(std::is_copy_assignable_v<T>, "peek() requires T to be copy assignable");
        
        node* currentHead = head_;
        node* nextNode = currentHead->next.load(std::memory_order_acquire);
        
        if (nextNode == nullptr) {
            return false;
        }
        
        value = currentHead->data;
        return true;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    bool lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::empty() const {
        return head_->next.load(std::memory_order_acquire) == nullptr;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    size_t lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::size() const noexcept {
        if constexpr (TrackMetrics) {
            return size_.load(std::memory_order_relaxed);
        }
        return 0;
    }

    template <typename T, typename Allocator, bool TrackMetrics>
    size_t lockfreeSpscUnbounded<T, Allocator, TrackMetrics>::maxSize() const noexcept {
        if constexpr (TrackMetrics) {
            return maxSizeVal_.load(std::memory_order_relaxed);
        }
        return 0;
    }

} // namespace tsfqueue::impl

#endif