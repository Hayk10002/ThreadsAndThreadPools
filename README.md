# ParallelSummation

## Table of Contents
- [Introduction](#introduction)
- [Build and Run](#build-and-run)
- [Possible Output](#possible-output)
- [How does this work](#how-does-this-work)

## Introduction
This project demonstrates the performance differences between sequential summation, multithreaded summation with atomics and with mutexes. It explores how overall vector size and chunk size can affect the performance 

## Build and Run
To clone and run this project, you'll need [Git](https://git-scm.com) and [CMake](https://cmake.org/) installed on your computer. From your command line:

```bash
# Clone this repository
$ git clone https://github.com/Hayk10002/ParallelSummation

# Go into the repository
$ cd ParallelSummation

# Generate the build files
$ cmake -DCMAKE_BUILD_TYPE=Release -S . -B build

# Build the project
$ cmake --build build --config Release

# Then, run the executable generated in the `build` directory with the iteration count to test the counters.
$ your/path/to/exe/main.exe {vector_size} {chunk_size}
# example - .../main.exe 500000000 10000
```

## Possible Output
(for vector size 500000000 and chunk size 10000)

```
Vector size: 500000000, Chunk size: 10000
One_thread: Time:   110ms, Sum:  332860
Atomic    : Time:    65ms, Sum:  332860
Mutex     : Time:    65ms, Sum:  332860
--------------------------
```

## How does this work
This project evaluates the summation time of three algorithms, first one is sequential, second is mutlithreaded and uses atomic variables, and third one is multithreaded and uses mutexes. The work is done with chunks, a thread takes a chunk and sums the elements and then it can pick another chunk. Tests show that when using small chunks (smaller than like 1000), multithreaded algorithms are almost no faster than the sequential one, and when using big chunks, both atomic and mutex take almost the same time, but are faster (x2 on my case) than the sequential one.
