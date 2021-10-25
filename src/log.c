
typedef struct
{
    u64 time;
    u32 hart;
    u32 tid;
    u64 pid;
    char* function_name;
    char* message;
    u64 line_number;
    u8 is_kernel;
} KernelLogEntry;

#define KERNEL_LOG_SIZE 200

KernelLogEntry KERNEL_LOG[KERNEL_LOG_SIZE];
atomic_s64 KERNEL_LOG_INDEX;
RWLock KERNEL_LOG_LOCK;

void kernel_log_init()
{
    atomic_s64_set(&KERNEL_LOG_INDEX, 0);
    rwlock_create(&KERNEL_LOG_LOCK);
}

#define kernel_log_user(hart, pid, tid, message) kernel_log_user_(hart, pid, tid, __func__, __LINE__, message)

void kernel_log_user_(u32 hart, u64 pid, u32 tid, char* function_name, u64 line_number, char* message)
{
    rwlock_acquire_read(&KERNEL_LOG_LOCK);
    s64 log_index = atomic_s64_increment(&KERNEL_LOG_INDEX) % KERNEL_LOG_SIZE;
    u64* mtime = (u64*)0x0200bff8;

    KernelLogEntry entry;
    entry.time = *mtime;
    entry.hart = hart;
    entry.is_kernel = 0;
    entry.pid = pid;
    entry.tid = tid;
    entry.function_name = function_name;
    entry.line_number = line_number;
    entry.message = message;
    KERNEL_LOG[log_index] = entry;
    rwlock_release_read(&KERNEL_LOG_LOCK);
}

