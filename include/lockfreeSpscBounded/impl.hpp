#ifndef LOCKFREE_spscBounded_IMPL_CT
#define LOCKFREE_spscBounded_IMPL_CT

#include "defs.hpp"
#include <memory>
#include <utility>

namespace tsfqueue::impl {

template <typename T, size_t Capacity>
void lockfreeSpscBounded<T, Capacity>::waitAndPush(T value) {
	size_t curTail = tail.load(std::memory_order_relaxed);
	//We should change the tail.load(acquire) to tail.load(relaxed) since only the producer thread has 
	// exclusive write access to the tail variable and a single thread always agrees upon the order of modification of an atomic variable.
	//tail.load(relaxed) is less costly.
	size_t nextTail = (curTail + 1) % capacity;

	// Spin and Wait for a free slot to push... 
	while (nextTail == headCache) {
		headCache = head.load(std::memory_order_acquire); 
	}

	std::construct_at(slotPtr(curTail), std::move(value));
	// tailCache = nextTail;
	tail.store(nextTail, std::memory_order_release);
}

template <typename T, size_t Capacity>
bool lockfreeSpscBounded<T, Capacity>::tryPush(T value) {
  	return emplace_back(std::move(value));
}

template <typename T, size_t Capacity>
bool lockfreeSpscBounded<T, Capacity>::tryPop(T &value) {
	// curTail = tail.load(std::memory_order_acquire);
	size_t curHead = head.load(std::memory_order_relaxed);
	//We should change the head.load(acquire) to head.load(relaxed) since only the producer thread has 
	// exclusive write access to the head variable and a single thread always agrees upon the order of modification of an atomic variable.
	//head.load(relaxed) is less costly.
	if (tailCache == curHead) {
		tailCache = tail.load(std::memory_order_acquire);
		if (tailCache == curHead)
		return false;
	}

	T *slot = slotPtr(curHead);
	value = std::move(*slot);
	std::destroy_at(slot);
	// headCache = (curHead + 1) % capacity;
	head.store((curHead + 1) % capacity, std::memory_order_release);
	return true;
}

template <typename T, size_t Capacity>
void lockfreeSpscBounded<T, Capacity>::waitAndPop(T &value) {
	size_t curHead = head.load(std::memory_order_relaxed);
	//We should change the head.load(acquire) to head.load(relaxed) since only the producer thread has 
	// exclusive write access to the head variable and a single thread always agrees upon the order of modification of an atomic variable.
	//head.load(relaxed) is less costly.
	while (tailCache == curHead) {
		tailCache = tail.load(std::memory_order_acquire); // busy wait
	}

	T *slot = slotPtr(curHead);
	value = std::move(*slot);
	std::destroy_at(slot);
	// headCache = (curHead + 1) % capacity;
	head.store((curHead + 1) % capacity, std::memory_order_release);
}

template <typename T, size_t Capacity>
bool lockfreeSpscBounded<T, Capacity>::peek(T &value) {
	size_t curHead = head.load(std::memory_order_acquire);
	if (curHead == tailCache) {
		tailCache = tail.load(std::memory_order_acquire);
		if (curHead == tailCache) {
		return false;
		}
	}
	value = *slotPtr(curHead);
	return true;
}

template <typename T, size_t Capacity>
template <typename... Args>
bool lockfreeSpscBounded<T, Capacity>::emplace_back(Args &&...args) {
	size_t curTail = tail.load(std::memory_order_relaxed);
	//We should change the tail.load(acquire) to tail.load(relaxed) since only the producer thread has 
	// exclusive write access to the tail variable and a single thread always agrees upon the order of modification of an atomic variable.
	//tail.load(relaxed) is less costly.
	size_t nextTail = (curTail + 1) % capacity;
	if (nextTail == headCache) {
		headCache = head.load(std::memory_order_acquire);
		if (nextTail == headCache) {
			return false;
		}
	}
	std::construct_at(slotPtr(curTail), std::forward<Args>(args)...);
	// tailCache = nextTail;
	tail.store(nextTail, std::memory_order_release);
	return true;
}

template <typename T, size_t Capacity>
bool lockfreeSpscBounded<T, Capacity>::empty() const {
	return head.load(std::memory_order_relaxed) ==
			tail.load(std::memory_order_relaxed);
	// since queue is very frequently modified
}

template <typename T, size_t Capacity>
size_t lockfreeSpscBounded<T, Capacity>::size() const {
	return (tail.load(std::memory_order_relaxed) -
			head.load(std::memory_order_relaxed) + capacity) %
			capacity;
	// again, since size is very frequently changing.
}

template <typename T, size_t Capacity>
lockfreeSpscBounded<T, Capacity>::~lockfreeSpscBounded() {
	size_t curHead = head.load(std::memory_order_relaxed);
	size_t curTail = tail.load(std::memory_order_relaxed);

	while (curHead != curTail) {
		std::destroy_at(slotPtr(curHead));
		curHead = (curHead + 1) % capacity;
		}
}

} // namespace tsfqueue::impl

#endif
