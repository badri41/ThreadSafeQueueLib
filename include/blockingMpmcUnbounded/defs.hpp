#ifndef BLOCKING_MPMC_UNBOUNDED_DEFS
#define BLOCKING_MPMC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>

namespace tsfqueue::impl {
template <typename T> class blockingMpmcUnbounded {
  static_assert(std::is_move_constructible_v<T>,"data type must be move constructible");
  static_assert(std::is_move_assignable_v<T>,"data type must be move assignable");
  static_assert(std::is_object_v<T>,"data type must be an object type");
  //Static Assert for references?
  // For the implementation, we start with a stub node and both head and tail
  // are initialized to it. When we push, we make a new stub node, move the data
  // into the current tail and then change the tail to the new stub. We have two
  // methods : waitAndPop() which waits on the queue and returns element &
  // tryPop() which returns an element if queue is not empty otherwise returns
  // some neutral element OR a false boolean whichever is applicable. Pop works
  // by returning the data stored in head node and replacing head to its next
  // node. We handle the empty queue gracefully as per the pop type.
private:
  using node = tsfqueue::utils::Node<T>;

  // Add private members :
  std::mutex headMutex;
  std::unique_ptr<node> head;
  std::mutex tailMutex;
  node *tail;
  std::condition_variable cond;
  node *getTail();
  std::unique_ptr<node> waitAndGet();
  std::unique_ptr<node> tryGet();
  std::atomic<size_t> sz{0};
  // Description of private members :                   
  // 1. std::mutex headMutex is used to prevent contention at the head pointer
  // This mutex is acquired when you are modifying std::unique_ptr<node> head to
  // prevent data race.                  
																		 
  // 2. std::unique_ptr<node> head is for the head pointer. We are using      
  // unique_ptr because this will ensure they are deleted automatically and we          
  // need not call delete manually. Also see the Node we use from utils have
  // std::unique_ptr<Node<T>> as the next pointers which forms a chain of
  // automatic delete(s).

  // 3. std::mutex tailMutex is used whenever tail is accessed. Mutex is locked
  // either manually or is locked by our condition variable

  // 4. node* tail is the pointer to tail. Note we cannot have tail as
  // unique_ptr as that would make two unique_ptr(s) to tail (one through)
  // linked list and one through our decalaration. Thus we make this a normal
  // pointer and this pointer is safely deallocated using the linked list
  // unique_ptr during call to destructor

  // 5. condition_variable cond is used to check whether queue is empty or not
  // and do a blocking wait on

  // Private member functions :
  // node *getTail() : Helper function to get normal pointer to tail at a
  // particular instant std::unique_ptr waitAndGet() : Helper function to
  // blocking wait on unique_ptr of head after popping std::unique_ptr tryGet()
  // : Helper function to try to get unique_ptr of head after popping

public:
  // Public member functions :
  blockingMpmcUnbounded(){
	head = std::make_unique<node>();
	tail = head.get(); //head.get() gives the raw pointer to the tail and not the ownership to tail  
  }
  blockingMpmcUnbounded(const blockingMpmcUnbounded&) = delete;
  blockingMpmcUnbounded& operator=(const blockingMpmcUnbounded&) = delete;

  blockingMpmcUnbounded(blockingMpmcUnbounded&&) = delete;
  blockingMpmcUnbounded& operator=(blockingMpmcUnbounded&&) = delete;
  void push(T value);
  void waitAndPop(T& value);
  std::shared_ptr<T> waitAndPop(void);
  bool tryPop(T& value);
  std::shared_ptr<T> tryPop();
  bool empty();
  size_t size() const;
  ~blockingMpmcUnbounded() = default; //default constructor: destroying head deletes all the nodes as head owns the node.
  // Add relevant constructors and destructors -> Add these here only
  // 1. void push(value) : Pushes the value inside the queue, copies the value
  // 2. void waitAndPop(value ref) : Blocking wait on queue, returns value in
  // the reference passed as parameter
  // 3. std::shared_ptr waitAndPop(void) : Blocking wait on queue, returns
  // value as a shared ptr allocated inside the call
  // 4. bool tryPop(value ref) : Returns true and gives the value in reference
  // passed, false otherwise
  // 5. std::shared_ptr tryPop() : Returns a shared ptr with data, returns
  // nullptr if failed
  // 6. bool empty() : Returns whether the queue is empty or not at that instant
  // 7. Add static asserts
  // 8. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  // 9. Add size() function
  // 10. Any more suggestions ??
};
} // namespace tsfqueue::impl

#endif