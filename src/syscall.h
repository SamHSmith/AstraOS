
void syscall_surface_commit(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    TrapFrame* frame = &(*current_thread)->frame;
    Process* process = KERNEL_PROCESS_ARRAY[(*current_thread)->process_pid];

    u64 surface_slot = frame->regs[11];

    assert(surface_slot < process->surface_count &&
        ((SurfaceSlot*)process->surface_alloc.memory)[surface_slot].is_initialized,
        "surface_commit: the surface slot contains to a valid surface");

    frame->regs[10] = surface_commit(surface_slot, process);
    (*current_thread)->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

/*
    If you pass 0 page_count the function returns the page_count
    required for the acquire.
    To successfully acquire surface_slot must point to a valid slot,
    fb must be a 4096 byte aligned pointer, page_count must be
    the page_count required for this acquire.
*/
void syscall_surface_acquire(volatile Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    TrapFrame* frame = &(*current_thread)->frame;
    Process* process = KERNEL_PROCESS_ARRAY[(*current_thread)->process_pid];

    u64 surface_slot = frame->regs[11];
    Framebuffer* fb = frame->regs[12];
    u64 page_count = frame->regs[13];
    u64 ret = 0;

    SurfaceSlot* slot = &((SurfaceSlot*)process->surface_alloc.memory)[surface_slot];

    if(surface_slot < process->surface_count && slot->is_initialized && slot->has_been_fired)
    {
        assert(!slot->has_acquired, "you have not already acquired");
        surface_prepare_draw_framebuffer(surface_slot, process);
        if(page_count == 0 && !fb)
        {
            ret = slot->fb_draw->alloc.page_count;
        }
        else if(page_count >= slot->fb_draw->alloc.page_count)
        {
            ret = surface_acquire(surface_slot, fb, process);
        }
    }
    frame->regs[10] = ret;
    (*current_thread)->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_awake_after_time(Thread** current_thread, u64 hart, u64 mtime)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    TrapFrame* frame = &(*current_thread)->frame;
    u64 sleep_time = frame->regs[11];
    Thread* t = *current_thread;
    t->program_counter += 4;

    if(t->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = t->awake_count;
    t->awake_count++;

    t->awakes[awake_index].awake_type = THREAD_AWAKE_TIME;
    t->awakes[awake_index].awake_time = sleep_time + mtime;

    frame->regs[10] = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_awake_on_surface(Thread** current_thread, u64 hart, u64 mtime)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_surface_slots = frame->regs[11];
    u64 count = frame->regs[12];
    t->program_counter += 4;

    if(count == 0 || t->awake_count + count > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 surface_slot_array[THREAD_MAX_AWAKE_COUNT];
    u64 surface_slot_count = 0;

    for(u64 i = 0; i < count; i++)
    {
        u64* surface_slot;
        if(mmu_virt_to_phys(process->mmu_table, user_surface_slots + i*8, (u64*)&surface_slot) == 0)
        {
            SurfaceSlot* slot=
        ((SurfaceSlot*)process->surface_alloc.memory) + *surface_slot;

            if(*surface_slot < process->surface_count &&
                slot->is_initialized)
            {
                surface_slot_array[surface_slot_count] = *surface_slot;
                surface_slot_count++;
            }
        }
    }

    if(surface_slot_count == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = t->awake_count;
    t->awake_count += surface_slot_count;
    for(u64 i = 0; i < surface_slot_count; i++)
    {
        t->awakes[awake_index + i].awake_type = THREAD_AWAKE_SURFACE;
        t->awakes[awake_index + i].surface_slot = surface_slot_array[i];
    }
    frame->regs[10] = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_sleep(Thread** current_thread, u64 hart, u64 mtime)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    t->program_counter += 4;

    t->is_running = t->awake_count == 0;
    if(thread_runtime_is_live(t, mtime))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    // go to sleep
    frame->regs[10] = 1;
    kernel_choose_new_thread(current_thread, mtime, hart);
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_awake_on_keyboard(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    t->program_counter += 4;

    if(t->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = t->awake_count;
    t->awake_count++;

    t->awakes[awake_index].awake_type = THREAD_AWAKE_KEYBOARD;

    frame->regs[10] = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_awake_on_mouse(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    t->program_counter += 4;

    if(t->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = t->awake_count;
    t->awake_count++;

    t->awakes[awake_index].awake_type = THREAD_AWAKE_MOUSE;

    frame->regs[10] = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_rawmouse_events(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];

    u64 user_buf = frame->regs[11];
    u64 len = frame->regs[12];

    // TODO: add multiple mouse support. Probably something we do after there is a test case

    if(user_buf != 0)
    {
        RawMouseEvent* buf;
        if(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf))
        {
            frame->regs[10] = 0;
            t->program_counter += 4;
            rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(len > process->mouse_event_queue.event_count)
        { len = process->mouse_event_queue.event_count; }
        for(u64 i = 0; i < len; i++)
        {
            buf[i] = process->mouse_event_queue.events[i];
        }
        u64 event_queue_len = process->mouse_event_queue.event_count;
        for(u64 i = 0; i < event_queue_len - len; i++)
        {
            process->mouse_event_queue.events[i] = process->mouse_event_queue.events[i + len];
        }
        frame->regs[10] = len;
        process->mouse_event_queue.event_count -= len;
    }
    else
    {
        frame->regs[10] = process->mouse_event_queue.event_count;
    }

    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_keyboard_events(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];

    u64 user_buf = frame->regs[11];
    u64 len = frame->regs[12];

    if(user_buf != 0)
    {
        KeyboardEvent* buf;
        assert(
            mmu_virt_to_phys(process->mmu_table, user_buf + sizeof(KeyboardEvent)*len, (u64*)&buf) == 0,
            "you didn't do a memory bad"
        );
        assert(
            mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf) == 0,
            "you didn't do a memory bad"
        );
        len = keyboard_poll_events(&process->kbd_event_queue, buf, len);
    }
    else
    {
        len = process->kbd_event_queue.count;
    }
    frame->regs[10] = len;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_switch_vo(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;

    u64 new_vo = frame->regs[11];
    u64 ret = 0;

    if(vos[current_vo].pid != t->process_pid)
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
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

/*
    if the running process is in a VO
        the VO's id will be stored in arg1 if arg1 is not null
        the function will return true
    if not
        the function will return false
*/
void syscall_get_vo_id(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;

    u64 ret = 0;
    u64 vo_id = 0;
    for(u64 i = 0; i < VO_COUNT; i++)
    {
        if(vos[i].is_active && vos[i].pid == t->process_pid)
        { ret = 1; vo_id = i; }
    }

    u64 user_vo_id_ptr = frame->regs[11];
    if(ret && user_vo_id_ptr != 0)
    {
        Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
        u64* ptr;
        assert(
            mmu_virt_to_phys(process->mmu_table, user_vo_id_ptr, (u64*)&ptr) == 0,
            "you didn't do a memory bad"
        );
        *ptr = vo_id;
    }
    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_alloc_pages(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;

    u64 vaddr = frame->regs[11];
    u64 page_count = frame->regs[12];
    u64 ret = 0; //default case is failed allocation

    Kallocation a = kalloc_pages(page_count);
    if(a.page_count > 0)
    {
        ret = process_alloc_pages(process, vaddr, a);
    }
    if(!ret)
    {
        kfree_pages(a);
    }

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_shrink_allocation(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;

    u64 vaddr = frame->regs[11];
    u64 new_page_count = frame->regs[12];
    u64 ret = 0; //default case is failed shrink

    Kallocation remove = process_shrink_allocation(process, vaddr, new_page_count);
    if(remove.memory != 0)
    {
        kfree_pages(remove);
        ret = 1;
    }

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_has_commited(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    u64 consumer_slot = frame->regs[11];
    frame->regs[10] = surface_consumer_has_commited(t->process_pid, consumer_slot);
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_get_size(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;

    u64 consumer_slot = frame->regs[11];
    u64 user_width = frame->regs[12];
    u64 user_height = frame->regs[13];
    u64 ret = 1;

    u32* width;
    u32* height;

    if(user_width != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_width, (u64*)&width) == 0))
        { ret = 0; }
    }
    if(user_height != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_height, (u64*)&height) == 0))
        { ret = 0; }
    }

    if(ret)
    {
        ret = surface_consumer_get_size(t->process_pid, consumer_slot, width, height);
    }

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_set_size(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
 
    u64 consumer_slot = frame->regs[11];
    u32 width = frame->regs[12];
    u32 height = frame->regs[13];

    u64 ret = surface_consumer_set_size(t->process_pid, consumer_slot, width, height);

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_fetch(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];

    u64 consumer_slot = frame->regs[11];
    Framebuffer* fb = frame->regs[12];
    u64 page_count = frame->regs[13];
    u64 ret = 0;

    ret = surface_consumer_fetch(t->process_pid, consumer_slot, fb, page_count);

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_time_get_seconds(Thread** current_thread, u64 hart, u64 mtime)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    *((f64*)&frame->regs[10]) = ((f64)mtime) / 10000000.0;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_get_name(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_file_id = frame->regs[11];
    u64 user_buf = frame->regs[12];
    u64 buf_size = frame->regs[13];

    u64 file_id;
    if(!process_get_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        t->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
 
    u64 buf = 0;
    if(buf_size != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf) == 0))
        {
            frame->regs[10] = 0;
            t->program_counter += 4;
            rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
 
        {
            u64 num_pages = (buf_size + PAGE_SIZE) / PAGE_SIZE;
            u64 ptr = user_buf + PAGE_SIZE;
            u64 temp;
            for(u64 i = 1; i < num_pages; i++)
            {
                if(!(mmu_virt_to_phys(process->mmu_table, ptr, (u64*)&temp) == 0))
                {
                    frame->regs[10] = 0;
                    t->program_counter += 4;
                    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
                    return;
                }
                ptr += PAGE_SIZE;
            }
        }
    }
 
    frame->regs[10] = kernel_file_get_name(file_id, buf, buf_size);
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_get_files(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 dir_id = frame->regs[11];
    u64 user_buf = frame->regs[12];
    u64 buf_size = frame->regs[13];

    u64 buf = 0;
    if(buf_size != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf) == 0))
        {
            frame->regs[10] = 0;
            t->program_counter += 4;
            rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }

        {
            u64 num_pages = (buf_size*8 + PAGE_SIZE) / PAGE_SIZE;
            u64 ptr = user_buf + PAGE_SIZE;
            u64 temp;
            for(u64 i = 1; i < num_pages; i++)
            {
                if(!(mmu_virt_to_phys(process->mmu_table, ptr, (u64*)&temp) == 0))
                {
                    frame->regs[10] = 0;
                    t->program_counter += 4;
                    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
                    return;
                }
                ptr += PAGE_SIZE;
            }
        }
    }

    u64 temp_buf[buf_size];
    u64 ret = kernel_directory_get_files(dir_id, temp_buf, buf_size);
    u64 count = ret;
    if(buf_size < count) { count = buf_size; }
    u64* array = buf;
    for(u64 i = 0; i < count; i++)
  { array[i] = process_new_file_access(t->process_pid, temp_buf[i], FILE_ACCESS_PERMISSION_READ_WRITE_BIT); }

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_create_process_from_file(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_file_id = frame->regs[11];
    u64 user_pid_ptr = frame->regs[12];

    u64 file_id = 0;
    u64 ret = process_get_read_access(process, user_file_id, &file_id);
 
    u64* pid_ptr = 0;
    if(!ret && !(mmu_virt_to_phys(process->mmu_table, user_pid_ptr + sizeof(u64), (u64*)&pid_ptr) == 0) ||
               !(mmu_virt_to_phys(process->mmu_table, user_pid_ptr, (u64*)&pid_ptr) == 0))
    {
        frame->regs[10] = 0;
        t->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    // ret is true
    ret = create_process_from_file(file_id, pid_ptr);

    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_create(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_foreign_pid = frame->regs[11];
    u64 user_consumer_ptr = frame->regs[12];

    u64* consumer_ptr = 0;
    if(!(mmu_virt_to_phys(process->mmu_table, user_consumer_ptr + sizeof(u64), (u64*)&consumer_ptr) == 0) ||
       !(mmu_virt_to_phys(process->mmu_table, user_consumer_ptr, (u64*)&consumer_ptr) == 0) ||
       !(user_foreign_pid < KERNEL_PROCESS_ARRAY_LEN && KERNEL_PROCESS_ARRAY[user_foreign_pid]->mmu_table))
    {
        frame->regs[10] = 0;
        t->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign_process = KERNEL_PROCESS_ARRAY[user_foreign_pid];

    frame->regs[10] = surface_consumer_create(process, foreign_process, consumer_ptr);
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_fire(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    TrapFrame* frame = &t->frame;
    u64 consumer_slot = frame->regs[11];

    frame->regs[10] = surface_consumer_fire(t->process_pid, consumer_slot);
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_forward_to_consumer(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 surface_slot =  frame->regs[11];
    u64 consumer_slot = frame->regs[12];

    SurfaceSlot* slot = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;
    SurfaceConsumer* con = ((SurfaceConsumer*)process->surface_consumer_alloc.memory) + consumer_slot;

    u64 ret = 0;
    if(surface_slot < process->surface_count && slot->is_initialized &&
        consumer_slot < process->surface_consumer_count && con->is_initialized)
    {
        slot->is_defering_to_consumer_slot = 1;
        slot->defer_consumer_slot = consumer_slot;
        surface_slot_fire(process, surface_slot);
        ret = 1;
    }
    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_stop_forwarding_to_consumer(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 surface_slot =  frame->regs[11];
 
    SurfaceSlot* slot = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;
    u64 ret = 0;
    if(surface_slot < process->surface_count && slot->is_initialized)
    {
        slot->is_defering_to_consumer_slot = 0;
        slot->has_commited = 0;
        slot->has_been_fired = 1;
        ret = 1;
    }
    frame->regs[10] = ret;
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_forward_keyboard_events(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process_orig = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_buf = frame->regs[11];
    u64 user_len = frame->regs[12];
    u64 user_pid = frame->regs[13];

    // TODO: do proper security to only allow sending keystrokes to *owned* processes.
    Process* process_other = KERNEL_PROCESS_ARRAY[user_pid];
    if(user_pid >= KERNEL_PROCESS_ARRAY_LEN || !process_other->mmu_table)
    {
        frame->regs[10] = 0;
        t->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 ret = 1;

    if(user_len + process_other->kbd_event_queue.count > KEYBOARD_EVENT_QUEUE_LEN)
    { user_len = KEYBOARD_EVENT_QUEUE_LEN - process_other->kbd_event_queue.count; }

    if(!user_len)
    {
        frame->regs[10] = 0;
        t->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 page_count = (user_len * sizeof(KeyboardEvent) + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 user_ptr = user_buf + page_count * PAGE_SIZE;
    u64 ptr;

    for(u64 i = 0; i < page_count; i++)
    {
        user_ptr -= PAGE_SIZE;
        if(mmu_virt_to_phys(process_orig->mmu_table, user_ptr, (u64*)&ptr) != 0)
        {
            ret = 0;
            break;
        }
    }
    if(ret)
    {
        KeyboardEvent* event_buf = ptr;
        u64 start_index = process_other->kbd_event_queue.count;
        process_other->kbd_event_queue.count += user_len;
        for(u64 i = 0; i < user_len; i++)
        {
            process_other->kbd_event_queue.new_events[i+start_index] = event_buf[i];
        }
        frame->regs[10] = user_len;
    }
    else
    { frame->regs[10] = 0; }
    t->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_stream_put(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_out_stream = frame->regs[11];
    u64 user_memory = frame->regs[12];
    u64 user_count = frame->regs[13];
    t->program_counter += 4;

    Stream** out_stream_array = process->out_stream_alloc.memory;
    if(user_out_stream >= process->out_stream_count || out_stream_array[user_out_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Stream* out_stream = out_stream_array[user_out_stream];

    u64 page_count = (user_count+(user_memory % PAGE_SIZE) + PAGE_SIZE - 1) / PAGE_SIZE;

    u8* memory = 0;
    if(page_count)
    {
    u64 i = page_count - 1;
    while(1)
    {
        if(mmu_virt_to_phys(process->mmu_table, user_memory + (PAGE_SIZE * i),
            (u64*)&memory) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }

    frame->regs[10] = stream_put(out_stream, memory, user_count);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_stream_take(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_in_stream = frame->regs[11];
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];
    u64 user_byte_count_in_stream = frame->regs[14];
    t->program_counter += 4;

    u64* byte_count_in_stream_ptr;
    if(mmu_virt_to_phys(process->mmu_table, user_byte_count_in_stream + sizeof(u64),
                        (u64*)&byte_count_in_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_byte_count_in_stream,
                        (u64*)&byte_count_in_stream_ptr) != 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    *byte_count_in_stream_ptr = 0;

    Stream** in_stream_array = process->in_stream_alloc.memory;
    if(user_in_stream >= process->in_stream_count || in_stream_array[user_in_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Stream* in_stream = in_stream_array[user_in_stream];

    u64 page_count = (user_buffer_size+(user_buffer % PAGE_SIZE) + PAGE_SIZE - 1) / PAGE_SIZE;
 
    u8* buffer = 0;
    if(page_count)
    {
    u64 i = page_count - 1;
    while(1)
    {
        if(mmu_virt_to_phys(process->mmu_table, user_buffer + PAGE_SIZE * i, (u64*)&buffer) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }

    frame->regs[10] = stream_take(in_stream, buffer, user_buffer_size, byte_count_in_stream_ptr);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_create_out_stream(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_process_handle = frame->regs[11]; // as of now just the pid, very insecure
    u64 user_foreign_out_stream_ptr = frame->regs[12];
    u64 user_owned_in_stream_ptr = frame->regs[13];
    t->program_counter += 4;

    if( user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];
 
    u64* foreign_out_stream_ptr = 0;
    if(user_foreign_out_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_foreign_out_stream_ptr + sizeof(u64),
                        (u64*)&foreign_out_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_foreign_out_stream_ptr,
                        (u64*)&foreign_out_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64* owned_in_stream_ptr = 0;
    if(user_owned_in_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_owned_in_stream_ptr + sizeof(u64),
                        (u64*)&owned_in_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_owned_in_stream_ptr,
                        (u64*)&owned_in_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 out_stream;
    u64 in_stream;
    process_create_between_stream(foreign, process, &out_stream, &in_stream);

    frame->regs[10] = 1;
    if(foreign_out_stream_ptr) { *foreign_out_stream_ptr = out_stream; }
    if(owned_in_stream_ptr)    { *owned_in_stream_ptr = in_stream; }
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_create_in_stream(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    u64 user_process_handle = frame->regs[11]; // as of now just the pid, very insecure
    u64 user_owned_out_stream_ptr = frame->regs[12];
    u64 user_foreign_in_stream_ptr = frame->regs[13];
    t->program_counter += 4;
 
    if( user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];
 
    u64* owned_out_stream_ptr = 0;
    if(user_owned_out_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_owned_out_stream_ptr + sizeof(u64),
                        (u64*)&owned_out_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_owned_out_stream_ptr,
                        (u64*)&owned_out_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
 
    u64* foreign_in_stream_ptr = 0;
    if(user_foreign_in_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_foreign_in_stream_ptr + sizeof(u64),
                        (u64*)&foreign_in_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_foreign_in_stream_ptr,
                        (u64*)&foreign_in_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
 
    u64 out_stream;
    u64 in_stream;
    process_create_between_stream(process, foreign, &out_stream, &in_stream);
 
    frame->regs[10] = 1;
    if(owned_out_stream_ptr)  { *owned_out_stream_ptr = out_stream; }
    if(foreign_in_stream_ptr) { *foreign_in_stream_ptr = in_stream; }
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_start(Thread** current_thread, u64 hart)
{
    {
        volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
        volatile u64* mtime = (u64*)0x0200bff8;
        u64 start_wait = *mtime;
        rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        u64 end_wait = *mtime;

        wait_time_acc[hart] += end_wait - start_wait;
        wait_time_times[hart] += 1;
    }
    Thread* t = *current_thread;
    Process* process = KERNEL_PROCESS_ARRAY[t->process_pid];
    TrapFrame* frame = &t->frame;
    t->program_counter += 4;
    u64 user_process_handle = frame->regs[11];

    if(user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];

    if(foreign->thread_count == 0 || foreign->threads[0].is_initialized == 0)
    {
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    foreign->threads[0].is_running = 1;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void do_syscall(Thread** current_thread, u64 mtime, u64 hart)
{
    u64 call_num = (*current_thread)->frame.regs[10];
         if(call_num == 0)
    { syscall_surface_commit(current_thread, hart); }
    else if(call_num == 1)
    { syscall_surface_acquire(current_thread, hart); }
    else if(call_num == 2)
    { syscall_thread_awake_after_time(current_thread, hart, mtime); }
    else if(call_num == 3)
    { syscall_awake_on_surface(current_thread, hart, mtime); }
    else if(call_num == 4)
    { syscall_get_rawmouse_events(current_thread, hart); }
    else if(call_num == 5)
    { syscall_get_keyboard_events(current_thread, hart); }
    else if(call_num == 6)
    { syscall_switch_vo(current_thread, hart); }
    else if(call_num == 7)
    { syscall_get_vo_id(current_thread, hart); }
    else if(call_num == 8)
    { syscall_alloc_pages(current_thread, hart); }
    else if(call_num == 9)
    { syscall_shrink_allocation(current_thread, hart); }
    else if(call_num == 10)
    { syscall_surface_consumer_has_commited(current_thread, hart); }
    else if(call_num == 11)
    { syscall_surface_consumer_get_size(current_thread, hart); }
    else if(call_num == 12)
    { syscall_surface_consumer_set_size(current_thread, hart); }
    else if(call_num == 13)
    { syscall_surface_consumer_fetch(current_thread, hart); }
    else if(call_num == 14)
    { syscall_time_get_seconds(current_thread, hart, mtime); }
    else if(call_num == 15)
    { syscall_file_get_name(current_thread, hart); }
    else if(call_num == 23)
    { syscall_directory_get_files(current_thread, hart); }
    else if(call_num == 26)
    { syscall_create_process_from_file(current_thread, hart); }
    else if(call_num == 27)
    { syscall_surface_consumer_create(current_thread, hart); }
    else if(call_num == 28)
    { syscall_surface_consumer_fire(current_thread, hart); }
    else if(call_num == 29)
    { syscall_surface_forward_to_consumer(current_thread, hart); }
    else if(call_num == 30)
    { syscall_surface_stop_forwarding_to_consumer(current_thread, hart); }
    else if(call_num == 31)
    { syscall_forward_keyboard_events(current_thread, hart); }
    else if(call_num == 32)
    { syscall_thread_sleep(current_thread, hart, mtime); }
    else if(call_num == 33)
    { syscall_thread_awake_on_keyboard(current_thread, hart); }
    else if(call_num == 34)
    { syscall_thread_awake_on_mouse(current_thread, hart); }
    else if(call_num == 35)
    { syscall_stream_put(current_thread, hart); }
    else if(call_num == 36)
    { syscall_stream_take(current_thread, hart); }
    else if(call_num == 37)
    { syscall_process_create_out_stream(current_thread, hart); }
    else if(call_num == 38)
    { syscall_process_create_in_stream(current_thread, hart); }
    else if(call_num == 39)
    { syscall_process_start(current_thread, hart); }
    else
    { printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
