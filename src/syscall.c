
void syscall_surface_commit(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    kernel_log_user(hart,
                    kernel_current_threads[hart].process_pid,
                    kernel_current_thread_tid[hart],
                    "process is attempting to commit surface");

    rwlock_acquire_write(&process->process_lock);

    u64 surface_slot = frame->regs[11];

    assert(surface_slot < process->surface_count &&
        ((SurfaceSlot*)process->surface_alloc.memory)[surface_slot].is_initialized,
        "surface_commit: the surface slot contains to a valid surface");

    frame->regs[10] = surface_commit(surface_slot, process);
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

/*
    If you pass 0 page_count the function returns the page_count
    required for the acquire.
    To successfully acquire surface_slot must point to a valid slot,
    fb must be a 4096 byte aligned pointer, page_count must be
    the page_count required for this acquire.
*/
void syscall_surface_acquire(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    kernel_log_user(hart,
                    kernel_current_threads[hart].process_pid,
                    kernel_current_thread_tid[hart],
                    "process is attempting to acquire surface");

    rwlock_acquire_write(&process->process_lock);

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

            if(ret)
            {
                kernel_log_user(hart,
                                kernel_current_threads[hart].process_pid,
                                kernel_current_thread_tid[hart],
                                "process has acquired surface");
            }
        }
    }
    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_awake_after_time(u64 hart, u64 mtime)
{
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    u64 sleep_time = frame->regs[11];
    current_thread->program_counter += 4;

    if(current_thread->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        return;
    }

    u64 awake_index = current_thread->awake_count;
    current_thread->awake_count++;

    current_thread->awakes[awake_index].awake_type = THREAD_AWAKE_TIME;
    current_thread->awakes[awake_index].awake_time = sleep_time + mtime;

    frame->regs[10] = 1;
}

void syscall_thread_awake_on_surface(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    u64 user_surface_slots = frame->regs[11];
    u64 count = frame->regs[12];
    current_thread->program_counter += 4;

    if(count == 0 || current_thread->awake_count + count > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u16 surface_slot_array[THREAD_MAX_AWAKE_COUNT];
    u64 surface_slot_count = 0;

    u64 page_count = ((count*sizeof(u16))+PAGE_SIZE-1)/PAGE_SIZE;
    u16* surface_slots;
    for(s64 i = page_count; i >= 0; i--)
    {
        u64 convert_addr = user_surface_slots + i*PAGE_SIZE;
        if(i == page_count)
        { convert_addr -= i*PAGE_SIZE - count*sizeof(u16) + 1; }
        if(mmu_virt_to_phys(process->mmu_table, convert_addr, (u64*)&surface_slots) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_read(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
    }
    for(u64 i = 0; i < count; i++)
    {
        {
            SurfaceSlot* slot=
        ((SurfaceSlot*)process->surface_alloc.memory) + surface_slots[i];

            if( surface_slots[i] < process->surface_count &&
                slot->is_initialized)
            {
                surface_slot_array[surface_slot_count] = surface_slots[i];
                surface_slot_count++;
            }
        }
    }

    if(surface_slot_count == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = current_thread->awake_count;
    current_thread->awake_count += surface_slot_count;
    for(u64 i = 0; i < surface_slot_count; i++)
    {
        current_thread->awakes[awake_index + i].awake_type = THREAD_AWAKE_SURFACE;
        current_thread->awakes[awake_index + i].surface_slot = surface_slot_array[i];
    }
    frame->regs[10] = 1;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_sleep(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    current_thread->program_counter += 4;

    current_thread->is_running = current_thread->awake_count == 0;
    if(thread_runtime_is_live(current_thread, mtime))
    {
        frame->regs[10] = 0;
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    // go to sleep
    frame->regs[10] = 1;
    kernel_choose_new_thread(mtime, hart);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_awake_on_keyboard(u64 hart)
{
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    current_thread->program_counter += 4;

    if(current_thread->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        return;
    }

    u64 awake_index = current_thread->awake_count;
    current_thread->awake_count++;

    current_thread->awakes[awake_index].awake_type = THREAD_AWAKE_KEYBOARD;

    frame->regs[10] = 1;
}

void syscall_thread_awake_on_mouse(u64 hart)
{
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    current_thread->program_counter += 4;

    if(current_thread->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        return;
    }

    u64 awake_index = current_thread->awake_count;
    current_thread->awake_count++;

    current_thread->awakes[awake_index].awake_type = THREAD_AWAKE_MOUSE;

    frame->regs[10] = 1;
}

void syscall_thread_awake_on_semaphore(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    u64 user_semaphore = frame->regs[11];
    current_thread->program_counter += 4;

    ProcessSemaphore* semaphores = process->semaphore_alloc.memory;
    if(user_semaphore >= process->semaphore_count ||
       !semaphores[user_semaphore].is_initialized ||
        current_thread->awake_count + 1 > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 awake_index = current_thread->awake_count;
    current_thread->awake_count++;

    current_thread->awakes[awake_index].awake_type = THREAD_AWAKE_SEMAPHORE;
    current_thread->awakes[awake_index].semaphore = user_semaphore;

    frame->regs[10] = 1;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_rawmouse_events(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    rwlock_acquire_write(&process->process_lock);

    u64 user_buf = frame->regs[11];
    u64 len = frame->regs[12];

    // TODO: add multiple mouse support. Probably something we do after there is a test case

    if(user_buf != 0)
    {
        RawMouseEvent* buf;
        if(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf))
        {
            frame->regs[10] = 0;
            current_thread->program_counter += 4;
            rwlock_release_write(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
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

    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_keyboard_events(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    rwlock_acquire_write(&process->process_lock);

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
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_switch_vo(u64 hart)
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
    spinlock_acquire(&KERNEL_SPINLOCK);

    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u64 new_vo = frame->regs[11];
    u64 ret = 0;

    if(vos[current_vo].pid != current_thread->process_pid)
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
    current_thread->program_counter += 4;

    spinlock_release(&KERNEL_SPINLOCK);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

/*
    if the running process is in a VO
        the VO's id will be stored in arg1 if arg1 is not null
        the function will return true
    if not
        the function will return false
*/
void syscall_get_vo_id(u64 hart)
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
    spinlock_acquire(&KERNEL_SPINLOCK);
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u64 ret = 0;
    u64 vo_id = 0;
    for(u64 i = 0; i < VO_COUNT; i++)
    {
        if(vos[i].is_active && vos[i].pid == current_thread->process_pid)
        { ret = 1; vo_id = i; }
    }

    u64 user_vo_id_ptr = frame->regs[11];
    if(ret && user_vo_id_ptr != 0)
    {
        u64* ptr;
        assert(
            mmu_virt_to_phys(process->mmu_table, user_vo_id_ptr, (u64*)&ptr) == 0,
            "you didn't do a memory bad"
        );
        *ptr = vo_id;
    }
    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    spinlock_release(&KERNEL_SPINLOCK);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_alloc_pages(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    rwlock_acquire_write(&process->process_lock);

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
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_shrink_allocation(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    rwlock_acquire_write(&process->process_lock);

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
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_has_commited(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);
    u64 consumer_slot = frame->regs[11];
    frame->regs[10] = surface_consumer_has_commited(process, consumer_slot); // locks internally
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_get_size(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

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
        ret = surface_consumer_get_size(process, consumer_slot, width, height);
    }

    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_set_size(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);
 
    u64 consumer_slot = frame->regs[11];
    u32 width = frame->regs[12];
    u32 height = frame->regs[13];

    u64 ret = surface_consumer_set_size(process, consumer_slot, width, height);

    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_fetch(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 consumer_slot = frame->regs[11];
    Framebuffer* fb = frame->regs[12];
    u64 page_count = frame->regs[13];
    u64 ret = 0;

    kernel_log_user(hart,
                    kernel_current_thread_pid[hart],
                    kernel_current_thread_tid[hart],
                    "process called consumer fetch");

    ret = surface_consumer_fetch(process, consumer_slot, fb, page_count, hart);

    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_cpu_time(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    frame->regs[10] = mtime;
    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_get_cpu_timer_frequency(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    frame->regs[10] = MACHINE_TIMER_SECOND;
    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_get_name(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u64 user_file_id = frame->regs[11];
    u64 user_buf = frame->regs[12];
    u64 buf_size = frame->regs[13];

    u64 file_id;
    if(!process_get_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
 
    u64 buf = 0;
    if(buf_size != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf) == 0))
        {
            frame->regs[10] = 0;
            current_thread->program_counter += 4;
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
                    current_thread->program_counter += 4;
                    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
                    return;
                }
                ptr += PAGE_SIZE;
            }
        }
    }
 
    frame->regs[10] = kernel_file_get_name(file_id, buf, buf_size);
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_get_files(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u64 dir_id = frame->regs[11];
    u64 user_buf = frame->regs[12];
    u64 buf_size = frame->regs[13];

    u64 buf = 0;
    if(buf_size != 0)
    {
        if(!(mmu_virt_to_phys(process->mmu_table, user_buf, (u64*)&buf) == 0))
        {
            frame->regs[10] = 0;
            current_thread->program_counter += 4;
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
                    current_thread->program_counter += 4;
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
  { array[i] = process_new_file_access(kernel_current_threads[hart].process_pid, temp_buf[i], FILE_ACCESS_PERMISSION_READ_WRITE_BIT); }

    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_create_process_from_file(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    u64 user_file_id = frame->regs[11];
    u64 user_pid_ptr = frame->regs[12];

    u64 file_id = 0;
    u64 ret = process_get_read_access(process, user_file_id, &file_id);
 
    u64* pid_ptr = 0;
    if(!ret && !(mmu_virt_to_phys(process->mmu_table, user_pid_ptr + sizeof(u64), (u64*)&pid_ptr) == 0) ||
               !(mmu_virt_to_phys(process->mmu_table, user_pid_ptr, (u64*)&pid_ptr) == 0))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process->process_lock);
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    // ret is true
    u64 parent_count = process->parent_count + 1;
    u64 parents[parent_count];
    for(u64 i = 0; i < process->parent_count; i++)
    { parents[i] = ((u64*)process->parent_alloc.memory)[i]; }
    parents[parent_count - 1] = process->pid;
    u64 child_pid;
    ret = create_process_from_file(file_id, &child_pid, parents, parent_count);
    rwlock_release_read(&process->process_lock);

    for(u64 i = 0; i < parent_count; i++)
    {
        u64 pid = parents[i];
        Process* parent_process = KERNEL_PROCESS_ARRAY[pid];
        rwlock_acquire_write(&parent_process->process_lock);
        process_create_owned_process(parent_process, child_pid);
        rwlock_release_write(&parent_process->process_lock);
    }

    rwlock_acquire_read(&process->process_lock);
    u64 child_owned_process_index;
    assert(process_child_pid_to_owned_process_index(process, child_pid, &child_owned_process_index), "we do actually own this child we just created");

    *pid_ptr = child_owned_process_index;
    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_create(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_foreign_proxy_pid = frame->regs[11];
    u64 user_consumer_ptr = frame->regs[12];
    u64 user_surface_ptr = frame->regs[13];

    u64* consumer_ptr = 0;
    if(!(mmu_virt_to_phys(process->mmu_table, user_consumer_ptr + sizeof(u64), (u64*)&consumer_ptr) == 0) ||
       !(mmu_virt_to_phys(process->mmu_table, user_consumer_ptr, (u64*)&consumer_ptr) == 0))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64* surface_ptr = 0;
    if(!(mmu_virt_to_phys(process->mmu_table, user_surface_ptr + sizeof(u64), (u64*)&surface_ptr) == 0) ||
       !(mmu_virt_to_phys(process->mmu_table, user_surface_ptr, (u64*)&surface_ptr) == 0))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    frame->regs[10] = surface_consumer_create(process, user_foreign_proxy_pid, consumer_ptr, surface_ptr);
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_consumer_fire(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    u64 consumer_slot = frame->regs[11];

    kernel_log_user(hart,
                    kernel_current_thread_pid[hart],
                    kernel_current_thread_tid[hart],
                    "process is firing a consumer");

    frame->regs[10] = surface_consumer_fire(process, consumer_slot);
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_forward_to_consumer(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 surface_slot =  frame->regs[11];
    u64 consumer_slot = frame->regs[12];

    SurfaceSlot* slot = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;
    SurfaceConsumer* con = ((SurfaceConsumer*)process->surface_consumer_alloc.memory) + consumer_slot;

    u64 ret = 0;
    if(surface_slot < process->surface_count && slot->is_initialized &&
        consumer_slot < process->surface_consumer_count && con->is_initialized)
    {
        if(!(slot->is_defering_to_consumer_slot && slot->defer_consumer_slot == consumer_slot))
        {
            slot->is_defering_to_consumer_slot = 1;
            slot->defer_consumer_slot = consumer_slot;
            surface_slot_fire(process, surface_slot, 1);
            ret = 1;
        }
    }
    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_surface_stop_forwarding_to_consumer(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 surface_slot =  frame->regs[11];
 
    SurfaceSlot* slot = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;
    u64 ret = 0;
    if(surface_slot < process->surface_count && slot->is_initialized && slot->is_defering_to_consumer_slot)
    {
        slot->is_defering_to_consumer_slot = 0;
        slot->has_commited = 0;
        slot->has_been_fired = 1;
        ret = 1;
    }
    frame->regs[10] = ret;
    current_thread->program_counter += 4;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_forward_keyboard_events(u64 hart)
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
    Process* process_orig = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process_orig->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process_orig->process_lock);

    u64 user_buf = frame->regs[11];
    u64 user_len = frame->regs[12];
    if(user_len > AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT) { user_len = AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT; }
    u64 user_proxy_pid = frame->regs[13];

    OwnedProcess* ops = process_orig->owned_process_alloc.memory;
    if(user_proxy_pid >= process_orig->owned_process_count ||
       !ops[user_proxy_pid].is_initialized ||
       !ops[user_proxy_pid].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process_orig->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 user_pid = ops[user_proxy_pid].pid;

    u64 page_count = (user_len * sizeof(KeyboardEvent) + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 user_ptr = user_buf + page_count * PAGE_SIZE;
    KeyboardEvent* ptr;

    for(u64 i = 0; i < page_count; i++)
    {
        user_ptr -= PAGE_SIZE;
        if(mmu_virt_to_phys(process_orig->mmu_table, user_ptr, (u64*)&ptr) != 0)
        {
            frame->regs[10] = 0;
            current_thread->program_counter += 4;
            rwlock_release_read(&process_orig->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
    }
    KeyboardEvent kbd_events[AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT];
    for(u64 i = 0; i < user_len; i++)
    { kbd_events[i] = ptr[i]; }
    rwlock_release_read(&process_orig->process_lock);

    // TODO: do proper security to only allow sending keystrokes to *owned* processes.
    Process* process_other = KERNEL_PROCESS_ARRAY[user_pid];
    rwlock_acquire_write(&process_other->process_lock);
    if(user_pid >= KERNEL_PROCESS_ARRAY_LEN || !process_other->mmu_table)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process_other->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    if(user_len + process_other->kbd_event_queue.count > KEYBOARD_EVENT_QUEUE_LEN)
    { user_len = KEYBOARD_EVENT_QUEUE_LEN - process_other->kbd_event_queue.count; }

    if(!user_len)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process_other->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    KeyboardEvent* event_buf = kbd_events;
    u64 start_index = process_other->kbd_event_queue.count;
    process_other->kbd_event_queue.count += user_len;
    for(u64 i = 0; i < user_len; i++)
    {
        process_other->kbd_event_queue.new_events[i+start_index] = event_buf[i];
    }
    frame->regs[10] = user_len;

    current_thread->program_counter += 4;
    rwlock_release_write(&process_other->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_stream_put(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    u64 user_out_stream = frame->regs[11];
    u64 user_memory = frame->regs[12];
    u64 user_count = frame->regs[13];
    current_thread->program_counter += 4;

    Stream** out_stream_array = process->out_stream_alloc.memory;
    if(user_out_stream >= process->out_stream_count || out_stream_array[user_out_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
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
            rwlock_release_read(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }

    frame->regs[10] = stream_put(out_stream, memory, user_count);
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_stream_take(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    u64 user_in_stream = frame->regs[11];
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];
    u64 user_byte_count_in_stream = frame->regs[14];
    current_thread->program_counter += 4;

    u64* byte_count_in_stream_ptr;
    if(mmu_virt_to_phys(process->mmu_table, user_byte_count_in_stream + sizeof(u64),
                        (u64*)&byte_count_in_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_byte_count_in_stream,
                        (u64*)&byte_count_in_stream_ptr) != 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    *byte_count_in_stream_ptr = 0;

    Stream** in_stream_array = process->in_stream_alloc.memory;
    if(user_in_stream >= process->in_stream_count || in_stream_array[user_in_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
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
            rwlock_release_read(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }

    frame->regs[10] = stream_take(in_stream, buffer, user_buffer_size, byte_count_in_stream_ptr);
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_create_out_stream(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_proxy_process_handle = frame->regs[11]; // as of now just the pid, very insecure
    u64 user_foreign_out_stream_ptr = frame->regs[12];
    u64 user_owned_in_stream_ptr = frame->regs[13];
    current_thread->program_counter += 4;

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_proxy_process_handle >= process->owned_process_count ||
       !ops[user_proxy_process_handle].is_initialized ||
       !ops[user_proxy_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 user_process_handle = ops[user_proxy_process_handle].pid;
 
    u64* foreign_out_stream_ptr = 0;
    if(user_foreign_out_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_foreign_out_stream_ptr + sizeof(u64),
                        (u64*)&foreign_out_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_foreign_out_stream_ptr,
                        (u64*)&foreign_out_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
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
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    if( user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 in_stream_index = process_create_in_stream_slot(process);
    rwlock_release_write(&process->process_lock);
    rwlock_acquire_read(&process->process_lock);

    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];
    rwlock_acquire_write(&foreign->process_lock);
    u64 out_stream_index  = process_create_out_stream_slot(foreign);

    process_create_between_stream(foreign, process, out_stream_index, in_stream_index);

    frame->regs[10] = 1;
    if(foreign_out_stream_ptr) { *foreign_out_stream_ptr = out_stream_index; }
    if(owned_in_stream_ptr)    { *owned_in_stream_ptr = in_stream_index; }
    rwlock_release_write(&foreign->process_lock);
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_create_in_stream(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_proxy_process_handle = frame->regs[11]; // as of now just the pid, very insecure
    u64 user_owned_out_stream_ptr = frame->regs[12];
    u64 user_foreign_in_stream_ptr = frame->regs[13];
    current_thread->program_counter += 4;

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_proxy_process_handle >= process->owned_process_count ||
       !ops[user_proxy_process_handle].is_initialized ||
       !ops[user_proxy_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 user_process_handle = ops[user_proxy_process_handle].pid;
 
    u64* owned_out_stream_ptr = 0;
    if(user_owned_out_stream_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_owned_out_stream_ptr + sizeof(u64),
                        (u64*)&owned_out_stream_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_owned_out_stream_ptr,
                        (u64*)&owned_out_stream_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
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
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    if( user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 out_stream_index = process_create_out_stream_slot(process);
    rwlock_release_write(&process->process_lock);
    rwlock_acquire_read(&process->process_lock);

    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];
    rwlock_acquire_write(&foreign->process_lock);
    u64 in_stream_index  = process_create_in_stream_slot(foreign);

    process_create_between_stream(process, foreign, out_stream_index, in_stream_index);

    frame->regs[10] = 1;
    if(owned_out_stream_ptr) { *owned_out_stream_ptr = out_stream_index; }
    if(foreign_in_stream_ptr)    { *foreign_in_stream_ptr = in_stream_index; }
    rwlock_release_write(&foreign->process_lock);
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_start(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_read(&process->process_lock);

    current_thread->program_counter += 4;
    u64 user_proxy_process_handle = frame->regs[11];

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_proxy_process_handle >= process->owned_process_count ||
       !ops[user_proxy_process_handle].is_initialized ||
       !ops[user_proxy_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 user_process_handle = ops[user_proxy_process_handle].pid;

    if(user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];

    if(foreign->thread_count == 0 || foreign->threads[0].is_initialized == 0)
    {
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    foreign->threads[0].is_running = 1;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_thread_new(u64 hart)
{
    rwlock_acquire_write(&KERNEL_PROCESS_ARRAY_RWLOCK);

    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 pid = current_thread->process_pid;
    u32 tid = kernel_current_thread_tid[hart];

    u64 user_program_counter = frame->regs[11];
    u64 user_register_values = frame->regs[12];
    u32 user_thread_group = frame->regs[13];
    current_thread->program_counter += 4;

    AOS_TrapFrame* register_values_ptr;
    if(mmu_virt_to_phys(process->mmu_table, user_register_values + sizeof(AOS_TrapFrame),
                        (u64*)&register_values_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_register_values,
                        (u64*)&register_values_ptr) != 0)
    {
        rwlock_release_write(&process->process_lock);
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        frame->regs[10] = 0;
        return;
    }

    u32 tid2 = process_thread_create(pid, user_thread_group, hart, 0);
    process = KERNEL_PROCESS_ARRAY[pid];
    current_thread = &process->threads[kernel_current_thread_tid[hart]];

    Thread* new_thread = &process->threads[tid2];
    new_thread->program_counter = user_program_counter;

    for(u64 i = 0; i < 32; i++)
    {
        new_thread->frame.regs[i] = register_values_ptr->regs[i];
        new_thread->frame.fregs[i]= register_values_ptr->fregs[i];
    }
    new_thread->is_running = 1;

    frame->regs[10] = 1;

    rwlock_release_write(&process->process_lock);
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphore_create(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u32 user_initial_value = frame->regs[11];
    u32 user_max_value = frame->regs[12];
    current_thread->program_counter += 4;

    rwlock_acquire_write(&process->process_lock);
    frame->regs[10] = process_create_semaphore(process, user_initial_value, user_max_value);
    rwlock_release_write(&process->process_lock);

    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphore_release(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

            // We need exclusive releasing access so that we don't overflow the semaphore
    rwlock_acquire_write(&process->process_lock);

    u32 user_semaphore = frame->regs[11];
    u32 user_release_count = frame->regs[12];
    u64 user_previous_value_ptr = frame->regs[13];
    current_thread->program_counter += 4;

    u32* previous_value_ptr = 0;
    if(user_previous_value_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_previous_value_ptr + sizeof(u64),
                        (u64*)&previous_value_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_previous_value_ptr,
                        (u64*)&previous_value_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    ProcessSemaphore* semaphores = process->semaphore_alloc.memory;
    if( user_semaphore >= process->semaphore_count ||
        !semaphores[user_semaphore].is_initialized)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    s64 top_value = atomic_s64_read(&semaphores[user_semaphore].counter);
    if(top_value < 0) { top_value = 0; }
    if( user_release_count &&
        user_release_count + (u32)top_value
        > semaphores[user_semaphore].max_value)
    {
        if(previous_value_ptr)
        {
            *previous_value_ptr = (u32)atomic_s64_read(&semaphores[user_semaphore].counter);
        }
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    s64 value;
    if(user_release_count)
    {
        value = atomic_s64_add(&semaphores[user_semaphore].counter, user_release_count);
    }
    else
    {
        value = atomic_s64_read(&semaphores[user_semaphore].counter);
    }
    if(value < 0) { value = 0; }
    if(previous_value_ptr)
    {
        *previous_value_ptr = value;
    }

    frame->regs[10] = 1;
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_exit(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];

    rwlock_acquire_write(&process->process_lock);
    process_flag_all_threads_for_destruction(process);
    rwlock_release_write(&process->process_lock);

    kernel_choose_new_thread(mtime, hart);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_is_alive(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;
    current_thread->program_counter += 4;

    rwlock_acquire_read(&process->process_lock);

    u64 user_proxy_pid = frame->regs[11];

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_proxy_pid >= process->owned_process_count ||
       !ops[user_proxy_pid].is_initialized ||
       !ops[user_proxy_pid].is_alive)
    {
        frame->regs[10] = 0;
    }
    else
    {
        frame->regs[10] = 1;
    }

    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_kill(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    current_thread->program_counter += 4;
    u64 user_proxy_pid = frame->regs[11];

    rwlock_acquire_read(&process->process_lock);

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_proxy_pid >= process->owned_process_count ||
       !ops[user_proxy_pid].is_initialized ||
       !ops[user_proxy_pid].is_alive)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    frame->regs[10] = 1;
    Process* p2 = KERNEL_PROCESS_ARRAY[ops[user_proxy_pid].pid];
    rwlock_release_read(&process->process_lock);
    rwlock_acquire_write(&p2->process_lock);
    process_flag_all_threads_for_destruction(p2);
    rwlock_release_write(&p2->process_lock);

    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_out_stream_destroy(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    current_thread->program_counter += 4;
    u64 user_out_stream = frame->regs[11];

    rwlock_acquire_read(&process->process_lock);

    Stream** streams = process->out_stream_alloc.memory;
    if(user_out_stream >= process->out_stream_count ||
        streams[user_out_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    stream_destroy(streams[user_out_stream]);

    streams[user_out_stream] = 0;

    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_in_stream_destroy(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    current_thread->program_counter += 4;
    u64 user_in_stream = frame->regs[11];

    rwlock_acquire_read(&process->process_lock);

    Stream** streams = process->in_stream_alloc.memory;
    if(user_in_stream >= process->in_stream_count ||
        streams[user_in_stream] == 0)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    stream_destroy(streams[user_in_stream]);

    streams[user_in_stream] = 0;

    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_IPFC_handler_create(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_handler_name_buffer = frame->regs[11];
    u64 user_handler_name_buffer_len = frame->regs[12];
    u64 user_handler_function_entry = frame->regs[13];
    u64 user_stack_pages_start = frame->regs[14];
    u64 user_pages_per_stack = frame->regs[15];
    u64 user_stack_count = frame->regs[16];
    u64 user_handler_id_ptr = frame->regs[17];
    current_thread->program_counter += 4;
    
    if((user_stack_pages_start % PAGE_SIZE) != 0 || user_handler_name_buffer_len > 64)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 page_count = (user_handler_name_buffer_len+(user_handler_name_buffer % PAGE_SIZE) + PAGE_SIZE - 1)
    					/ PAGE_SIZE;
    					
    u64* handler_id_ptr = 0;
    if(user_handler_id_ptr &&
      (mmu_virt_to_phys(process->mmu_table, user_handler_id_ptr + sizeof(u64),
                        (u64*)&handler_id_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_handler_id_ptr,
                        (u64*)&handler_id_ptr) != 0))
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u8* memory = 0;
    if(page_count)
    {
    u64 i = page_count - 1;
    while(1)
    {
        if(mmu_virt_to_phys(process->mmu_table, user_handler_name_buffer + (PAGE_SIZE * i),
            (u64*)&memory) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_write(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }

    frame->regs[10] = process_ipfc_handler_create(
                            process,
    						memory,
    						user_handler_name_buffer_len,
    						user_handler_function_entry,
    						user_stack_pages_start,
    						user_pages_per_stack,
    						user_stack_count,
    						handler_id_ptr);
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_IPFC_session_init(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_handler_name_buffer = frame->regs[11];
    u64 user_handler_name_buffer_len = frame->regs[12];
    u64 user_session_id_ptr = frame->regs[13];
    current_thread->program_counter += 4;
    
    u64 page_count = (user_handler_name_buffer_len+(user_handler_name_buffer % PAGE_SIZE) + PAGE_SIZE - 1)
                        / PAGE_SIZE;
                        
    u64* session_id_ptr = 0;
    if(mmu_virt_to_phys(process->mmu_table, user_session_id_ptr + sizeof(u64),
                        (u64*)&session_id_ptr) != 0 ||
       mmu_virt_to_phys(process->mmu_table, user_session_id_ptr,
                        (u64*)&session_id_ptr) != 0)
    {
        frame->regs[10] = 0;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
 
    u8* memory = 0;
    if(page_count)
    {
    u64 i = page_count - 1;
    while(1)
    {
        if(mmu_virt_to_phys(process->mmu_table, user_handler_name_buffer + (PAGE_SIZE * i),
            (u64*)&memory) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_write(&process->process_lock);
            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        if(i == 0) { break; }
        i--;
    }
    }
 
    frame->regs[10] = process_ipfc_session_init(
                            process,
                            memory,
                            user_handler_name_buffer_len,
                            session_id_ptr);
    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_IPFC_session_close(u64 hart)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_session_id = frame->regs[11];
    current_thread->program_counter += 4;

    if(user_session_id < process->ipfc_session_count)
    {
        IPFCSession* sessions = process->ipfc_session_alloc.memory;
        sessions[user_session_id].is_initialized = 0;
    }

    rwlock_release_write(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_IPFC_call(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    u64 user_session_id = frame->regs[11];
    u16 user_function_index = frame->regs[12];
    void* user_ipfc_static_data_1024_bytes_in = frame->regs[13];
    void* user_ipfc_static_data_1024_bytes_out = frame->regs[14];
    current_thread->program_counter += 4;

    u64 ipfc_static_data_1024_bytes_in[1024/sizeof(u64)];
    if(user_ipfc_static_data_1024_bytes_in)
    {
        u64* pointer;
        if(mmu_virt_to_phys(process->mmu_table, user_ipfc_static_data_1024_bytes_in + 1024,
                            (u64*)&pointer) != 0 ||
           mmu_virt_to_phys(process->mmu_table, user_ipfc_static_data_1024_bytes_in,
                            (u64*)&pointer) != 0)
        {
            frame->regs[10] = 0;
            rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
            return;
        }
        for(u64 i = 0; i < 128; i++)
        { ipfc_static_data_1024_bytes_in[i] = pointer[i]; }
    }
    else
    {
        for(u64 i = 0; i < 128; i++)
        { ipfc_static_data_1024_bytes_in[i] = 0; }
    }

    assert(current_thread->IPFC_status == 0, "You are doing a ipfc call from a normal thread. Currently, ipfc threads can't themselves perform ipfc calls. This should be fixed and allowed for. Probably by using IPFC_status as a bitfield instead of an integer of state.");

    assert(
        user_session_id < process->ipfc_session_count &&
        ((IPFCSession*)process->ipfc_session_alloc.memory)[user_session_id].is_initialized,
        "'Calling a valid IPFC session', we should probably crash the calling process instead of the whole system in the future.");

    u16 parent_index = ((IPFCSession*)process->ipfc_session_alloc.memory)[user_session_id].parent_index;
    u16 handler_index = ((IPFCSession*)process->ipfc_session_alloc.memory)[user_session_id].handler_index;
    u16 owned_process_index = ((IPFCSession*)process->ipfc_session_alloc.memory)[user_session_id].owned_process_index;

    u64 parent_pid = ((u64*)process->parent_alloc.memory)[parent_index];
    u64 process_pid = process->pid;
    u64 thread_tid = current_thread - process->threads;

    Process* ipfc_process = KERNEL_PROCESS_ARRAY[parent_pid];

    // In order to jump directly into the ipfc thread if there is space for it
    // we must put it at the front of the round robin.
    u64 ipfc_thread_runtime_index;
    u32 ipfc_tid = process_thread_create(parent_pid, 0, hart, &ipfc_thread_runtime_index); // This guy can move ipfc_process
    ipfc_process =  KERNEL_PROCESS_ARRAY[parent_pid];                   //  be wary.
    Thread* ipfc_thread = &ipfc_process->threads[ipfc_tid];

    ipfc_thread->IPFC_status = 2;
    ipfc_thread->IPFC_other_tid = thread_tid;
    ipfc_thread->IPFC_other_pid = process_pid;
    ipfc_thread->IPFC_function_index = user_function_index;
    ipfc_thread->IPFC_handler_index = handler_index;
    ipfc_thread->IPFC_caller_runtime_index = current_thread->thread_runtime_index;

    ipfc_thread->frame.regs[10] = owned_process_index;
    ipfc_thread->frame.regs[11] = user_function_index;

    for(u64 i = 0; i < 128; i++)
    { ipfc_thread->ipfc_static_data_1024_bytes[i] = ipfc_static_data_1024_bytes_in[i]; }

    current_thread->IPFC_status = 1;
    current_thread->IPFC_other_pid = parent_pid;
    current_thread->IPFC_other_tid = ipfc_tid;
    current_thread->IPFC_handler_index = handler_index;
    current_thread->is_running = 0;

    try_assign_ipfc_stack(ipfc_process, ipfc_thread);

    kernel_log_user(hart,
                    kernel_current_threads[hart].process_pid,
                    kernel_current_thread_tid[hart],
                    "thread called ipfc");

    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
    rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);

    ipfc_process =  KERNEL_PROCESS_ARRAY[parent_pid];
    ipfc_thread = &ipfc_process->threads[ipfc_tid];

    if(ipfc_thread->IPFC_status == 3)
    {
        rwlock_acquire_read(&THREAD_RUNTIME_ARRAY_LOCK);
        ThreadRuntime* runtime_array = THREAD_RUNTIME_ARRAY_ALLOC.memory;

        current_thread_runtimes[hart] = ipfc_thread->thread_runtime_index;

        ThreadRuntime runtime = runtime_array[current_thread_runtimes[hart]];

        rwlock_release_read(&THREAD_RUNTIME_ARRAY_LOCK);

        kernel_current_thread_has_thread[hart] = 1;
        kernel_current_thread_tid[hart] = runtime.tid;
        kernel_current_thread_pid[hart] = runtime.pid;
    }
    else
    {
        kernel_choose_new_thread(mtime, hart);
    }
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_IPFC_return(u64 hart, u64 mtime)
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
    Process* process = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
    Thread* current_thread = &process->threads[kernel_current_thread_tid[hart]];
    TrapFrame* frame = &current_thread->frame;

    rwlock_acquire_write(&process->process_lock);

    u64 user_return_value = frame->regs[11];
    current_thread->program_counter += 4;

    u64 ipfc_static_data_1024_bytes_out[1024/sizeof(u64)];
    {
        u64* pointer;
        if(mmu_virt_to_phys(process->mmu_table, current_thread->ipfc_static_data_virtual_addr + 1024 - 1,
                            (u64*)&pointer) != 0 ||
           mmu_virt_to_phys(process->mmu_table, current_thread->ipfc_static_data_virtual_addr,
                            (u64*)&pointer) != 0)
        {
            assert(0, "big bad.");
        }
        for(u64 i = 0; i < 128; i++)
        { ipfc_static_data_1024_bytes_out[i] = pointer[i]; }
    }

    if(current_thread->IPFC_status == 3)
    {
        IPFCHandler* handler =
            ((Kallocation*)process->ipfc_handler_alloc.memory)[current_thread->IPFC_handler_index].memory;
        handler->function_executions[current_thread->IPFC_stack_index].is_initialized = 0;

        current_thread->should_be_destroyed = 1;
		rwlock_release_write(&process->process_lock);

        Process* other_process = KERNEL_PROCESS_ARRAY[current_thread->IPFC_other_pid];
        rwlock_acquire_write(&other_process->process_lock);
        Thread* other_thread = other_process->threads + current_thread->IPFC_other_tid;
        if( current_thread->IPFC_other_pid < KERNEL_PROCESS_ARRAY_LEN &&
            current_thread->IPFC_other_tid < other_process->thread_count &&
            other_thread->is_initialized &&
            other_thread->IPFC_status == 1 &&
            other_thread->IPFC_other_pid == process->pid &&
            other_thread->IPFC_other_tid == current_thread - process->threads)
        {
            other_thread->IPFC_status = 0;
            other_thread->is_running = 1;
            other_thread->frame.regs[10] = user_return_value;

            void* user_ipfc_static_data_1024_bytes_out = other_thread->frame.regs[14];

            if(user_ipfc_static_data_1024_bytes_out)
            {
                u64* pointer;
                if(mmu_virt_to_phys(other_process->mmu_table, user_ipfc_static_data_1024_bytes_out + 1024,
                                    (u64*)&pointer) != 0 ||
                   mmu_virt_to_phys(other_process->mmu_table, user_ipfc_static_data_1024_bytes_out,
                                    (u64*)&pointer) != 0)
                {
                    assert(0, "second big bad in this function.");
                }
                for(u64 i = 0; i < 128; i++)
                { pointer[i] = ipfc_static_data_1024_bytes_out[i]; }
            }
        }
        rwlock_release_write(&other_process->process_lock);
    }
    else
    { rwlock_release_write(&process->process_lock); }

    kernel_log_user(hart,
                    kernel_current_threads[hart].process_pid,
                    kernel_current_thread_tid[hart],
                    "thread returned ipfc");

    kernel_choose_new_thread(mtime, hart);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void do_syscall(TrapFrame* frame, u64 mtime, u64 hart)
{
    u64 call_num = frame->regs[10];
         if(call_num == 0)
    { syscall_surface_commit(hart); }
    else if(call_num == 1)
    { syscall_surface_acquire(hart); }
    else if(call_num == 2)
    { syscall_thread_awake_after_time(hart, mtime); }
    else if(call_num == 3)
    { syscall_thread_awake_on_surface(hart, mtime); }
    else if(call_num == 4)
    { syscall_get_rawmouse_events(hart); }
    else if(call_num == 5)
    { syscall_get_keyboard_events(hart); }
    else if(call_num == 6)
    { syscall_switch_vo(hart); }
    else if(call_num == 7)
    { syscall_get_vo_id(hart); }
    else if(call_num == 8)
    { syscall_alloc_pages(hart); }
    else if(call_num == 9)
    { syscall_shrink_allocation(hart); }
    else if(call_num == 10)
    { syscall_surface_consumer_has_commited(hart); }
    else if(call_num == 11)
    { syscall_surface_consumer_get_size(hart); }
    else if(call_num == 12)
    { syscall_surface_consumer_set_size(hart); }
    else if(call_num == 13)
    { syscall_surface_consumer_fetch(hart); }
    else if(call_num == 14)
    { syscall_get_cpu_time(hart, mtime); }
    else if(call_num == 15)
    { syscall_file_get_name(hart); }
    else if(call_num == 23)
    { syscall_directory_get_files(hart); }
    else if(call_num == 26)
    { syscall_create_process_from_file(hart); }
    else if(call_num == 27)
    { syscall_surface_consumer_create(hart); }
    else if(call_num == 28)
    { syscall_surface_consumer_fire(hart); }
    else if(call_num == 29)
    { syscall_surface_forward_to_consumer(hart); }
    else if(call_num == 30)
    { syscall_surface_stop_forwarding_to_consumer(hart); }
    else if(call_num == 31)
    { syscall_forward_keyboard_events(hart); }
    else if(call_num == 32)
    { syscall_thread_sleep(hart, mtime); }
    else if(call_num == 33)
    { syscall_thread_awake_on_keyboard(hart); }
    else if(call_num == 34)
    { syscall_thread_awake_on_mouse(hart); }
    else if(call_num == 35)
    { syscall_stream_put(hart); }
    else if(call_num == 36)
    { syscall_stream_take(hart); }
    else if(call_num == 37)
    { syscall_process_create_out_stream(hart); }
    else if(call_num == 38)
    { syscall_process_create_in_stream(hart); }
    else if(call_num == 39)
    { syscall_process_start(hart); }
    else if(call_num == 40)
    { syscall_thread_new(hart); }
    else if(call_num == 41)
    { syscall_semaphore_create(hart); }
    else if(call_num == 42)
    { syscall_semaphore_release(hart); }
    else if(call_num == 43)
    { syscall_thread_awake_on_semaphore(hart); }
    else if(call_num == 44)
    { syscall_process_exit(hart, mtime); }
    else if(call_num == 45)
    { syscall_process_is_alive(hart); }
    else if(call_num == 46)
    { syscall_process_kill(hart); }
    else if(call_num == 47)
    { syscall_out_stream_destroy(hart); }
    else if(call_num == 48)
    { syscall_in_stream_destroy(hart); }
    else if(call_num == 49)
    { syscall_IPFC_handler_create(hart); }
    else if(call_num == 50)
    { syscall_IPFC_session_init(hart); }
    else if(call_num == 51)
    { syscall_IPFC_session_close(hart); }
    else if(call_num == 52)
    { syscall_IPFC_call(hart, mtime); }
    else if(call_num == 53)
    { syscall_IPFC_return(hart, mtime); }
    else if(call_num == 54)
    { syscall_get_cpu_timer_frequency(hart, mtime); }
    else
    { printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
