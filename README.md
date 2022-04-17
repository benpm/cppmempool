# :floppy_disk: MemPool
A *very* simple (only ~150 lines) C++ thread safe heterogenous header-only memory pool class with smart pointer support. Use this if you want to eliminate frequent allocations for **small, short-lived** objects.

## Features
- Heterogenous types: use any mix of any type in the pool
- Basic thread safety using `std::mutex`
- Support for directly creating smart pointers (that's actually all it can do rn... working on it)
- No dependencies! Not that that's surprising

## Limitations
With the implementation being this simple, there are definitely some **significant tradeoffs**:
- Objects allocated cannot be larger than `Chunk::size` (default is 8192 bytes). You *should* get a compiler error if you try this
- Unused space in allocated chunks is *not reused* until *the entire chunk is empty!* So if some of your objects have long lifetimes, expect your memory usage to just continue growing as you make more allocations in the pool.
- Blocks are currently not deallocated when empty, but they are re-used

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
Performance gains over normal allocation in allocating new objects in old blocks, as well as sequential memory access. This project is WIP so these numbers should hopefully look better soon?

```
N = 10000000

RAW POINTER WITH MEMORY POOL
initial insert:    155ms
half removal:       58ms
second insert:      78ms
random access:     899ms
sequential access:  19ms
destruction:       111ms

RAW POINTER
initial insert:    199ms
half removal:       34ms
second insert:     100ms
random access:     847ms
sequential access:  28ms
destruction:       103ms

SHARED_PTR WITH MEMORY POOL
initial insert:    346ms
half removal:       71ms
second insert:     113ms
random access:     950ms
sequential access:  21ms
destruction:       166ms

SHARED_PTR WITHOUT MEMORY POOL
initial insert:    165ms
half removal:       56ms
second insert:      86ms
random access:     1051ms
sequential access:  35ms
destruction:       116ms
```

## Contribution
I'm still a C++ baby so if you have some ideas for improvement, please feel free to make an issue or a PR!

### Todo
- [x] Support for non-smart pointer alloc
- [ ] `std::unique_ptr` support
- [x] Benchmarks (with both insert after and insert before)
- [ ] More complex example
- [x] Block alignment
- [ ] Explanation of how it works