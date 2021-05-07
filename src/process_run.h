#define SCHEDUALER_MIX_IN_WINDOW 50

u64 thread_runtime_is_live(ThreadRuntime r, u64 time_passed)
{
    Thread* t = &KERNEL_PROCESS_ARRAY[r.pid]->threads[r.tid];
 
         if(t->thread_state == THREAD_STATE_RUNNING)
    {  return 1;  }
    else if(t->thread_state == THREAD_STATE_TIME_SLEEP)
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
            if(!surface_has_commited(slot->surface)) { wake = 1; }
        }
        if(wake || t->surface_slot_wait.count == 0)
        {
            t->thread_state = THREAD_STATE_RUNNING;
            return 1;
        }
    }
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
        if(thread_runtime_is_live(KERNEL_THREAD_RUNTIME_ARRAY[i], time_passed))
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
        if(thread_runtime_is_live(KERNEL_THREAD_RUNTIME_ARRAY[i], 0))
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


void thread1_func();
void thread2_func();
void thread3_func();

void process_init()
{
    u64 pid = process_create();
    u64 pid2 = process_create();

    surface_create(KERNEL_PROCESS_ARRAY[pid]);
    surface_create(KERNEL_PROCESS_ARRAY[pid2]);

    vos[0].pid = pid;
    vos[0].is_active = 1;

    vos[1].pid = pid2;
    vos[1].is_active = 1;

    u64* table = KERNEL_PROCESS_ARRAY[pid]->mmu_table;
    u64* table2 = KERNEL_PROCESS_ARRAY[pid2]->mmu_table;

    mmu_kernel_map_range(table, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);
    mmu_kernel_map_range(table2, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table2, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table2, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table2, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);

//mmu_kernel_map_range(table, (u64*)HEAP_START, (u64*)(HEAP_START + HEAP_SIZE),   2 + 4);

    mmu_kernel_map_range(table, 0x10000000, 0x10000000, 2 + 4);
    mmu_kernel_map_range(table2, 0x10000000, 0x10000000, 2 + 4);

    u32 thread1 = process_thread_create(pid);
    u32 thread2 = process_thread_create(pid);
    u32 thread3 = process_thread_create(pid);

    u32 tp2 = process_thread_create(pid2);
    Thread* tp2t = &KERNEL_PROCESS_ARRAY[pid2]->threads[tp2];
    tp2t->stack_alloc = kalloc_pages(60);
    tp2t->frame.regs[2] = 
        ((u64)tp2t->stack_alloc.memory) + tp2t->stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table2,
            (u64*)tp2t->stack_alloc.memory,
            (u64*)tp2t->frame.regs[2],
            2 + 4
        );
    tp2t->program_counter = (u64)thread1_func;
    tp2t->thread_state = THREAD_STATE_RUNNING;

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

    tarr[thread1].thread_state = THREAD_STATE_RUNNING;
    tarr[thread2].thread_state = THREAD_STATE_RUNNING;
    tarr[thread3].thread_state = THREAD_STATE_RUNNING;

    u64 cs;
    if(surface_consumer_create(pid, pid2, &cs))
    {
        printf("consumer slot: %llu\n", cs);
    }

    u64* mtimecmp = (u64*)0x02004000;
    u64* mtime = (u64*)0x0200bff8;
    *mtimecmp = *mtime;
}

