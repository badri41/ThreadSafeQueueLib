#ifndef LOCKFREE_SPSC_UNBOUNDED_DEFS
#define LOCKFREE_SPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace tsfqueue::__impl {
template <typename T> class lockfree_spsc_unbounded {
  // Works exactly same as the blocking_mpmc_unbounded queue (see this once)
  // with tail pointer pointing to stub node and your head pointer updates as
  // per the pushes. See the Lockless_Node in utils to understand the working.
  // Note that the next pointers are atomic there. Why ?? [Reason this]
  // Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
  // me for details)

  // [Copy of blocking_mpmc_unbounded]
  // For the implementation, we start with a stub node and both head and tail
  // are initialized to it. When we push, we make a new stub node, move the data
  // into the current tail and then change the tail to the new stub. We have two
  // methods : wait_and_pop() which waits on the queue and returns element &
  // try_pop() which returns an element if queue is not empty otherwise returns
  // some neutral element OR a false boolean whichever is applicable. Pop works
  // by returning the data stored in head node and replacing head to its next
  // node. We handle the empty queue gracefully as per the pop type.
private:
  using node = tsfqueue::__utils::Lockless_Node<T>;

  // Add the private members :
  alignas(tsfq::__impl::cache_line_size) 
  node* head; //This is a spsc queue so, there is no data race between multiple consumer threads and hence 
              // head not need be declared atomic.
  alignas(tsfq::__impl::cache_line_size) 
  std::atomic<node*> tail; //tail is required to be atomic as producer changes the tail while pushing
                          // and consumer may try to read the tail while checking if the queue is empty.
                          // If tail was not atomic this might lead to data race, that is while a consumer tries
                          // to read it; the tail might get updated by the producer but the consumer gets the old value.


  // Description of private members :
  // 1. node* head -> Pointer to the head node
  // 2. node* tail -> Pointer to tail node
  // 3. Cache align 1-2
  alignas(tsfq::__impl::cache_line_size) std::atomic<size_t> sz{0};


public:
  // Public member functions :
  // Add relevant constructors and destructors -> Add these here only
  lockfree_spsc_unbounded() {
      node* stub = new node();
      stub->next.store(nullptr,std::memory_order_relaxed);
      head = stub;
      tail.store(stub,std::memory_order_relaxed);
      // memory_order_relaxed is used here since while calling the constructor
      // multiple threads do not access the queue, rather once the queue is constructed
      // then only we need to take care of data race as multiple threads start accessing the queue
  }

  ~lockfree_spsc_unbounded() {
      while(head != nullptr) {
          node* temp = head;
          head = head->next.load(std::memory_order_relaxed);
          delete temp;
      }
      // destructor is called when no thread accesses the queue and in that case since no thread accesses the queue, no chance of data race
  }
  
  void push(T value); // Pushes the value inside the queue, copies the value
  void wait_and_pop(T& value); // : Blocking wait on queue, returns value in
  // the reference passed as parameter
  bool try_pop(T& value); // : Returns true and
  // gives the value in reference passed, false otherwise
  bool empty(); // : Returns
  // whether the queue is empty or not at that instant
  bool peek(T& value); // : Returns the front/top element of queue in ref (false if empty queue)
  // 6. Add static asserts
  // 7. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  size_t size() const;
  // 8. Add size() function
  // 9. Any more suggestions ??
  // 10. Why no shared_ptr ?? [Reason this]
};
} // namespace tsfqueue::__impl

#endif 
