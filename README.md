### NetWeave

A multithreaded C++ chat server with a worker pool and client

### Demo
https://github.com/Mayon-Francis/NetWeave/assets/75270610/fb5f2302-5c89-461d-9120-c1475ac306b1

### Requirements for now

-   g++, gdb, cmake, build-essentials, ninja-build

This C++ implementation of the server mainly focuses on managing active clients via a [WorkerPool](https://github.com/Mayon-Francis/NetWeave/blob/main/src/libs/worker/pool.cpp) and Epoll for scalability.

For an implementation of the same I wrote using C and pthreads, checkout [this](https://github.com/Mayon-Francis/CN_LAB_S6/tree/main/4_chatServer).
