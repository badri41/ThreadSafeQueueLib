#ifndef LOCKFREE_SPSC_UNBOUNDED_DEFS
#define LOCKFREE_SPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace tsfqueue::impl {
template <typename T, typename Allocator = std::allocator<T>> class lockfree_spsc_unbounded {
	static_assert(std::is_object_v<T>, "T must be an object type");
	static_assert(!std::is_reference_v<T>, "Queue cannot store reference types");
	static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
	static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
				  "T must be move or copy constructible");
	static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
				  "T must be move or copy assignable");
	static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

	using node = tsfqueue::utils::Lockless_Node<T>;

	using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
	using node_alloc_traits = std::allocator_traits<node_allocator>;

	// Works exactly same as the blocking_mpmc_unbounded queue (see this once)
	// with tail pointer pointing to stub node and your head pointer updates as
	// per the pushes. See the Lockless_Node in utils to understand the working.
	// Note that the next pointers are atomic there. Why ?? [Reason this]
	// Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
	// me for details)

	public:
	// Public member functions :
	// Add relevant constructors and destructors -> Add these here only
	lockfree_spsc_unbounded();

	lockfree_spsc_unbounded(const lockfree_spsc_unbounded &) = delete;
	lockfree_spsc_unbounded &operator=(const lockfree_spsc_unbounded &) = delete;
	
	lockfree_spsc_unbounded(lockfree_spsc_unbounded &&other) noexcept;

	lockfree_spsc_unbounded &operator=(lockfree_spsc_unbounded &&other) noexcept;

	~lockfree_spsc_unbounded();

	void swap(lockfree_spsc_unbounded &other) noexcept;

	void push(T value); // Pushes the value inside the queue, copies the value

	template <typename... Args>
	void emplace(Args &&...args);

	void wait_and_pop(T& value); // : Blocking wait on queue, returns value in
	// the reference passed as parameter
	bool try_pop(T& value); // : Returns true and
	// gives the value in reference passed, false otherwise
	[[nodiscard]] bool empty() const; // : Returns
	// whether the queue is empty or not at that instant
	[[nodiscard]] bool peek(T& value) const; // : Returns the front/top element of queue in ref (false if empty queue)
	// 6. Add static asserts
	// 7. Add emplace_back using perfect forwarding and variadic templates (you
	// can use this in push then)
	[[nodiscard]] size_t size() const noexcept;
	// 8. Add size() function
	// 9. Any more suggestions ??
	// 10. Why no shared_ptr ?? [Reason this]

private:
	node *allocate_node_();
	void deallocate_node_(node *p) noexcept;

	// Add the private members :
	node_allocator alloc_{};

	alignas(cache_line_size) node* head_{}; //This is a spsc queue so, there is no data race between multiple consumer threads and hence 
				// head not need be declared atomic.
	alignas(cache_line_size) 
	std::atomic<node*> tail_{}; //tail is required to be atomic as producer changes the tail while pushing
							// and consumer may try to read the tail while checking if the queue is empty.
							// If tail was not atomic this might lead to data race, that is while a consumer tries
							// to read it; the tail might get updated by the producer but the consumer gets the old value.


	// Description of private members :
	// 1. node* head -> Pointer to the head node
	// 2. node* tail -> Pointer to tail node
	// 3. Cache align 1-2
	alignas(cache_line_size) std::atomic<size_t> size_{0};

};
} // namespace tsfqueue::impl

#endif 