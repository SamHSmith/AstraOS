
#ifndef __SPINLOCK_H
#define __SPINLOCK_H

#include "types.h"

typedef struct Spinlock
{
    u32 _word;
} Spinlock;

void spinlock_create(Spinlock* lock);
void spinlock_acquire(Spinlock* lock);
u64  spinlock_try_acquire(Spinlock* lock);
void spinlock_release(Spinlock* lock);

#endif
