
typedef struct
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
} TrapFrame;

#define THREAD_STATE_UNINITIALIZED 0
#define THREAD_STATE_INITIALIZED 1
#define THREAD_STATE_RUNNING 2
typedef struct
{
    TrapFrame frame;
    Kallocation stack_alloc;
    u64 program_counter;
    u64 thread_state;
    u64 proccess_pid;
} Thread;

typedef struct
{
    Kallocation proc_alloc;
    u64* mmu_table; // does not change during the lifetime of the proccess
    u32 thread_count;
    Thread threads[];
} Proccess;

Kallocation KERNEL_PROCCESS_ARRAY_ALLOCATION = {0};
#define KERNEL_PROCCESS_ARRAY ((Proccess**)KERNEL_PROCCESS_ARRAY_ALLOCATION.memory)
u64 KERNEL_PROCCESS_ARRAY_LEN = 0;

u64 proccess_create()
{
    Kallocation _proc = kalloc_pages(1);
    Proccess* proccess = (Proccess*)_proc.memory;
    proccess->proc_alloc = _proc;

    proccess->mmu_table = (u64*)kalloc_single_page();
    for(u64 i = 0; i < 512; i++) { proccess->mmu_table[i] = 0; }
    proccess->thread_count = 0;

    for(u64 i = 0; i < KERNEL_PROCCESS_ARRAY_LEN; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[i]->mmu_table == 0)
        {
            KERNEL_PROCCESS_ARRAY[i] = proccess;
            return i;
        }
    }
    if(KERNEL_PROCCESS_ARRAY_LEN % 512 == 0)
    {
        Kallocation new_alloc=kalloc_pages(KERNEL_PROCCESS_ARRAY_ALLOCATION.page_count+1);
        Proccess** new_array = (Proccess**)new_alloc.memory;
        for(u64 i = 0; i < KERNEL_PROCCESS_ARRAY_LEN; i++)
        {
            new_array[i] = KERNEL_PROCCESS_ARRAY[i];
        }
        if(KERNEL_PROCCESS_ARRAY_ALLOCATION.page_count != 0) //at init this is false
        {
            kfree_pages(KERNEL_PROCCESS_ARRAY_ALLOCATION);
        }
        KERNEL_PROCCESS_ARRAY_ALLOCATION = new_alloc;
    }
    u64 index = KERNEL_PROCCESS_ARRAY_LEN;
    KERNEL_PROCCESS_ARRAY[index] = proccess;
    KERNEL_PROCCESS_ARRAY_LEN += 1;
    return index;
}

u64 mmu_table_ptr_to_satp(u64* mmu_table)
{
    u64 root_ppn = ((u64)mmu_table) >> 12;
    u64 satp_val = (((u64)8) << 60) | root_ppn;
    return satp_val;
}


typedef struct
{
    u64 pid;
    u32 tid;
    u32 state;
    u64 runtime;
    s64 data;
} ThreadRuntime;

#define THREAD_RUNTIME_UNINITIALIZED 0
#define THREAD_RUNTIME_RUNNING 1
#define THREAD_RUNTIME_TIME_SLEEP 2

u64 thread_runtime_is_live(ThreadRuntime* r, u64 time_passed)
{
         if(r->state == THREAD_RUNTIME_RUNNING)
    {  return 1;  }
    else if(r->state == THREAD_RUNTIME_TIME_SLEEP)
    {
        r->data -= time_passed;
        if(r->data > 0)
        {  return 0;  }
        else
        {
            r->state = THREAD_RUNTIME_RUNNING;
            return 1;
        }
    }
    return 0;
}
 
Kallocation KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION = {0};
#define KERNEL_THREAD_RUNTIME_ARRAY ((ThreadRuntime*)KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION.memory)
u64 KERNEL_THREAD_RUNTIME_ARRAY_LEN = 0;
 

u32 proccess_thread_create(u64 pid)
{
    assert(pid < KERNEL_PROCCESS_ARRAY_LEN, "pid is within range");
    assert(KERNEL_PROCCESS_ARRAY[pid]->mmu_table != 0, "pid refers to a valid proccess");

    u64 thread_satp = mmu_table_ptr_to_satp(KERNEL_PROCCESS_ARRAY[pid]->mmu_table);

    u8 has_been_allocated = 0;
    u32 tid = 0;

    for(u32 i = 0; i < KERNEL_PROCCESS_ARRAY[pid]->thread_count; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state == THREAD_STATE_UNINITIALIZED)
        {
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state = THREAD_STATE_INITIALIZED;
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].frame.satp = thread_satp;
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].proccess_pid = pid;
            tid = i;
            has_been_allocated = 1;
        }
    }
    if(!has_been_allocated)
    {
        if(sizeof(Proccess) + (KERNEL_PROCCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread) > 
            KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.page_count + 1);
            for(u64 i = 0; i < (new_alloc.page_count - 1) * (PAGE_SIZE / 8); i++)
            {
                *(((u64*)new_alloc.memory) + i) =
                        *(((u64*)KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.memory) + i);
            }
            kfree_pages(KERNEL_PROCCESS_ARRAY[pid]->proc_alloc);
            KERNEL_PROCCESS_ARRAY[pid] = (Proccess*)new_alloc.memory;
            KERNEL_PROCCESS_ARRAY[pid]->proc_alloc = new_alloc;
        }
        tid = KERNEL_PROCCESS_ARRAY[pid]->thread_count;
        KERNEL_PROCCESS_ARRAY[pid]->thread_count += 1;

        KERNEL_PROCCESS_ARRAY[pid]->threads[tid].thread_state = THREAD_STATE_INITIALIZED;
        KERNEL_PROCCESS_ARRAY[pid]->threads[tid].frame.satp = thread_satp;
        KERNEL_PROCCESS_ARRAY[pid]->threads[tid].proccess_pid = pid;
    }

    // Now the thread has been created it has to be allocated a "runtime" so that it can be schedualed
    u64 runtime = 0;
    u8 has_runtime = 0;

    for(u64 i = 0; i < KERNEL_THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        if(KERNEL_THREAD_RUNTIME_ARRAY[i].state == THREAD_RUNTIME_UNINITIALIZED)
        {
            runtime = i;
            has_runtime = 1;
        }
    }
    // We maybe must allocate a new runtime
    if(!has_runtime)
    {
        if((KERNEL_THREAD_RUNTIME_ARRAY_LEN + 1) * sizeof(ThreadRuntime) >
            KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION.page_count + 1);
            for(u64 i = 0; i < (new_alloc.page_count - 1) * (PAGE_SIZE / 8); i++)
            {
                *(((u64*)new_alloc.memory) + i) =
                        *(((u64*)KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION.memory) + i);
            }
            kfree_pages(KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION);
            KERNEL_THREAD_RUNTIME_ARRAY_ALLOCATION = new_alloc;
        }

        runtime = KERNEL_THREAD_RUNTIME_ARRAY_LEN;
        KERNEL_THREAD_RUNTIME_ARRAY_LEN += 1;
    }

    KERNEL_THREAD_RUNTIME_ARRAY[runtime].pid = pid;
    KERNEL_THREAD_RUNTIME_ARRAY[runtime].tid = tid;
    KERNEL_THREAD_RUNTIME_ARRAY[runtime].runtime = 0;
//    KERNEL_THREAD_RUNTIME_ARRAY[runtime].state = THREAD_RUNTIME_RUNNING;
    KERNEL_THREAD_RUNTIME_ARRAY[runtime].state = THREAD_RUNTIME_TIME_SLEEP;
    KERNEL_THREAD_RUNTIME_ARRAY[runtime].data = 50000000;

    return tid;
}

Thread* kernel_current_thread;

u64 current_thread_runtime = 0;
u64 previous_lowest_runtime = 0;

u64 last_mtime = 0; 
Thread* kernel_choose_new_thread(u64 new_mtime, u64 apply_time)
{
    u64 time_passed = 0;
    if(last_mtime != 0) { time_passed = new_mtime - last_mtime; }
    last_mtime = new_mtime;

    if(apply_time) { KERNEL_THREAD_RUNTIME_ARRAY[current_thread_runtime].runtime += time_passed; }

    u64 time_to_remove = previous_lowest_runtime;
    previous_lowest_runtime = U64_MAX;

    u8 found_new_thread = 0;
    u64 new_thread_runtime = 0;
    u64 new_thread_runtime_weight = U64_MAX;
 
    for(u64 i = 0; i < KERNEL_THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        if(KERNEL_THREAD_RUNTIME_ARRAY[i].runtime < time_to_remove)
        {  KERNEL_THREAD_RUNTIME_ARRAY[i].runtime = time_to_remove;  } // We don't want to underflow
        KERNEL_THREAD_RUNTIME_ARRAY[i].runtime -= time_to_remove;

        if(KERNEL_THREAD_RUNTIME_ARRAY[i].runtime < previous_lowest_runtime)
        { previous_lowest_runtime = KERNEL_THREAD_RUNTIME_ARRAY[i].runtime; }

        if(thread_runtime_is_live(KERNEL_THREAD_RUNTIME_ARRAY + i, time_passed) &
            new_thread_runtime_weight > KERNEL_THREAD_RUNTIME_ARRAY[i].runtime
        )
        {
            found_new_thread = 1;
            new_thread_runtime = i;
            new_thread_runtime_weight = KERNEL_THREAD_RUNTIME_ARRAY[i].runtime;
        }
    }
    if(!found_new_thread)
    {  return 0;  } // Causes the KERNEL nop thread to be loaded

    current_thread_runtime = new_thread_runtime;

    ThreadRuntime runtime = KERNEL_THREAD_RUNTIME_ARRAY[current_thread_runtime];
    return &KERNEL_PROCCESS_ARRAY[runtime.pid]->threads[runtime.tid];
}


void thread1_func();
void thread2_func();
void thread3_func();

void proccess_init()
{
    u64 pid = proccess_create();

    u64* table = KERNEL_PROCCESS_ARRAY[pid]->mmu_table;

    mmu_kernel_map_range(table, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);

//mmu_kernel_map_range(table, (u64*)HEAP_START, (u64*)(HEAP_START + HEAP_SIZE),   2 + 4);

    mmu_kernel_map_range(table, 0x10000000, 0x10000000, 2 + 4);

    u32 thread1 = proccess_thread_create(pid);
    u32 thread2 = proccess_thread_create(pid);
    u32 thread3 = proccess_thread_create(pid);

    Thread* tarr = KERNEL_PROCCESS_ARRAY[pid]->threads;

    tarr[thread1].stack_alloc = kalloc_pages(4);
    tarr[thread1].frame.regs[2] = 
        ((u64)tarr[thread1].stack_alloc.memory) + tarr[thread1].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread1].stack_alloc.memory,
            (u64*)tarr[thread1].frame.regs[2],
            2 + 4
        );

    tarr[thread2].stack_alloc = kalloc_pages(4);
    tarr[thread2].frame.regs[2] = 
        ((u64)tarr[thread2].stack_alloc.memory) + tarr[thread2].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread2].stack_alloc.memory,
            (u64*)tarr[thread2].frame.regs[2],
            2 + 4
        );

    tarr[thread3].stack_alloc = kalloc_pages(4);
    tarr[thread3].frame.regs[2] = 
        ((u64)tarr[thread3].stack_alloc.memory) + tarr[thread3].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread3].stack_alloc.memory,
            (u64*)tarr[thread3].frame.regs[2],
            2 + 4
        );

    tarr[thread1].program_counter = (u64)thread1_func;
    tarr[thread2].program_counter = (u64)thread2_func;
    tarr[thread3].program_counter = (u64)thread3_func;

    u64* mtimecmp = (u64*)0x02004000;
    u64* mtime = (u64*)0x0200bff8;
    *mtimecmp = *mtime;
}


