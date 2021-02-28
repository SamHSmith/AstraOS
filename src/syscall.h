


void syscall_surface_commit(Thread** current_thread)
{
    TrapFrame* frame = &(*current_thread)->frame; // Seems dumb to pull the whole frame onto the stack
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[(*current_thread)->proccess_pid];

    u64 surface_slot = frame->regs[11];

    surface_commit(surface_slot, proccess);
    (*current_thread)->program_counter += 4;
}

void syscall_surface_acquire(volatile Thread** current_thread)
{
    TrapFrame* frame = &(*current_thread)->frame; // Seems dumb to pull the whole frame onto the stack
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[(*current_thread)->proccess_pid];

    u64 surface_slot = frame->regs[11];
    Framebuffer** fb;
    assert(
        mmu_virt_to_phys(proccess->mmu_table, frame->regs[12], (u64*)&fb) == 0,
        "you didn't do a memory bad"
    );

    frame->regs[10] = surface_acquire(surface_slot, fb, proccess);
    (*current_thread)->program_counter += 4;
}

void syscall_thread_sleep(Thread** current_thread, u64 mtime)
{
    TrapFrame* frame = &(*current_thread)->frame; // Seems dumb to pull the whole frame onto the stack
    u64 sleep_time = frame->regs[11];
    Thread* t = *current_thread;

    t->thread_state = THREAD_STATE_INITIALIZED; // Switch to a different thread
    *current_thread = kernel_choose_new_thread(mtime, 1);

    t->thread_state = THREAD_STATE_TIME_SLEEP; // Then set the sleep time
    t->sleep_time = sleep_time;
    t->program_counter += 4;
}

void syscall_wait_for_surface_draw(Thread** current_thread, u64 mtime)
{
    TrapFrame* frame = &(*current_thread)->frame; // Seems dumb to pull the whole frame onto the stack
    u64 surface_slot = frame->regs[11];
    Thread* t = *current_thread;

    if(surface_has_commited(surface)) // replace surface with slot later
    {
        t->surface_slot_wait = surface_slot;
        t->thread_state = THREAD_STATE_SURFACE_WAIT;
        *current_thread = kernel_choose_new_thread(mtime, 1);
    }
    t->program_counter += 4;
}

void do_syscall(Thread** current_thread, u64 mtime)
{
    u64 call_num = (*current_thread)->frame.regs[10];
         if(call_num == 0)
    { syscall_surface_commit(current_thread); }
    else if(call_num == 1)
    { syscall_surface_acquire(current_thread); }
    else if(call_num == 2)
    { syscall_thread_sleep(current_thread, mtime); }
    else if(call_num == 3)
    { syscall_wait_for_surface_draw(current_thread, mtime); }
    else
    { printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
