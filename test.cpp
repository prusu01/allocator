// tiny_repro.cpp
#include <cassert>
#include <iostream>
#include <string>
#include "alloc.hpp"  // put the class above here

int main() {
    // 1) Fill well past capacity to trigger the OOB if end is wrong.
    {
        my_allocator<int> pool(2);
        int* a = pool.allocate(); assert(a);
        int* b = pool.allocate(); assert(b);
        // Third should be nullptr with correct end; your original walks OOB.
        int* c = pool.allocate();
        std::cout << "c=" << (void*)c << " (should be nullptr)\n";
        assert(c == nullptr);
        pool.destroy(a);
        pool.destroy(b);
    }

    // 2) Reuse after free (checks free list works and union usage is ok).
    {
        my_allocator<std::string> pool(3);
        auto* s1 = pool.allocate(); *s1 = "hello";
        auto* s2 = pool.allocate(); *s2 = "world";
        pool.destroy(s1);
        auto* s3 = pool.allocate(); // should reuse s1's slot
        *s3 += "!";
        std::cout << *s3 << " " << *s2 << "\n";
        pool.destroy(s2);
        pool.destroy(s3);
    }

    std::cout << "OK\n";
}
