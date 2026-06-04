#ifndef LOCKFREE_SPSC_UNBOUNDED_DEFS
#define LOCKFREE_SPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace tsfqueue::impl {
template <typename T, typename Allocator = std::allocator<T>, bool TrackMetrics = false> 
class lockfreeSpscUnbounded {
    static_assert(std::is_object_v<T>, "T must be an object type");
    static_assert(!std::is_reference_v<T>, "Queue cannot store reference types");
    static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
    static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                  "T must be move or copy constructible");
    static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
                  "T must be move or copy assignable");
    static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

    using node = tsfqueue::utils::Lockless_Node<T>;

    using nodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
    using nodeAllocTraits = std::allocator_traits<nodeAllocator>;

    // Works exactly same as the blockingMpmcUnbounded queue (see this once)
    // with tail pointer pointing to stub node and your head pointer updates as
    // per the pushes. See the Lockless_Node in utils to understand the working.
    // Note that the next pointers are atomic there. Why ?? [Reason this]
    // Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
    // me for details)

    public:
    // Public member functions :
    // Add relevant constructors and destructors -> Add these here only
    lockfreeSpscUnbounded();

    lockfreeSpscUnbounded(const lockfreeSpscUnbounded &) = delete;
    lockfreeSpscUnbounded &operator=(const lockfreeSpscUnbounded &) = delete;
    
    lockfreeSpscUnbounded(lockfreeSpscUnbounded &&other) noexcept;

    lockfreeSpscUnbounded &operator=(lockfreeSpscUnbounded &&other) noexcept;

    ~lockfreeSpscUnbounded();

    void swap(lockfreeSpscUnbounded &other) noexcept;

    void push(T value); // Pushes the value inside the queue, copies the value

    template <typename... Args>
    void emplace(Args &&...args);

    void waitAndPop(T& value); // : Blocking wait on queue, returns value in
    // the reference passed as parameter
    bool tryPop(T& value); // : Returns true and
    // gives the value in reference passed, false otherwise
    [[nodiscard]] bool empty() const; // : Returns
    // whether the queue is empty or not at that instant
    [[nodiscard]] bool peek(T& value) const; // : Returns the front/top element of queue in ref (false if empty queue). Fails to compile for move-only types.
    // 6. Add static asserts
    // 7. Add emplace_back using perfect forwarding and variadic templates (you
    // can use this in push then)
    
    [[nodiscard]] size_t size() const noexcept;
    
    [[nodiscard]] size_t maxSize() const noexcept;
    // 8. Add size() function
    // 9. Any more suggestions ??
    // 10. Why no shared_ptr ?? [Reason this]

private:
    node *allocateNode_();
    void deallocateNode_(node *p) noexcept;

    // Add the private members :
    nodeAllocator alloc_{};

    // Reverted to simply cacheLineSize
    alignas(cacheLineSize) node* head_{}; //This is a spsc queue so, there is no data race between multiple consumer threads and hence 
                // head not need be declared atomic.
    alignas(cacheLineSize) node* tail_{}; // tail does not need to be atomic because it is only accessed and modified by the producer thread.
                            // Cross-thread synchronization is handled by the atomic next pointers in the nodes themselves.


    // Description of private members :
    // 1. node* head -> Pointer to the head node
    // 2. node* tail -> Pointer to tail node
    // 3. Cache align 1-2
    alignas(cacheLineSize) std::atomic<size_t> size_{0};
    alignas(cacheLineSize) std::atomic<size_t> maxSizeVal_{0};
};
} // namespace tsfqueue::impl

#endif