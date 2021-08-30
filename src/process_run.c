#define SCHEDUALER_MIX_IN_WINDOW 50
#define SCHEDUALER_SEND_BACK_FRACTION_NUMERATOR 1
#define SCHEDUALER_SEND_BACK_FRACTION_DENOMINATOR 3


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

Thread* kernel_current_threads[KERNEL_MAX_HART_COUNT];

u64 current_thread_runtimes[KERNEL_MAX_HART_COUNT];

u64 last_mtimes[KERNEL_MAX_HART_COUNT];

struct xoshiro256ss_state kernel_choose_new_thread_rando_state[KERNEL_MAX_HART_COUNT];
void kernel_choose_new_thread(Thread** out_thread, u64 new_mtime, u64 hart)
{
    u64 apply_time = 1;
    if(!*out_thread)
    { apply_time = 0; }

    u64 time_passed = 0;
    if(last_mtimes[hart] != 0) { time_passed = new_mtime - last_mtimes[hart]; }
    last_mtimes[hart] = new_mtime;

    if(spinlock_try_acquire(&thread_runtime_commons_lock))
    {

        u64 own_count = 0;
        u64 total_in_queue_time = 0;
        ThreadRuntime* own_arr = local_thread_runtimes[hart].memory_alloc.memory;
        for(u64 i = 0; i < local_thread_runtimes[hart].len; i++)
        {
            own_arr[i].time_in_runqueue += time_passed;
            if(own_arr[i].state == THREAD_RUNTIME_INITIALIZED && own_arr[i].time_in_runqueue > 0)
            {
                own_count += 1;
                total_in_queue_time += own_arr[i].time_in_runqueue;
            }
        }
        //printf("hart#%llu has %llu\n", hart, own_count);

        u64 own_temporary_storage_index = 0;
        ThreadRuntime own_temporary_storage[own_count];

        for(u64 i = 0; i < local_thread_runtimes[hart].len; i++)
        {
            if(own_arr[i].state == THREAD_RUNTIME_INITIALIZED && own_arr[i].time_in_runqueue > 0)
            {
                u64 random_number = xoshiro256ss(&kernel_choose_new_thread_rando_state[hart]);
                u64 bar = (U64_MAX / SCHEDUALER_SEND_BACK_FRACTION_DENOMINATOR) * SCHEDUALER_SEND_BACK_FRACTION_NUMERATOR;
                u64 amount = (u64)own_arr[i].time_in_runqueue;
                u64 total = total_in_queue_time/own_count;
                if(amount > total) { amount = total; }
                bar /= total;
                bar *= amount;
                //printf("%llu < to commons %llu\n%llu\n", random_number, random_number < bar, bar);
                if(!(random_number < bar))
                { continue; }
                own_arr[i].time_in_runqueue = 0;
                own_temporary_storage[own_temporary_storage_index] = own_arr[i];
                own_temporary_storage_index++;
                own_arr[i].state = THREAD_RUNTIME_UNINITIALIZED;
            }
        }
        u64 own_temporary_storage_count = own_temporary_storage_index;
        own_temporary_storage_index = 0;

        s64 count = 0;
        u64 total_out_of_queue_count = 0;
        ThreadRuntime* arr = thread_runtime_commons.memory_alloc.memory;
        for(u64 i = 0; i < thread_runtime_commons.len; i++)
        {
            arr[i].time_in_runqueue -= (s64)time_passed;
            if(arr[i].state == THREAD_RUNTIME_INITIALIZED && arr[i].time_in_runqueue < 0)
            {
                count += 1;
                total_out_of_queue_count += (u64)(-arr[i].time_in_runqueue);
            }
        }
        //printf("In the commons there are %llu\n", count);

        for(u64 i = 0; i < thread_runtime_commons.len; i++)
        {
            if(arr[i].state == THREAD_RUNTIME_INITIALIZED && arr[i].time_in_runqueue < 0)
            {
                u64 random_number = xoshiro256ss(&kernel_choose_new_thread_rando_state[hart]);
                u64 bar = (U64_MAX / SCHEDUALER_SEND_BACK_FRACTION_DENOMINATOR) * SCHEDUALER_SEND_BACK_FRACTION_NUMERATOR;
                u64 amount = (u64)(-arr[i].time_in_runqueue);
                u64 total = total_out_of_queue_count/count;
                if(amount > total) { amount = total; }
                bar /= total;
                bar *= amount;
                //printf("%llu < to queue %llu\n%llu\n", random_number, random_number < bar, bar);
                if(!(random_number < bar))
                { continue; }
                arr[i].time_in_runqueue = 0;
                thread_runtime_array_add(&local_thread_runtimes[hart], arr[i]);
                arr[i].state = THREAD_RUNTIME_UNINITIALIZED;
            }
        }

        // add those in temporary storage to commons
        for(u64 i = 0; i < own_temporary_storage_count; i++)
        {
            thread_runtime_array_add(&thread_runtime_commons, own_temporary_storage[i]);
        }

        //temp
/*        {
            u64 own_count = 0;
        u64 total_in_queue_time = 0;
        ThreadRuntime* own_arr = local_thread_runtimes[hart].memory_alloc.memory;
        for(u64 i = 0; i < local_thread_runtimes[hart].len; i++)
        {
            own_arr[i].time_in_runqueue += time_passed;
            if(own_arr[i].state == THREAD_RUNTIME_INITIALIZED && own_arr[i].time_in_runqueue > 0)
            {
                own_count += 1;
                total_in_queue_time += own_arr[i].time_in_runqueue;
            }
        }
        //printf("hart#%llu has %llu\n", hart, own_count);
        }*/

        spinlock_release(&thread_runtime_commons_lock);
    }
    ThreadRuntime* runtime_array = local_thread_runtimes[hart].memory_alloc.memory;
    u64 runtime_array_len = local_thread_runtimes[hart].len;

    if(apply_time) {
        runtime_array[current_thread_runtimes[hart]].runtime = ( time_passed +
        (runtime_array[current_thread_runtimes[hart]].runtime* (SCHEDUALER_MIX_IN_WINDOW-1))
          )  / SCHEDUALER_MIX_IN_WINDOW;
    }

    u64 total_runtime = 0;
    u64 participant_count = 0;

    for(u64 i = 0; i < runtime_array_len; i++)
    {
        if(runtime_array[i].state != THREAD_RUNTIME_INITIALIZED)
        { continue; }
        if(apply_time && i != current_thread_runtimes[hart])
        {
            runtime_array[i].runtime *= SCHEDUALER_MIX_IN_WINDOW-1;
            runtime_array[i].runtime /= SCHEDUALER_MIX_IN_WINDOW;
        }
        Thread* thread = &KERNEL_PROCESS_ARRAY[runtime_array[i].pid]
                            ->threads[runtime_array[i].tid];
        if(thread->should_be_destroyed)
        {
            runtime_array[i].state = THREAD_RUNTIME_UNINITIALIZED;
            rwlock_acquire_write(&KERNEL_PROCESS_ARRAY[runtime_array[i].pid]->process_lock);
            process_destroy_thread(KERNEL_PROCESS_ARRAY[runtime_array[i].pid], runtime_array[i].tid);
            continue;
        }
        if(thread->IPFC_status == 2) // thread is awaiting IPFC stack
        {
            try_assign_ipfc_stack(KERNEL_PROCESS_ARRAY[runtime_array[i].pid], thread);
        }
        if(thread_runtime_is_live(thread, new_mtime))
        {
            total_runtime += runtime_array[i].runtime;
            participant_count += 1;
        }
    }

    u64 total_points = total_runtime * (participant_count - 1);
    u64 to_umax_factor = U64_MAX / total_points;

    u64 rando = xoshiro256ss(&kernel_choose_new_thread_rando_state[hart]);
    u64 bump = 0;
    u64 participant = 0;

    u8 found_new_thread = 0;
    u64 new_thread_runtime = 0;

    for(u64 i = 0; i < runtime_array_len; i++)
    {
        if(runtime_array[i].state != THREAD_RUNTIME_INITIALIZED)
        { continue; }
        Thread* thread = &KERNEL_PROCESS_ARRAY[runtime_array[i].pid]
                            ->threads[runtime_array[i].tid];
        if(thread_runtime_is_live(thread, new_mtime))
        {
            u64 point = total_runtime - runtime_array[i].runtime;
            bump += point * to_umax_factor;
            if(bump >= rando || participant == (participant_count - 1))
            {
                found_new_thread = 1;
                new_thread_runtime = i;
                break;
            }
            participant += 1;
        }
    }

    if(!found_new_thread)
    {
        // Causes the KERNEL nop thread to be loaded
        *out_thread = 0;
        return;
    }

    current_thread_runtimes[hart] = new_thread_runtime;

    ThreadRuntime runtime = runtime_array[current_thread_runtimes[hart]];
    *out_thread = &KERNEL_PROCESS_ARRAY[runtime.pid]->threads[runtime.tid];
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

    u32 thread1 = process_thread_create(pid);

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
    tarr[thread1].frame.regs[10] = drive1_partition_directory;

    tarr[thread1].is_running = 1;
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
printf("------------------------did I find?\n");
    if(!has_found)
    {
printf("there was no space\n");
        rwlock_release_write(&process->process_lock);
        return;
    }

    handler->function_executions[found_index].is_initialized = 1;
    thread->IPFC_stack_index = found_index;

    thread->frame.regs[8] = (found_index + 1) * handler->pages_per_stack * PAGE_SIZE;
    thread->frame.regs[8] += handler->stack_pages_start;
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

