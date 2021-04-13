
typedef struct
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
} TrapFrame;

#define THREAD_STATE_UNINITIALIZED 0
#define THREAD_STATE_INITIALIZED 1
#define THREAD_STATE_RUNNING 2
#define THREAD_STATE_TIME_SLEEP 3
#define THREAD_STATE_SURFACE_WAIT 4

typedef struct
{
    u64 count;
    u64 surface_slot[512];
} ThreadSurfaceSlotWait;
typedef struct
{
    TrapFrame frame;
    Kallocation stack_alloc;
    u64 program_counter;
    u64 thread_state;
    u64 proccess_pid;
    union
    {
        u64 sleep_time;
        ThreadSurfaceSlotWait surface_slot_wait;
    }
} Thread;

typedef struct
{
    Kallocation proc_alloc;
    u64* mmu_table; // does not change during the lifetime of the proccess

    Kallocation allocations_alloc;
    u64 allocations_count;

    Kallocation surface_alloc;
    u64 surface_count;

    Kallocation surface_consumer_alloc;
    u64 surface_consumer_count;

    KeyboardEventQueue kbd_event_queue;
    RawMouse mouse;

    u32 thread_count;
    Thread threads[];
} Proccess;

Kallocation KERNEL_PROCCESS_ARRAY_ALLOCATION = {0};
#define KERNEL_PROCCESS_ARRAY ((Proccess**)KERNEL_PROCCESS_ARRAY_ALLOCATION.memory)
u64 KERNEL_PROCCESS_ARRAY_LEN = 0;

u64 proccess_create()
{
    Kallocation _proc = kalloc_pages((sizeof(Proccess)/PAGE_SIZE) + 1);
    Proccess* proccess = (Proccess*)_proc.memory;
    memset(proccess, 0, sizeof(Proccess));
    proccess->proc_alloc = _proc;

    proccess->mmu_table = (u64*)kalloc_single_page();
    for(u64 i = 0; i < 512; i++) { proccess->mmu_table[i] = 0; }

    for(u64 i = 0; i < KERNEL_PROCCESS_ARRAY_LEN; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[i]->mmu_table == 0)
        {
            KERNEL_PROCCESS_ARRAY[i] = proccess;
            return i;
        }
    }
    if((KERNEL_PROCCESS_ARRAY_LEN+1) * sizeof(Proccess*)
        > KERNEL_PROCCESS_ARRAY_ALLOCATION.page_count*PAGE_SIZE)
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
} ThreadRuntime;

#define THREAD_RUNTIME_UNINITIALIZED 0
#define THREAD_RUNTIME_INITIALIZED 1

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
            Kallocation new_alloc = kalloc_pages(
            ((sizeof(Proccess) + (KERNEL_PROCCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread))/PAGE_SIZE)
            + 1
            );
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
    KERNEL_THREAD_RUNTIME_ARRAY[runtime].state = THREAD_RUNTIME_INITIALIZED;

    return tid;
}

u64 proccess_alloc_pages(Proccess* proccess, u64 vaddr, Kallocation mem)
{
    u64 ret = 0;
    if(mem.page_count != 0 && vaddr % PAGE_SIZE == 0)
    {
        if((proccess->allocations_count + 1) * sizeof(Kallocation) >
            proccess->allocations_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(proccess->allocations_alloc.page_count + 1);
            if(new_alloc.page_count == 0) // ran out of memory
            { return 0; }
            for(u64 i = 0; i < proccess->allocations_count; i++)
            {
                ((Kallocation*)new_alloc.memory)[i] = ((Kallocation*)proccess->allocations_alloc.memory)[i];
            }
            kfree_pages(proccess->allocations_alloc);
            proccess->allocations_alloc = new_alloc;
        }
        Kallocation* alloc = ((Kallocation*)proccess->allocations_alloc.memory) +
                                proccess->allocations_count;
        *alloc = mem;
        if(alloc->page_count != 0)
        {
            proccess->allocations_count++;
            ret = 1;

            mmu_map_kallocation(proccess->mmu_table, *alloc, vaddr, 2 + 4); // read + write
        }
    }
    return ret;
}
/*
    Returns the kallocation that is no longer being tracked by the proccess.
    If new_page_count is 0 then this is the whole kallocation at vaddr.

    If the shrink fails or encounters an error state the returned kallocation
    points to NULL.
*/
Kallocation proccess_shrink_allocation(Proccess* proccess, u64 vaddr, u64 new_page_count)
{
    Kallocation ret_a;
    u64 ret = 0;
    u64 should_remove = 0;
    u64 remove_index = 0;

    void* ptr;
    if(mmu_virt_to_phys(proccess->mmu_table, vaddr, (u64*)&ptr) == 0)
    {
        for(u64 i = 0; i < proccess->allocations_count; i++)
        {
            Kallocation* k = ((Kallocation*)proccess->allocations_alloc.memory) + i;
            if(k->memory == ptr)
            {
                if(k->page_count < new_page_count)
                { break; }

                mmu_map_kallocation(proccess->mmu_table, *k, vaddr, 0);

                Kallocation remove = {0};
                remove.memory = (u64)(((u64)k->memory) + PAGE_SIZE * new_page_count);
                remove.page_count = k->page_count - new_page_count;
                ret_a = remove;

                k->page_count = new_page_count;

                mmu_map_kallocation(proccess->mmu_table, *k, vaddr, 2 + 4); // read + write

                if(k->page_count == 0)
                {
                    should_remove = 1;
                    remove_index = i;
                }
                ret = 1;
                break;
            }
        }
        Kallocation* array = ((Kallocation*)proccess->allocations_alloc.memory);
        if(should_remove)
        {
            for(u64 i = proccess->allocations_count - 1; i > remove_index; i--)
            {
                array[i-1] = array[i];
            }
            proccess->allocations_count -= 1;
        }
        if(!ret)
        {
            ret_a.memory = 0;
            ret_a.page_count = 0;
        }
    }
    return ret_a;
}
