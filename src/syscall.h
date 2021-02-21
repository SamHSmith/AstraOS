


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

void do_syscall(Thread** current_thread)
{
    u64 call_num = (*current_thread)->frame.regs[10];
         if(call_num == 0)
    { syscall_surface_commit(current_thread); }
    else if(call_num == 1)
    { syscall_surface_acquire(current_thread); }
    else
    { printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
