cmake_minimum_required(VERSION 3.10)

project(AtomicCounter)

set(CMAKE_CXX_STANDARD 23)

add_executable(main main.cpp)

# Add the main executable as a test
enable_testing()
add_test(NAME HelpMenu COMMAND main --help)
add_test(Name DefaultConfig COMMAND main)
add_test(NAME SmallVectors COMMAND main -v 100:1001:100 -b 10:101:10)
add_test(NAME LargeVectors COMMAND main -v 10000000:10000001:1000000 -b 1000:10001:1000)

add_test(NAME Async COMMAND main -v 10000000 -b 1000:10001:1000 -c async)
add_test(NAME ThreadPool COMMAND main -v 10000000 -b 1000:10001:1000 -c thread_pool)
add_test(NAME Threads COMMAND main -v 10000000 -b 1000:10001:1000 -c threads)