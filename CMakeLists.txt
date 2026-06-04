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

# spsc tests (unbounded)
add_executable(testSpsc tests/testLockfreeSpscUnbounded.cpp)
target_link_libraries(testSpsc GTest::gtest_main Threads::Threads)

# spsc tests (bounded)
add_executable(testSpscBounded tests/testLockfreeSpscBounded.cpp)
target_link_libraries(testSpscBounded GTest::gtest_main Threads::Threads)

# mpsc tests    
add_executable(testMpsc tests/testMpsc.cpp)
target_link_libraries(testMpsc GTest::gtest_main Threads::Threads)

# mpmc tests
add_executable(testMpmc tests/testMpmc.cpp)
target_link_libraries(testMpmc GTest::gtest_main Threads::Threads)

# Register tests
add_test(NAME spsc COMMAND testSpsc)
add_test(NAME spscBounded COMMAND testSpscBounded)
add_test(NAME mpsc COMMAND testMpsc)
add_test(NAME mpmc COMMAND testMpmc)

# mpsc isolated benchmarks
add_executable(benchLatMpsc benchmarking/benchLatencyMpsc.cpp)
target_link_libraries(benchLatMpsc Threads::Threads)

add_executable(benchThrMpsc benchmarking/benchThroughputMpsc.cpp)
target_link_libraries(benchThrMpsc Threads::Threads)