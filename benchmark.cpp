#include <cstdio>
#include <chrono>
#include <iostream>
#include <random>
#include <benpm/mempool.hpp>

struct Item {
    std::string name;
    size_t val;
    Item(std::string name, size_t val) : name(name), val(val) {}
};

using namespace benpm;

constexpr size_t N = 10000000;
constexpr size_t expectedSum = 59772134412938;

using Times = std::array<int64_t, 6>;

Times testMemPoolShared() {
    std::mt19937 gen(1234);
    std::uniform_int_distribution<> dis(0, N-1);
    std::chrono::high_resolution_clock clock;
    std::vector<std::shared_ptr<Item>> list(N);
    auto t = clock.now();
    Times times;

    {// Bench memory pool
        MemPool<> pool;
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = pool.makeShared<Item>("object", i);
            }
            times[0] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                list[i] = nullptr;
            }
            times[1] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = pool.makeShared<Item>("object", dis(gen));
                }
            }
            times[2] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            times[3] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            if (sum != expectedSum) {
                std::cout << "Sum is " << sum << " instead of " << expectedSum << std::endl;
            }
            times[4] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            list[i] = nullptr;
        }
    }
    times[5] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
    return times;
}

Times testMemShared() {
    std::mt19937 gen(1234);
    std::uniform_int_distribution<> dis(0, N-1);
    std::chrono::high_resolution_clock clock;
    std::vector<std::shared_ptr<Item>> list(N);
    auto t = clock.now();
    Times times;

    {// Bench memory pool
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = std::make_shared<Item>("object", i);
            }
            times[0] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                list[i] = nullptr;
            }
            times[1] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = std::make_shared<Item>("object", dis(gen));
                }
            }
            times[2] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            times[3] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            if (sum != expectedSum) {
                std::cout << "Sum is " << sum << " instead of " << expectedSum << std::endl;
            }
            times[4] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            list[i] = nullptr;
        }
    }
    times[5] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
    return times;
}

Times testMemPool() {
    std::mt19937 gen(1234);
    std::uniform_int_distribution<> dis(0, N-1);
    std::chrono::high_resolution_clock clock;
    std::vector<Item*> list(N);
    auto t = clock.now();
    Times times;

    {// Bench memory pool
        MemPool<> pool;
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = pool.make<Item>("object", i);
            }
            times[0] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                pool.free(list[i]);
                list[i] = nullptr;
            }
            times[1] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = pool.make<Item>("object", dis(gen));
                }
            }
            times[2] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            times[3] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            if (sum != expectedSum) {
                std::cout << "Sum is " << sum << " instead of " << expectedSum << std::endl;
            }
            times[4] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            pool.free(list[i]);
        }
        times[5] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
    }
    return times;
}

Times testMem() {
    std::mt19937 gen(1234);
    std::uniform_int_distribution<> dis(0, N-1);
    std::chrono::high_resolution_clock clock;
    std::vector<Item*> list(N);
    auto t = clock.now();
    Times times;

    {// Bench memory pool
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = new Item("object", i);
            }
            times[0] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                delete list[i];
                list[i] = nullptr;
            }
            times[1] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = new Item("object", dis(gen));
                }
            }
            times[2] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            times[3] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            if (sum != expectedSum) {
                std::cout << "Sum is " << sum << " instead of " << expectedSum << std::endl;
            }
            times[4] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            delete list[i];
        }
    }
    times[5] = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count();
    return times;
}


int main(int argc, char const *argv[]) {
    Times t_rawPool = testMemPool();
    Times t_rawNorm = testMem();
    Times t_sharedPool = testMemPoolShared();
    Times t_sharedNorm = testMemShared();
    // Print a markdown formatted tables of all the recorded times
    printf("| operation                    | time (pool) | time (no pool) |\n");
    printf("| ---------------------------- | ----------- | -------------- |\n");
    const std::array<const char*, 6> labels = {
        "init insert",
        "random removal",
        "second insert",
        "random access",
        "sequential access",
        "destruction"
    };
    for (size_t i = 0; i < labels.size(); i++) {
        std::string l = "(raw) " + std::string(labels[i]);
        printf("| %-28s | %9ldms | %12ldms |\n", l.c_str(), t_rawPool[i], t_rawNorm[i]);
    }
    for (size_t i = 0; i < labels.size(); i++) {
        std::string l = "(shared) " + std::string(labels[i]);
        printf("| %-28s | %9ldms | %12ldms |\n", l.c_str(), t_sharedPool[i], t_sharedNorm[i]);
    }
    return 0;
}
