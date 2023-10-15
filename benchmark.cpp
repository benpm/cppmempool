#include <benpm/flat_allocator.hpp>
#include <benpm/mempool.hpp>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>

struct Item {
  std::string name;
  size_t val;
  Item(std::string name, size_t val) : name(name), val(val) {}
  Item() = default;
};

using namespace benpm;

size_t N = 1000000;
size_t expectedSum = 0;

using Times = std::array<int64_t, 6>;

int64_t clock() {
  static auto t0 = std::chrono::high_resolution_clock::now();
  auto diff = std::chrono::high_resolution_clock::now() - t0;
  t0 = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
}

struct RNG {
  RNG(uint32_t seed, uint32_t n) : generator(seed), distribution(0, n - 1) {}
  std::mt19937 generator;
  std::uniform_int_distribution<> distribution;

  size_t operator()() { return distribution(generator); }
};

std::tuple<RNG, Times, std::string> setup(const std::string& label) {
  RNG rng(1234, N);
  Times times;
  clock();
  std::cout << label << std::endl;
  return {rng, times, label};
}

void checkSum(std::string label, size_t sum) {
  if (sum != expectedSum) {
    std::cout << label << ": Sum is " << sum << " instead of " << expectedSum << std::endl;
  }
}

Times testMemPoolShared() {
  auto [gen, times, label] = setup("testMemPoolShared");

  {
    MemPool<> pool;
    std::vector<std::shared_ptr<Item>> list(N);

    // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = pool.makeShared<Item>("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      list[gen()] = nullptr;
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (list[i] == nullptr) {
        list[i] = pool.makeShared<Item>("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    checkSum(label, sum);
    times[4] = clock();

    // Destruction
    for (size_t i = 0; i < N; i++) {
      list[i] = nullptr;
    }
  }
  times[5] = clock();
  return times;
}

Times testMemShared() {
  auto [gen, times, label] = setup("testMemShared");

  {
    std::vector<std::shared_ptr<Item>> list(N);

    // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = std::make_shared<Item>("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      list[gen()] = nullptr;
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (list[i] == nullptr) {
        list[i] = std::make_shared<Item>("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    expectedSum = sum;
    times[4] = clock();

    // Destruction
    clock();
    for (size_t i = 0; i < N; i++) {
      list[i] = nullptr;
    }
  }
  times[5] = clock();
  return times;
}

Times testMemPool() {
  auto [gen, times, label] = setup("testMemPool");

  {
    MemPool<> pool;
    std::vector<Item *> list(N);

    // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = pool.make<Item>("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      const size_t j = gen();
      if (list[j] != nullptr) {
        pool.free(list[j]);
        list[j] = nullptr;
      }
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (list[i] == nullptr) {
        list[i] = pool.make<Item>("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    checkSum(label, sum);
    times[4] = clock();

    // Destruction
    clock();
    for (size_t i = 0; i < N; i++) {
      pool.free(list[i]);
    }
  }
  times[5] = clock();
  return times;
}

Times testMem() {
  auto [gen, times, label] = setup("testMem");
  std::vector<Item *> list(N);

  {  // Bench memory pool
     // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = new Item("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      const size_t j = gen();
      if (list[j] != nullptr) {
        delete list[j];
        list[j] = nullptr;
      }
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (list[i] == nullptr) {
        list[i] = new Item("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    checkSum(label, sum);
    times[4] = clock();

    // Destruction
    clock();
    for (size_t i = 0; i < N; i++) {
      delete list[i];
    }
  }
  times[5] = clock();
  return times;
}

Times testRawHeap() {
  auto [gen, times, label] = setup("testRawHeap");
  std::vector<Item *> list(N);

  {  // Bench memory pool
    FlatHeap<Item> flatHeap;
    // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = flatHeap.emplace("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      const size_t j = gen();
      if (list[j] != nullptr) {
        flatHeap.erase(list[j]);
        list[j] = nullptr;
      }
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (!flatHeap.contains(i)) {
        list[i] = flatHeap.emplace("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    checkSum(label, sum);
    times[4] = clock();

    // Destruction
    flatHeap.clear();
  }
  times[5] = clock();
  return times;
}

Times testHeapShared() {
  auto [gen, times, label] = setup("testHeapShared");
  std::vector<std::shared_ptr<Item>> list(N);

  {  // Bench memory flatHeap
    FlatHeap<Item> flatHeap;
    // Initial insertion
    for (size_t i = 0; i < N; i++) {
      list[i] = flatHeap.makeShared("object", i);
    }
    times[0] = clock();

    // Random removal
    for (size_t i = 0; i < N / 2; i++) {
      list[gen()] = nullptr;
    }
    times[1] = clock();

    // Insertion
    for (size_t i = 0; i < N; i++) {
      if (list[i] == nullptr) {
        list[i] = flatHeap.makeShared("object", gen());
      }
    }
    times[2] = clock();

    // Random access: randomly assign new values to object fields
    for (size_t i = 0; i < N; i++) {
      list[gen()]->val = i;
    }
    times[3] = clock();

    // Sequential access: access object fields sequentially, summing all values
    size_t sum = 0;
    for (size_t i = 0; i < N * 8; i++) {
      sum += list[i % N]->val;
    }
    checkSum(label, sum);
    times[4] = clock();

    // Destruction
    clock();
    for (size_t i = 0; i < N; i++) {
      list[i] = nullptr;
    }
  }
  times[5] = clock();
  return times;
}

int main(int argc, char const *argv[]) {
  std::cout << getchar() << std::endl;

  // Read N as argument
  if (argc > 1) {
    N = std::atoi(argv[1]);
  }
  std::cout << "starting with N=" << N << std::endl;
  std::cout << std::flush;

  Times t_sharedNorm = testMemShared();
  Times t_rawPool = testMemPool();
  Times t_sharedPool = testMemPoolShared();
  Times t_rawHeap = testRawHeap();
  Times t_sharedHeap = testHeapShared();
  Times t_rawNorm = testMem();
  // Print a markdown formatted tables of all the recorded times
  printf("|                              | pool        | no pool        | flatheap\n");
  printf("| ---------------------------- | ----------- | -------------- | ---------------\n");
  const std::array<const char *, 6> labels = {"init insert",   "random removal",    "second insert",
                                              "random access", "sequential access", "destruction",};
  for (size_t i = 0; i < labels.size(); i++) {
    std::string l = "(raw) " + std::string(labels[i]);
    printf("| %-28s | %9ldms | %12ldms | %12ldms |\n", l.c_str(), t_rawPool[i], t_rawNorm[i], t_rawHeap[i]);
  }
  for (size_t i = 0; i < labels.size(); i++) {
    std::string l = "(shared) " + std::string(labels[i]);
    printf("| %-28s | %9ldms | %12ldms | %12ldms |\n", l.c_str(), t_sharedPool[i], t_sharedNorm[i], t_sharedHeap[i]);
  }
  return 0;
}
