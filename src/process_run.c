#define SCHEDUALER_SHUFFLE_CHANCE_INVERSE 32


u64 thread_runtime_is_live(Thread* t, u64 mtime)
{
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    rwlock_acquire_read(&process->process_lock);

    if(t->is_running)
    {  rwlock_release_read(&process->process_lock); return 1;  }

    if(t->is_waiting_on_semaphore_and_semaphore_handle & (1llu << 63))
    {
        u64 ansa_semaphori = t->is_waiting_on_semaphore_and_semaphore_handle & (~(1llu << 63));
        if(semaphorum_medium_expectare_conare(ansa_semaphori))
        {
            t->is_running = 1;
            t->is_waiting_on_semaphore_and_semaphore_handle = 0;
            semaphorum_medium_omitte(ansa_semaphori);
        }
        rwlock_release_read(&process->process_lock);
        return t->is_running;
    }

    for(u64 i = 0; i < t->awake_count; i++)
    {
        u8 wake_up = 0;
        if(t->awakes[i].awake_type == THREAD_AWAKE_TIME)
        {
            if(mtime >= t->awakes[i].awake_time)
            { wake_up = 1; }
        }
        else if(t->awakes[i].awake_type == THREAD_AWAKE_SURFACE)
        {
            SurfaceSlot* slot=((SurfaceSlot*)process->surface_alloc.memory)
                    + t->awakes[i].surface_slot;
            assert(t->awakes[i].surface_slot < process->surface_count
                    && slot->is_initialized,
                    "thread_runtime_is_live: the surface slot contains a valid surface");
            if( !slot->is_defering_to_consumer_slot &&
                !surface_slot_has_commited(process, t->awakes[i].surface_slot, 0) &&
                slot->has_been_fired) { wake_up = 1; }
        }
        else if(t->awakes[i].awake_type == THREAD_AWAKE_KEYBOARD)
        {
            if(process->kbd_event_queue.count)
            { wake_up = 1; }
        }
        else if(t->awakes[i].awake_type == THREAD_AWAKE_MOUSE)
        {
            if(process->mouse_event_queue.event_count)
            { wake_up = 1; }
        }
        else if(t->awakes[i].awake_type == THREAD_AWAKE_SEMAPHORE)
        {
            ProcessSemaphore* semaphores = process->semaphore_alloc.memory;
            if(atomic_s64_decrement(&semaphores[t->awakes[i].semaphore].counter) <= 0)
            {
                // no wake
                atomic_s64_increment(&semaphores[t->awakes[i].semaphore].counter);
            }
            else
            { wake_up = 1; }
        }
        if(wake_up)
        {
            t->awake_count = 0;
            t->is_running = 1;
            rwlock_release_read(&process->process_lock);
            return 1;
        }
    }
    rwlock_release_read(&process->process_lock);
    return 0;
}

void try_assign_ipfc_stack(Process* process, Thread* thread);

Thread kernel_current_threads[KERNEL_MAX_HART_COUNT];
u64 kernel_current_thread_pid[KERNEL_MAX_HART_COUNT];
u32 kernel_current_thread_tid[KERNEL_MAX_HART_COUNT];
u8 kernel_current_thread_has_thread[KERNEL_MAX_HART_COUNT];

u64 current_thread_runtimes[KERNEL_MAX_HART_COUNT];

u64 last_mtimes[KERNEL_MAX_HART_COUNT];

/*
 * Make sure you have a READ lock on KERNEL_PROCESS_ARRAY_RWLOCK
 * when calling kernel_choose_new_thread
 */

struct xoshiro256ss_state kernel_choose_new_thread_rando_state[KERNEL_MAX_HART_COUNT];
void kernel_choose_new_thread(u64 new_mtime, u64 hart)
{
    u64 before_time = kernel_rdtime();
    u64 before_instructions = kernel_rdinstret();

    rwlock_acquire_read(&THREAD_RUNTIME_ARRAY_LOCK);
    ThreadRuntime* runtime_array = THREAD_RUNTIME_ARRAY_ALLOC.memory;

    if(kernel_current_thread_has_thread[hart])
    {
        ThreadRuntime just_ran = runtime_array[current_thread_runtimes[hart]];
        for(u64 i = current_thread_runtimes[hart]; i + 1 < THREAD_RUNTIME_ARRAY_LEN; i++)
        {
            runtime_array[i] = runtime_array[i+1];
        }
        runtime_array[THREAD_RUNTIME_ARRAY_LEN-1] = just_ran;
    }

    kernel_current_thread_has_thread[hart] = 0;

    u8 found_new_thread = 0;
    u64 new_thread_runtime;

    u64 assign_array_index = 0;

    for(u64 i = 0; i < THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        Thread* thread = &KERNEL_PROCESS_ARRAY[runtime_array[i].pid]
                            ->threads[runtime_array[i].tid];
        if(thread->should_be_destroyed)
        {
            u64 pid = runtime_array[i].pid;
            u32 tid = runtime_array[i].tid;
            ThreadGroup* groups = THREAD_GROUP_ARRAY_ALLOC.memory;
            atomic_s64_decrement(&groups[runtime_array[i].thread_group_index].counts[hart]);

            rwlock_release_read(&THREAD_RUNTIME_ARRAY_LOCK);
            process_destroy_thread(KERNEL_PROCESS_ARRAY[pid], tid);
            rwlock_acquire_read(&THREAD_RUNTIME_ARRAY_LOCK);
            runtime_array = THREAD_RUNTIME_ARRAY_ALLOC.memory;

            continue;
        }
        u64 correct_index = assign_array_index++;
        runtime_array[correct_index] = runtime_array[i];

        if(thread->IPFC_status == 2) // thread is awaiting IPFC stack
        {
            try_assign_ipfc_stack(KERNEL_PROCESS_ARRAY[runtime_array[i].pid], thread);
        }
        if(thread->IPFC_status == 2) // thread is *still* awaiting IPFC stack
        {
            continue;
        }

        if(!found_new_thread)
        {
            u8 thread_live = thread_runtime_is_live(thread, new_mtime);
            if(thread_live)
            {
                found_new_thread = 1;
                new_thread_runtime = correct_index;
            }
        }
    }

    THREAD_RUNTIME_ARRAY_LEN = assign_array_index;

    if(!found_new_thread)
    {
        rwlock_release_read(&THREAD_RUNTIME_ARRAY_LOCK);
        // Causes the KERNEL nop thread to be loaded
        return;
    }

    current_thread_runtimes[hart] = new_thread_runtime;

    ThreadRuntime runtime = runtime_array[current_thread_runtimes[hart]];

    rwlock_release_read(&THREAD_RUNTIME_ARRAY_LOCK);

    kernel_current_thread_has_thread[hart] = 1;
    kernel_current_thread_tid[hart] = runtime.tid;
    kernel_current_thread_pid[hart] = runtime.pid;

    u64 after_time = kernel_rdtime();
    u64 after_instructions = kernel_rdinstret();
#if 0
    uart_printf("choose thread time = %llu\n      instructions = %llu\n",
                ((after_time - before_time) * 1000000) / MACHINE_TIMER_SECOND,
                after_instructions - before_instructions);
#endif
}

void program_loader_program(u64 drive1_partitions_directory);

void process_init()
{
    // system initialization
    for(u64 i = 0; i < KERNEL_MAX_HART_COUNT; i++)
    {
        last_mtimes[i] = 0;
    }
    rwlock_create(&KERNEL_PROCESS_ARRAY_RWLOCK);
    rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);

    u64 pid = process_create(0, 0);

    u64 out_stream = process_create_out_stream_slot(KERNEL_PROCESS_ARRAY[pid]);
    ((Stream**)KERNEL_PROCESS_ARRAY[pid]->out_stream_alloc.memory)[out_stream] = stream_create();
    u64 in_stream = process_create_in_stream_slot(KERNEL_PROCESS_ARRAY[pid]);
    ((Stream**)KERNEL_PROCESS_ARRAY[pid]->in_stream_alloc.memory)[in_stream] = stream_create();
    surface_create(KERNEL_PROCESS_ARRAY[pid]);

    vos[0].pid = pid;
    vos[0].is_active = 1;

    u64* table = KERNEL_PROCESS_ARRAY[pid]->mmu_table;

    mmu_kernel_map_range(table, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);

    mmu_kernel_map_range(table, 0x10000000, 0x10000000, 2 + 4);

    u32 thread1 = process_thread_create(pid, 1, 0, 0);

    Thread* tarr = KERNEL_PROCESS_ARRAY[pid]->threads;

    tarr[thread1].stack_alloc = kalloc_pages(60);
    tarr[thread1].frame.regs[2] = 
        ((u64)tarr[thread1].stack_alloc.memory) + tarr[thread1].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread1].stack_alloc.memory,
            (u64*)tarr[thread1].frame.regs[2],
            2 + 4
        );

    tarr[thread1].program_counter = (u64)program_loader_program;

    // give pid0 access to the root directory
    tarr[thread1].frame.regs[10] = drive1_partition_directory;
    process_new_filesystem_access(pid, drive1_partition_directory,
                                  FILE_ACCESS_PERMISSION_READ_WRITE_BIT |
                                  FILE_ACCESS_PERMISSION_IS_DIRECTORY_BIT);

    KERNEL_PROCESS_ARRAY[pid]->has_started = 1;

    tarr[thread1].is_running = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void try_assign_ipfc_stack(Process* process, Thread* thread)
{
    rwlock_acquire_write(&process->process_lock);

    IPFCHandler* handler =
        ((Kallocation*)process->ipfc_handler_alloc.memory)[thread->IPFC_handler_index].memory;
    u64 found_index = 0;
    u64 has_found = 0;
    for(u64 i = 0; i < handler->stack_count; i++)
    {
        if(!handler->function_executions[i].is_initialized)
        {
            has_found = 1;
            found_index = i;
            break;
        }
    }

    if(!has_found)
    {
        rwlock_release_write(&process->process_lock);
        return;
    }

    handler->function_executions[found_index].is_initialized = 1;
    thread->IPFC_stack_index = found_index;

    thread->frame.regs[8] = (found_index + 1) * handler->pages_per_stack * PAGE_SIZE;
    thread->frame.regs[8] += handler->stack_pages_start;

    thread->frame.regs[8] -= 1024;
    thread->frame.regs[12] = thread->frame.regs[8];
    thread->ipfc_static_data_virtual_addr = thread->frame.regs[8];
    u64* static_data_array;
    assert(!mmu_virt_to_phys(process->mmu_table, thread->frame.regs[8], (u64*)&static_data_array),
            "IPFCHandler.stack_pages_start and friends point to something valid. But this assert is for the static data array.\n");
    for(u64 i = 0; i < 128; i++)
    { static_data_array[i] = thread->ipfc_static_data_1024_bytes[i]; }

    thread->frame.regs[8] -= 2 * sizeof(u64);
    thread->frame.regs[2] = thread->frame.regs[8];
    u64* frame;
    assert(!mmu_virt_to_phys(process->mmu_table, thread->frame.regs[8], (u64*)&frame),
            "IPFCHandler.stack_pages_start and friends point to something valid.\n");
    *frame = 0;

    thread->program_counter = handler->ipfc_entry_point;

    thread->IPFC_status = 3;
    thread->is_running = 1;

    rwlock_release_write(&process->process_lock);
}

