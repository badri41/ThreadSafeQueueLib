#ifndef UTILS
#define UTILS

#include <atomic>
#include <memory>
#include <new>

namespace tsfqueue::impl {
#ifdef __cpp_lib_hardware_interference_size
inline constexpr size_t cache_line_size =
	std::hardware_destructive_interference_size;
#else
inline constexpr size_t cache_line_size = 64UL; // fallback
#endif
} // namespace tsfqueue::impl

namespace tsfqueue::utils {
template <typename T> struct Node {
	std::shared_ptr<T> data;
	std::unique_ptr<Node<T>> next;
};
template <typename T> struct Lockless_Node {
	T data;
	std::atomic<Lockless_Node *> next;
};	
} // namespace tsfqueue::utils

#endif