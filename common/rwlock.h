
#ifndef __RWLOCK_H
#define __RWLOCK_H

#include "atomics.h"

typedef struct
{
    atomic_s64 write;
    atomic_s64 read;
} RWLock;

void rwlock_create(RWLock* lock)
{
    atomic_s64_set(&lock->write, 0);
    atomic_s64_set(&lock->read,  0);
}

void rwlock_acquire_write(RWLock* lock)
{
start:
    if(atomic_s64_increment(&lock->write))
    {
        // there is already someone writing
        atomic_s64_decrement(&lock->write);
        goto start;
    }
    // we have achieved write lock, now we wait for there
    // to be no readers
    while(atomic_s64_read(&lock->read)) { __asm__("nop"); }
    // here we have sole access.
}
void rwlock_release_write(RWLock* lock)
{
    atomic_s64_decrement(&lock->write);
}

void rwlock_acquire_read(RWLock* lock)
{
start:
    if(atomic_s64_read(&lock->write))
    {
        // someone is writing
        goto start;
    }
    atomic_s64_increment(&lock->read);
    if(atomic_s64_read(&lock->write))
    {
        // someone wants to write
        atomic_s64_decrement(&lock->read);
        goto start;
    }
    // now we have read access
}
void rwlock_release_read(RWLock* lock)
{
    atomic_s64_decrement(&lock->read);
}

#endif
