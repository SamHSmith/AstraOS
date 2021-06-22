#define SCHEDUALER_MIX_IN_WINDOW 50

u64 thread_runtime_is_live(Thread* t, u64 mtime)
{
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
 
    if(t->is_running)
    {  return 1;  }

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
                    && slot->surface.is_initialized,
                    "thread_runtime_is_live: the surface slot contains a valid surface");
            if( !slot->is_defering_to_consumer_slot &&
                !surface_slot_has_commited(process, t->awakes[i].surface_slot) &&
                slot->surface.has_been_fired) { wake_up = 1; }
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
        if(wake_up)
        {
            t->awake_count = 0;
            t->is_running = 1;
            return 1;
        }
    }
/*    else if(t->thread_state == THREAD_STATE_TIME_SLEEP)
    {
        if(t->sleep_time > time_passed)
        {
            t->sleep_time -= time_passed;
            return 0;
        }
        else
        {
            t->sleep_time = 0;
            t->thread_state = THREAD_STATE_RUNNING;
            return 1;
        }
    }
    else if(t->thread_state == THREAD_STATE_SURFACE_WAIT)
    {
        u8 wake = 0;
        for(u64 i = 0; i < t->surface_slot_wait.count; i++)
        {
            SurfaceSlot* slot=((SurfaceSlot*)KERNEL_PROCESS_ARRAY[t->process_pid]->surface_alloc.memory)
                    + t->surface_slot_wait.surface_slot[i];
            assert(t->surface_slot_wait.surface_slot[i] < KERNEL_PROCESS_ARRAY[r.pid]->surface_count
                    && slot->surface.is_initialized,
                    "thread_runtime_is_live: the surface slot contains a valid surface");
            if( !slot->is_defering_to_consumer_slot &&
                !surface_slot_has_commited(process, t->surface_slot_wait.surface_slot[i]) &&
                slot->surface.has_been_fired) { wake = 1; }
        }
        if(wake || t->surface_slot_wait.count == 0)
        {
            t->thread_state = THREAD_STATE_RUNNING;
            return 1;
        }
    }
*/
    return 0;
}

Thread* kernel_current_thread;

u64 current_thread_runtime = 0;

u64 last_mtime = 0;
struct xoshiro256ss_state rando_state = {5, 42, 68, 1};
Thread* kernel_choose_new_thread(u64 new_mtime, u64 apply_time)
{
    u64 time_passed = 0;
    if(last_mtime != 0) { time_passed = new_mtime - last_mtime; }
    last_mtime = new_mtime;

    if(apply_time) {
        KERNEL_THREAD_RUNTIME_ARRAY[current_thread_runtime].runtime = ( time_passed +
        (KERNEL_THREAD_RUNTIME_ARRAY[current_thread_runtime].runtime* (SCHEDUALER_MIX_IN_WINDOW-1))
          )  / SCHEDUALER_MIX_IN_WINDOW;
    }

    u64 total_runtime = 0;
    u64 participant_count = 0;

    for(u64 i = 0; i < KERNEL_THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        if(apply_time && i != current_thread_runtime)
        {
            KERNEL_THREAD_RUNTIME_ARRAY[i].runtime *= SCHEDUALER_MIX_IN_WINDOW-1;
            KERNEL_THREAD_RUNTIME_ARRAY[i].runtime /= SCHEDUALER_MIX_IN_WINDOW;
        }
        Thread* thread = &KERNEL_PROCESS_ARRAY[KERNEL_THREAD_RUNTIME_ARRAY[i].pid]
                            ->threads[KERNEL_THREAD_RUNTIME_ARRAY[i].tid];
        if(thread_runtime_is_live(thread, new_mtime))
        {
            total_runtime += KERNEL_THREAD_RUNTIME_ARRAY[i].runtime;
            participant_count += 1;
        }
    }

    u64 total_points = total_runtime * (participant_count - 1);
    u64 to_umax_factor = U64_MAX / total_points;

    u64 rando = xoshiro256ss(&rando_state);
    u64 bump = 0;
    u64 participant = 0;
  
    u8 found_new_thread = 0;
    u64 new_thread_runtime = 0;

    for(u64 i = 0; i < KERNEL_THREAD_RUNTIME_ARRAY_LEN; i++)
    {
        Thread* thread = &KERNEL_PROCESS_ARRAY[KERNEL_THREAD_RUNTIME_ARRAY[i].pid]
                            ->threads[KERNEL_THREAD_RUNTIME_ARRAY[i].tid];
        if(thread_runtime_is_live(thread, new_mtime))
        {
            u64 point = total_runtime - KERNEL_THREAD_RUNTIME_ARRAY[i].runtime;
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
    {  return 0;  } // Causes the KERNEL nop thread to be loaded

    current_thread_runtime = new_thread_runtime;

    ThreadRuntime runtime = KERNEL_THREAD_RUNTIME_ARRAY[current_thread_runtime];
    return &KERNEL_PROCESS_ARRAY[runtime.pid]->threads[runtime.tid];
}


void program_loader_program(u64 drive1_partitions_directory);

void process_init()
{
    u64 pid = process_create();

    process_create_out_stream(KERNEL_PROCESS_ARRAY[pid]);
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

    u64* mtimecmp = (u64*)0x02004000;
    u64* mtime = (u64*)0x0200bff8;
    *mtimecmp = *mtime;
}

