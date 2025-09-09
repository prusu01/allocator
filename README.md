# my_allocator<T> — Single-Block Object Pool

A tiny, header-only object pool that preallocates **one big block** and serves fixed-size **slots** for objects of type `T`. No further system allocations during normal use.

> Scope (current): default-constructible `T` only · single-threaded · metadata stored inside freed slots.

---

## TL;DR

- **One allocation up front**
- **O(1) allocate / O(1) destroy**
- **Type-agnostic** (parametric polymorphism via templates)
- **Cache-friendly** reuse of recently freed slots

---

## What you get

- Deterministic performance: only the constructor touches the system allocator  
- Stable pointers: an object’s address remains valid until you destroy it  
- Minimal overhead: no external containers; free-list pointers live inside freed slots

---

## Interface (no code)

- **Constructor** — `my_allocator<T>(nb)`  
  Creates a pool with capacity for `nb` objects of type `T`. Performs the single, large allocation.

- **`allocate() → T*`**  
  Returns a pointer to a default-constructed `T` from the pool.  
  Returns `nullptr` if the pool is full. Amortized O(1).

- **`destroy(T* p)`**  
  Calls `~T()` and returns the slot to the pool’s free list.  
  Undefined behavior if `p` did not come from this pool, was already destroyed, or is `nullptr`.

- **`get_object_nb() → int`**  
  The number of **live** objects currently managed by the pool.

---

## How it works (in words)

- The pool holds an array of **slots**; each slot is a tiny union that stores either a pointer to the **next free slot** or a live `T`.  
- **Allocation** prefers the **free list** (pop a slot). If empty, it hands out the next **never-used** slot via a bump pointer. If neither is available, `allocate()` returns `nullptr`.  
- **Destruction** calls the object’s destructor and **pushes** the slot back onto the free list.  
- Because metadata lives *inside* freed slots, there is **zero extra memory** beyond the single big block.

---

## Guarantees

- Exactly **one** system allocation in the constructor  
- **O(1)** `allocate()` and `destroy()`  
- **No fragmentation** (fixed-size slots)  
- **Predictable reuse** (LIFO free-list by default)

---

## Safety & assumptions

- Default-constructible `T` only (current version)  
- **Ownership**: only pass pointers returned by this pool to `destroy()`; don’t mix across pools  
- **Lifecycle**: destroy all live objects before destroying the pool (no automatic sweep)  
- **Threading**: not thread-safe  
- **Alignment**: base version uses `malloc` (aligned to `std::max_align_t`). For over-aligned `T` (e.g., `alignas(64)`), switch to aligned allocation.

---

## Common mistakes to avoid

- Double-destroy or destroying a foreign pointer → undefined behavior  
- Using a pointer after `destroy()`  
- Assuming constructor arguments are supported (see “Extensions”)

---

## Extensions you may add

- Constructor forwarding: `allocate(Args&&…)` to support any `T` constructor  
- Aligned allocation with `operator new[]` + `std::align_val_t` for over-aligned `T`  
- `reset()` to destroy all live objects and rewind the pool  
- Stats: `capacity()`, `free_slots()`, high-water mark  
- Debug mode: optional range checks / double-free detection

---

## Glossary

- **Parametric polymorphism (generics)**: the allocator works for any `T` via templates  
- **Placement construction / explicit destruction**: build and tear down objects in pre-allocated storage  
- **Free list**: a simple list of freed slots stored inside the slots themselves

---

## License

Pick a permissive license (MIT/BSD/Apache-2.0) and add a `LICENSE` file. Contributions welcome.
