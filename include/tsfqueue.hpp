#ifndef TSFQ_HPP
#define TSFQ_HPP

#include <blocking_mpmc_unbounded/queue.hpp>
#include <lockfree_mpmc_bounded/queue.hpp>
#include <lockfree_mpsc_unbounded/queue.hpp>
#include <lockfree_spsc_bounded/queue.hpp> 
#include <lockfree_spsc_unbounded/queue.hpp>

// #define FAST
#ifdef FAST
#include <lockfree_spsc_unbounded_fast/queue.hpp>
#endif

namespace tsfqueue {
template <typename T>
using BlockingMPMCUnbounded = impl::blocking_mpmc_unbounded<T>;
// template <typename T, size_t N>
// using SPSCBounded = impl::lockfree_spsc_bounded<T, N>; // Enable after SPSC bounded is implemented
template <typename T> using SPSCUnbounded = impl::lockfree_spsc_unbounded<T>;
} // namespace tsfqueue

#endif
