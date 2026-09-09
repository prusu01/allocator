#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "alloc.hpp"

#ifdef NDEBUG
#error "the tests are assert-based, build them without NDEBUG"
#endif

namespace{

struct probe{
    static int alive;
    static int built;
    static int killed;

    int value;

    explicit probe(int v = 0) : value(v){
        alive++;
        built++;
    }

    ~probe(){
        alive--;
        killed++;
    }
};

int probe::alive = 0;
int probe::built = 0;
int probe::killed = 0;

void clear_probes(){
    assert(probe::alive == 0);
    probe::built = 0;
    probe::killed = 0;
}

struct point{
    int x;
    int y;

    point(int a, int b) : x(a), y(b){}
};

struct movable{
    std::unique_ptr<int> owned;

    explicit movable(std::unique_ptr<int> p) : owned(std::move(p)){}
};

struct alignas(64) wide{
    double v[8];
};

void exhaustion(){
    poolAllocator<int> pool(2);
    assert(pool.capacity() == 2);
    assert(pool.size() == 0);

    int *a = pool.allocate();
    int *b = pool.allocate(7);
    assert(a != nullptr && b != nullptr);
    assert(*a == 0 && *b == 7);
    assert(pool.size() == 2);

    assert(pool.allocate() == nullptr);
    assert(pool.allocate(9) == nullptr);
    assert(pool.size() == 2);

    pool.destroy(a);
    assert(pool.size() == 1);
    assert(pool.allocate(3) == a);
    assert(pool.allocate() == nullptr);

    poolAllocator<int> empty(0);
    assert(empty.capacity() == 0);
    assert(empty.allocate() == nullptr);
}

void reuse_order(){
    poolAllocator<int> pool(4);
    int *p[4];
    for(int i = 0; i < 4; i++){
        p[i] = pool.allocate(i);
        assert(p[i] != nullptr);
    }
    assert(pool.size() == 4);

    pool.destroy(p[1]);
    pool.destroy(p[3]);
    pool.destroy(p[0]);
    assert(pool.size() == 1);

    assert(pool.allocate() == p[0]);
    assert(pool.allocate() == p[3]);
    assert(pool.allocate() == p[1]);
    assert(pool.allocate() == nullptr);
    assert(*p[2] == 2);
}

void pointer_stability(){
    clear_probes();

    const int n = 64;
    poolAllocator<probe> pool(n);
    probe *live[n];
    probe *kept[n / 2];

    for(int i = 0; i < n; i++){
        live[i] = pool.allocate(i);
    }
    for(int i = 0; i < n; i += 2){
        kept[i / 2] = live[i];
    }

    for(int i = 1; i < n; i += 2){
        pool.destroy(live[i]);
    }
    for(int i = 1; i < n; i += 2){
        live[i] = pool.allocate(i + 1000);
    }

    for(int i = 0; i < n; i += 2){
        assert(live[i] == kept[i / 2]);
        assert(live[i]->value == i);
    }
    for(int i = 1; i < n; i += 2){
        assert(live[i]->value == i + 1000);
    }

    pool.reset();
    assert(probe::alive == 0);
}

void destructor_counts(){
    clear_probes();

    {
        poolAllocator<probe> pool(8);
        probe *a = pool.allocate(1);
        probe *b = pool.allocate(2);
        probe *c = pool.allocate(3);
        assert(probe::built == 3 && probe::killed == 0 && probe::alive == 3);

        pool.destroy(b);
        assert(probe::killed == 1 && probe::alive == 2);

        pool.destroy(a);
        pool.destroy(c);
        assert(probe::built == 3 && probe::killed == 3);
        assert(pool.size() == 0);
    }

    assert(probe::alive == 0);
}

void reset_rewinds(){
    clear_probes();

    poolAllocator<probe> pool(8);
    probe *p[5];
    for(int i = 0; i < 5; i++){
        p[i] = pool.allocate(i);
    }
    pool.destroy(p[1]);
    pool.destroy(p[3]);
    assert(probe::killed == 2 && probe::alive == 3);

    pool.reset();
    assert(probe::killed == 5 && probe::alive == 0);
    assert(pool.size() == 0);
    assert(pool.capacity() == 8);

    probe *q = pool.allocate(42);
    assert(q == p[0]);
    assert(probe::built == 6);
    pool.destroy(q);

    pool.reset();
    assert(probe::alive == 0);
    assert(pool.size() == 0);
}

void move_semantics(){
    static_assert(!std::is_copy_constructible_v<poolAllocator<int>>);
    static_assert(!std::is_copy_assignable_v<poolAllocator<int>>);
    static_assert(std::is_nothrow_move_constructible_v<poolAllocator<int>>);
    static_assert(std::is_nothrow_move_assignable_v<poolAllocator<int>>);

    clear_probes();

    poolAllocator<probe> src(4);
    probe *a = src.allocate(11);
    probe *b = src.allocate(22);
    src.destroy(a);

    poolAllocator<probe> moved(std::move(src));
    assert(moved.capacity() == 4 && moved.size() == 1);
    assert(src.capacity() == 0 && src.size() == 0);
    assert(src.allocate() == nullptr);
    assert(b->value == 22);
    assert(moved.allocate(33) == a);
    assert(moved.size() == 2);

    poolAllocator<probe> sink(1);
    probe *doomed = sink.allocate(44);
    sink.destroy(doomed);

    sink = std::move(moved);
    assert(sink.capacity() == 4 && sink.size() == 2);
    assert(moved.capacity() == 0 && moved.size() == 0);
    assert(b->value == 22);

    poolAllocator<probe> &alias = sink;
    sink = std::move(alias);
    assert(sink.capacity() == 4 && sink.size() == 2);

    sink.reset();
    assert(probe::alive == 0);
}

void forwarding(){
    static_assert(!std::is_default_constructible_v<point>);

    poolAllocator<point> pts(2);
    point *p = pts.allocate(3, 4);
    assert(p->x == 3 && p->y == 4);
    pts.destroy(p);

    poolAllocator<movable> mv(2);
    movable *m = mv.allocate(std::make_unique<int>(42));
    assert(m->owned && *m->owned == 42);
    mv.destroy(m);

    poolAllocator<std::string> str(2);
    std::string *s = str.allocate(64, 'x');
    assert(s->size() == 64 && (*s)[0] == 'x');
    str.destroy(s);
}

void small_and_over_aligned(){
    poolAllocator<char> chars(3);
    char *a = chars.allocate('a');
    char *b = chars.allocate('b');
    char *c = chars.allocate('c');
    chars.destroy(b);
    chars.destroy(a);
    assert(chars.allocate('d') == a);
    assert(chars.allocate('e') == b);
    assert(*a == 'd' && *b == 'e' && *c == 'c');

    poolAllocator<wide> pool(8);
    for(int i = 0; i < 8; i++){
        wide *w = pool.allocate();
        assert(w != nullptr);
        assert(reinterpret_cast<std::uintptr_t>(w) % alignof(wide) == 0);
    }
    assert(pool.allocate() == nullptr);
    pool.reset();
}

}

int main(){
    exhaustion();
    reuse_order();
    pointer_stability();
    destructor_counts();
    reset_rewinds();
    move_semantics();
    forwarding();
    small_and_over_aligned();

    std::puts("all tests passed");
    return 0;
}
