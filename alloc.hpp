#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>

template <class T>
class poolAllocator{
private:
    std::size_t objectCount;

    static constexpr std::size_t slotSize =
        sizeof(T) > sizeof(void *) ? sizeof(T) : sizeof(void *);
    static constexpr std::size_t slotAlign =
        alignof(T) > alignof(void *) ? alignof(T) : alignof(void *);

    struct alignas(slotAlign) slot{
        unsigned char storage[slotSize];
    };

    // A free slot keeps the next free-list link in the bytes a live T would occupy.
    static slot *&nextFreeLink(slot *freeSlot) noexcept{
        return *reinterpret_cast<slot **>(freeSlot->storage);
    }

    bool isSlotInFreeList(const slot *wanted) const noexcept{
        for(slot *node = freeListHead; node; node = nextFreeLink(node)){
            if(node == wanted){
                return true;
            }
        }
        return false;
    }

    slot *blockStart;
    slot *nextUnbuilt;
    slot *freeListHead;
    slot *blockEnd;

public:
    explicit poolAllocator(std::size_t slotCount) noexcept{
        blockStart = new (std::nothrow) slot[slotCount];
        nextUnbuilt = blockStart;
        blockEnd = blockStart + (blockStart ? slotCount : 0);
        objectCount = 0;
        freeListHead = nullptr;
    }

    ~poolAllocator() noexcept{
        delete[] blockStart;
    }

    poolAllocator(const poolAllocator &) = delete;
    poolAllocator &operator=(const poolAllocator &) = delete;

    poolAllocator(poolAllocator &&other) noexcept
        : objectCount(other.objectCount), blockStart(other.blockStart),
          nextUnbuilt(other.nextUnbuilt), freeListHead(other.freeListHead),
          blockEnd(other.blockEnd){
        other.blockStart = other.nextUnbuilt = other.freeListHead = other.blockEnd = nullptr;
        other.objectCount = 0;
    }

    poolAllocator &operator=(poolAllocator &&other) noexcept{
        poolAllocator tmp(std::move(other));
        std::swap(objectCount, tmp.objectCount);
        std::swap(blockStart, tmp.blockStart);
        std::swap(nextUnbuilt, tmp.nextUnbuilt);
        std::swap(freeListHead, tmp.freeListHead);
        std::swap(blockEnd, tmp.blockEnd);
        return *this;
    }

    template <class... Args>
    T *allocate(Args &&...args) noexcept{
        slot *chosenSlot = nullptr;

        if(freeListHead){
            chosenSlot = freeListHead;
            freeListHead = nextFreeLink(freeListHead);
        }
        else if(nextUnbuilt < blockEnd){
            chosenSlot = nextUnbuilt;
            nextUnbuilt++;
        }
        else{
            return nullptr;
        }

        T *obj = ::new(static_cast<void *>(chosenSlot->storage)) T(std::forward<Args>(args)...);
        objectCount++;
        return obj;
    }

    void destroy(T *obj) noexcept{
        slot *receivedSlot = reinterpret_cast<slot *>(obj);
        assert(receivedSlot >= blockStart && receivedSlot < nextUnbuilt
               && "pointer is not from this pool");

        obj->~T();
        nextFreeLink(receivedSlot) = freeListHead;
        freeListHead = receivedSlot;
        objectCount--;
    }

    void reset() noexcept{
        for(slot *current = blockStart; current < nextUnbuilt; current++){
            if(!isSlotInFreeList(current)){
                std::launder(reinterpret_cast<T *>(current->storage))->~T();
            }
        }
        nextUnbuilt = blockStart;
        freeListHead = nullptr;
        objectCount = 0;
    }

    std::size_t size() const noexcept{
        return objectCount;
    }

    std::size_t capacity() const noexcept{
        return static_cast<std::size_t>(blockEnd - blockStart);
    }
};
