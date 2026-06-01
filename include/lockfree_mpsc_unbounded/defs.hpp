#ifndef LOCKFREE_MPSC_UNBOUNDED_DEFS
#define LOCKFREE_MPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace tsfqueue::impl {
template <typename T, typename Allocator = std::allocator<T>> class lockfree_mpsc_unbounded {
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

    public:
      lockfree_mpsc_unbounded();

      lockfree_mpsc_unbounded(const lockfree_mpsc_unbounded &) = delete;
      lockfree_mpsc_unbounded &operator=(const lockfree_mpsc_unbounded &)=delete;

      lockfree_mpsc_unbounded(lockfree_mpsc_unbounded &&other) noexcept;

      lockfree_mpsc_unbounded &operator=(lockfree_mpsc_unbounded &&other) noexcept;

      ~lockfree_mpsc_unbounded();

      void swap(lockfree_mpsc_unbounded &other) noexcept;

      void push(T value);

      template<typename... Args>
      void emplace(Args &&...args);

      void wait_and_pop(T& value);
      bool try_pop(T& value);

      [[nodiscard]] bool empty() const;
      [[nodiscard]] bool peek(T& value) const;
      [[nodiscard]] size_t size() const noexcept;

    private:
      node* allocate_node();
      void deallocate_node(node* p) noexcept;

      node_allocator alloc{};

      alignas(cache_line_size) node* head{};  //This is a mpsc queue so, there is no data race between multiple consumer threads and hence 
											// head not need be declared atomic.
      alignas(cache_line_size) std::atomic<node*> tail{};   // tail needs to be atomic because it is accessed and modified by multiple producer threads.
      alignas(cache_line_size) std::atomic<size_t> size{0};
    };
}

#endif
