#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

namespace tsfqueue::impl {

template <typename T> void blockingMpmcUnbounded<T>::push(T value) {
	std::shared_ptr<T> temp = std::make_shared<T>(std::move(value));
	std::unique_ptr<node> t = std::make_unique<node>();
	std::lock_guard<std::mutex> lock(tailMutex);
	tail->data = temp;
	tail->next = std::move(t);
	tail = (tail->next).get();
	++sz;
	//lock.unlock();
	cond.notify_one();
}

template <typename T>
typename blockingMpmcUnbounded<T>::node *
blockingMpmcUnbounded<T>::getTail() {
	std::lock_guard<std::mutex> lock(tailMutex);
	return tail;
	// This is a private function because we cant allow user to use this function 
	// because it is returning a pointer. The user might modify the pointer and also
	// This might lead to race conditons, since after the function runs the user
	// has the access to the pointer but the tailMutex is not locked
}

template <typename T>
std::unique_ptr<typename blockingMpmcUnbounded<T>::node>
blockingMpmcUnbounded<T>::waitAndGet() {
	 std::unique_lock<std::mutex> lock(headMutex);
	 cond.wait(lock,[this]{ return head.get()!= getTail();});
	 //We use unique lock because we want extra control over the lock to unlock and relock it again.
	 std::unique_ptr<node> old = std::move(head);
	 head = std::move(old->next);
	 return old;
	 // head -> node -> node -> node ->node ----
	 // pop : old -> node -> node -> node ---
	 //                  head-|
}

template <typename T>
std::unique_ptr<typename blockingMpmcUnbounded<T>::node>
blockingMpmcUnbounded<T>::tryGet() {
	std::lock_guard<std::mutex> lock(headMutex);
	if(head.get()==getTail()){
		return std::unique_ptr<node>();
	}
	std::unique_ptr<node> old = std::move(head);
	head = std::move(old->next);
	return old;
}

template <typename T> void blockingMpmcUnbounded<T>::waitAndPop(T &value) {
	std::unique_ptr<node> old = waitAndGet();
	value = std::move(*(old->data));
	--sz;
}

template <typename T> std::shared_ptr<T> blockingMpmcUnbounded<T>::waitAndPop() {
	std::unique_ptr<node> old = waitAndGet();
	--sz;
	return old->data;
}

template <typename T> bool blockingMpmcUnbounded<T>::tryPop(T &value) {
	std::unique_ptr<node> old = std::move(tryGet());
	if(!old) return false;
	value = std::move(*(old->data));
	--sz;
	return true;
}

template <typename T> std::shared_ptr<T> blockingMpmcUnbounded<T>::tryPop() {
	std::unique_ptr<node> old = std::move(tryGet());
	if(!old) return nullptr;
	--sz;
	return old->data;
}

template <typename T> bool blockingMpmcUnbounded<T>::empty() {
	std::lock_guard<std::mutex> lock(headMutex);
	return (head.get()==getTail());
}         
// head ->  dummy <- tail
template <typename T> size_t blockingMpmcUnbounded<T>::size() const{
	return sz.load(std::memory_order_relaxed);
} 

} // end namespace tsfqueue::impl


#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ?? 

