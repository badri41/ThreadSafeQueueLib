#ifndef TSFQ_HPP
#define TSFQ_HPP

#include "blockingMpmcUnbounded/queue.hpp"
#include "lockfreeMpmcBounded/queue.hpp"
#include "lockfreeMpscUnbounded/queue.hpp"
#include "lockfreeSpscBounded/queue.hpp"
#include "lockfreeSpscUnbounded/queue.hpp"

// #define FAST
#ifdef FAST
#include <fastLockfreeSpscUnbounded/queue.hpp>
#endif

namespace tsfqueue {
template <typename T>
using blockingMpmcUnbounded = impl::blockingMpmcUnbounded<T>;
template <typename T, size_t N>
using spscBounded = impl::lockfreeSpscBounded<T, N>;
template <typename T> using spscUnbounded = impl::lockfreeSpscUnbounded<T>;
template <typename T> using mpscUnbounded = impl::lockfreeMpscUnbounded<T>;
} // namespace tsfqueue

#endif
