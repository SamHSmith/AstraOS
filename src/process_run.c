#define SCHEDUALER_SHUFFLE_CHANCE_INVERSE 32


u64 thread_runtime_is_live(Thread* t, u64 mtime)
{
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    rwlock_acquire_read(&process->process_lock);

    if(t->is_running)
    {  rwlock_release_read(&process->process_lock); return 1;  }

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
    rwlock_acquire_read(&THREAD_RUNTIME_ARRAY_LOCK);
    ThreadRuntime* runtime_array = THREAD_RUNTIME_ARRAY_ALLOC.memory;

    if(kernel_current_thread_has_thread[hart])
    {
        kernel_log_user(hart,
                        kernel_current_thread_pid[hart],
                        kernel_current_thread_tid[hart],
                        "hart has abandoned thread");
    }

    kernel_current_thread_has_thread[hart] = 0;

    u8 found_new_thread = 0;

    u64 thread_counter = 0;

    u64 new_thread_runtime;
    u32 highest_t_value = 0;

    for(u64 i = 0; i < THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        if(atomic_s64_read(&runtime_array[i].owning_hart) != hart || !runtime_array[i].is_initialized)
        { continue; }
        spinlock_acquire(&runtime_array[i].lock);
        thread_counter++;

        Thread* thread = &KERNEL_PROCESS_ARRAY[runtime_array[i].pid]
                            ->threads[runtime_array[i].tid];
        if(thread->should_be_destroyed)
        {
            ThreadGroup* groups = THREAD_GROUP_ARRAY_ALLOC.memory;
            atomic_s64_decrement(&groups[runtime_array[i].thread_group_index].counts[hart]);
            runtime_array[i].is_initialized = 0;
            process_destroy_thread(KERNEL_PROCESS_ARRAY[runtime_array[i].pid], runtime_array[i].tid);
            spinlock_release(&runtime_array[i].lock);
            continue;
        }

        if(thread->IPFC_status == 2) // thread is awaiting IPFC stack
        {
            try_assign_ipfc_stack(KERNEL_PROCESS_ARRAY[runtime_array[i].pid], thread);
        }
        if(thread->IPFC_status == 2) // thread is *still* awaiting IPFC stack
        {
            spinlock_release(&runtime_array[i].lock);
            continue;
        }

        u8 thread_live = thread_runtime_is_live(thread, new_mtime);

        // try punt thread to other hart
        // all threads involved with ipfc's are core locked
        // so they are not considered
        if(!thread->IPFC_status)
        {
            u64 lowest_hart = 0;
            u64 lowest_count = U32_MAX; // don't want to overflow
            u64 width_measure = 0;
            ThreadGroup* group = ((ThreadGroup*)THREAD_GROUP_ARRAY_ALLOC.memory)
                                    + runtime_array[i].thread_group_index;
            for(u64 j = 0; j < KERNEL_HART_COUNT.value; j++)
            {
                s64 c = atomic_s64_read(&group->counts[j]);
                if(c > 0)
                { width_measure++; }
                if(c < lowest_count)
                {
                    lowest_count = c;
                    lowest_hart = j;
                }
            }

            if( width_measure < runtime_array[i].allowed_width &&
                lowest_count + 1 < atomic_s64_read(&group->counts[hart]))
            {
                // punt
                atomic_s64_increment(&group->counts[lowest_hart]);
                atomic_s64_decrement(&group->counts[hart]);
                atomic_s64_set(&runtime_array[i].owning_hart, lowest_hart);
                spinlock_release(&runtime_array[i].lock);
                continue;
            }


            // should I try shuffle?
            u64 random_number = xoshiro256ss(&kernel_choose_new_thread_rando_state[hart]);
            if((!thread_live || !runtime_array[i].t_value) && random_number < U64_MAX / SCHEDUALER_SHUFFLE_CHANCE_INVERSE)
            {
                u64 send_hart = random_number % (u64)KERNEL_HART_COUNT.value;
                u64 me_count = atomic_s64_read(&group->counts[hart]);
                u64 dest_count = atomic_s64_read(&group->counts[send_hart]);
                u64 should_send = dest_count < me_count;

                if(width_measure >= runtime_array[i].allowed_width)
                {
                    should_send = should_send && (me_count == 1 || dest_count);
                }

                if(should_send)
                {
                    // send
                    atomic_s64_increment(&group->counts[send_hart]);
                    atomic_s64_decrement(&group->counts[hart]);
                    atomic_s64_set(&runtime_array[i].owning_hart, send_hart);
                    spinlock_release(&runtime_array[i].lock);
                    continue;
                }
            }
        }

        {
            u32 t = ++runtime_array[i].t_value;
            if(thread_live && t > highest_t_value)
            {
                highest_t_value = t;

                // unclaim previous claimed
                if(found_new_thread)
                {
                    spinlock_acquire(&runtime_array[new_thread_runtime].lock);
                    runtime_array[new_thread_runtime].is_being_run = 0;
                    spinlock_release(&runtime_array[new_thread_runtime].lock);
                }

                found_new_thread = 1;
                new_thread_runtime = i;
                // claim runtime
                runtime_array[new_thread_runtime].is_being_run = 1;
            }
        }

        spinlock_release(&runtime_array[i].lock);
    }

    // set t value to zero
    if(found_new_thread)
    {
        spinlock_acquire(&runtime_array[new_thread_runtime].lock);
        runtime_array[new_thread_runtime].t_value = 0;

//printf("hart#%llu - pid %llu tid %llu \n", hart, runtime_array[new_thread_runtime].pid, runtime_array[new_thread_runtime].tid);

        spinlock_release(&runtime_array[new_thread_runtime].lock);
    }

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

    kernel_log_user(hart,
                    kernel_current_thread_pid[hart],
                    kernel_current_thread_tid[hart],
                    "hart has chosen user thread");
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

