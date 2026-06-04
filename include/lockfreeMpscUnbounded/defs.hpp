#ifndef LOCKFREE_MPSC_UNBOUNDED_DEFS
#define LOCKFREE_MPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace tsfqueue::impl {
template <typename T, typename Allocator = std::allocator<T>, bool TrackMetrics = false> class lockfreeMpscUnbounded {
  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(!std::is_reference_v<T>, "Queue cannot store reference types");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
  static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
          "T must be move or copy constructible");
  static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
          "T must be move or copy assignable");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

  using node = tsfqueue::utils::Lockless_Node<T>;

  using nodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<node>;
  using nodeAllocTraits = std::allocator_traits<nodeAllocator>;

    public:
      lockfreeMpscUnbounded();

      lockfreeMpscUnbounded(const lockfreeMpscUnbounded &) = delete;
      lockfreeMpscUnbounded &operator=(const lockfreeMpscUnbounded &)=delete;

      lockfreeMpscUnbounded(lockfreeMpscUnbounded &&other) noexcept;

      lockfreeMpscUnbounded &operator=(lockfreeMpscUnbounded &&other) noexcept;

      ~lockfreeMpscUnbounded();

      void swap(lockfreeMpscUnbounded &other) noexcept;

      void push(T value);

      template<typename... Args>
      void emplace(Args &&...args);

      void waitAndPop(T& value);
      bool tryPop(T& value);

      [[nodiscard]] bool empty() const;
      [[nodiscard]] bool peek(T& value) const;
      [[nodiscard]] size_t getSize() const noexcept;
      [[nodiscard]] size_t maxSize() const noexcept;

    private:
      node* allocateNode();
      void deallocateNode(node* p) noexcept;

      nodeAllocator alloc{};

      alignas(cacheLineSize) node* head{};
      alignas(cacheLineSize) std::atomic<node*> tail{};
      alignas(cacheLineSize) std::atomic<size_t> size{0};
      alignas(cacheLineSize) std::atomic<size_t> maxSizeVal{0};
    };
}

#endif