# enter-the-arena

A header-only arena allocator for C. Arena allocators trade granular memory control for speed and simplicity - allocate fast, free everything at once.

## What It Does

An arena (also called a bump allocator) manages memory as a stack of blocks. You push allocations onto it sequentially, then either:
- Reset everything at once
- Mark a point and later release back to that mark

Think of it as a scratchpad: cheap to allocate, no fragmentation, perfect for temporary or phase-based memory.

## Core API

```c
Arena a = {0};
arena_init(&a, 64 * 1024);  // 64KB initial block

// allocate
int *nums = ARENA_ALLOC_N(&a, int, 1000);
char *str = arena_strdup(&a, "hello");

// mark and release pattern for temporary allocations
ArenaMark m = arena_mark(&a);
double *temp = ARENA_ALLOC_N(&a, double, 1<<20);
// ... use temp ...
arena_release(&a, m);  // reclaim temp's memory

arena_destroy(&a);  // free all blocks
```

## When To Use

**Good for:**
- Temporary allocations (parsing, string building)
- Per-frame or per-request memory
- Avoiding malloc overhead in hot paths
- Eliminating memory fragmentation

**Not for:**
- Long-lived objects with different lifetimes
- Scenarios needing granular deallocation
- Memory-constrained environments (arenas keep peak usage)

## How It Works

```
[Block 1: 64KB]
├─ used: 24KB
└─ free: 40KB → allocation pointer

[Block 2: 128KB] ← current block
├─ used: 0KB
└─ free: 128KB
```

When a block fills, the arena chains a new (larger) block. All blocks stay allocated until `arena_destroy()` or `arena_reset()`.

## Key Design Choices

- **Header-only**: Include and use, no linking
- **Growing blocks**: Each new block doubles in size (up to a limit)
- **Alignment-aware**: Respects type alignment automatically
- **Zero-dependency**: Just standard C library

## Build

```bash
gcc -O2 main.c -o main
./main
```

<br>
<br>
