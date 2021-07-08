
typedef struct atomic_s64
{
    s64 value;
} atomic_s64;

s64 atomic_s64_increment(atomic_s64* a);
s64 atomic_s64_decrement(atomic_s64* a);


