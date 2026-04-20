#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"
#include <thread>
#include <utility>

namespace tsfqueue::impl {

template <typename T, typename Allocator>
lockfree_spsc_unbounded<T, Allocator>::lockfree_spsc_unbounded() {
	node *stub = allocate_node_();
	head_ = stub;
	tail_.store(stub, std::memory_order_relaxed);
	// memory_order_relaxed is used here since while calling the constructor
	// multiple threads do not access the queue, rather once the queue is constructed
	// then only we need to take care of data race as multiple threads start accessing the queue
}

template <typename T, typename Allocator>
lockfree_spsc_unbounded<T, Allocator>::lockfree_spsc_unbounded(
	lockfree_spsc_unbounded &&other) noexcept
	: lockfree_spsc_unbounded() {
	swap(other);
}

template <typename T, typename Allocator>
lockfree_spsc_unbounded<T, Allocator> &
lockfree_spsc_unbounded<T, Allocator>::operator=(
	lockfree_spsc_unbounded &&other) noexcept {
	if (this == &other) {
		return *this;
	}

	lockfree_spsc_unbounded tmp(std::move(other));
	swap(tmp);
	return *this;
}

template <typename T, typename Allocator>
lockfree_spsc_unbounded<T, Allocator>::~lockfree_spsc_unbounded() {
	node *current = head_;
	while (current != nullptr) {
		node *next = current->next.load(std::memory_order_relaxed);
		deallocate_node_(current);
		current = next;
	}
	// destructor is called when no thread accesses the queue and in that case since no thread accesses the queue, no chance of data race
}

template <typename T, typename Allocator>
void lockfree_spsc_unbounded<T, Allocator>::swap(
	lockfree_spsc_unbounded &other) noexcept {
	using std::swap;
	swap(alloc_, other.alloc_);
	swap(head_, other.head_);

	node *this_tail = tail_.load(std::memory_order_relaxed);
	node *other_tail = other.tail_.load(std::memory_order_relaxed);
	tail_.store(other_tail, std::memory_order_relaxed);
	other.tail_.store(this_tail, std::memory_order_relaxed);

	size_t this_size = size_.load(std::memory_order_relaxed);
	size_t other_size = other.size_.load(std::memory_order_relaxed);
	size_.store(other_size, std::memory_order_relaxed);
	other.size_.store(this_size, std::memory_order_relaxed);
}

template <typename T, typename Allocator>
typename lockfree_spsc_unbounded<T, Allocator>::node *
lockfree_spsc_unbounded<T, Allocator>::allocate_node_() {
	node *p = node_alloc_traits::allocate(alloc_, 1);
	node_alloc_traits::construct(alloc_, p);
	p->next.store(nullptr, std::memory_order_relaxed);
	return p;
}

template <typename T, typename Allocator>
void lockfree_spsc_unbounded<T, Allocator>::deallocate_node_(node *p) noexcept {
	node_alloc_traits::destroy(alloc_, p);
	node_alloc_traits::deallocate(alloc_, p, 1);
}

template <typename T, typename Allocator>
void lockfree_spsc_unbounded<T, Allocator>::push(T value) {
	emplace(std::move(value));
}

template <typename T, typename Allocator>
template <typename... Args>
void lockfree_spsc_unbounded<T, Allocator>::emplace(Args &&...args) {
	node *new_stub = allocate_node_();
	node *current_tail = tail_.load(std::memory_order_relaxed);

	current_tail->data = T(std::forward<Args>(args)...);
	current_tail->next.store(new_stub, std::memory_order_release);
	tail_.store(new_stub, std::memory_order_relaxed);
	size_.fetch_add(1, std::memory_order_relaxed);
}
// head -> stub <-tail

template <typename T, typename Allocator>
bool lockfree_spsc_unbounded<T, Allocator>::try_pop(T &value) {
	node *old_head = head_;
	node *next = old_head->next.load(std::memory_order_acquire);
	if (next == nullptr) {
		return false;
	}

	value = std::move(old_head->data);
	head_ = next;
	deallocate_node_(old_head);
	size_.fetch_sub(1, std::memory_order_relaxed);
	return true;
}

template <typename T, typename Allocator>
void lockfree_spsc_unbounded<T, Allocator>::wait_and_pop(T &value) {
	while (!try_pop(value)) {
		std::this_thread::yield();
	}
}

template <typename T, typename Allocator>
bool lockfree_spsc_unbounded<T, Allocator>::peek(T &value) const { //Not atomic as a whole, so it is not a proper peek operation in general except spsc.
	node *current_head = head_;
	node *next = current_head->next.load(std::memory_order_acquire);
	if (next == nullptr) {
		return false;
	}

	value = current_head->data;
	return true;
}

template <typename T, typename Allocator>
bool lockfree_spsc_unbounded<T, Allocator>::empty() const {
	return head_->next.load(std::memory_order_acquire) == nullptr;
}

template <typename T, typename Allocator>
size_t lockfree_spsc_unbounded<T, Allocator>::size() const noexcept {
	return size_.load(std::memory_order_relaxed);
}

} // namespace tsfqueue::impl

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??
