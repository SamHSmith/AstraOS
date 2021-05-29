

#ifndef __TYPES_H
#define __TYPES_H

#include <stdint.h>
#include <float.h>

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

#define S8_MIN INT8_MIN
#define S8_MAX INT8_MAX
#define U8_MAX UINT8_MAX
#define S16_MIN INT16_MIN
#define S16_MAX INT16_MAX
#define U16_MAX UINT16_MAX
#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define U32_MAX UINT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX
#define U64_MAX UINT64_MAX

typedef float f32;
typedef double f64;
typedef long double f128;

#endif
