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

int main(int argc, char const *argv[]) {
    constexpr size_t N = 10000000;
    std::mt19937 gen(1234);
    std::uniform_int_distribution<> dis(0, N-1);
    std::chrono::high_resolution_clock clock;
    std::vector<std::shared_ptr<Item>> list(N);

    std::cout << "N = " << N << std::endl;

    auto t = clock.now();

    {// Bench memory pool
        std::cout << "\nWITH MEMORY POOL" << std::endl;
        MemPool pool;
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = pool.emplace<Item>("object", i);
            }
            std::cout << "initial insert: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                list[dis(gen) % list.size()] = nullptr;
            }
            std::cout << "random removal: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = pool.emplace<Item>("object", dis(gen));
                }
            }
            std::cout << "second insert: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            std::cout << "random access: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            std::cout << "sequential access: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
            std::cout << "  ... sum is " << sum << std::endl;
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            list[i] = nullptr;
        }
    }
    std::cout << "destruction: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;


    {// Bench memory pool
        std::cout << "\nWITHOUT MEMORY POOL" << std::endl;
        MemPool pool;
        {// Initial insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[i] = std::make_shared<Item>("object", i);
            }
            std::cout << "initial insert: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Random removal
            t = clock.now();
            for (size_t i = 0; i < N/2; i++) {
                list[dis(gen) % list.size()] = nullptr;
            }
            std::cout << "random removal: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Insertion  
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                if (list[i] == nullptr) {
                    list[i] = std::make_shared<Item>("object", dis(gen));
                }
            }
            std::cout << "second insert: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Random access: randomly assign new values to object fields
            t = clock.now();
            for (size_t i = 0; i < N; i++) {
                list[dis(gen)]->val = i;
            }
            std::cout << "random access: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
        }
        {// Sequential access: access object fields sequentially, summing all values
            t = clock.now();
            size_t sum = 0;
            for (size_t i = 0; i < N; i++) {
                sum += list[i]->val;
            }
            std::cout << "sequential access: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;
            std::cout << "  ... sum is " << sum << std::endl;
        }
        // Destruction
        t = clock.now();
        for (size_t i = 0; i < N; i++) {
            list[i] = nullptr;
        }
    }
    std::cout << "destruction: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - t).count() << "ms" << std::endl;

    return 0;
}
