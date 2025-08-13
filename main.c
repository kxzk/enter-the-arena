#include "arena.h"
#include <stdio.h>

int main() {
  Arena a = {0};
  arena_init(&a, 64 * 1024); // Initialize arena with 64KB

  int *xs = ARENA_ALLOC_N(&a, int, 1000); // Allocate space for 1000 integers
  for (int i = 0; i < 1000; ++i) xs[i] = i;

  char *name = arena_strdup(&a, "blah blah blah");

  ArenaMark m = arena_mark(&a);
  double *scratch = ARENA_ALLOC_N(&a, double, 1<<20); // Allocate space for 1MB of doubles

  arena_release(&a, m);

  printf("%zu used / %zu reserved\n", arena_bytes_used(&a), arena_bytes_reserved(&a));

  arena_destroy(&a);
  return 0;
}
