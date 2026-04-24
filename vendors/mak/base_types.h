#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <stdbool.h> // for bool
#include <stddef.h>  // for size_t, ptrdiff_t
#include <stdint.h>  // for fixed-width integer types

// Unsigned integer types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Signed integer types
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Platform-sized types
typedef size_t usize;
typedef ptrdiff_t isize;

// Floating-point types
typedef float f32;
typedef double f64;

// Boolean type
typedef bool b8;

// Constant definitions for true/false (optional, for clarity)
#ifndef TRUE
#define TRUE ((b8)1)
#endif

#ifndef FALSE
#define FALSE ((b8)0)
#endif

// Useful macro for array size
#ifndef ARRAY_LEN
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

// Force inline (portable)
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#else
#define FORCE_INLINE inline
#endif

// Restrict keyword compatibility
#if defined(_MSC_VER)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

#endif // BASE_TYPES_H
