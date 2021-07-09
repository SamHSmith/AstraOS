
#ifndef __ATOMICS_H
#define __ATOMICS_H

#include "types.h"

typedef struct atomic_s64
{
    s64 value;
} atomic_s64;

// returns the value before the operation
s64 atomic_s64_set(atomic_s64* a, s64 value);
s64 atomic_s64_increment(atomic_s64* a);
s64 atomic_s64_decrement(atomic_s64* a);
s64 atomic_s64_read(atomic_s64* a);

#endif
