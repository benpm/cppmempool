#pragma once

#include <stdlib.h>

#include <functional>
#include <list>
#include <memory>
#include <mutex>

// #define MEMPOOL_THREADSAFE
#define MEMPOOL_EMPTY_INSERT_AFTER

namespace benpm {
class MemPool {
 private:  // ------------------------------------------------------------
  struct Chunk {
    static constexpr size_t size = 4096 * 2;

    char* head;   // Next free byte in chunk
    Chunk* next;  // Next free chunk
    size_t used;  // Occupied bytes in chunk

    // Initialize chunk
    void init(Chunk* next) {
      this->next = next;
      this->used = sizeof(Chunk);
      this->head = ((char*)this) + sizeof(Chunk);
    }
    // Returns if chunk is empty and can be used for more allocations
    bool empty() const { return this->used == sizeof(Chunk); }
    // Emplace object of type T in chunk
    template <class T, class... V>
    std::shared_ptr<T> emplace(MemPool* pool, V&&... v) {
      static_assert(sizeof(T) <= Chunk::size - sizeof(Chunk), "Object is too large for chunk");
      T* obj = new (head) T(std::forward<V>(v)...);
      this->head += sizeof(T);
      this->used += sizeof(T);
      return std::shared_ptr<T>(
          obj, std::bind(&MemPool::destructHandler<T>, pool, this, obj));
    }
  };

  // Configuration --------------------------------------------------------
  static constexpr size_t chunksPerBlock = 32;
  static constexpr size_t blockSize = Chunk::size * chunksPerBlock;
  static constexpr bool emptyInsertAfter = true;
  // ----------------------------------------------------------------------

  Chunk* curChunk = nullptr;    // Non-full chunk which is currently being used
  std::vector<Chunk*> blocks;   // All allocated blocks
  std::mutex mutex;

  // Bound, called for a particular chunk and object when shared_ptr ref count
  // hits 0
  template <class T>
  void destructHandler(Chunk* chunk, T* obj) {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(this->mutex);
    #endif
    chunk->used -= sizeof(T);
    obj->~T();
    if (chunk->empty()) {
      #ifdef MEMPOOL_EMPTY_INSERT_AFTER
        // Insert self after current chunk in linked list
        chunk->next = this->curChunk->next;
        this->curChunk->next = chunk;
      #else
        // Insert self before current chunk in linked list, then make myself
        // current
        chunk->next = this->curChunk;
        this->curChunk = chunk;
      #endif
    }
  }

  // Allocates a new block of chunks
  void allocBlock() {
    Chunk* block = reinterpret_cast<Chunk*>(malloc(blockSize));
    Chunk* c = block;
    for (size_t i = 0; i < chunksPerBlock - 1; c = c->next, i++) {
      c->init(reinterpret_cast<Chunk*>(reinterpret_cast<char*>(c) + Chunk::size));
    }
    c->init(nullptr);
    if (this->curChunk != nullptr) {
      this->curChunk->next = block;
    }
    this->curChunk = block;
    blocks.push_back(block);
  }

 public:  // ------------------------------------------------------------
  MemPool() { allocBlock(); }

  ~MemPool() {
    for (Chunk* block : blocks) {
      free(block);
    }
    this->curChunk = nullptr;
  }

  /**
   * @brief Allocates object in memory pool, returns shared_ptr to object.
   * 
   * @note This function is thread-safe.
   * 
   * @tparam T The object type to allocate
   * @tparam V The argument types to pass to the constructor of T
   * @param v The arguments to pass to the constructor of T
   * @return std::shared_ptr<T> The allocated object
   */
  template <class T, class... V>
  std::shared_ptr<T> emplace(V&&... v) {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(mutex);
    #endif
    if (this->curChunk->head - (char*)this->curChunk + sizeof(T) > Chunk::size) {
      if (this->curChunk->next == nullptr) {
        allocBlock();
      } else {
        this->curChunk = this->curChunk->next;
      }
    }
    return this->curChunk->emplace<T>(this, std::forward<V>(v)...);
  }
};
}  // namespace benpm