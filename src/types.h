

#ifndef __TYPES_H
#define __TYPES_H

typedef char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long int s64;
typedef unsigned long long int u64;

#define S8_MIN ((s8)-128)
#define S8_MAX  ((s8)127)
#define U8_MAX  ((u8)255)
#define S16_MIN ((s16)-32768)
#define S16_MAX  ((s16)32767)
#define U16_MAX  ((u16)65535)
#define S32_MIN ((s32)-2147483648)
#define S32_MAX  ((s32)2147483647)
#define U32_MAX  ((u32)4294967295)
#define S64_MIN ((s64)-9223372036854775808)
#define S64_MAX  ((s64)9223372036854775807)
#define U64_MAX  ((u64)18446744073709551615)

#endif
