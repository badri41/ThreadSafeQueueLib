#ifndef LOCKFREE_SPSC_BOUNDED_DEFS
#define LOCKFREE_SPSC_BOUNDED_DEFS
#include "../utils.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
namespace tsfqueue::impl {
template <typename T, size_t Capacity> class lockfree_spsc_bounded {
  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(std::is_destructible_v<T>, "T must be destructible");

  // For the implementation, we first take the size of the bounded queue from
  // user inside the templates so that we can do compile time memory allocation.
  // We have two atomic pointer, head and tail, tail for pushing the element and
  // head for popping. We also add check tail == head for empty which means one
  // redundant element during allocation. We keep head_cache and tail_cache as
  // cached copies to have a cache efficient code (discuss with me for details).
  // All the data members are cache aligned to prevent cache-line bouncing.
  // The user is provided with both set of functions : try_pop() and try_push()
  // for a wait-free code And wait_and_pop() and wait_and_push() for a lock-less
  // code but not wait-free variant. Thus, the user is given a choice to choose
  // among the preferred endpoints as per use case.
private:
  // Add the private members :
  // alignas(cache_line_size) std::atomic<size_t> head;
  // alignas(cache_line_size) size_t tail_cache;
  // alignas(cache_line_size) std::atomic<size_t> tail;
  // alignas(cache_line_size) size_t head_cache;
  // static constexpr size_t capacity = Capacity + 1;
  // alignas(cache_line_size) T arr[capacity];
  // aligned the start of the array too
  // Description of private members :
  // 1. std::atomic<size_t> head is the atomic head pointer
  // 2. std::atomic<size_t> tail is the atomic tail pointer
  // 3. size_t head_cache is the cached head pointer
  // 4. size_t tail_cache is the cached tail pointer
  // 5. T arr[] compile time allocated array
  // Cache align 1-5.
  // 6. static constexpr size_t capcity to store the capcity for operations in
  // functions Why static ?? Why constexpr ?? [Reason this]
  alignas(cache_line_size) std::atomic<size_t> head{0};
  alignas(cache_line_size) size_t tail_cache{0};
  alignas(cache_line_size) std::atomic<size_t> tail{0};
  alignas(cache_line_size) size_t head_cache{0};
  static constexpr size_t capacity = Capacity + 1; 
  alignas(cache_line_size) alignas(T) std::byte arr[capacity * sizeof(T)]{};
	
  // Make these functions which expose raw pointers private for safety... 
  T *slot_ptr(size_t idx) noexcept {
	return std::launder(
		reinterpret_cast<T *>(arr + (idx * sizeof(T))));
  }

  const T *slot_ptr(size_t idx) const noexcept {
	return std::launder(
		reinterpret_cast<const T *>(arr + (idx * sizeof(T))));
  }

public:
  // Public Member functions :
  // Add appropriate constructors and destructors -> Add here only
  // 1. void wait_and_push(value) : Busy wait until element is pushed
  // 2. bool try_push(value) : Try to push if not full else leave (returns false
  // if could not push else true)
  // 3. void wait_and_pop(value ref) : Busy wait until we have atmost 1 elt and
  // then pop it and store in reference
  // 4. bool try_pop(value ref) : Try to pop and return false if failed bool
  // 5. empty(void) : Checks if the queue is empty and return bool
  // 6. bool peek(value ref) : Peek the top of the queue.
  // Will work only in SPSC/MPSC why ?? [Reason this]
  // 8. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  // 9. Add size() function
  // 10. Any more suggestions ??
  // can make the functions peek , empty and size const
  // 11. Why no shared_ptr ?? [Reason this]
  lockfree_spsc_bounded() = default;
  lockfree_spsc_bounded(const lockfree_spsc_bounded &) = delete;
  lockfree_spsc_bounded &operator=(const lockfree_spsc_bounded &) = delete;
  lockfree_spsc_bounded(lockfree_spsc_bounded &&) = delete;
  lockfree_spsc_bounded &operator=(lockfree_spsc_bounded &&) = delete;
  ~lockfree_spsc_bounded();

  void wait_and_push(T value);
  bool try_push(T value);
  void wait_and_pop(T &value);
  bool try_pop(T &value);
  bool peek(T &value);

  template <typename... Args> bool emplace_back(Args &&...args); // emplace using variadic templates and perfect forwarding

  bool empty() const;
  size_t size() const;
};
} // namespace tsfqueue::impl
#endif
