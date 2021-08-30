
typedef struct
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
} TrapFrame;

#define THREAD_AWAKE_TIME 0
#define THREAD_AWAKE_SURFACE 1
#define THREAD_AWAKE_KEYBOARD 2
#define THREAD_AWAKE_MOUSE 3
#define THREAD_AWAKE_SEMAPHORE 4

typedef struct
{
    u64 awake_type;
    union
    {
        u64 awake_time;
        u64 surface_slot;
        u64 semaphore;
    };
} ThreadAwakeCondition;

#define THREAD_MAX_AWAKE_COUNT 64
typedef struct
{
    TrapFrame frame;
    Kallocation stack_alloc;
    u64 program_counter;
    u64 process_pid;
    u8 is_initialized;
    u8 is_running;
    u8 should_be_destroyed;

    u8 IPFC_status;
    // 0 is normal thread
    // 1 is normal thread awaiting ipfc completion
    // 2 is IPFC thread awaiting stack
    // 3 is IPFC running
    u64 IPFC_other_pid;
    u32 IPFC_other_tid;
    u16 IPFC_function_index;
    u16 IPFC_handler_index;
    u16 IPFC_stack_index;

    u32 awake_count;
    ThreadAwakeCondition awakes[THREAD_MAX_AWAKE_COUNT];
} Thread;

typedef struct
{
    u64 pid;
    u8 is_alive;
    u8 is_initialized;
} OwnedProcess;

typedef struct
{
    u8 is_initialized;
} IPFCFunctionExecution;

typedef struct
{
    u64 name_len;
    u8  name[64];
    void* ipfc_entry_point;
    void* stack_pages_start;
    u64 pages_per_stack;
    u64 stack_count;
    IPFCFunctionExecution function_executions[]; // this is used to track and avoid collisions
} IPFCHandler;

typedef struct
{
    // parent_index refers to the parent array on Process
    u16 parent_index; // saving on memory here. Seems *very* unlikely to not be enough range
    u16 handler_index;
    u8 is_initialized;
} IPFCSession;

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

    Kallocation out_stream_alloc; // Stream* array
    u64 out_stream_count;

    Kallocation in_stream_alloc; // Stream* array
    u64 in_stream_count;

    Kallocation semaphore_alloc; // ProcessSemaphore array
    u64 semaphore_count;

    Kallocation owned_process_alloc; // OwnedProcess array
    u64 owned_process_count;

    Kallocation parent_alloc; // u64/pid array
    u64 parent_count; // goes from highest up parent at array[0] to iminent parent
                      // at array[len-1]
    
    Kallocation ipfc_handler_alloc; // Kallocation that points to an IPFCHandler struct, array
    u64 ipfc_handler_count;

    Kallocation ipfc_session_alloc; // IPFCSession array
    u64 ipfc_session_count;

    KeyboardEventQueue kbd_event_queue;
    RawMouseEventQueue mouse_event_queue;

    u32 thread_count;
    u32 reference_count_for_threads;
    RWLock process_lock;
    u32 reference_count_for_processes;
    u64 pid;
    Thread threads[];
} Process;

Kallocation KERNEL_PROCESS_ARRAY_ALLOCATION = {0};
#define KERNEL_PROCESS_ARRAY ((Process**)KERNEL_PROCESS_ARRAY_ALLOCATION.memory)
u64 KERNEL_PROCESS_ARRAY_LEN = 0;
RWLock KERNEL_PROCESS_ARRAY_RWLOCK;

u64 process_create(u64* parents, u64 parent_count)
{
    Kallocation _proc = kalloc_pages((sizeof(Process)/PAGE_SIZE) + 1);
    Process* process = (Process*)_proc.memory;
    memset(process, 0, sizeof(Process));
    process->proc_alloc = _proc;

    u64 parent_array_page_count = (parent_count*sizeof(u64) + PAGE_SIZE - 1) / PAGE_SIZE;
    Kallocation parent_array_alloc = kalloc_pages(parent_array_page_count);
    for(u64 i = 0; i < parent_count; i++)
    { ((u64*)parent_array_alloc.memory)[i] = parents[i]; }
    process->parent_alloc = parent_array_alloc;
    process->parent_count = parent_count;

    process->reference_count_for_processes = 1;

    rwlock_create(&process->process_lock);
    process->mmu_table = create_mmu_table();
    for(u64 i = 0; i < 512; i++) { process->mmu_table[i] = 0; }

    for(u64 i = 0; i < KERNEL_PROCESS_ARRAY_LEN; i++)
    {
        if(KERNEL_PROCESS_ARRAY[i] == 0)
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

    s64 time_in_runqueue;
} ThreadRuntime;

#define THREAD_RUNTIME_UNINITIALIZED 0
#define THREAD_RUNTIME_INITIALIZED 1

typedef struct
{
    Kallocation memory_alloc;
    u64 len;
} ThreadRuntimeArray;

void thread_runtime_array_add(ThreadRuntimeArray* array, ThreadRuntime item)
{
#define THREAD_RUNTIME_ARRAY ((ThreadRuntime*)array->memory_alloc.memory)

    u64 runtime = 0;
    u8 has_runtime = 0;

    for(u64 i = 0; i < array->len; i++)
    {
        if(THREAD_RUNTIME_ARRAY[i].state == THREAD_RUNTIME_UNINITIALIZED)
        {
            runtime = i;
            has_runtime = 1;
        }
    }
    // We maybe must allocate a new runtime
    if(!has_runtime)
    {
        if((array->len + 1) * sizeof(ThreadRuntime) >
            array->memory_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(array->memory_alloc.page_count + 1);
            for(u64 i = 0; i < (new_alloc.page_count - 1) * (PAGE_SIZE / 8); i++)
            {
                *(((u64*)new_alloc.memory) + i) =
                        *(((u64*)array->memory_alloc.memory) + i);
            }
            kfree_pages(array->memory_alloc);
            array->memory_alloc = new_alloc;
        }

        runtime = array->len;
        array->len += 1;
    }

    THREAD_RUNTIME_ARRAY[runtime] = item;
}

ThreadRuntimeArray thread_runtime_commons;
Spinlock thread_runtime_commons_lock;
ThreadRuntimeArray local_thread_runtimes[KERNEL_MAX_HART_COUNT];

u32 process_thread_create(u64 pid)
{
    assert(pid < KERNEL_PROCESS_ARRAY_LEN, "pid is within range");
    assert(KERNEL_PROCESS_ARRAY[pid]->mmu_table != 0, "pid refers to a valid process");

    u64 thread_satp = mmu_table_ptr_to_satp(KERNEL_PROCESS_ARRAY[pid]->mmu_table);

    u8 has_been_allocated = 0;
    u32 tid = 0;

    for(u32 i = 0; i < KERNEL_PROCESS_ARRAY[pid]->thread_count; i++)
    {
        if(!KERNEL_PROCESS_ARRAY[pid]->threads[i].is_initialized)
        {
            memset(&KERNEL_PROCESS_ARRAY[pid]->threads[i], 0, sizeof(Thread));
            KERNEL_PROCESS_ARRAY[pid]->threads[i].is_initialized = 1;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].is_running = 0;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].IPFC_status = 0;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].should_be_destroyed = 0;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].awake_count = 0;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].frame.satp = thread_satp;
            KERNEL_PROCESS_ARRAY[pid]->threads[i].process_pid = pid;
            KERNEL_PROCESS_ARRAY[pid]->reference_count_for_threads++;
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
        memset(&KERNEL_PROCESS_ARRAY[pid]->threads[tid], 0, sizeof(Thread));
        KERNEL_PROCESS_ARRAY[pid]->thread_count += 1;

        KERNEL_PROCESS_ARRAY[pid]->threads[tid].is_initialized = 1;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].is_running = 0;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].IPFC_status = 0;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].should_be_destroyed = 0;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].awake_count = 0;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].frame.satp = thread_satp;
        KERNEL_PROCESS_ARRAY[pid]->threads[tid].process_pid = pid;
        KERNEL_PROCESS_ARRAY[pid]->reference_count_for_threads++;
    }

    // Now the thread has been created it has to be allocated a "runtime" so that it can be schedualed
    ThreadRuntime runtime;
    runtime.pid = pid;
    runtime.tid = tid;
    runtime.runtime = 0;
    runtime.time_in_runqueue = 0;
    runtime.state = THREAD_RUNTIME_INITIALIZED;
    spinlock_acquire(&thread_runtime_commons_lock);
    thread_runtime_array_add(&thread_runtime_commons, runtime);
    spinlock_release(&thread_runtime_commons_lock);

    return tid;
}

// you need read lock on process
u64 process_child_pid_to_owned_process_index(Process* process, u64 child_pid, u64* owned_process_index)
{
    for(u64 i = 0; i < process->owned_process_count; i++)
    {
        OwnedProcess* ops = process->owned_process_alloc.memory;
        if(ops[i].is_initialized && ops[i].is_alive && ops[i].pid == child_pid)
        {
            *owned_process_index = i;
            return 1;
        }
    }
    return 0;
}

// we assume write lock on process
void process_flag_all_threads_for_destruction(Process* process)
{
    for(u64 i = 0; i < process->thread_count; i++)
    {
        process->threads[i].should_be_destroyed = 1;
    }
}

// we still assume write lock on process
void process_destroy(Process* process)
{
    assert(process->reference_count_for_processes, "reference_count_for_processes > 0");
    process->reference_count_for_processes--;
    if(process->reference_count_for_processes)
    {
        rwlock_release_write(&process->process_lock);
        return;
    }

    u64* parents = process->parent_alloc.memory;
    for(u64 i = 0; i < process->parent_count; i++)
    {
        Process* pa = KERNEL_PROCESS_ARRAY[parents[i]];
        u64 owned_index;
        if(!process_child_pid_to_owned_process_index(pa, process->pid, &owned_index))
        { continue; }
        OwnedProcess* ops = pa->owned_process_alloc.memory;
        ops[owned_index].is_alive = 0;
    }

    mmu_unmap_table(process->mmu_table);
    kfree_single_page(process->mmu_table);
    KERNEL_PROCESS_ARRAY[process->pid] = 0;

    for(u64 i = 0; i < process->allocations_count; i++)
    {
        kfree_pages(((Kallocation*)process->allocations_alloc.memory)[i]);
    }
    kfree_pages(process->allocations_alloc);

    //Kallocation surface_alloc; // TODO
    //u64 surface_count;

    //Kallocation surface_consumer_alloc; // TODO
    //u64 surface_consumer_count;

    kfree_pages(process->file_access_redirects_alloc);
    kfree_pages(process->file_access_permissions_alloc);

    for(u64 i = 0; i < process->out_stream_count; i++)
    {
        Stream* stream = ((Stream**)process->out_stream_alloc.memory)[i];
        if(!stream) { continue; }
        stream_destroy(stream);
    }
    kfree_pages(process->out_stream_alloc);

    for(u64 i = 0; i < process->in_stream_count; i++)
    {
        Stream* stream = ((Stream**)process->in_stream_alloc.memory)[i];
        if(!stream) { continue; }
        stream_destroy(stream);
    }
    kfree_pages(process->in_stream_alloc);

    kfree_pages(process->semaphore_alloc);
    kfree_pages(process->owned_process_alloc);
    kfree_pages(process->parent_alloc);

    kfree_pages(process->proc_alloc);
}


// we assume write lock on process
void process_destroy_with_children(Process* process)
{
    u64 children[process->owned_process_count];
    u64 child_count = 0;
    OwnedProcess* ops = process->owned_process_alloc.memory;
    for(u64 i = 0; i < process->owned_process_count; i++)
    {
        if(ops[i].is_initialized && ops[i].is_alive)
        {
            children[child_count] = ops[i].pid;
            child_count++;
        }
    }
    rwlock_release_write(&process->process_lock);
    for(u64 i = 0; i < child_count; i++)
    {
        Process* p2 = KERNEL_PROCESS_ARRAY[children[i]];
        rwlock_acquire_write(&p2->process_lock);
        process_flag_all_threads_for_destruction(p2);
        rwlock_release_write(&p2->process_lock);
    }
    rwlock_acquire_write(&process->process_lock);
    process_destroy(process);
}

// we assume exclusive write lock to process
void process_destroy_thread(Process* process, u32 tid)
{
    Thread* t = &process->threads[tid];
    assert(process && process->mmu_table && process->reference_count_for_threads, "process is valid\n");
    assert(t->is_initialized, "thread exists");
    if(t->stack_alloc.page_count)
    {
        kfree_pages(t->stack_alloc);
    }
    t->is_initialized = 0;
    process->reference_count_for_threads--;
    if(!process->reference_count_for_threads)
    {
        process_destroy_with_children(process);
    }
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

u64 process_new_file_access(u64 pid, u64 redirect, u8 permission) // TODO: optimize beyond singular
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

    if(permissions_page_count > process->file_access_permissions_alloc.page_count)
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



u64 process_create_out_stream_slot(Process* process)
{
    for(u64 i = 0; i < process->out_stream_count; i++)
    {
        Stream** out_streams = process->out_stream_alloc.memory;
        if(out_streams[i] == 0)
        {
            out_streams[i] = 1;
            return i;
        }
    }

    if((process->out_stream_count + 1) * sizeof(Stream*) > process->out_stream_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(process->out_stream_alloc.page_count + 1);
        Stream** new_array = new_alloc.memory;
        Stream** old_array = process->out_stream_alloc.memory;
        for(u64 i = 0; i < process->out_stream_count; i++)
        {
            new_array[i] = old_array[i];
        }
        if(process->out_stream_alloc.page_count)
        {
            kfree_pages(process->out_stream_alloc);
        }
        process->out_stream_alloc = new_alloc;
    }

    u64 index = process->out_stream_count;
    process->out_stream_count++;

    Stream** array = process->out_stream_alloc.memory;
    array[index] = 1;
    return index;
}

u64 process_create_in_stream_slot(Process* process)
{
    for(u64 i = 0; i < process->in_stream_count; i++)
    {
        Stream** in_streams = process->in_stream_alloc.memory;
        if(in_streams[i] == 0)
        {
            in_streams[i] = 1;
            return i;
        }
    }
 
    if((process->in_stream_count + 1) * sizeof(Stream*) > process->in_stream_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(process->in_stream_alloc.page_count + 1);
        Stream** new_array = new_alloc.memory;
        Stream** old_array = process->in_stream_alloc.memory;
        for(u64 i = 0; i < process->in_stream_count; i++)
        {
            new_array[i] = old_array[i];
        }
        if(process->in_stream_alloc.page_count)
        {
            kfree_pages(process->in_stream_alloc);
        }
        process->in_stream_alloc = new_alloc;
    }
 
    u64 index = process->in_stream_count;
    process->in_stream_count++;
 
    Stream** array = process->in_stream_alloc.memory;
    array[index] = 1;
    return index;
}

void process_create_between_stream(Process* p1, Process* p2, u64 out_stream_index, u64 in_stream_index)
{
    Stream* stream = stream_create();
    atomic_s64_increment(&stream->reference_counter);
    ((Stream**) p1->out_stream_alloc.memory)[out_stream_index] = stream;
    ((Stream**) p2->in_stream_alloc.memory)[in_stream_index]   = stream;
}

typedef struct
{
    atomic_s64 counter;
    u32 max_value;
    u8 is_initialized;
    u8 _padding[3];
} ProcessSemaphore;

u64 process_create_semaphore(Process* process, u32 initial_value, u32 max_value)
{
    for(u64 i = 0; i < process->semaphore_count; i++)
    {
        ProcessSemaphore* semaphores = process->semaphore_alloc.memory;
        if(semaphores[i].is_initialized == 0)
        {
            semaphores[i].is_initialized = 1;
            semaphores[i].max_value = max_value;
            atomic_s64_set(&semaphores[i].counter, (s64)initial_value);
            return i;
        }
    }

    if((process->semaphore_count + 1) * sizeof(ProcessSemaphore) > process->semaphore_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(process->semaphore_alloc.page_count + 1);
        ProcessSemaphore* new_array = new_alloc.memory;
        ProcessSemaphore* old_array = process->semaphore_alloc.memory;
        for(u64 i = 0; i < process->semaphore_count; i++)
        {
            new_array[i] = old_array[i];
        }
        if(process->semaphore_alloc.page_count)
        {
            kfree_pages(process->semaphore_alloc);
        }
        process->semaphore_alloc = new_alloc;
    }

    u64 index = process->semaphore_count;
    process->semaphore_count++;

    ProcessSemaphore* semaphores = process->semaphore_alloc.memory;
    semaphores[index].is_initialized = 1;
    semaphores[index].max_value = max_value;
    atomic_s64_set(&semaphores[index].counter, (s64)initial_value);
    return index;
}

// Assumes you have a write lock on process
// Assumes that child_pid is alive
u64 process_create_owned_process(Process* process, u64 child_pid)
{
    for(u64 i = 0; i < process->owned_process_count; i++)
    {
        OwnedProcess* ops = process->owned_process_alloc.memory;
        if(!ops[i].is_initialized)
        {
            ops[i].is_initialized = 1;
            ops[i].is_alive = 1;
            ops[i].pid = child_pid;
            process->reference_count_for_processes++;
            return i;
        }
    }

    if((process->owned_process_count + 1) * sizeof(OwnedProcess) > process->owned_process_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(process->owned_process_alloc.page_count + 1);
        OwnedProcess* new_array = new_alloc.memory;
        OwnedProcess* old_array = process->owned_process_alloc.memory;
        for(u64 i = 0; i < process->owned_process_count; i++)
        {
            new_array[i] = old_array[i];
        }
        if(process->owned_process_alloc.page_count)
        {
            kfree_pages(process->owned_process_alloc);
        }
        process->owned_process_alloc = new_alloc;
    }

    u64 i = process->owned_process_count;
    process->owned_process_count++;

    OwnedProcess* ops = process->owned_process_alloc.memory;
    ops[i].is_initialized = 1;
    ops[i].is_alive = 1;
    ops[i].pid = child_pid;
    return i;
}



u64 process_ipfc_handler_create(
        Process* process,
		u8* name,
		u64 name_len,
		void* ipfc_entry_point,
		void* stack_pages_start,
		u64 pages_per_stack,
		u64 stack_count,
		u64* out_handler_id_ptr)
{
    for(u64 i = 0; i < process->ipfc_handler_count; i++)
    {
        Kallocation* array = process->ipfc_handler_alloc.memory;
        if(!array[i].page_count)
        { continue; }
        IPFCHandler* handler = array[i].memory;
        if(handler->name_len != name_len)
        { continue; }
        if(!memcmp(handler->name, name, name_len))
        { continue; }
        // Name already taken
        return 0;
    }
    
    u8 found_empty = 0;
    u64 found_index;
    for(u64 i = 0; i < process->ipfc_handler_count; i++)
    {
        Kallocation* array = process->ipfc_handler_alloc.memory;
        if(!array[i].page_count)
        {
            found_empty = 1;
            found_index = i;
            break;
        }
    }
    if(!found_empty)
    {
        if((process->ipfc_handler_count + 1) * sizeof(Kallocation) >
            process->ipfc_handler_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(process->ipfc_handler_alloc.page_count + 1);
            Kallocation* new_array = new_alloc.memory;
            Kallocation* old_array = process->ipfc_handler_alloc.memory;
            for(u64 i = 0; i < process->ipfc_handler_count; i++)
            {
                new_array[i] = old_array[i];
            }
            if(process->ipfc_handler_alloc.page_count)
            {
                kfree_pages(process->ipfc_handler_alloc);
            }
            process->ipfc_handler_alloc = new_alloc;
        }
        found_index = process->ipfc_handler_count;
        process->ipfc_handler_count++;
    }
    
    Kallocation alloc = kalloc_pages(
        (sizeof(IPFCHandler) + sizeof(IPFCFunctionExecution) * stack_count + PAGE_SIZE - 1) / PAGE_SIZE
    );
    IPFCHandler* handler = alloc.memory;
    handler->name_len = name_len;
    for(u64 i = 0; i < name_len; i++)
    { handler->name[i] = name[i]; }
    handler->ipfc_entry_point = ipfc_entry_point;
    handler->stack_pages_start = stack_pages_start;
    handler->pages_per_stack = pages_per_stack;
    handler->stack_count = stack_count;
    for(u64 i = 0; i < handler->stack_count; i++)
    { handler->function_executions[i].is_initialized = 0; }
    
    Kallocation* array = process->ipfc_handler_alloc.memory;
    array[found_index] = alloc;
    *out_handler_id_ptr = found_index;
    return 1;
}

u64 process_ipfc_session_init(Process* process, u8* name, u64 name_len, u64* session_id_ptr)
{
    u64* parent_array = process->parent_alloc.memory;
    for(s64 i = (s64)process->parent_count - 1; i >= 0; i--)
    {
        Process* parent = KERNEL_PROCESS_ARRAY[parent_array[i]];
        for(u64 j = 0; j < parent->ipfc_handler_count; j++)
        {
            IPFCHandler* handler;
            {
                Kallocation* handler_allocs = parent->ipfc_handler_alloc.memory;
                handler = handler_allocs[j].memory;
            }
            printf("handler_name=\"%.*s\"\n", handler->name_len, handler->name);
            if(handler->name_len != name_len)
            { continue; }

            u64 unequal = 0;
            for(u64 k = 0; k < name_len; k++)
            {
                if(name[k] != handler->name[k])
                { unequal = 1; break; }
            }
            if(unequal)
            { continue; }

            // match
            // now check for duplicate sessions
            for(u64 k = 0; k < process->ipfc_session_count; k++)
            {
                IPFCSession* sessions = process->ipfc_session_alloc.memory;
                if(sessions[k].parent_index == parent_array[i] && sessions[k].handler_index == j)
                { return 0; }
            }

            u8 found_empty = 0;
            u64 found_index;
            for(u64 k = 0; k < process->ipfc_session_count; k++)
            {
                IPFCSession* array = process->ipfc_session_alloc.memory;
                if(!array[k].is_initialized)
                {
                    found_empty = 1;
                    found_index = k;
                    break;
                }
            }
            if(!found_empty)
            {
                if((process->ipfc_session_count + 1) * sizeof(IPFCSession) >
                    process->ipfc_session_alloc.page_count * PAGE_SIZE)
                {
                    Kallocation new_alloc = kalloc_pages(process->ipfc_session_alloc.page_count + 1);
                    IPFCSession* new_array = new_alloc.memory;
                    IPFCSession* old_array = process->ipfc_session_alloc.memory;
                    for(u64 i = 0; i < process->ipfc_session_count; i++)
                    {
                        new_array[i] = old_array[i];
                    }
                    if(process->ipfc_session_alloc.page_count)
                    {
                        kfree_pages(process->ipfc_session_alloc);
                    }
                    process->ipfc_session_alloc = new_alloc;
                }
                found_index = process->ipfc_session_count;
                process->ipfc_session_count++;
            }
            IPFCSession* session = process->ipfc_session_alloc.memory;
            session += found_index;
            session->is_initialized = 1;
            session->parent_index = parent_array[i];
            session->handler_index = j;

            *session_id_ptr = found_index;
            return 1;
        }
    }
    return 0;
}



