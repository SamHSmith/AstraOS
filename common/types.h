

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

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define POW2LLU(x) (1llu << x)
#define NEXT_POWER_OF_2(x) ((x > POW2LLU(63)) ? POW2LLU(64) : ( \
                            (x > POW2LLU(62)) ? POW2LLU(63) : ( \
                            (x > POW2LLU(61)) ? POW2LLU(62) : ( \
                            (x > POW2LLU(60)) ? POW2LLU(61) : ( \
                            (x > POW2LLU(59)) ? POW2LLU(60) : ( \
                            (x > POW2LLU(58)) ? POW2LLU(59) : ( \
                            (x > POW2LLU(57)) ? POW2LLU(58) : ( \
                            (x > POW2LLU(56)) ? POW2LLU(57) : ( \
                            (x > POW2LLU(55)) ? POW2LLU(56) : ( \
                            (x > POW2LLU(54)) ? POW2LLU(55) : ( \
                            (x > POW2LLU(53)) ? POW2LLU(54) : ( \
                            (x > POW2LLU(52)) ? POW2LLU(53) : ( \
                            (x > POW2LLU(51)) ? POW2LLU(52) : ( \
                            (x > POW2LLU(50)) ? POW2LLU(51) : ( \
                            (x > POW2LLU(49)) ? POW2LLU(50) : ( \
                            (x > POW2LLU(48)) ? POW2LLU(49) : ( \
                            (x > POW2LLU(47)) ? POW2LLU(48) : ( \
                            (x > POW2LLU(46)) ? POW2LLU(47) : ( \
                            (x > POW2LLU(45)) ? POW2LLU(46) : ( \
                            (x > POW2LLU(44)) ? POW2LLU(45) : ( \
                            (x > POW2LLU(43)) ? POW2LLU(44) : ( \
                            (x > POW2LLU(42)) ? POW2LLU(43) : ( \
                            (x > POW2LLU(41)) ? POW2LLU(42) : ( \
                            (x > POW2LLU(40)) ? POW2LLU(41) : ( \
                            (x > POW2LLU(39)) ? POW2LLU(40) : ( \
                            (x > POW2LLU(38)) ? POW2LLU(39) : ( \
                            (x > POW2LLU(37)) ? POW2LLU(38) : ( \
                            (x > POW2LLU(36)) ? POW2LLU(37) : ( \
                            (x > POW2LLU(35)) ? POW2LLU(36) : ( \
                            (x > POW2LLU(34)) ? POW2LLU(35) : ( \
                            (x > POW2LLU(33)) ? POW2LLU(34) : ( \
                            (x > POW2LLU(32)) ? POW2LLU(33) : ( \
                            (x > POW2LLU(31)) ? POW2LLU(32) : ( \
                            (x > POW2LLU(30)) ? POW2LLU(31) : ( \
                            (x > POW2LLU(29)) ? POW2LLU(30) : ( \
                            (x > POW2LLU(28)) ? POW2LLU(29) : ( \
                            (x > POW2LLU(27)) ? POW2LLU(28) : ( \
                            (x > POW2LLU(26)) ? POW2LLU(27) : ( \
                            (x > POW2LLU(25)) ? POW2LLU(26) : ( \
                            (x > POW2LLU(24)) ? POW2LLU(25) : ( \
                            (x > POW2LLU(23)) ? POW2LLU(24) : ( \
                            (x > POW2LLU(22)) ? POW2LLU(23) : ( \
                            (x > POW2LLU(21)) ? POW2LLU(22) : ( \
                            (x > POW2LLU(20)) ? POW2LLU(21) : ( \
                            (x > POW2LLU(19)) ? POW2LLU(20) : ( \
                            (x > POW2LLU(18)) ? POW2LLU(19) : ( \
                            (x > POW2LLU(17)) ? POW2LLU(18) : ( \
                            (x > POW2LLU(16)) ? POW2LLU(17) : ( \
                            (x > POW2LLU(15)) ? POW2LLU(16) : ( \
                            (x > POW2LLU(14)) ? POW2LLU(15) : ( \
                            (x > POW2LLU(13)) ? POW2LLU(14) : ( \
                            (x > POW2LLU(12)) ? POW2LLU(13) : ( \
                            (x > POW2LLU(11)) ? POW2LLU(12) : ( \
                            (x > POW2LLU(10)) ? POW2LLU(11) : ( \
                            (x > POW2LLU(9)) ? POW2LLU(10) : ( \
                            (x > POW2LLU(8)) ? POW2LLU(9) : ( \
                            (x > POW2LLU(7)) ? POW2LLU(8) : ( \
                            (x > POW2LLU(6)) ? POW2LLU(7) : ( \
                            (x > POW2LLU(5)) ? POW2LLU(6) : ( \
                            (x > POW2LLU(4)) ? POW2LLU(5) : ( \
                            (x > POW2LLU(3)) ? POW2LLU(4) : ( \
                            (x > POW2LLU(2)) ? POW2LLU(3) : ( \
                            (x > POW2LLU(1)) ? POW2LLU(2) : ( \
                            (x > POW2LLU(0)) ? POW2LLU(1) : POW2LLU(0) \
                            ))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

#define NEXT_POWER_OF_2_POWER(x) ((x > POW2LLU(63)) ? 64 : ( \
                            (x > POW2LLU(62)) 63 : ( \
                            (x > POW2LLU(61)) 62 : ( \
                            (x > POW2LLU(60)) 61 : ( \
                            (x > POW2LLU(59)) 60 : ( \
                            (x > POW2LLU(58)) 59 : ( \
                            (x > POW2LLU(57)) 58 : ( \
                            (x > POW2LLU(56)) 57 : ( \
                            (x > POW2LLU(55)) 56 : ( \
                            (x > POW2LLU(54)) 55 : ( \
                            (x > POW2LLU(53)) 54 : ( \
                            (x > POW2LLU(52)) 53 : ( \
                            (x > POW2LLU(51)) 52 : ( \
                            (x > POW2LLU(50)) 51 : ( \
                            (x > POW2LLU(49)) 50 : ( \
                            (x > POW2LLU(48)) 49 : ( \
                            (x > POW2LLU(47)) 48 : ( \
                            (x > POW2LLU(46)) 47 : ( \
                            (x > POW2LLU(45)) 46 : ( \
                            (x > POW2LLU(44)) 45 : ( \
                            (x > POW2LLU(43)) 44 : ( \
                            (x > POW2LLU(42)) 43 : ( \
                            (x > POW2LLU(41)) 42 : ( \
                            (x > POW2LLU(40)) 41 : ( \
                            (x > POW2LLU(39)) 40 : ( \
                            (x > POW2LLU(38)) 39 : ( \
                            (x > POW2LLU(37)) 38 : ( \
                            (x > POW2LLU(36)) 37 : ( \
                            (x > POW2LLU(35)) 36 : ( \
                            (x > POW2LLU(34)) 35 : ( \
                            (x > POW2LLU(33)) 34 : ( \
                            (x > POW2LLU(32)) 33 : ( \
                            (x > POW2LLU(31)) 32 : ( \
                            (x > POW2LLU(30)) 31 : ( \
                            (x > POW2LLU(29)) 30 : ( \
                            (x > POW2LLU(28)) 29 : ( \
                            (x > POW2LLU(27)) 28 : ( \
                            (x > POW2LLU(26)) 27 : ( \
                            (x > POW2LLU(25)) 26 : ( \
                            (x > POW2LLU(24)) 25 : ( \
                            (x > POW2LLU(23)) 24 : ( \
                            (x > POW2LLU(22)) 23 : ( \
                            (x > POW2LLU(21)) 22 : ( \
                            (x > POW2LLU(20)) 21 : ( \
                            (x > POW2LLU(19)) 20 : ( \
                            (x > POW2LLU(18)) 19 : ( \
                            (x > POW2LLU(17)) 18 : ( \
                            (x > POW2LLU(16)) 17 : ( \
                            (x > POW2LLU(15)) 16 : ( \
                            (x > POW2LLU(14)) 15 : ( \
                            (x > POW2LLU(13)) 14 : ( \
                            (x > POW2LLU(12)) 13 : ( \
                            (x > POW2LLU(11)) 12 : ( \
                            (x > POW2LLU(10)) 11 : ( \
                            (x > POW2LLU(9)) 10 : ( \
                            (x > POW2LLU(8)) 9 : ( \
                            (x > POW2LLU(7)) 8 : ( \
                            (x > POW2LLU(6)) 7 : ( \
                            (x > POW2LLU(5)) 6 : ( \
                            (x > POW2LLU(4)) 5 : ( \
                            (x > POW2LLU(3)) 4 : ( \
                            (x > POW2LLU(2)) 3 : ( \
                            (x > POW2LLU(1)) 2 : ( \
                            (x > POW2LLU(0)) 1 : 0 \
                            ))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

#endif
