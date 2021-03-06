# :floppy_disk: MemPool
A *very* simple (only ~150 lines) C++ thread safe heterogenous header-only memory pool class with smart pointer support. Use this if you want to eliminate frequent allocations for **small, short-lived** objects.

## Features
- Heterogenous types: use any mix of any type in the pool
- Basic thread safety using `std::mutex`
- Support for directly creating smart pointers (that's actually all it can do rn... working on it)
- No dependencies! Not that that's surprising
- Configured through template arguments

## Limitations
With the implementation being this simple, there are definitely some **significant tradeoffs**:
- Objects allocated cannot be larger than `Chunk::size` (default is 8192 bytes). You *should* get a compiler error if you try this
- Unused space in allocated chunks is *not reused* until *the entire chunk is empty!* So if some of your objects have long lifetimes, expect your memory usage to just continue growing as you make more allocations in the pool.
- Blocks are currently not deallocated when empty, but they are re-used
- Requires C++11

## Usage

```C++
#include <iostream>
#include <mempool.hpp>

using namespace benpm;

struct Foo {
    std::string name;
    int hp;
    Foo(std::string name, int hp) : name(name), hp(hp) {}
};

struct Bar {
    float val;
    Bar(float val) : val(val) {}
};

int main(void) {
    MemPool pool;
    std::shared_ptr<Foo> fooA = pool.emplace<Foo>("foo A", 10);
    std::shared_ptr<Bar> barA = pool.emplace<Bar>(16.16);
    std::cout << fooA->name << std::endl;
    std::cout << barA->val << std::endl;
    ///TODO: put example for normal allocation and free
}
```
Check out benchmark.cpp for more

## How it Works
todo

## Benchmark
(This is a work in progress)

| operation                    | time (pool) | time (no pool) |
| ---------------------------- | ----------- | -------------- |
| (raw) init insert            |       176ms |          190ms |
| (raw) random removal         |        51ms |           36ms |
| (raw) second insert          |        75ms |           78ms |
| (raw) random access          |       817ms |          898ms |
| (raw) sequential access      |        18ms |           22ms |
| (raw) destruction            |       101ms |           72ms |
| (shared) init insert         |       413ms |          251ms |
| (shared) random removal      |        55ms |           55ms |
| (shared) second insert       |       129ms |          108ms |
| (shared) random access       |       974ms |          930ms |
| (shared) sequential access   |        19ms |           33ms |
| (shared) destruction         |       213ms |          120ms |

## Contribution
I'm still a C++ baby so if you have some ideas for improvement, please feel free to make an issue or a PR!

### Todo
- [x] Support for non-smart pointer alloc
- [ ] `std::unique_ptr` support
- [x] Benchmarks (with both insert after and insert before)
- [ ] More complex example
- [x] Block alignment
- [ ] Explanation of how it works