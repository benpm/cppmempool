// Defines a memory allocator that allocates objects of a given type in contiguous blocks of memory
#pragma once

#include <variant>
#include <cstddef>
#include <functional>
#include <atomic>
#include <tuple>
#include <any>
#include <memory>
#include <typeindex>
#include <memory_resource>
#include <type_traits>
#include <concepts>
#include <any>
#include <utility>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <bitset>
#include <filesystem>
#include <list>

template <typename T> class FlatHeap;
template <typename T> class FlatItr;

/**
 * @brief An allocator that allocates objects of type T in contiguous blocks of memory
 *
 * @tparam T The stored type
 */
template <typename T> class FlatAllocator
{
    /// TODO: Use std::vector<size_t> to store skip list : store n such that i+n is empty

   public:
    /*AllocatorTraits*/ using value_type = T;
    /*AllocatorTraits*/ using pointer = T*;
    /*AllocatorTraits*/ using const_pointer = const T*;
    /*AllocatorTraits*/ using size_type = size_t;

    constexpr static size_t _blockTotalBytes = 4096u * 8u;

   private:
    struct BlockHeader
    {
        // The index of this block in the block pointers vector
        const size_t blockIdx = 0u;
        // A free index ahead of which is all empty, if this is _blockLen then the block is full
        size_t nextEmpty = 0u;
        // A free index somewhere in the block, behind nextEmpty
        size_t prevEmpty = 0u;
        // Indicates which indices are used
        std::bitset<_blockTotalBytes / sizeof(T)> used = { 0 };

        BlockHeader() = delete;
        BlockHeader(const size_t blockIdx) : blockIdx(blockIdx) {}
    };

    static_assert(
        _blockTotalBytes >= sizeof(T) + sizeof(BlockHeader),
        "block size too small for object"
    );

    // Number of objects stored per block
    static constexpr size_t _blockLen = ((_blockTotalBytes - sizeof(BlockHeader)) / sizeof(T)) - 1u;
    static_assert(_blockLen > 0u, "block size too small");

    struct Block : public BlockHeader
    {
        T data;

        Block() = delete;
        Block(const size_t blockIdx) : BlockHeader(blockIdx){}

        bool full() const { return this->nextEmpty >= _blockLen; }
        bool empty() const { return this->nextEmpty == 0u; }
        pointer allocate()
        {

            const size_t idx = this->prevEmpty;

            if (idx == this->nextEmpty) {
                this->nextEmpty += 1u;
            }
            if (this->prevEmpty < this->nextEmpty && !this->used[this->prevEmpty + 1u]) {
                this->prevEmpty += 1u;
            } else {
                this->prevEmpty = this->nextEmpty;
            }

            this->used[idx] = true;
            return this->objs() + idx;
        }
        void erase(const_pointer p)
        {
            size_t i = this->getIdx(p);

            this->used[i] = false;

            if (this->nextEmpty - 1u == i) {
                this->nextEmpty = i;
            }
            this->prevEmpty = std::min(this->prevEmpty, i);
        }
        // Returns the object index of given pointer within this block
        size_t getIdx(const_pointer p) const
        {
            const size_t i = (p - this->objs());
            return i;
        }
        // Frees all objects in this block
        void clear()
        {
            this->nextEmpty = 0u;
            this->prevEmpty = 0u;
            this->used.reset();
        }
        // Pointer to start of objects
        inline const_pointer objs() const { return &this->data; }
        inline pointer objs() { return &this->data; }
    };

    inline static std::pmr::monotonic_buffer_resource monoBufferResource;
    inline static std::pmr::polymorphic_allocator<Block> blockAllocator{ &monoBufferResource };

    // Blocks of memory, indexed by block indices
    std::list<Block*> blockList;
    // Block index -> pointer to block. !THIS VECTOR NEVER SHRINKS!
    std::vector<Block*> blockPtrs;

    // Gets the latest block
    inline const Block* curBlock() const { return this->blockList.front(); }
    inline Block* curBlock() { return this->blockList.front(); }
    // Gets the block that contains the given pointer - just convert the address to the aligned
    // block address
    constexpr Block* getBlock(const T* ptr) const
    {
        return (Block*)(void*)(((size_t)((void*)ptr) / _blockTotalBytes) * _blockTotalBytes);
    }
    // Gets the block from the given global stored object index
    inline Block* getBlock(size_t objIdx) const
    {
        return this->blockPtrs[objIdx / _blockLen];
    }
    // Get object index from pointer
    inline size_t getIdx(const T* ptr) const
    {
        const Block* block = this->getBlock(ptr);
        return block->blockIdx * _blockLen + block->getIdx(ptr);
    }
    // Gets the object pointer from the global index
    T* getPtr(size_t objIdx) { return &this->getBlock(objIdx)->objs()[objIdx % _blockLen]; }
    // Gets object pointer by the global index
    const T* getPtr(size_t idx) const { return &this->getBlock(idx)->objs()[idx % _blockLen]; }
    // Creates a new block, which becomes latest block
    Block* newBlock()
    {
        Block* ptr = new ((Block*)blockAllocator.allocate_bytes(_blockTotalBytes, _blockTotalBytes)) Block(this->blockList.size());
        this->blockList.push_front(ptr);
        return this->blockPtrs.emplace_back(ptr);
    }
    // Deletes the block currently being allocated into
    void deleteCurBlock()
    {
        blockAllocator.deallocate_bytes((void*)this->curBlock(), _blockTotalBytes, _blockTotalBytes);
        this->blockList.pop_front();
    }

   public:
    FlatAllocator()
    {
        this->blockPtrs.reserve(128u);
        this->newBlock();
    }

    // Allocates space for one object
    pointer allocate()
    {
        if (this->curBlock()->full()) {
            this->newBlock();
        }
        return this->curBlock()->allocate();
    }
    // Deallocates given pointer to object
    void deallocate(pointer p)
    {
        Block* block = this->getBlock(p);
        block->erase(p);
        if (block == this->curBlock() && this->blockList.size() > 1u && block->empty()) {
            this->deleteCurBlock();
        }
    }
    // Deallocates all allocated memory
    void clear()
    {
        this->blockPtrs.clear();
        this->blockList.clear();
        this->newBlock();
    }

    friend class FlatHeap<T>;
    friend class FlatItr<T>;
};

// Add allocator traits to FlatAllocator
template <typename T> class std::allocator_traits<FlatAllocator<T>>
{
};

/**
 * @brief Iterator for FlatAllocator
 *
 * @tparam T
 * @tparam Allocator
 */
template <typename T> class FlatItr
{
   public:
    /* IteratorTraits */ using difference_type = std::ptrdiff_t;
    /* IteratorTraits */ using value_type = T;
    /* IteratorTraits */ using pointer = T*;
    /* IteratorTraits */ using reference = T&;
    /* IteratorTraits */ using iterator_category = std::random_access_iterator_tag;
    /* IteratorTraits */ using iterator_concept = std::random_access_iterator_tag;

   protected:
    const FlatAllocator<T>* alloc = nullptr;
    std::ptrdiff_t idx = 0u;

   public:
    static FlatItr<T> begin(const FlatAllocator<T>* alloc) { return { alloc, 0u }; }
    static FlatItr<T> end(const FlatAllocator<T>* alloc)
    {
        return { alloc, (std::ptrdiff_t)alloc->size() };
    }

    // Creates new iterator at the beginning of the given allocator
    FlatItr(const FlatAllocator<T>* alloc) : alloc(alloc){};
    // Creates new iterator at the given index within the allocator
    FlatItr(const FlatAllocator<T>* alloc, std::ptrdiff_t idx) : alloc(alloc), idx(idx) {}
    FlatItr() = default;

    // Gets the pointer relative to this iterator
    pointer get(std::ptrdiff_t n = 0) const
    {
        return (pointer)this->alloc->getPtr(this->idx + n);
    }

    FlatItr<T>& operator+=(std::ptrdiff_t n)
    {
        this->idx += n;
        return *this;
    }
    FlatItr<T>& operator-=(std::ptrdiff_t n)
    {
        this->idx -= n;
        return *this;
    }

    reference operator[](std::ptrdiff_t n) const { return *this->get(n); }

    FlatItr<T>& operator++() { return *this += 1; }
    FlatItr<T> operator++(int) { return *this + 1; }
    FlatItr<T>& operator--() { return *this -= 1; }
    FlatItr<T> operator--(int) { return *this - 1; }

    reference operator*() const { return *this->get(); }
    pointer operator->() const { return this->get(); }

    friend bool operator==(const FlatItr<T>& lhs, const FlatItr<T>& rhs)
    {
        return lhs.alloc == rhs.alloc && lhs.idx == rhs.idx;
    }
    friend bool operator!=(const FlatItr<T>& lhs, const FlatItr<T>& rhs)
    {
        return lhs.alloc == rhs.alloc && lhs.idx != rhs.idx;
    }
    friend bool operator<(const FlatItr<T>& lhs, const FlatItr<T>& rhs)
    {
        return lhs.alloc == rhs.alloc && lhs.idx < rhs.idx;
    }
    friend bool operator>(const FlatItr<T>& lhs, const FlatItr<T>& rhs)
    {
        return lhs.alloc == rhs.alloc && lhs.idx > rhs.idx;
    }
    friend bool operator<=(const FlatItr<T>& lhs, const FlatItr<T>& rhs) { return !(lhs > rhs); }
    friend bool operator>=(const FlatItr<T>& lhs, const FlatItr<T>& rhs) { return !(lhs < rhs); }
    friend FlatItr<T> operator+(const FlatItr<T>& itr, std::ptrdiff_t n)
    {
        return { itr.alloc, itr.idx + n };
    }
    friend FlatItr<T> operator+(std::ptrdiff_t n, const FlatItr<T>& itr)
    {
        return { itr.alloc, itr.idx + n };
    }
    friend FlatItr<T> operator-(const FlatItr<T>& itr, std::ptrdiff_t n)
    {
        return { itr.alloc, itr.idx - n };
    }
    friend std::ptrdiff_t operator-(const FlatItr<T>& lhs, const FlatItr<T>& rhs)
    {
        return lhs.idx - rhs.idx;
    }
};

struct DynBitset {
    std::vector<uint8_t> data;
    // Size in bits
    size_t size = 0u;

    DynBitset() = default;
    DynBitset(size_t size) : size(size) {
        this->resize(size);
    }

    inline bool operator[](size_t idx) const {
        return this->data[idx / 8u] & (1u << (idx % 8u));
    }
    inline void set(size_t idx) {
        this->data[idx / 8u] |= (1u << (idx % 8u));
    }
    inline void unset(size_t idx) {
        this->data[idx / 8u] &= ~(1u << (idx % 8u));
    }
    inline void resize(size_t size) {
        this->data.resize((size + 7u) / 8u, 0u);
        this->size = size;
    }
    inline void clear() {
        this->data.clear();
        this->data.resize(this->size, 0u);
        this->size = 0u;
    }
};

/**
 * @brief A basic data structure that allocates objects of type T in contiguous blocks of memory.
 *
 *
 * @tparam T
 */
template <typename T> class FlatHeap
{
   private:
    size_t _size = 0u;
    DynBitset used{4096u};
    FlatAllocator<T> alloc;

   public:
    using iterator_t = FlatItr<T>;
    static_assert(std::random_access_iterator<iterator_t>);

    void erase(T* p, size_t idx)
    {
        this->alloc.deallocate(p);
        this->used.unset(idx);
    }

    void erase(iterator_t itr) { 
        this->erase(itr.get(), itr - this->begin());
    }

    void erase(T* p) {
        this->erase(p, this->alloc.getIdx(p));
    }

    inline const T* get(size_t idx) const { return this->alloc.getPtr(idx); }
    inline T* get(size_t idx) { return (T*)this->alloc.getPtr(idx); }

    T* operator[](size_t idx) const { return this->get(idx); }

    bool contains(size_t idx) const { return this->used[idx]; }

    template <typename... TArgs> T* emplace(TArgs&&... __args)
    {
        const size_t idx = this->size();
        if (this->size() >= this->used.size) {
            this->used.resize(this->used.size * 2u);
        }
        this->used.set(idx);
        this->_size += 1u;
        T* ptr = this->alloc.allocate();
        return new (ptr) T(std::forward<TArgs>(__args)...);
    }

    template <typename... TArgs> std::shared_ptr<T> makeShared(TArgs&&... __args)
    {
        const size_t idx = this->size();
        return std::shared_ptr<T>(
            this->emplace(std::forward<TArgs>(__args)...),
            [&](T* p) { this->erase(p); }
        );
    }

    void clear() {
        this->used.clear();
        this->alloc.clear();
        this->_size = 0u;
    }

    size_t size() const { return this->_size; }

    iterator_t begin() const { return { &this->alloc, 0u }; }
    iterator_t end() const { return { &this->alloc, (std::ptrdiff_t)this->size() }; }
};