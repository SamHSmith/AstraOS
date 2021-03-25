
void syscall_surface_commit(Thread** current_thread)
{
    TrapFrame* frame = &(*current_thread)->frame;
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[(*current_thread)->proccess_pid];

    u64 surface_slot = frame->regs[11];

    assert(surface_slot < proccess->surface_count &&
        ((Surface*)proccess->surface_alloc.memory)[surface_slot].is_initialized,
        "surface_commit: the surface slot contains to a valid surface");

    surface_commit(surface_slot, proccess);
    (*current_thread)->program_counter += 4;
}

void syscall_surface_acquire(volatile Thread** current_thread)
{
    TrapFrame* frame = &(*current_thread)->frame;
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[(*current_thread)->proccess_pid];

    u64 surface_slot = frame->regs[11];
    Framebuffer** fb;
    assert(
        mmu_virt_to_phys(proccess->mmu_table, frame->regs[12], (u64*)&fb) == 0,
        "you didn't do a memory bad"
    );

    assert(surface_slot < proccess->surface_count &&
        ((Surface*)proccess->surface_alloc.memory)[surface_slot].is_initialized,
        "surface_aquire: the surface slot contains to a valid surface");

    frame->regs[10] = surface_acquire(surface_slot, fb, proccess);
    (*current_thread)->program_counter += 4;
}

void syscall_thread_sleep(Thread** current_thread, u64 mtime)
{
    TrapFrame* frame = &(*current_thread)->frame;
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
    TrapFrame* frame = &(*current_thread)->frame;
    u64 surface_slot = frame->regs[11];
    Thread* t = *current_thread;

    Surface* surface=((Surface*)KERNEL_PROCCESS_ARRAY[t->proccess_pid]->surface_alloc.memory) + surface_slot;
    assert(surface_slot < KERNEL_PROCCESS_ARRAY[t->proccess_pid]->surface_count && surface->is_initialized,
            "wait_for_surface_draw: the surface slot contains to a valid surface");

    if(surface_has_commited(surface)) // replace surface with slot later
    {
        t->surface_slot_wait = surface_slot;
        t->thread_state = THREAD_STATE_SURFACE_WAIT;
        *current_thread = kernel_choose_new_thread(mtime, 1);
    }
    t->program_counter += 4;
}

// Vulkan style call that gets all the mouse state and resets it.
void syscall_get_raw_mouse(Thread** current_thread)
{
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[t->proccess_pid];

    u64 user_buf = frame->regs[11];
    u64 len = frame->regs[12];

    u64 mouse_count = 1;

    if(user_buf != 0)
    {
        RawMouse* buf;
        assert(
            mmu_virt_to_phys(proccess->mmu_table, user_buf, (u64*)&buf) == 0,
            "you didn't do a memory bad"
        );
        if(len < mouse_count) { mouse_count = len; }
        for(u64 i = 0; i < mouse_count; i++)
        {
            buf[i] = fetch_mouse_data(&proccess->mouse); // in the future there will be more
        }
    }
    frame->regs[10] = mouse_count;
    t->program_counter += 4;
}

void syscall_get_keyboard_events(Thread** current_thread)
{
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    Proccess* proccess = KERNEL_PROCCESS_ARRAY[t->proccess_pid];
 
    u64 user_buf = frame->regs[11];
    u64 len = frame->regs[12];
 
    u64 kbd_count = 1;
 
    if(user_buf != 0)
    {
        KeyboardEvent* buf;
        assert(
            mmu_virt_to_phys(proccess->mmu_table, user_buf, (u64*)&buf) == 0,
            "you didn't do a memory bad"
        );
        if(len < kbd_count) { kbd_count = len; }
        for(u64 i = 0; i < kbd_count; i++)
        {
            keyboard_poll_events(&proccess->kbd_event_queue, buf);
        }
    }
    frame->regs[10] = kbd_count;
    t->program_counter += 4;
}

void syscall_switch_vo(Thread** current_thread)
{
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;

    u64 new_vo = frame->regs[11];
    u64 ret = 0;

    if(vos[current_vo].pid != t->proccess_pid)
    {
        ret = 1; // failed, not authorized
    }
    else if(new_vo >= VO_COUNT || !(vos[new_vo].is_active))
    {
        ret = 2; // failed, invalid new_vo
    }
    else
    {
        ret = 0; //success
        current_vo = new_vo;
    }
    frame->regs[10] = ret;
    t->program_counter += 4;
}

/*
    if the running proccess is in a VO
        the VO's id will be stored in arg1 if arg1 is not null
        the function will return true
    if not
        the function will return false
*/
void syscall_get_vo_id(Thread** current_thread)
{
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;

    u64 ret = 0;
    u64 vo_id = 0;
    for(u64 i = 0; i < VO_COUNT; i++)
    {
        if(vos[i].is_active && vos[i].pid == t->proccess_pid)
        { ret = 1; vo_id = i; }
    }

    u64 user_vo_id_ptr = frame->regs[11];
    if(ret && user_vo_id_ptr != 0)
    {
        Proccess* proccess = KERNEL_PROCCESS_ARRAY[t->proccess_pid];
        u64* ptr;
        assert(
            mmu_virt_to_phys(proccess->mmu_table, user_vo_id_ptr, (u64*)&ptr) == 0,
            "you didn't do a memory bad"
        );
        *ptr = vo_id;
    }
    frame->regs[10] = ret;
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
    else if(call_num == 4)
    { syscall_get_raw_mouse(current_thread); }
    else if(call_num == 5)
    { syscall_get_keyboard_events(current_thread); }
    else if(call_num == 6)
    { syscall_switch_vo(current_thread); }
    else if(call_num == 7)
    { syscall_get_vo_id(current_thread); }
    else
    { printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
