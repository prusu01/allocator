#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "alloc.hpp"

struct payload{
    std::uint64_t id;
    double x;
    char pad[16];
};

// Summing the addresses keeps the allocations observable. Checksumming only the
// contents is not enough: the compiler then deletes the new/delete pairs outright.
std::uint64_t checksum = 0;

using steadyClock = std::chrono::steady_clock;

const int warmupRuns = 1;
const int timedRuns = 5;

const std::size_t objectCount = 1000000;
const std::size_t batchSize = 64;

static_assert(objectCount % batchSize == 0, "the random scenario walks whole batches");

std::size_t heapBytes(){
    const struct mallinfo2 info = mallinfo2();
    return info.uordblks + info.hblkhd;
}

double bytesPerObjectPool(std::vector<payload *> &payloadPtrs){
    const std::size_t before = heapBytes();
    poolAllocator<payload> pool(objectCount);
    for(std::size_t i = 0; i < objectCount; i++){
        payloadPtrs[i] = pool.allocate();
    }
    const std::size_t used = heapBytes() - before;

    pool.reset();
    return static_cast<double>(used) / objectCount;
}

double bytesPerObjectNew(std::vector<payload *> &payloadPtrs){
    const std::size_t before = heapBytes();
    for(std::size_t i = 0; i < objectCount; i++){
        payloadPtrs[i] = new payload();
    }
    const std::size_t used = heapBytes() - before;

    for(std::size_t i = 0; i < objectCount; i++){
        delete payloadPtrs[i];
    }
    return static_cast<double>(used) / objectCount;
}

double bytesPerObjectStd(std::vector<payload *> &payloadPtrs){
    std::allocator<payload> alloc;

    const std::size_t before = heapBytes();
    for(std::size_t i = 0; i < objectCount; i++){
        payloadPtrs[i] = std::construct_at(alloc.allocate(1));
    }
    const std::size_t used = heapBytes() - before;

    for(std::size_t i = 0; i < objectCount; i++){
        std::destroy_at(payloadPtrs[i]);
        alloc.deallocate(payloadPtrs[i], 1);
    }
    return static_cast<double>(used) / objectCount;
}

double nsPerOp(steadyClock::time_point start, steadyClock::time_point stop, std::size_t ops){
    const double elapsed = std::chrono::duration<double, std::nano>(stop - start).count();
    return elapsed / static_cast<double>(ops);
}

double randomPool(std::vector<payload *> &live, const std::vector<std::uint32_t> &order){
    poolAllocator<payload> pool(objectCount);
    double best = 1e18;

    for(int run = 0; run < warmupRuns + timedRuns; run++){
        for(std::size_t i = 0; i < objectCount; i++){
            live[i] = pool.allocate();
        }

        const auto start = steadyClock::now();
        for(std::size_t i = 0; i < objectCount; i += batchSize){
            for(std::size_t j = i; j < i + batchSize; j++){
                pool.destroy(live[order[j]]);
            }
            for(std::size_t j = i; j < i + batchSize; j++){
                payload *p = pool.allocate();
                p->id = j;
                checksum += reinterpret_cast<std::uintptr_t>(p) + p->id;
                live[order[j]] = p;
            }
        }
        const auto stop = steadyClock::now();

        if(run >= warmupRuns){
            best = std::min(best, nsPerOp(start, stop, objectCount));
        }
        pool.reset();
    }
    return best;
}

double randomNew(std::vector<payload *> &live, const std::vector<std::uint32_t> &order){
    double best = 1e18;

    for(int run = 0; run < warmupRuns + timedRuns; run++){
        for(std::size_t i = 0; i < objectCount; i++){
            live[i] = new payload();
        }

        const auto start = steadyClock::now();
        for(std::size_t i = 0; i < objectCount; i += batchSize){
            for(std::size_t j = i; j < i + batchSize; j++){
                delete live[order[j]];
            }
            for(std::size_t j = i; j < i + batchSize; j++){
                payload *p = new payload();
                p->id = j;
                checksum += reinterpret_cast<std::uintptr_t>(p) + p->id;
                live[order[j]] = p;
            }
        }
        const auto stop = steadyClock::now();

        if(run >= warmupRuns){
            best = std::min(best, nsPerOp(start, stop, objectCount));
        }
        for(std::size_t i = 0; i < objectCount; i++){
            delete live[i];
        }
    }
    return best;
}

double randomStd(std::vector<payload *> &live, const std::vector<std::uint32_t> &order){
    std::allocator<payload> alloc;
    double best = 1e18;

    for(int run = 0; run < warmupRuns + timedRuns; run++){
        for(std::size_t i = 0; i < objectCount; i++){
            live[i] = std::construct_at(alloc.allocate(1));
        }

        const auto start = steadyClock::now();
        for(std::size_t i = 0; i < objectCount; i += batchSize){
            for(std::size_t j = i; j < i + batchSize; j++){
                std::destroy_at(live[order[j]]);
                alloc.deallocate(live[order[j]], 1);
            }
            for(std::size_t j = i; j < i + batchSize; j++){
                payload *p = std::construct_at(alloc.allocate(1));
                p->id = j;
                checksum += reinterpret_cast<std::uintptr_t>(p) + p->id;
                live[order[j]] = p;
            }
        }
        const auto stop = steadyClock::now();

        if(run >= warmupRuns){
            best = std::min(best, nsPerOp(start, stop, objectCount));
        }
        for(std::size_t i = 0; i < objectCount; i++){
            std::destroy_at(live[i]);
            alloc.deallocate(live[i], 1);
        }
    }
    return best;
}

int main(){
    std::vector<payload *> payloadPtrs(objectCount);
    std::vector<std::uint32_t> order(objectCount);
    std::iota(order.begin(), order.end(), 0u);
    std::shuffle(order.begin(), order.end(), std::mt19937(12345));

    const double bytesPool = bytesPerObjectPool(payloadPtrs);
    const double bytesNew = bytesPerObjectNew(payloadPtrs);
    const double bytesStd = bytesPerObjectStd(payloadPtrs);

    const double randomPoolNs = randomPool(payloadPtrs, order);
    const double randomNewNs = randomNew(payloadPtrs, order);
    const double randomStdNs = randomStd(payloadPtrs, order);

    std::printf("payload %zu bytes, %zu objects live, batch %zu\n",
                sizeof(payload), objectCount, batchSize);
    std::printf("best of %d runs after %d warm-up, compiler version %s\n\n",
                timedRuns, warmupRuns, __VERSION__);

    std::printf("| scenario | poolAllocator | new/delete | std::allocator |\n");
    std::printf("| --- | --- | --- | --- |\n");
    std::printf("| random-order free + alloc (ns/pair) | %.2f | %.2f | %.2f |\n",
                randomPoolNs, randomNewNs, randomStdNs);

    std::printf("| memory (bytes/object) | %.1f | %.1f | %.1f |\n",
                bytesPool, bytesNew, bytesStd);

    std::printf("\nchecksum %llu\n", (unsigned long long)checksum);
    return 0;
}
