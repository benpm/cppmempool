#pragma once

#include <cassert>
#include <cstdlib>

#include <unordered_map>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

#define MEMPOOL_THREADSAFE
// #define MEMPOOL_EMPTY_INSERT_AFTER

namespace benpm {
class MemPool {
 private:  // ------------------------------------------------------------

  // Configuration --------------------------------------------------------
  static constexpr size_t chunkSize = 8192;
  static constexpr size_t chunksPerBlock = 32;
  static constexpr size_t blockSize = chunkSize * chunksPerBlock;
  // ----------------------------------------------------------------------

  struct Chunk {
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
    std::shared_ptr<T> makeShared(MemPool* pool, V&&... v) {
      return std::shared_ptr<T>(
        this->make<T>(pool, std::forward<V>(v)...),
        [pool,this](T* o){ pool->destructHandler<T>(this, o); });
    }

    template <class T, class... V>
    T* make(MemPool* pool, V&&... v) {
      static_assert(sizeof(T) <= chunkSize - sizeof(Chunk), "Object is too large for chunk");
      T* obj = new (head) T(std::forward<V>(v)...);
      this->head += sizeof(T);
      this->used += sizeof(T);
      return obj;
    }
  };

  // Non-full chunk which is currently being used
  Chunk* curChunk = nullptr;
  // Map from block index to block pointer
  std::unordered_map<size_t, Chunk*> blocks;
  // Mutex for thread safety
  mutable std::mutex mutex;

  // Bound, called for a particular chunk and object when shared_ptr ref count
  // hits 0
  template <class T>
  void destructHandler(Chunk* chunk, T* obj) {
    assert(this->contains(obj));
    assert(this->inChunk(obj, chunk));
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(this->mutex);
    #endif
    chunk->used -= sizeof(T);
    obj->~T();
    if (chunk->empty()) {
      chunk->head = ((char*)chunk) + sizeof(Chunk);
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
    Chunk* block = (Chunk*)(aligned_alloc(blockSize, blockSize));
    assert((size_t)(char*)block % blockSize == 0);
    Chunk* c = block;
    for (size_t i = 0; i < chunksPerBlock - 1; c = c->next, i++) {
      c->init((Chunk*)((char*)(c) + chunkSize));
    }
    c->init(nullptr);
    if (this->curChunk != nullptr) {
      this->curChunk->next = block;
    }
    this->curChunk = block;
    assert(blocks.count(getBlockIdx(block)) == 0);
    blocks.emplace(getBlockIdx(block), block);
  }

  // Returns the block index of a memory address
  size_t getBlockIdx(void* ptr) const {
    return (size_t)(char*)ptr / blockSize;
  }

  // Returns true if given memory address resides in this pool
  bool contains(void* ptr) const {
    return this->blocks.count(this->getBlockIdx(ptr));
  }

  // Returns true if given memory address resides in given chunk
  bool inChunk(void* ptr, Chunk* chunk) const {
    return (char*)ptr >= (char*)chunk &&
           (char*)ptr < (char*)chunk + chunkSize;
  }

 public:  // ------------------------------------------------------------
  MemPool() { allocBlock(); }

  ~MemPool() {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(mutex);
    #endif
    for (auto it : blocks) {
      std::free(it.second);
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
  std::shared_ptr<T> makeShared(V&&... v) {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(mutex);
    #endif
    if (this->curChunk->head - (char*)this->curChunk + sizeof(T) >= chunkSize) {
      if (this->curChunk->next == nullptr) {
        this->allocBlock();
      } else {
        this->curChunk = this->curChunk->next;
      }
    }
    return this->curChunk->makeShared<T>(this, std::forward<V>(v)...);
  }

  /**
   * @brief Allocates object in memory pool, returns pointer to object
   * 
   * @note This function is thread-safe.
   * 
   * @tparam T The object type to allocate
   * @tparam V The argument types to pass to the constructor of T
   * @param v The arguments to pass to the constructor of T
   * @return T* The allocated object
   */
  template <class T, class... V>
  T* make(V&&... v) {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(mutex);
    #endif
    if (this->curChunk->head - (char*)this->curChunk + sizeof(T) >= chunkSize) {
      if (this->curChunk->next == nullptr) {
        this->allocBlock();
      } else {
        this->curChunk = this->curChunk->next;
      }
    }
    return this->curChunk->make<T>(this, std::forward<V>(v)...);
  }

  /**
   * @brief Frees object from memory pool
   * 
   * @tparam T Object type
   * @param obj Pointer to object that was alloc'd in this pool
   */
  template <class T>
  void free(T* obj) {
    Chunk* chunk;
    {
      #ifdef MEMPOOL_THREADSAFE
        std::lock_guard<std::mutex> lock(mutex);
      #endif
      assert(this->contains(obj));
      const size_t blockIdx = this->getBlockIdx((void*)obj);
      assert(blocks.count(blockIdx));
      Chunk* block = blocks.at(blockIdx);
      const size_t chunkIdx = ((size_t)((char*)obj - (char*)block) / chunkSize);
      chunk = (Chunk*)(((char*)block) + chunkIdx * chunkSize);
    }
    this->destructHandler<T>(chunk, obj);
  }

  /**
   * @brief Returns the number of blocks allocated
   * 
   * @return size_t
   */
  size_t getNumBlocks() const {
    #ifdef MEMPOOL_THREADSAFE
      std::lock_guard<std::mutex> lock(mutex);
    #endif
    return this->blocks.size();
  }
};
}  // namespace benpm