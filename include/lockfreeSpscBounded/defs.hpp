#ifndef LOCKFREE_spscBounded_DEFS
#define LOCKFREE_spscBounded_DEFS
#include "../utils.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
namespace tsfqueue::impl {
template <typename T, size_t Capacity> class lockfreeSpscBounded {
	static_assert(std::is_object_v<T>, "T must be an object type");
	static_assert(std::is_destructible_v<T>, "T must be destructible");
	static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
	static_assert(std::is_move_constructible_v<T>, "T must be move constructible");

	// For the implementation, we first take the size of the bounded queue from
	// user inside the templates so that we can do compile time memory allocation.
	// We have two atomic pointer, head and tail, tail for pushing the element and
	// head for popping. We also add check tail == head for empty which means one
	// redundant element during allocation. We keep headCache and tailCache as
	// cached copies to have a cache efficient code (discuss with me for details).
	// All the data members are cache aligned to prevent cache-line bouncing.
	// The user is provided with both set of functions : tryPop() and tryPush()
	// for a wait-free code And waitAndPop() and waitAndPush() for a lock-less
	// code but not wait-free variant. Thus, the user is given a choice to choose
	// among the preferred endpoints as per use case.
public:
	// Public Member functions :
	// Add appropriate constructors and destructors -> Add here only
	// 1. void waitAndPush(value) : Busy wait until element is pushed
	// 2. bool tryPush(value) : Try to push if not full else leave (returns false
	// if could not push else true)
	// 3. void waitAndPop(value ref) : Busy wait until we have atmost 1 elt and
	// then pop it and store in reference
	// 4. bool tryPop(value ref) : Try to pop and return false if failed bool
	// 5. empty(void) : Checks if the queue is empty and return bool
	// 6. bool peek(value ref) : Peek the top of the queue.
	// Will work only in spsc/mpsc why ?? [Reason this]
	// 8. Add emplace_back using perfect forwarding and variadic templates (you
	// can use this in push then)
	// 9. Add size() function
	// 10. Any more suggestions ??
	// can make the functions peek , empty and size const
	// 11. Why no shared_ptr ?? [Reason this]
	lockfreeSpscBounded() = default;
	lockfreeSpscBounded(const lockfreeSpscBounded &) = delete;
	lockfreeSpscBounded &operator=(const lockfreeSpscBounded &) = delete;
	lockfreeSpscBounded(lockfreeSpscBounded &&) = delete; // move constructor deleted...
	lockfreeSpscBounded &operator=(lockfreeSpscBounded &&) = delete; // move assignment operator deleted...
	~lockfreeSpscBounded();

	void waitAndPush(T value);
	bool tryPush(T value);
	void waitAndPop(T &value);
	bool tryPop(T &value);
	bool peek(T &value);

	template <typename... Args> bool emplace_back(Args &&...args); // emplace using variadic templates and perfect forwarding

	bool empty() const;
	size_t size() const;

private:
	// Add the private members :
	// alignas(cacheLineSize) std::atomic<size_t> head;
	// alignas(cacheLineSize) size_t tailCache;
	// alignas(cacheLineSize) std::atomic<size_t> tail;
	// alignas(cacheLineSize) size_t headCache;
	// static constexpr size_t capacity = Capacity + 1;
	// alignas(cacheLineSize) T arr[capacity];
	// aligned the start of the array too
	// Description of private members :
	// 1. std::atomic<size_t> head is the atomic head pointer
	// 2. std::atomic<size_t> tail is the atomic tail pointer
	// 3. size_t headCache is the cached head pointer
	// 4. size_t tailCache is the cached tail pointer
	// 5. T arr[] compile time allocated array
	// Cache align 1-5.
	// 6. static constexpr size_t capcity to store the capcity for operations in
	// functions Why static ?? Why constexpr ?? [Reason this]
	alignas(cacheLineSize) std::atomic<size_t> head{0};
	alignas(cacheLineSize) size_t tailCache{0};
	alignas(cacheLineSize) std::atomic<size_t> tail{0};
	alignas(cacheLineSize) size_t headCache{0};
	static constexpr size_t capacity = Capacity + 1; 

	// Use byte array to store the data to avoid unnecessary construction and destruction ... 
	alignas(cacheLineSize) alignas(T) std::byte arr[capacity * sizeof(T)]{};
		
	// Make these functions which expose raw pointers private for safety... 
	T *slotPtr(size_t idx) noexcept {
		return std::launder(reinterpret_cast<T *>(arr + (idx * sizeof(T))));
	}
};
} // namespace tsfqueue::impl
#endif
