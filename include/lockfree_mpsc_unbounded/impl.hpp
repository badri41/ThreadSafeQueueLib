#ifndef LOCKFREE_MPSC_UNBOUNDED_IMPL
#define LOCKFREE_MPSC_UNBOUNDED_IMPL

#include "defs.hpp"
#include<thread>
#include<utility>

namespace tsfqueue::impl {
    template<typename T, typename Allocator>
    lockfree_mpsc_unbounded<T,Allocator>::lockfree_mpsc_unbounded(){
        node* stub = allocate_node();
        head=stub;
        tail.store(stub, std::memory_order_relaxed);
        // memory_order is relaxed as this is the constructor 
        // and called only once when the queue is made before the producer and consumer
        // threads start accessing the queue.
    }
    template<typename T, typename Allocator>
    lockfree_mpsc_unbounded<T,Allocator>::lockfree_mpsc_unbounded(
        lockfree_mpsc_unbounded &&other) noexcept
        : lockfree_mpsc_unbounded(){
            swap(other);
        }
    template <typename T, typename Allocator>
    lockfree_mpsc_unbounded<T, Allocator> &
    lockfree_mpsc_unbounded<T, Allocator>::operator=(
	lockfree_mpsc_unbounded &&other) noexcept {
	if (this == &other) {
		return *this;
	}

	lockfree_mpsc_unbounded tmp(std::move(other));
	swap(tmp);
	return *this;
    }

    template<typename T, typename Allocator>
    lockfree_mpsc_unbounded<T,Allocator>::~lockfree_mpsc_unbounded(){
        node* curr=head;
        while(curr!=nullptr){
            node* nxt = curr->next.load(std::memory_order_relaxed);
            deallocate_node(curr);
            curr=nxt;
        }
        //memory_order is relaxed as this is destructor and only accessed
        //when no thread is accessing the queue so no chance of data race.
    }

    template<typename T, typename Allocator>
    void lockfree_mpsc_unbounded<T,Allocator>::swap(
        lockfree_mpsc_unbounded &other) noexcept {
            using std::swap;
            swap(alloc,other.alloc);
            swap(head,other.head);
            node* this_tail=tail.load(std::memory_order_relaxed);
            node* other_tail=other.tail.load(std::memory_order_relaxed);
            tail.store(other_tail,std::memory_order_relaxed);
            other.tail.store(this_tail,std::memory_order_relaxed);

			size_t this_size = size_.load(std::memory_order_relaxed);
			size_t other_size = other.size_.load(std::memory_order_relaxed);
			size_.store(other_size, std::memory_order_relaxed);
			other.size_.store(this_size, std::memory_order_relaxed);
        }
    
    template <typename T, typename Allocator>
    typename lockfree_mpsc_unbounded<T, Allocator>::node *
    lockfree_mpsc_unbounded<T, Allocator>::allocate_node() {
	node *p = node_alloc_traits::allocate(alloc, 1);
	node_alloc_traits::construct(alloc, p);
	p->next.store(nullptr, std::memory_order_relaxed);
	return p;
   }

   template <typename T, typename Allocator>
   void lockfree_mpsc_unbounded<T, Allocator>::deallocate_node(node *p) noexcept {
	node_alloc_traits::destroy(alloc, p);
	node_alloc_traits::deallocate(alloc, p, 1);
  }

  template <typename T, typename Allocator>
  void lockfree_mpsc_unbounded<T, Allocator>::push(T value) {
	emplace(std::move(value));
}

  template <typename T, typename Allocator>
  template <typename... Args>
  void lockfree_mpsc_unbounded<T, Allocator>::emplace(Args &&...args) {
	node *new_stub = allocate_node();
    node *old_tail = tail.exchange(new_stub, std::memory_order_acq_rel);
    // The above memory ordering is to be discussed further.
	old_tail->data = T(std::forward<Args>(args)...);
	old_tail->next.store(new_stub, std::memory_order_release);
	size_.fetch_add(1, std::memory_order_relaxed);
}
  template <typename T, typename Allocator>
  bool lockfree_mpsc_unbounded<T,Allocator>::try_pop(T& value){
    node *old_head = head;
    node *nxt = old_head->next.load(std::memory_order_acquire);
    if(nxt==nullptr) return false;
    value = std::move(old_head->data);
    head=nxt;
    deallocate_node(old_head);
    size_.fetch_sub(1, std::memory_order_relaxed);
    return true;
  }

  template <typename T, typename Allocator>
  void lockfree_mpsc_unbounded<T, Allocator>::wait_and_pop(T &value) {
	while (!try_pop(value)) {
		std::this_thread::yield();
	}
  }

  template <typename T, typename Allocator>
 bool lockfree_mpsc_unbounded<T, Allocator>::peek(T &value) const { //Not atomic as a whole, so it is not a proper peek operation in general except spsc.
	static_assert(std::is_copy_assignable_v<T>, "peek() requires T to be copy assignable");
	node *current_head = head;
	node *next = current_head->next.load(std::memory_order_acquire);
	if (next == nullptr) {
		return false;
	}
	value = current_head->data;
	return true;
}

 template <typename T, typename Allocator>
 bool lockfree_mpsc_unbounded<T, Allocator>::empty() const {
	return head->next.load(std::memory_order_acquire) == nullptr;
}

 template <typename T, typename Allocator>
 size_t lockfree_mpsc_unbounded<T, Allocator>::size() const noexcept {
	return size_.load(std::memory_order_relaxed);
}

}

#endif
