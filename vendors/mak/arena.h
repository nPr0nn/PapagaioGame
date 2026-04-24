#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Memory size helpers
#define ARENA_KB(x) ((size_t)(x) * 1024)
#define ARENA_MB(x) ((size_t)(x) * 1024 * 1024)
#define ARENA_GB(x) ((size_t)(x) * 1024 * 1024 * 1024)

// Default chunk size when arena grows
#ifndef ARENA_DEFAULT_CHUNK_SIZE
#define ARENA_DEFAULT_CHUNK_SIZE ARENA_KB(8)
#endif

// --- Public Types ---

// Memory chunk in a linked list
typedef struct ArenaChunk {
  struct ArenaChunk *next;
  struct ArenaChunk *prev;
  size_t capacity; // number of usable bytes in data[]
  size_t used;     // bytes used in data[]
  uint8_t data[];  // flexible array member
} ArenaChunk;

// Arena with automatic growth via chunks
typedef struct {
  ArenaChunk *begin;
  ArenaChunk *end;
} ArenaAllocator;

// --- Public API ---

// Allocate memory from arena (grows automatically if needed)
void *arena_alloc(ArenaAllocator *arena, size_t size);

// Allocate and zero-initialize memory
void *arena_calloc(ArenaAllocator *arena, size_t size);

// Free all memory used by arena
void arena_free(ArenaAllocator *arena);

// Reset arena without freeing memory (reuse chunks)
void arena_reset(ArenaAllocator *arena);

// Get total used memory across all chunks
size_t arena_size(const ArenaAllocator *arena);

// Get total capacity (reserved memory) across all chunks
size_t arena_capacity(const ArenaAllocator *arena);

// Allocate a dedicated chunk (for large allocations)
void *arena_alloc_chunk(ArenaAllocator *arena, size_t size);

// Free a specific chunk (useful for large temporary allocations)
void arena_free_chunk(ArenaAllocator *arena, void *ptr);

// Reallocate a chunk to a new size
void *arena_realloc_chunk(ArenaAllocator *arena, void *ptr, size_t new_size);

// String helpers that allocate inside arena (added to public API)
char *arena_strdup(ArenaAllocator *arena, const char *str);
char *arena_strndup(ArenaAllocator *arena, const char *str, size_t n);

// --- Convenience Macros

#define arena_alloc_type(arena, type) ((type *)arena_alloc(arena, sizeof(type)))

#define arena_alloc_array(arena, type, count)                                  \
  ((type *)arena_alloc(arena, (size_t)sizeof(type) * (size_t)(count)))

#define arena_calloc_type(arena, type)                                         \
  ((type *)arena_calloc(arena, sizeof(type)))

#define arena_calloc_array(arena, type, count)                                 \
  ((type *)arena_calloc(arena, (size_t)sizeof(type) * (size_t)(count)))

// --- Implementation (include in exactly one C file)

#ifdef ARENA_IMPLEMENTATION

#include <limits.h>
#include <stdlib.h>
#include <string.h>

// --- Internal Helpers (internal linkage) ---

// Create a chunk with the given capacity. Return NULL on failure.
// Do overflow checks to avoid wraparound when computing malloc size.
static ArenaChunk *arena_chunk_create(size_t capacity) {
  // Avoid zero-sized allocations for clarity; treat 0 as 1
  if (capacity == 0)
    capacity = 1;

  // Prevent overflow when computing sizeof(ArenaChunk) + capacity
  if (capacity > SIZE_MAX - sizeof(ArenaChunk))
    return NULL;

  ArenaChunk *chunk = (ArenaChunk *)malloc(sizeof(ArenaChunk) + capacity);
  if (!chunk)
    return NULL;

  chunk->next = NULL;
  chunk->prev = NULL;
  chunk->capacity = capacity;
  chunk->used = 0;

  return chunk;
}

static void arena_chunk_destroy(ArenaChunk *chunk) {
  if (!chunk)
    return;
  free(chunk);
}

// Align to 8-byte boundary (internal helper)
static inline size_t arena_align(size_t size) {
  const size_t align = 8;
  return (size + (align - 1)) & ~(align - 1);
}

// --- Public Implementation ---

void *arena_alloc(ArenaAllocator *arena, size_t size) {
  if (!arena || size == 0)
    return NULL;

  size = arena_align(size);
  if (size > SIZE_MAX - 1)
    return NULL;

  ArenaChunk *chunk = arena->end;

  // Check if current chunk has enough space
  if (!chunk || chunk->used + size > chunk->capacity) {
    size_t chunk_size =
        (size > ARENA_DEFAULT_CHUNK_SIZE) ? size : ARENA_DEFAULT_CHUNK_SIZE;

    // Defensive cap: avoid absurdly large allocation requests
    if (chunk_size > SIZE_MAX - sizeof(ArenaChunk))
      return NULL;

    ArenaChunk *new_chunk = arena_chunk_create(chunk_size);
    if (!new_chunk)
      return NULL;

    // Link into arena
    if (arena->end) {
      arena->end->next = new_chunk;
      new_chunk->prev = arena->end;
      arena->end = new_chunk;
    } else {
      arena->begin = arena->end = new_chunk;
    }

    chunk = new_chunk;
  }

  // Now allocate from chunk
  if (chunk->used + size > chunk->capacity)
    return NULL;

  void *ptr = (void *)(chunk->data + chunk->used);
  chunk->used += size;
  return ptr;
}

void *arena_calloc(ArenaAllocator *arena, size_t size) {
  void *ptr = arena_alloc(arena, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

void arena_free(ArenaAllocator *arena) {
  if (!arena)
    return;

  ArenaChunk *chunk = arena->begin;
  while (chunk) {
    ArenaChunk *next = chunk->next;
    arena_chunk_destroy(chunk);
    chunk = next;
  }

  arena->begin = NULL;
  arena->end = NULL;
}

void arena_reset(ArenaAllocator *arena) {
  if (!arena)
    return;

  for (ArenaChunk *chunk = arena->begin; chunk; chunk = chunk->next) {
    chunk->used = 0;
  }

  // Keep chunks allocated for reuse
  arena->end = arena->begin ? arena->begin : NULL;
}

size_t arena_size(const ArenaAllocator *arena) {
  if (!arena)
    return 0;

  size_t total = 0;
  for (ArenaChunk *chunk = arena->begin; chunk; chunk = chunk->next) {
    if (chunk->used > SIZE_MAX - total)
      return SIZE_MAX;
    total += chunk->used;
  }
  return total;
}

size_t arena_capacity(const ArenaAllocator *arena) {
  if (!arena)
    return 0;

  size_t total = 0;
  for (ArenaChunk *chunk = arena->begin; chunk; chunk = chunk->next) {
    if (chunk->capacity > SIZE_MAX - total)
      return SIZE_MAX;
    total += chunk->capacity;
  }
  return total;
}

void *arena_alloc_chunk(ArenaAllocator *arena, size_t size) {
  if (!arena || size == 0)
    return NULL;

  size = arena_align(size);

  ArenaChunk *chunk = arena_chunk_create(size);
  if (!chunk)
    return NULL;

  if (arena->end) {
    arena->end->next = chunk;
    chunk->prev = arena->end;
    arena->end = chunk;
  } else {
    arena->begin = arena->end = chunk;
  }

  // Mark as fully used
  chunk->used = size;
  return chunk->data;
}

void arena_free_chunk(ArenaAllocator *arena, void *ptr) {
  if (!arena || !ptr)
    return;

  // Find the chunk containing this pointer (match data pointer)
  for (ArenaChunk *chunk = arena->begin; chunk; chunk = chunk->next) {
    if (chunk->data == ptr) {
      if (chunk->prev) {
        chunk->prev->next = chunk->next;
      } else {
        arena->begin = chunk->next;
      }

      if (chunk->next) {
        chunk->next->prev = chunk->prev;
      } else {
        arena->end = chunk->prev;
      }

      arena_chunk_destroy(chunk);
      return;
    }
  }
}

void *arena_realloc_chunk(ArenaAllocator *arena, void *ptr, size_t new_size) {
  if (!arena || !ptr || new_size == 0)
    return NULL;

  new_size = arena_align(new_size);

  for (ArenaChunk *chunk = arena->begin; chunk; chunk = chunk->next) {
    if (chunk->data == ptr) {
      void *new_ptr = arena_alloc_chunk(arena, new_size);
      if (!new_ptr)
        return NULL;

      size_t copy_size = (new_size < chunk->used) ? new_size : chunk->used;
      memcpy(new_ptr, ptr, copy_size);

      arena_free_chunk(arena, ptr);

      return new_ptr;
    }
  }

  return NULL;
}

// --- String Utilities (public) ---

char *arena_strdup(ArenaAllocator *arena, const char *str) {
  if (!arena || !str)
    return NULL;

  size_t len = strlen(str);

  // Defensive check for overflow when len + 1
  if (len > SIZE_MAX - 1)
    return NULL;

  char *copy = (char *)arena_alloc(arena, len + 1);
  if (copy) {
    memcpy(copy, str, len + 1);
  }
  return copy;
}

char *arena_strndup(ArenaAllocator *arena, const char *str, size_t n) {
  if (!arena || !str)
    return NULL;

  size_t len = strnlen(str, n);

  if (len > SIZE_MAX - 1)
    return NULL;

  char *copy = (char *)arena_alloc(arena, len + 1);
  if (copy) {
    memcpy(copy, str, len);
    copy[len] = '\0';
  }
  return copy;
}

// --- Temporary Arena Mark/Release Pattern (internal) ---

typedef struct {
  ArenaChunk *chunk;
  size_t used;
} ArenaMark;

/* Save current arena state */
static inline ArenaMark arena_mark(ArenaAllocator *arena) {
  ArenaMark mark = {0};
  if (arena && arena->end) {
    mark.chunk = arena->end;
    mark.used = arena->end->used;
  }
  return mark;
}

// Restore arena to saved state (frees any chunks allocated after mark)
// This is internal (static inline) so it is not callable externally.
static inline void arena_release(ArenaAllocator *arena, ArenaMark mark) {
  if (!arena || !mark.chunk)
    return;

  // Free all chunks after the marked chunk
  ArenaChunk *chunk = mark.chunk->next;
  while (chunk) {
    ArenaChunk *next = chunk->next;
    arena_chunk_destroy(chunk);
    chunk = next;
  }

  // Restore state
  mark.chunk->next = NULL;
  mark.chunk->used = mark.used;
  arena->end = mark.chunk;
}

#endif // ARENA_IMPLEMENTATION
#endif // ARENA_H
