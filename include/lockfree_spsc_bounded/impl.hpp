#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"
#include <utility>

namespace tsfqueue::impl {
template <typename T, size_t Capacity>
void lockfree_spsc_bounded<T, Capacity>::wait_and_push(T value) {
  size_t cur_tail = tail.load(std::memory_order_relaxed);
  //We should change the tail.load(acquire) to tail.load(relaxed) since only the producer thread has 
  // exclusive write access to the tail variable and a single thread always agrees upon the order of modification of an atomic variable.
  //tail.load(relaxed) is less costly.
  size_t next_tail = (cur_tail + 1) % capacity;
  // size_t curr_head = head.load(std::memory_order_acquire);
  while (next_tail == head_cache) {
	head_cache = head.load(std::memory_order_acquire); // busy wait
  }

  arr[cur_tail] = std::move(value);
  // tail_cache = next_tail;
  tail.store(next_tail, std::memory_order_release);
}

template <typename T, size_t Capacity>
bool lockfree_spsc_bounded<T, Capacity>::try_push(T value) {
  return emplace_back(std::move(value));
}

template <typename T, size_t Capacity>
bool lockfree_spsc_bounded<T, Capacity>::try_pop(T &value) {
  // cur_tail = tail.load(std::memory_order_acquire);
  size_t cur_head = head.load(std::memory_order_relaxed);
  //We should change the head.load(acquire) to head.load(relaxed) since only the producer thread has 
  // exclusive write access to the head variable and a single thread always agrees upon the order of modification of an atomic variable.
  //head.load(relaxed) is less costly.
  if (tail_cache == cur_head) {
	tail_cache = tail.load(std::memory_order_acquire);
	if (tail_cache == cur_head)
	  return false;
  }

  value = std::move(arr[cur_head]);
  // head_cache = (cur_head + 1) % capacity;
  head.store((cur_head + 1) % capacity, std::memory_order_release);
  return true;
}

template <typename T, size_t Capacity>
void lockfree_spsc_bounded<T, Capacity>::wait_and_pop(T &value) {
  size_t cur_head = head.load(std::memory_order_relaxed);
  //We should change the head.load(acquire) to head.load(relaxed) since only the producer thread has 
  // exclusive write access to the head variable and a single thread always agrees upon the order of modification of an atomic variable.
  //head.load(relaxed) is less costly.
  while (tail_cache == cur_head) {
	tail_cache = tail.load(std::memory_order_acquire); // busy wait
  }

  value = std::move(arr[cur_head]);
  // head_cache = (cur_head + 1) % capacity;
  head.store((cur_head + 1) % capacity, std::memory_order_release);
}

template <typename T, size_t Capacity>
bool lockfree_spsc_bounded<T, Capacity>::peek(T &value) {
  size_t cur_head = head.load(std::memory_order_acquire);
  if (cur_head == tail_cache) {
	tail_cache = tail.load(std::memory_order_acquire);
	if (cur_head == tail_cache) {
	  return false;
	}
  }
  value = arr[cur_head];
  return true;
}

template <typename T, size_t Capacity>
template <typename... Args>
bool lockfree_spsc_bounded<T, Capacity>::emplace_back(Args &&...args) {
  size_t cur_tail = tail.load(std::memory_order_relaxed);
  //We should change the tail.load(acquire) to tail.load(relaxed) since only the producer thread has 
  // exclusive write access to the tail variable and a single thread always agrees upon the order of modification of an atomic variable.
  //tail.load(relaxed) is less costly.
  size_t next_tail = (cur_tail + 1) % capacity;
  if (next_tail == head_cache) {
	head_cache = head.load(std::memory_order_acquire);
	if (next_tail == head_cache) {
	  return false;
	}
  }
  arr[cur_tail] = T(std::forward<Args>(args)...);
  // tail_cache = next_tail;
  tail.store(next_tail, std::memory_order_release);
  return true;
}

template <typename T, size_t Capacity>
bool lockfree_spsc_bounded<T, Capacity>::empty() const {
  return head.load(std::memory_order_relaxed) ==
		 tail.load(std::memory_order_relaxed);
  // since queue is very frequently modified
}

template <typename T, size_t Capacity>
size_t lockfree_spsc_bounded<T, Capacity>::size() const {
  return (tail.load(std::memory_order_relaxed) -
		  head.load(std::memory_order_relaxed) + capacity) %
		 capacity;
  // again, since size is very frequently changing.
}
} // namespace tsfqueue::impl

#endif
