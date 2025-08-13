#ifndef ARENA_H
#define ARENA_H

#include <limits.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ArenaBlock {
  struct ArenaBlock *prev;
  size_t capacity;
  size_t offset;
  unsigned char data[];
} ArenaBlock;

typedef struct Arena {
  ArenaBlock *current;
  size_t default_block_size;
  size_t reserved; // sum of block capacities
} Arena;

typedef struct ArenaMark {
  ArenaBlock *block;
  size_t offset;
} ArenaMark;

static inline uintptr_t arena__align_up_ptr(uintptr_t p, size_t align) {
  if (align == 0)
    align = _Alignof(max_align_t);
  size_t rem = (size_t)(p % align);
  uintptr_t add = (uintptr_t)(align - rem);
  if (p > UINTPTR_MAX - add)
    return 0; // overflow
  return p + add;
}

static inline size_t arena__max(size_t a, size_t b) { return a > b ? a : b; }

static inline ArenaBlock *arena__new_block(size_t capacity) {
  if (capacity == 0)
    capacity = 1;
  if (capacity > SIZE_MAX - sizeof(ArenaBlock))
    return NULL; // overflow
  ArenaBlock *b = (ArenaBlock *)malloc(sizeof(ArenaBlock) + capacity);
  if (!b)
    return NULL; // allocation failed
  b->prev = NULL;
  b->capacity = capacity;
  b->offset = 0;
  return b;
}

static inline void arena_init(Arena *a, size_t initial_block_size) {
  a->current = arena__new_block(initial_block_size);
  a->default_block_size = initial_block_size ? initial_block_size : 1;
  a->reserved = a->current ? a->current->capacity : 0;
}

static inline void arena_destroy(Arena *a) {
  ArenaBlock *b = a->current;
  while (b) {
    ArenaBlock *prev = b->prev;
    free(b);
    b = prev;
  }
  a->current = NULL;
  a->default_block_size = 0;
  a->reserved = 0;
}

static inline void *arena_alloc_align(Arena *a, size_t size, size_t align) {
  if (size == 0)
    return NULL;
  if (!a->current) {
    arena_init(a,
               arena__max(a->default_block_size, size + (align ? align : 0)));
    if (!a->current)
      return NULL; // allocation failed
  }
  ArenaBlock *b = a->current;

  for (;;) {
    uintptr_t base = (uintptr_t)b->data + (uintptr_t)b->offset;
    uintptr_t aligned = arena__align_up_ptr(base, align);
    if (aligned == 0)
      return NULL;
    size_t aligned_off = (size_t)(aligned - (uintptr_t)b->data);

    if (aligned_off <= b->capacity && b->capacity - aligned_off >= size) {
      b->offset = aligned_off + size;
      return (void *)aligned;
    }

    // new block; ensure room for alignment slack
    if (align && size > SIZE_MAX - (align - 1))
      return NULL; // overflow
    size_t min_needed = size = (align ? (align - 1) : 0);
    size_t cap = arena__max(a->default_block_size, min_needed);
    ArenaBlock *nb = arena__new_block(cap);
    if (!nb)
      return NULL; // allocation failed
    nb->prev = a->current;
    a->current = nb;
    a->reserved += nb->capacity;
    b = nb;
  }
}

static inline void *arena_alloc(Arena *a, size_t size) {
  return arena_alloc_align(a, size, _Alignof(max_align_t));
}

static inline void *arena_zalloc_align(Arena *a, size_t size, size_t align) {
  void *p = arena_alloc_align(a, size, align);
  if (p)
    memset(p, 0, size);
  return p;
}

static inline void arena_reset(Arena *a) {
  if (!a->current)
    return;
  // keep oldest block to avoid malloc thrash
  ArenaBlock *b = a->current;
  while (b->prev) {
    ArenaBlock *prev = b->prev;
    free(b);
    a->reserved -= b->capacity;
    b = prev;
  }
  a->current = b;
  a->current->offset = 0;
}

static inline ArenaMark arena_mark(const Arena *a) {
  ArenaMark m;
  m.block = a->current;
  m.offset = a->current ? a->current->offset : 0;
  return m;
}

static inline void arena_release(Arena *a, ArenaMark m) {
  if (!a->current)
    return;
  while (a->current && a->current != m.block) {
    ArenaBlock *prev = a->current->prev;
    a->reserved -= a->current->capacity;
    free(a->current);
    a->current = prev;
  }
  if (a->current == m.block && a->current) {
    a->current->offset = (m.offset <= a->current->capacity) ? m.offset : 0;
  }
}

static inline size_t arena_bytes_reserved(const Arena *a) {
  return a->reserved;
}

static inline size_t arena_bytes_used(const Arena *a) {
  size_t used = 0;
  for (const ArenaBlock *b = a->current; b; b = b->prev)
    used += b->offset;
  return used;
}

static inline char *arena_strdup(Arena *a, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)arena_alloc_align(a, n, _Alignof(char));
  if (p)
    memcpy(p, s, n);
  return p;
}

#define ARENA_ALLOC(a, T) (T *)arena_alloc_align((a), sizeof(T), _Alignof(T))
#define ARENA_ALLOC_N(a, T, n)                                                 \
  (T *)arena_alloc_align((a), sizeof(T) * (n), _Alignof(T))
#define ARENA_ZALLOC_N(a, T, n)                                                \
  (T *)arena_zalloc_align((a), sizeof(T) * (n), _Alignof(T))

#endif
