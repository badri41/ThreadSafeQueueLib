cmake_minimum_required(VERSION 3.14)
project(ThreadsafeQueueLib)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()
include(CTest)

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent compiling gtest with MSVC's multithreaded CRT if we use MinGW, etc.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(include)

option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
endif()

# SPSC tests
add_executable(test_spsc tests/test_lockfree_spsc_unbounded.cpp)
target_link_libraries(test_spsc GTest::gtest_main Threads::Threads)

# MPSC tests    
add_executable(test_mpsc tests/test_mpsc.cpp)
target_link_libraries(test_mpsc GTest::gtest_main Threads::Threads)

# MPMC tests
add_executable(test_mpmc tests/test_mpmc.cpp)
target_link_libraries(test_mpmc GTest::gtest_main Threads::Threads)

# Register tests
add_test(NAME SPSC COMMAND test_spsc)
add_test(NAME MPSC COMMAND test_mpsc)
add_test(NAME MPMC COMMAND test_mpmc)