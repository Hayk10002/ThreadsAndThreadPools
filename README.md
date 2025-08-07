# ThreadsAndThreadPools

## Table of Contents
- [Introduction](#introduction)
- [Build and Run](#build-and-run)
- [Possible Output](#possible-output)
- [How does this work](#how-does-this-work)

## Introduction
This project demonstrates the performance differences between parallel algorithms using threads, thread pools, and std::async.

## Build and Run
To clone and run this project, you'll need [Git](https://git-scm.com) and [CMake](https://cmake.org/) installed on your computer. From your command line:

```bash
# Clone this repository
$ git clone https://github.com/Hayk10002/ThreadsAndThreadPools

# Go into the repository
$ cd ThreadsAndThreadPools

# Generate the build files
$ cmake -DCMAKE_BUILD_TYPE=Release -S . -B build

# Build the project
$ cmake --build build --config Release

# Then, run the executable generated in the `build` directory with --help to get usage information
$ your/path/to/exe/main.exe --help
# one valid way of calling main is
# example - .../main.exe -v 10000000 -b 1000:10001:1000 -c async
# this means: vector size 10000000, batch size from 1000 to 10001 (excluded) with step 100, and using std::async for parallelization
```

## Possible Output
(command: `main.exe -v 10000000 -b 1000:10001:1000 -c async`)
(for vector size 10000000, batch size from 1000 to 10001 (excluded) with step 100, and using std::async for parallelization)

```
Using concurrency type: threadpool
Starting benchmarks for vector size: 10000000
Batch size:  1000, Max concurrency: 8, Time:  46098us, Result:   504931295
Batch size:  2000, Max concurrency: 8, Time:  33487us, Result:   504931295
Batch size:  3000, Max concurrency: 8, Time:  33994us, Result:   504931295
Batch size:  4000, Max concurrency: 8, Time:  29650us, Result:   504931295
Batch size:  5000, Max concurrency: 8, Time:  30331us, Result:   504931295
Batch size:  6000, Max concurrency: 8, Time:  27541us, Result:   504931295
Batch size:  7000, Max concurrency: 8, Time:  25970us, Result:   504931295
Batch size:  8000, Max concurrency: 8, Time:  26169us, Result:   504931295
Batch size:  9000, Max concurrency: 8, Time:  27242us, Result:   504931295
Batch size: 10000, Max concurrency: 8, Time:  24728us, Result:   504931295
```

The result can be wrong because of integer overflow, but it is displayed to show that the algorithm gets the same answer every time with different settings.

## How does this work
The overall work is divided into batches that will be computed concurrently, and each batch itself is processed sequentially. The max concurrency is the number maximum number of batches that are processed concurrently from users perspective.

This project uses three concurrent algorithms to sum elements of a vector:
1. **Threads**: It creates a thread for every batch that will be calculated. (If max concurrency is 1000, there will be 1000 threads running in parallel.)

2. **Thread Pool**: It creates a thread pool with a fixed number of threads (determined automatically based hardware) and uses it to process batches concurrently. This is more efficient than creating a thread for each batch, as it reuses threads.

3. **std::async**: It uses the C++ standard library's `std::async` to run tasks asynchronously. This is a high-level abstraction that allows the library to manage threads and their lifetimes automatically. Specifically, it uses `std::launch::async` to ensure that the tasks are executed asynchronously.

If you play with the parameters, you will notice that the performance of the `thread` and `async` implementations is very similar or the same. It's because `std::async(std::launch::async)` creates an individual thread for each task, which is exactly the same as the `thread` implementation. And `std::async(std::launch::deferred)` is not used here, as it would defer the execution to be run in the same thread that calls `get()` on the future, which would not be concurrent.
