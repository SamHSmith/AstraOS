
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
    u64 process_pid;
    union
    {
        u64 sleep_time;
        ThreadSurfaceSlotWait surface_slot_wait;
    }
} Thread;

typedef struct
{
    Kallocation proc_alloc;
    u64* mmu_table; // does not change during the lifetime of the process

    Kallocation allocations_alloc;
    u64 allocations_count;

    Kallocation surface_alloc;
    u64 surface_count;

    Kallocation surface_consumer_alloc;
    u64 surface_consumer_count;

    Kallocation file_access_redirects_alloc; // u64 array of file handles
    Kallocation file_access_permissions_alloc; // u8 array of flags
    u64 file_access_count;

    KeyboardEventQueue kbd_event_queue;
    RawMouse mouse;

    u32 thread_count;
    u64 pid;
    Thread threads[];
} Process;

Kallocation KERNEL_PROCESS_ARRAY_ALLOCATION = {0};
#define KERNEL_PROCESS_ARRAY ((Process**)KERNEL_PROCESS_ARRAY_ALLOCATION.memory)
u64 KERNEL_PROCESS_ARRAY_LEN = 0;

u64 process_create()
{
    Kallocation _proc = kalloc_pages((sizeof(Process)/PAGE_SIZE) + 1);
    Process* process = (Process*)_proc.memory;
    memset(process, 0, sizeof(Process));
    process->proc_alloc = _proc;

    process->mmu_table = create_mmu_table();
    for(u64 i = 0; i < 512; i++) { process->mmu_table[i] = 0; }

    for(u64 i = 0; i < KERNEL_PROCESS_ARRAY_LEN; i++)
    {
        if(KERNEL_PROCESS_ARRAY[i]->mmu_table == 0)
        {
            process->pid = i;
            KERNEL_PROCESS_ARRAY[process->pid] = process;
            return process->pid;
        }
    }
    if((KERNEL_PROCESS_ARRAY_LEN+1) * sizeof(Process*)
        > KERNEL_PROCESS_ARRAY_ALLOCATION.page_count*PAGE_SIZE)
    {
        Kallocation new_alloc=kalloc_pages(KERNEL_PROCESS_ARRAY_ALLOCATION.page_count+1);
        Process** new_array = (Process**)new_alloc.memory;
        for(u64 i = 0; i < KERNEL_PROCESS_ARRAY_LEN; i++)
        {
            new_array[i] = KERNEL_PROCESS_ARRAY[i];
        }
        if(KERNEL_PROCESS_ARRAY_ALLOCATION.page_count != 0) //at init this is false
        {
            kfree_pages(KERNEL_PROCESS_ARRAY_ALLOCATION);
        }
        KERNEL_PROCESS_ARRAY_ALLOCATION = new_alloc;
    }
    process->pid = KERNEL_PROCESS_ARRAY_LEN;
    KERNEL_PROCESS_ARRAY[process->pid] = process;
    KERNEL_PROCESS_ARRAY_LEN += 1;
    return process->pid;
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

u32 process_thread_create(u64 pid)
{
    assert(pid < KERNEL_PROCESS_ARRAY_LEN, "pid is within range");
    assert(KERNEL_PROCESS_ARRAY[pid]->mmu_table != 0, "pid refers to a valid process");

    u64 thread_satp = mmu_table_ptr_to_satp(KERNEL_PROCESS_ARRAY[pid]->mmu_table);

    u8 has_been_allocated = 0;
    u32 tid = 0;

    for(u32 i = 0; i < KERNEL_PROCESS_ARRAY[pid]->thread_count; i++)
    {
        if(KERNEL_PROCESS_ARRAY[pid]->threads[i].thread_state == THREAD_STATE_UNINITIALIZED)
        {
            KERNEL_PROCESS_ARRAY[pid]->threads[i].thread_state = THREAD_STATE_INITIALIZED;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].frame.satp = thread_satp;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].process_pid = pid;
            tid = i;
            has_been_allocated = 1;
        }
    }
    if(!has_been_allocated)
    {
        if(sizeof(Process) + (KERNEL_PROCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread) > 
            KERNEL_PROCESS_ARRAY[pid]->proc_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(
            ((sizeof(Process) + (KERNEL_PROCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread))/PAGE_SIZE)
            + 1
            );
            for(u64 i = 0; i < (new_alloc.page_count - 1) * (PAGE_SIZE / 8); i++)
            {
                *(((u64*)new_alloc.memory) + i) =
                        *(((u64*)KERNEL_PROCESS_ARRAY[pid]->proc_alloc.memory) + i);
            }
            kfree_pages(KERNEL_PROCESS_ARRAY[pid]->proc_alloc);
            KERNEL_PROCESS_ARRAY[pid] = (Process*)new_alloc.memory;
            KERNEL_PROCESS_ARRAY[pid]->proc_alloc = new_alloc;
        }
        tid = KERNEL_PROCESS_ARRAY[pid]->thread_count;
        KERNEL_PROCESS_ARRAY[pid]->thread_count += 1;

        KERNEL_PROCESS_ARRAY[pid]->threads[tid].thread_state = THREAD_STATE_INITIALIZED;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].frame.satp = thread_satp;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].process_pid = pid;
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

u64 process_alloc_pages(Process* process, u64 vaddr, Kallocation mem)
{
    u64 ret = 0;
    if(mem.page_count != 0 && vaddr % PAGE_SIZE == 0)
    {
        if((process->allocations_count + 1) * sizeof(Kallocation) >
            process->allocations_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(process->allocations_alloc.page_count + 1);
            if(new_alloc.page_count == 0) // ran out of memory
            { return 0; }
            for(u64 i = 0; i < process->allocations_count; i++)
            {
                ((Kallocation*)new_alloc.memory)[i] = ((Kallocation*)process->allocations_alloc.memory)[i];
            }
            kfree_pages(process->allocations_alloc);
            process->allocations_alloc = new_alloc;
        }
        Kallocation* alloc = ((Kallocation*)process->allocations_alloc.memory) +
                                process->allocations_count;
        *alloc = mem;
        if(alloc->page_count != 0)
        {
            process->allocations_count++;
            ret = 1;

            mmu_map_kallocation(process->mmu_table, *alloc, vaddr, 2 + 4); // read + write
        }
    }
    return ret;
}
/*
    Returns the kallocation that is no longer being tracked by the process.
    If new_page_count is 0 then this is the whole kallocation at vaddr.

    If the shrink fails or encounters an error state the returned kallocation
    points to NULL.
*/
Kallocation process_shrink_allocation(Process* process, u64 vaddr, u64 new_page_count)
{
    Kallocation ret_a;
    u64 ret = 0;
    u64 should_remove = 0;
    u64 remove_index = 0;

    void* ptr;
    if(mmu_virt_to_phys(process->mmu_table, vaddr, (u64*)&ptr) == 0)
    {
        for(u64 i = 0; i < process->allocations_count; i++)
        {
            Kallocation* k = ((Kallocation*)process->allocations_alloc.memory) + i;
            if(k->memory == ptr)
            {
                if(k->page_count < new_page_count)
                { break; }

                mmu_map_kallocation(process->mmu_table, *k, vaddr, 0);

                Kallocation remove = {0};
                remove.memory = (u64)(((u64)k->memory) + PAGE_SIZE * new_page_count);
                remove.page_count = k->page_count - new_page_count;
                ret_a = remove;

                k->page_count = new_page_count;

                mmu_map_kallocation(process->mmu_table, *k, vaddr, 2 + 4); // read + write

                if(k->page_count == 0)
                {
                    should_remove = 1;
                    remove_index = i;
                }
                ret = 1;
                break;
            }
        }
        Kallocation* array = ((Kallocation*)process->allocations_alloc.memory);
        if(should_remove)
        {
            for(u64 i = remove_index; i + 1 < process->allocations_count; i++)
            {
                array[i] = array[i+1];
            }
            process->allocations_count -= 1;
        }
        if(!ret)
        {
            ret_a.memory = 0;
            ret_a.page_count = 0;
        }
    }
    return ret_a;
}

#define FILE_ACCESS_PERMISSION_READ_BIT 1
#define FILE_ACCESS_PERMISSION_READ_WRITE_BIT 2

u64 process_new_file_access(u64 pid, u64 redirect, u8 permission) // todo optimize beyond singular
{
    assert(permission != 0, "permission is not zero");
    assert(is_valid_file_id(redirect), "redirect is a valid file");

    Process* process = KERNEL_PROCESS_ARRAY[pid];
    u64* redirects = process->file_access_redirects_alloc.memory;
    u8* permissions = process->file_access_permissions_alloc.memory;

    {
        for(u64 i = 0; i < process->file_access_count; i++)
        {
            if(!permissions[i])
            {
                redirects[i] = redirect;
                permissions[i] = permission;
                return i;
            }
        }
    }

    u64 redirects_page_count = ((process->file_access_count + 1) * 8 + (PAGE_SIZE-1)) / PAGE_SIZE;
    u64 permissions_page_count = ((process->file_access_count + 1) * 1 + (PAGE_SIZE-1)) / PAGE_SIZE;

    if(redirects_page_count > process->file_access_redirects_alloc.page_count)
    {
        Kallocation new_alloc = kalloc_pages(redirects_page_count);
        u64* new_array = new_alloc.memory;
        for(u64 i = 0; i < process->file_access_count; i++)
        {
            new_array[i] = redirects[i];
        }
        if(process->file_access_redirects_alloc.page_count != 0)
        {
            kfree_pages(process->file_access_redirects_alloc);
        }
        process->file_access_redirects_alloc = new_alloc;
        redirects = process->file_access_redirects_alloc.memory;
    }

    if(permissions_page_count > process->file_access_permissions_alloc.memory)
    {
        Kallocation new_alloc = kalloc_pages(permissions_page_count);
        u8* new_array = new_alloc.memory;
        for(u64 i = 0; i < process->file_access_count; i++)
        {
            new_array[i] = permissions[i];
        }
        if(process->file_access_permissions_alloc.page_count != 0)
        {
            kfree_pages(process->file_access_permissions_alloc);
        }
        process->file_access_permissions_alloc = new_alloc;
        permissions = process->file_access_permissions_alloc.memory;
    }
    u64 i = process->file_access_count;
    process->file_access_count++;

    permissions[i] = permission;
    redirects[i] = redirect;
    return i;
}

u64 process_get_write_access(Process* process, u64 local_file_id, u64* file_id)
{
    if(local_file_id >= process->file_access_count) { return 0; }
    u64* redirects = process->file_access_redirects_alloc.memory;
    u64* permissions = process->file_access_permissions_alloc.memory;
    if(permissions[local_file_id] & FILE_ACCESS_PERMISSION_READ_WRITE_BIT == 0) { return 0; }
    if(file_id) { *file_id = redirects[local_file_id]; }
    return 1;
}

u64 process_get_read_access(Process* process, u64 local_file_id, u64* file_id)
{
    if(local_file_id >= process->file_access_count) { return 0; }
    u64* redirects = process->file_access_redirects_alloc.memory;
    u64* permissions = process->file_access_permissions_alloc.memory;
    if(permissions[local_file_id] &
        (FILE_ACCESS_PERMISSION_READ_BIT | FILE_ACCESS_PERMISSION_READ_WRITE_BIT) == 0) { return 0; }
    if(file_id) { *file_id = redirects[local_file_id]; }
    return 1;
}
