
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

    assert(user_surface_slots % 2 == 0, "user_surface_slots is aligned");

    if(count == 0 || current_thread->awake_count + count > THREAD_MAX_AWAKE_COUNT)
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u16 surface_slot_array[THREAD_MAX_AWAKE_COUNT];
    u64 surface_slot_count = 0;

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_surface_slots, count * sizeof(u16))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    {
        frame->regs[10] = 0;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    for(u64 i = 0; i < count; i++)
    {
        {
            u16 surface_slot_index = *((u16*)mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u16) * i));
            SurfaceSlot* slot=
        ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot_index;

            if( surface_slot_index < process->surface_count &&
                slot->is_initialized)
            {
                surface_slot_array[surface_slot_count] = surface_slot_index;
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

// TODO MERGE WITH KEYBOARD

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

// TODO MERGE WITH MOUSE

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
        AOS_KeyboardEvent* buf;
        assert(
            mmu_virt_to_phys(process->mmu_table, user_buf + sizeof(AOS_KeyboardEvent)*len, (u64*)&buf) == 0,
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
    assert(user_vo_id_ptr % 8 == 0, "the vo_id pointer passed to get vo_id is 8 byte aligned");
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

    assert(vaddr % PAGE_SIZE == 0, "vaddr passed to alloc_pages is 4096 aka PAGE_SIZE aligned");

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

    assert(vaddr % PAGE_SIZE == 0, "vaddr passed to shrink_allocation is 4096 aka PAGE_SIZE aligned");

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

    assert(user_width % 4 == 0, "width pointer passed to consumer_get_size is 4 byte aligned");
    assert(user_height % 4== 0, "height pointer passed to consumer_get_size is 4 byte aligned");

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
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    u64 file_id;
    if(!process_get_file_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 actual_count = user_buffer_size;
    u8 temp_buf[KERNEL_FILE_MAX_NAME_LEN];
    if(actual_count > KERNEL_FILE_MAX_NAME_LEN)
    { actual_count = KERNEL_FILE_MAX_NAME_LEN; }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, actual_count)

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        u64 copy_count = kernel_file_get_name(file_id, temp_buf, actual_count);

        for(u64 i = 0; i < copy_count; i++)
        { *((u8*)mmu_virt_to_phys_buffer_get_address(my_buffer, i)) = temp_buf[i]; }
        frame->regs[10] = copy_count;
    }
 
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_get_name(u64 hart)
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

    u64 user_dir_id = frame->regs[11];
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    u64 dir_id;
    if(!process_get_directory_read_access(process, user_dir_id, &dir_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 actual_count = user_buffer_size;
    u8 temp_buf[KERNEL_FILE_MAX_NAME_LEN];
    if(actual_count > KERNEL_FILE_MAX_NAME_LEN)
    { actual_count = KERNEL_FILE_MAX_NAME_LEN; }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, actual_count)

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        u64 copy_count = kernel_directory_get_name(dir_id, temp_buf, actual_count);

        for(u64 i = 0; i < copy_count; i++)
        { *((u8*)mmu_virt_to_phys_buffer_get_address(my_buffer, i)) = temp_buf[i]; }
        frame->regs[10] = copy_count;
    }

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

    u64 local_dir_id = frame->regs[11];
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    // TODO lock around file access array
    u64 dir_id;
    u8 has_read_access = process_get_directory_read_access(process, local_dir_id, &dir_id);
    u8 has_write_access = process_get_directory_write_access(process, local_dir_id, &dir_id);

    assert(user_buffer % 8 == 0, "buffer passed to syscall_directory_get_files is 8 byte aligned");

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, user_buffer_size * sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer) || !has_read_access)
    {
        frame->regs[10] = 0;
    }
    else
    {
        u64 user_index = 0;
        u64 user_buffer_space_left = user_buffer_size;
        // TODO LOCK FILESYSTEM
        u64 temp_buf[512];
        u64 ret = kernel_directory_get_files(dir_id, 0, temp_buf, 512);
        u64 file_count_beyond_start = ret;

        u8 access_bits = FILE_ACCESS_PERMISSION_READ_BIT;
        if(has_write_access) { access_bits = FILE_ACCESS_PERMISSION_READ_WRITE_BIT; }
        while(1)
        {
            for(u64 i = 0; i < 512 && file_count_beyond_start && user_buffer_space_left; i++)
            {
                *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)*user_index)) =
                    process_new_filesystem_access(kernel_current_threads[hart].process_pid, temp_buf[i], access_bits);
                user_index++;
                user_buffer_space_left--;
                file_count_beyond_start--;
            }
            if(!user_buffer_space_left || !file_count_beyond_start)
            { break; }

            file_count_beyond_start = kernel_directory_get_files(dir_id, user_index, temp_buf, 512);
        }
        // TODO UNLOCK FILESYSTEM
        frame->regs[10] = ret;
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_get_subdirectories(u64 hart)
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

    u64 local_dir_id = frame->regs[11];
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    // TODO lock around file access array
    u64 dir_id;
    u8 has_read_access = process_get_directory_read_access(process, local_dir_id, &dir_id);
    u8 has_write_access = process_get_directory_write_access(process, local_dir_id, &dir_id);

    assert(user_buffer % 8 == 0, "buffer passed to syscall_directory_get_files is 8 byte aligned");

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, user_buffer_size * sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer) || !has_read_access)
    {
        frame->regs[10] = 0;
    }
    else
    {
        u64 user_index = 0;
        u64 user_buffer_space_left = user_buffer_size;
        // TODO LOCK FILESYSTEM
        u64 temp_buf[512];
        u64 ret = kernel_directory_get_subdirectories(dir_id, 0, temp_buf, 512);
        u64 file_count_beyond_start = ret;

        u8 access_bits = FILE_ACCESS_PERMISSION_READ_BIT | FILE_ACCESS_PERMISSION_IS_DIRECTORY_BIT;
        if(has_write_access) { access_bits = FILE_ACCESS_PERMISSION_READ_WRITE_BIT | FILE_ACCESS_PERMISSION_IS_DIRECTORY_BIT; }
        while(1)
        {
            for(u64 i = 0; i < 512 && file_count_beyond_start && user_buffer_space_left; i++)
            {
                *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)*user_index)) =
                    process_new_filesystem_access(kernel_current_threads[hart].process_pid, temp_buf[i], access_bits);
                user_index++;
                user_buffer_space_left--;
                file_count_beyond_start--;
            }
            if(!user_buffer_space_left || !file_count_beyond_start)
            { break; }

            file_count_beyond_start = kernel_directory_get_subdirectories(dir_id, user_index, temp_buf, 512);
        }
        // TODO UNLOCK FILESYSTEM
        frame->regs[10] = ret;
    }

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

    assert(user_pid_ptr % 8 == 0, "pid pointer passed to create_process_from_file is 8 byte aligned");

    u64 file_id = 0;
    u64 ret = process_get_file_read_access(process, user_file_id, &file_id);
 
    u64* pid_ptr = 0;
    if(mmu_virt_to_phys(process->mmu_table, user_pid_ptr, (u64*)&pid_ptr) || !ret)
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

    assert(user_consumer_ptr % 8 == 0, "consumer pointer passed to surface_consumer_create is 8 byte aligned");
    assert(user_surface_ptr % 8 == 0, "surface pointer passed to surface_consumer_create is 8 byte aligned");

    u64* consumer_ptr = 0;
    if(!(mmu_virt_to_phys(process->mmu_table, user_consumer_ptr, (u64*)&consumer_ptr) == 0))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64* surface_ptr = 0;
    if(!(mmu_virt_to_phys(process->mmu_table, user_surface_ptr, (u64*)&surface_ptr) == 0))
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

    u64 user_buffer = frame->regs[11];
    u64 user_buffer_length = frame->regs[12];
    if(user_buffer_length > AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT) { user_buffer_length = AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT; }
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

    assert(user_buffer % sizeof(AOS_KeyboardEvent) == 0, "user_buffer passed to forward_keyboard_events is aligned to the size of a KeyboardEvent");

    mmu_virt_to_phys_buffer(my_buffer, process_orig->mmu_table, user_buffer, user_buffer_length * sizeof(AOS_KeyboardEvent))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process_orig->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    AOS_KeyboardEvent kbd_events[AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT];
    for(u64 i = 0; i < user_buffer_length; i++)
    { kbd_events[i] = *((AOS_KeyboardEvent*)mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(AOS_KeyboardEvent) * i)); }
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

    if(user_buffer_length + process_other->kbd_event_queue.count > KEYBOARD_EVENT_QUEUE_LEN)
    { user_buffer_length = KEYBOARD_EVENT_QUEUE_LEN - process_other->kbd_event_queue.count; }

    if(!user_buffer_length)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&process_other->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    AOS_KeyboardEvent* event_buf = kbd_events;
    u64 start_index = process_other->kbd_event_queue.count;
    process_other->kbd_event_queue.count += user_buffer_length;
    for(u64 i = 0; i < user_buffer_length; i++)
    {
        process_other->kbd_event_queue.new_events[i+start_index] = event_buf[i];
    }
    frame->regs[10] = user_buffer_length;

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

    u64 actual_count = user_count;
    u8 temp_buf[STREAM_SIZE];
    if(actual_count > STREAM_SIZE)
    { actual_count = STREAM_SIZE; }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_memory, actual_count)

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        for(u64 i = 0; i < actual_count; i++)
        { temp_buf[i] = *((u8*)mmu_virt_to_phys_buffer_get_address(my_buffer, i)); }
        frame->regs[10] = stream_put(out_stream, temp_buf, actual_count);
    }

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

    u64 actual_count = user_buffer_size;
    u8 temp_buf[STREAM_SIZE];
    if(actual_count > STREAM_SIZE)
    { actual_count = STREAM_SIZE; }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, actual_count)

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        u64 copy_count = stream_take(in_stream, temp_buf, actual_count, byte_count_in_stream_ptr);

        for(u64 i = 0; i < copy_count; i++)
        { *((u8*)mmu_virt_to_phys_buffer_get_address(my_buffer, i)) = temp_buf[i]; }
        frame->regs[10] = copy_count;
    }

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

    u64 user_proxy_process_handle = frame->regs[11];
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

    rwlock_acquire_write(&process->process_lock);

    current_thread->program_counter += 4;
    u64 user_proxy_process_handle = frame->regs[11];

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

    if(user_process_handle >= KERNEL_PROCESS_ARRAY_LEN ||
        KERNEL_PROCESS_ARRAY[user_process_handle]->mmu_table == 0)
    {
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    Process* foreign = KERNEL_PROCESS_ARRAY[user_process_handle];

    if(foreign->thread_count == 0 || foreign->threads[0].is_initialized == 0)
    {
        rwlock_release_write(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    foreign->has_started = 1;

    foreign->threads[0].is_running = 1;
    rwlock_release_write(&process->process_lock);
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
    u32 ipfc_tid = process_thread_create(parent_pid, 0, hart, 1); // This guy can move ipfc_process
    ipfc_process =  KERNEL_PROCESS_ARRAY[parent_pid];                   //  be wary.
    Thread* ipfc_thread = &ipfc_process->threads[ipfc_tid];

    ipfc_thread->IPFC_status = 2;
    ipfc_thread->IPFC_other_tid = thread_tid;
    ipfc_thread->IPFC_other_pid = process_pid;
    ipfc_thread->IPFC_function_index = user_function_index;
    ipfc_thread->IPFC_handler_index = handler_index;

    ipfc_thread->frame.regs[10] = owned_process_index;
    ipfc_thread->frame.regs[11] = user_function_index;

    for(u64 i = 0; i < 128; i++)
    { ipfc_thread->ipfc_static_data_1024_bytes[i] = ipfc_static_data_1024_bytes_in[i]; }

    current_thread->IPFC_status = 1;
    current_thread->IPFC_other_pid = parent_pid;
    current_thread->IPFC_other_tid = ipfc_tid;
    current_thread->IPFC_handler_index = handler_index;
    current_thread->is_running = 0;

    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
    rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);

    kernel_choose_new_thread(mtime, hart);

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

    kernel_choose_new_thread(mtime, hart);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_get_absolute_ids(hart)
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

    u64 user_local_id_buffer = frame->regs[11];
    u64 user_absolute_id_buffer = frame->regs[12];
    u64 user_count = frame->regs[13];

    assert(user_local_id_buffer % sizeof(u64) == 0, "user_local_id_buffer is aligned");
    assert(user_absolute_id_buffer % sizeof(u64) == 0, "user_absolute_id_buffer is aligned");

    mmu_virt_to_phys_buffer(mmu_local_id_buffer, process->mmu_table, user_local_id_buffer, user_count * sizeof(u64))
    mmu_virt_to_phys_buffer(mmu_absolute_id_buffer, process->mmu_table, user_absolute_id_buffer, user_count * sizeof(u64))

    if( mmu_virt_to_phys_buffer_return_value(mmu_local_id_buffer) ||
        mmu_virt_to_phys_buffer_return_value(mmu_absolute_id_buffer)) // buffer is not mapped
    {
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    for(u64 i = 0; i < user_count; i++)
    {
        u64* local_id = mmu_virt_to_phys_buffer_get_address(mmu_local_id_buffer, i * sizeof(u64));
        u64* absolute_id = mmu_virt_to_phys_buffer_get_address(mmu_absolute_id_buffer, i * sizeof(u64));

        u64 dir_id;
        if(process_get_directory_read_access(process, *local_id, &dir_id))
        { *absolute_id = dir_id; }
        else
        { *absolute_id = U64_MAX; }
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_give(hart)
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

    u64 user_foreign_process_handle = frame->regs[11];
    u64 user_local_id_buffer = frame->regs[12];
    u64 user_foreign_id_buffer = frame->regs[13];
    u64 user_count = frame->regs[14];
    u64 user_give_write_access = frame->regs[15];

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_foreign_process_handle >= process->owned_process_count ||
       !ops[user_foreign_process_handle].is_initialized ||
       !ops[user_foreign_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 foreign_pid = ops[user_foreign_process_handle].pid;

    assert(user_local_id_buffer % sizeof(u64) == 0, "user_local_id_buffer is aligned");
    assert(user_foreign_id_buffer % sizeof(u64) == 0, "user_foreign_id_buffer is aligned");

    mmu_virt_to_phys_buffer(mmu_local_id_buffer, process->mmu_table, user_local_id_buffer, user_count * sizeof(u64))
    mmu_virt_to_phys_buffer(mmu_foreign_id_buffer, process->mmu_table, user_foreign_id_buffer, user_count * sizeof(u64))

    if( mmu_virt_to_phys_buffer_return_value(mmu_local_id_buffer) ||
        mmu_virt_to_phys_buffer_return_value(mmu_foreign_id_buffer)) // buffer is not mapped
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    for(u64 i = 0; i < user_count; i++)
    {
        u64* local_id = mmu_virt_to_phys_buffer_get_address(mmu_local_id_buffer, i * sizeof(u64));
        u64* foreign_id = mmu_virt_to_phys_buffer_get_address(mmu_foreign_id_buffer, i * sizeof(u64));

        u64 dir_id;
        if(!user_give_write_access && process_get_directory_read_access(process, *local_id, &dir_id))
        {
            u64 access = process_new_filesystem_access(
                                    foreign_pid,
                                    dir_id,
                                    FILE_ACCESS_PERMISSION_READ_BIT |
                                    FILE_ACCESS_PERMISSION_IS_DIRECTORY_BIT
            );
            *foreign_id = access;
        }
        else if(process_get_directory_write_access(process, *local_id, &dir_id))
        {
            u64 access = process_new_filesystem_access(
                                    foreign_pid,
                                    dir_id,
                                    FILE_ACCESS_PERMISSION_READ_WRITE_BIT |
                                    FILE_ACCESS_PERMISSION_IS_DIRECTORY_BIT
            );
            *foreign_id = access;
        }
        else
        { *foreign_id = U64_MAX; }
    }

    frame->regs[10] = 1;
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_give(hart)
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

    u64 user_foreign_process_handle = frame->regs[11];
    u64 user_local_id_buffer = frame->regs[12];
    u64 user_foreign_id_buffer = frame->regs[13];
    u64 user_count = frame->regs[14];
    u64 user_give_write_access = frame->regs[15];

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_foreign_process_handle >= process->owned_process_count ||
       !ops[user_foreign_process_handle].is_initialized ||
       !ops[user_foreign_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 foreign_pid = ops[user_foreign_process_handle].pid;

    assert(user_local_id_buffer % sizeof(u64) == 0, "user_local_id_buffer is aligned");
    assert(user_foreign_id_buffer % sizeof(u64) == 0, "user_foreign_id_buffer is aligned");

    mmu_virt_to_phys_buffer(mmu_local_id_buffer, process->mmu_table, user_local_id_buffer, user_count * sizeof(u64))
    mmu_virt_to_phys_buffer(mmu_foreign_id_buffer, process->mmu_table, user_foreign_id_buffer, user_count * sizeof(u64))

    if( mmu_virt_to_phys_buffer_return_value(mmu_local_id_buffer) ||
        mmu_virt_to_phys_buffer_return_value(mmu_foreign_id_buffer)) // buffer is not mapped
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    for(u64 i = 0; i < user_count; i++)
    {
        u64* local_id = mmu_virt_to_phys_buffer_get_address(mmu_local_id_buffer, i * sizeof(u64));
        u64* foreign_id = mmu_virt_to_phys_buffer_get_address(mmu_foreign_id_buffer, i * sizeof(u64));

        u64 file_id;
        if(!user_give_write_access && process_get_file_read_access(process, *local_id, &file_id))
        {
            u64 access = process_new_filesystem_access(
                                    foreign_pid,
                                    file_id,
                                    FILE_ACCESS_PERMISSION_READ_BIT
            );
            *foreign_id = access;
        }
        else if(process_get_file_write_access(process, *local_id, &file_id))
        {
            u64 access = process_new_filesystem_access(
                                    foreign_pid,
                                    file_id,
                                    FILE_ACCESS_PERMISSION_READ_WRITE_BIT
            );
            *foreign_id = access;
        }
        else
        { *foreign_id = U64_MAX; }
    }

    frame->regs[10] = 1;
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_add_program_argument_string(hart)
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

    u64 user_foreign_process_handle = frame->regs[11];
    u64 user_string_buffer = frame->regs[12];
    u64 user_string_length = frame->regs[13];

    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_foreign_process_handle >= process->owned_process_count ||
       !ops[user_foreign_process_handle].is_initialized ||
       !ops[user_foreign_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 foreign_pid = ops[user_foreign_process_handle].pid;
    Process* foreign_process = KERNEL_PROCESS_ARRAY[foreign_pid];

    mmu_virt_to_phys_buffer(mmu_string_buffer, process->mmu_table, user_string_buffer, user_string_length)

    if(mmu_virt_to_phys_buffer_return_value(mmu_string_buffer) || foreign_process->has_started) // buffer is not mapped or process started already
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    if(foreign_process->string_argument_buffer_length + user_string_length >
        foreign_process->string_argument_buffer_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(foreign_process->string_argument_buffer_alloc.page_count + 1);
        memcpy(new_alloc.memory, foreign_process->string_argument_buffer_alloc.memory, foreign_process->string_argument_buffer_length);
        kfree_pages(foreign_process->string_argument_buffer_alloc);
        foreign_process->string_argument_buffer_alloc = new_alloc;
    }

    u64 string_index = foreign_process->string_argument_buffer_length;
    foreign_process->string_argument_buffer_length += user_string_length;
    for(u64 i = 0; i < user_string_length; i++)
    {
        u8* dest_ptr = foreign_process->string_argument_buffer_alloc.memory + string_index + i;
        *dest_ptr = *((u8*)mmu_virt_to_phys_buffer_get_address(mmu_string_buffer, i));
    }

    ProgramArgument arg;
    arg.type = PROGRAM_ARGUMENT_TYPE_STRING;
    arg.string_offset = string_index;
    arg.string_length = user_string_length;

    process_add_program_argument(foreign_process, arg);

    frame->regs[10] = 1;
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_get_program_argument_string(hart)
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

    u64 user_argument_index = frame->regs[11];
    u64 user_string_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    mmu_virt_to_phys_buffer(mmu_string_buffer, process->mmu_table, user_string_buffer, user_buffer_size)

    if(mmu_virt_to_phys_buffer_return_value(mmu_string_buffer)) // buffer is not mapped
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    ProgramArgument* arg = ((ProgramArgument*)process->program_argument_alloc.memory) + user_argument_index;
    if(user_argument_index >= process->program_argument_count || arg->type != PROGRAM_ARGUMENT_TYPE_STRING)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 copy_amount = arg->string_length;
    if(copy_amount > user_buffer_size) { copy_amount = user_buffer_size; }
    for(u64 i = 0; i < copy_amount; i++)
    {
        u8* src_ptr = process->string_argument_buffer_alloc.memory + arg->string_offset + i;
        *((u8*)mmu_virt_to_phys_buffer_get_address(mmu_string_buffer, i)) = *src_ptr;
    }

    frame->regs[10] = arg->string_length;
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_read_blocks(u64 hart)
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
    u64 user_op_array = frame->regs[12];
    u64 user_op_count = frame->regs[13];

    u64 file_id;
    if(!process_get_file_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_op_array, user_op_count*2*sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        Kallocation memory_op_temp_buf_alloc = kalloc_pages(((user_op_count*2*sizeof(u64)) + PAGE_SIZE - 1) / PAGE_SIZE);
        u64* op_ptr = memory_op_temp_buf_alloc.memory;

        for(u64 i = 0; i < user_op_count; i++)
        {
            u64* block_num = mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)  *(i*2 + 0));
            u64* dest_memory = mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)*(i*2 + 1));

            u64 actual_dest_memory;
            if(mmu_virt_to_phys(process->mmu_table, *dest_memory, &actual_dest_memory) || actual_dest_memory % PAGE_SIZE != 0)
            {
                kfree_pages(memory_op_temp_buf_alloc);
                frame->regs[10] = 0;
                current_thread->program_counter += 4;
                rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
                return;
            }
            *(op_ptr++) = *block_num;
            *(op_ptr++) = actual_dest_memory;
        }

        kernel_file_read_blocks(file_id, memory_op_temp_buf_alloc.memory, user_op_count);

        kfree_pages(memory_op_temp_buf_alloc);

        frame->regs[10] = 1;
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_write_blocks(u64 hart)
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
    u64 user_op_array = frame->regs[12];
    u64 user_op_count = frame->regs[13];

    u64 file_id;
    if(!process_get_file_write_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_op_array, user_op_count*2*sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        Kallocation memory_op_temp_buf_alloc = kalloc_pages(((user_op_count*2*sizeof(u64)) + PAGE_SIZE - 1) / PAGE_SIZE);
        u64* op_ptr = memory_op_temp_buf_alloc.memory;

        for(u64 i = 0; i < user_op_count; i++)
        {
            u64* block_num = mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)  *(i*2 + 0));
            u64* dest_memory = mmu_virt_to_phys_buffer_get_address(my_buffer, sizeof(u64)*(i*2 + 1));

            u64 actual_dest_memory;
            if(mmu_virt_to_phys(process->mmu_table, *dest_memory, &actual_dest_memory) || actual_dest_memory % PAGE_SIZE != 0)
            {
                kfree_pages(memory_op_temp_buf_alloc);
                frame->regs[10] = 0;
                current_thread->program_counter += 4;
                rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
                return;
            }
            *(op_ptr++) = *block_num;
            *(op_ptr++) = actual_dest_memory;
        }

        kernel_file_write_blocks(file_id, memory_op_temp_buf_alloc.memory, user_op_count);

        kfree_pages(memory_op_temp_buf_alloc);

        frame->regs[10] = 1;
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_get_size(u64 hart)
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

    u64 file_id;
    if(!process_get_file_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    frame->regs[10] = kernel_file_get_size(file_id);
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_get_block_count(u64 hart)
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

    u64 file_id;
    if(!process_get_file_read_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    frame->regs[10] = kernel_file_get_block_count(file_id);
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_add_program_argument_file(hart)
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

    u64 user_foreign_process_handle = frame->regs[11];
    u64 user_foreign_file_handle = frame->regs[12];
    OwnedProcess* ops = process->owned_process_alloc.memory;
    if(user_foreign_process_handle >= process->owned_process_count ||
       !ops[user_foreign_process_handle].is_initialized ||
       !ops[user_foreign_process_handle].is_alive)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }
    u64 foreign_pid = ops[user_foreign_process_handle].pid;
    Process* foreign_process = KERNEL_PROCESS_ARRAY[foreign_pid];

    u64 file_id;
    if(!process_get_file_read_access(foreign_process, user_foreign_file_handle, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    ProgramArgument arg;
    arg.type = PROGRAM_ARGUMENT_TYPE_FILE;
    arg.file_id = user_foreign_file_handle;

    process_add_program_argument(foreign_process, arg);

    frame->regs[10] = 1;
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_process_get_program_argument_file(hart)
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

    u64 user_argument_index = frame->regs[11];

    ProgramArgument* arg = ((ProgramArgument*)process->program_argument_alloc.memory) + user_argument_index;
    if(user_argument_index >= process->program_argument_count || arg->type != PROGRAM_ARGUMENT_TYPE_FILE)
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_read(&process->process_lock);
        rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    frame->regs[10] = arg->file_id;
    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_set_size(u64 hart)
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
    u64 user_new_size = frame->regs[12];

    u64 file_id;
    if(!process_get_file_write_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    frame->regs[10] = kernel_file_set_size(file_id, user_new_size);
    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_directory_create_file(u64 hart)
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

    u64 local_dir_id = frame->regs[11];
    u64 user_out_file_id = frame->regs[12];

    // TODO lock around file access array
    u64 dir_id;
    u8 has_write_access = process_get_directory_write_access(process, local_dir_id, &dir_id);

    assert(user_out_file_id % 8 == 0, "out_file_id is 8 byte aligned");

    u64* out_file_id;
    if(mmu_virt_to_phys(process->mmu_table, user_out_file_id, &out_file_id) || !has_write_access)
    {
        frame->regs[10] = 0;
    }
    else
    {
        u64 temp_id;
        frame->regs[10] = kernel_directory_create_file(dir_id, &temp_id);
        *out_file_id = process_new_filesystem_access(process->pid, temp_id, FILE_ACCESS_PERMISSION_READ_WRITE_BIT);
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_file_set_name(u64 hart)
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
    u64 user_buffer = frame->regs[12];
    u64 user_buffer_size = frame->regs[13];

    u64 file_id;
    if(!process_get_file_write_access(process, user_file_id, &file_id))
    {
        frame->regs[10] = 0;
        current_thread->program_counter += 4;
        rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
        return;
    }

    u64 actual_count = user_buffer_size;
    u8 temp_buf[KERNEL_FILE_MAX_NAME_LEN + 1];
    if(actual_count > KERNEL_FILE_MAX_NAME_LEN)
    { actual_count = KERNEL_FILE_MAX_NAME_LEN; }

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_buffer, actual_count)

    if(mmu_virt_to_phys_buffer_return_value(my_buffer))
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        for(u64 i = 0; i < actual_count; i++)
        { temp_buf[i] = *((u8*)mmu_virt_to_phys_buffer_get_address(my_buffer, i)); }
        temp_buf[actual_count] = 0;
        frame->regs[10] = kernel_file_set_name(file_id, temp_buf);
    }

    current_thread->program_counter += 4;
    rwlock_release_write(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_charta_media_crea(u64 hart)
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

    u64 user_numerus_paginae = frame->regs[11];
    u64 user_index_ad_ansam_chartae = frame->regs[12];

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_index_ad_ansam_chartae, sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer) || (user_index_ad_ansam_chartae % sizeof(u64)) != 0)
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        // do stuff
        u64 ansa_universa;
        if(!charta_media_crea(user_numerus_paginae, &ansa_universa))
        {
            frame->regs[10] = 0;
        }
        else
        {
            rwlock_acquire_write(&process->process_lock);
            u64 ansa_programmatis = programmatis_chartam_mediam_crea(process, ansa_universa);
            rwlock_release_write(&process->process_lock);
            frame->regs[10] = 1;
            *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer, 0)) = ansa_programmatis;

            chartam_mediam_omitte(ansa_universa);
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_chartam_mediam_omitte(u64 hart)
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

    u64 user_ansa_chartae = frame->regs[11];

    {
        rwlock_acquire_read(&process->process_lock);

        u8 was_valid = 0;
        ProgrammatisChartaMedia buffer_to_be_destroyed;
        if(user_ansa_chartae < process->magnitudo_lineae_chartarum_mediarum)
        {
            ProgrammatisChartaMedia* chartae = process->adsignatio_lineae_chartarum_mediarum.memory;
            spinlock_acquire(&chartae[user_ansa_chartae].sera_versandi);
            if(chartae[user_ansa_chartae].si_creata_et_numerus_ponendi > 0)
            {
                buffer_to_be_destroyed = chartae[user_ansa_chartae];
                chartae[user_ansa_chartae].si_creata_et_numerus_ponendi = 0;
                was_valid = 1;
            }
            spinlock_release(&chartae[user_ansa_chartae].sera_versandi);
        }
        rwlock_release_read(&process->process_lock);

        if(was_valid) // then forget about the buffer
        {
            rwlock_acquire_write(&process->process_lock);

            ProgrammatisLocusPonendiChartaeMediae* loca = buffer_to_be_destroyed.adsignatio_lineae_locorum_ponendi.memory;
            for(u64 i = 0; i + 1 < buffer_to_be_destroyed.si_creata_et_numerus_ponendi; i++)
            {
                Kallocation dummy;
                dummy.memory = 0;
                dummy.page_count = loca[i].numerus_paginae;

                mmu_map_kallocation(process->mmu_table, dummy, loca[i].vaddr, 0);
            }
            chartam_mediam_omitte(buffer_to_be_destroyed.ansa_chartae_mediae_superae);

            rwlock_release_write(&process->process_lock);
            frame->regs[10] = 1;
        }
        else
        {
            frame->regs[10] = 0;
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_chartae_mediae_magnitudem_disce(u64 hart)
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

    u64 user_ansa_chartae = frame->regs[11];

    {
        rwlock_acquire_read(&process->process_lock);

        if(user_ansa_chartae < process->magnitudo_lineae_chartarum_mediarum)
        {
            ProgrammatisChartaMedia* chartae = process->adsignatio_lineae_chartarum_mediarum.memory;
            spinlock_acquire(&chartae[user_ansa_chartae].sera_versandi);

            if(chartae[user_ansa_chartae].si_creata_et_numerus_ponendi > 0)
            {
                frame->regs[10] = chartae[user_ansa_chartae].adsignatio_chartae_mediae_superae.page_count;
            }
            else
            { frame->regs[10] = 0; }

            spinlock_release(&chartae[user_ansa_chartae].sera_versandi);
        }
        rwlock_release_read(&process->process_lock);
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_chartam_mediam_pone(u64 hart)
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

    u64 user_ansa_chartae = frame->regs[11];
    u64 user_index_ad_locum_ponendi = frame->regs[12];
    u64 user_pagina_prima = frame->regs[13];
    u64 user_numerus_paginae = frame->regs[14];


    frame->regs[10] = 0;
    if(user_index_ad_locum_ponendi & 0xfff != 0 || user_numerus_paginae == 0)
    {}
    else
    {
        rwlock_acquire_read(&process->process_lock);
        ProgrammatisChartaMedia* chartae = process->adsignatio_lineae_chartarum_mediarum.memory;

        u8 was_valid = 0;
        if(user_ansa_chartae < process->magnitudo_lineae_chartarum_mediarum)
        {
            spinlock_acquire(&chartae[user_ansa_chartae].sera_versandi);

            if( chartae[user_ansa_chartae].si_creata_et_numerus_ponendi > 0 &&
                user_pagina_prima + user_numerus_paginae <=
                chartae[user_ansa_chartae].adsignatio_chartae_mediae_superae.page_count
            )
            { was_valid = 1; }

            spinlock_release(&chartae[user_ansa_chartae].sera_versandi);
        }
        rwlock_release_read(&process->process_lock);

        if(was_valid)
        {
            rwlock_acquire_write(&process->process_lock);                   // TODO allow memory allocation without locking all threads
            chartae = process->adsignatio_lineae_chartarum_mediarum.memory;
            ProgrammatisChartaMedia* charta = chartae + user_ansa_chartae;

            if(charta->si_creata_et_numerus_ponendi)
            {
                Kallocation sub_kallocation_to_map = charta->adsignatio_chartae_mediae_superae;
                sub_kallocation_to_map.memory += user_pagina_prima * PAGE_SIZE;
                sub_kallocation_to_map.page_count = user_numerus_paginae;

                u32 numerus_ponendi = charta->si_creata_et_numerus_ponendi - 1;

                if( sizeof(ProgrammatisLocusPonendiChartaeMediae) * (numerus_ponendi+1) >
                    PAGE_SIZE * charta->adsignatio_lineae_locorum_ponendi.page_count)
                {
                    Kallocation new_alloc = kalloc_pages(charta->adsignatio_lineae_locorum_ponendi.page_count + 1);
                    ProgrammatisLocusPonendiChartaeMediae* new_array = new_alloc.memory;
                    ProgrammatisLocusPonendiChartaeMediae* old_array = charta->adsignatio_lineae_locorum_ponendi.memory;
                    for(u64 i = 0; i < numerus_ponendi; i++)
                    {
                        new_array[i] = old_array[i];
                    }
                    kfree_pages(charta->adsignatio_lineae_locorum_ponendi);
                    charta->adsignatio_lineae_locorum_ponendi = new_alloc;
                }
                ProgrammatisLocusPonendiChartaeMediae* loca = charta->adsignatio_lineae_locorum_ponendi.memory;
                u64 index = numerus_ponendi++;
                charta->si_creata_et_numerus_ponendi = numerus_ponendi + 1;

                loca[index].vaddr = user_index_ad_locum_ponendi;
                loca[index].numerus_paginae = sub_kallocation_to_map.page_count;
                mmu_map_kallocation(process->mmu_table, sub_kallocation_to_map, loca[index].vaddr, 2 + 4); // read + write

                frame->regs[10] = 1;
            }

            rwlock_release_write(&process->process_lock);
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_chartam_mediam_deme(u64 hart)
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

    u64 user_ansa_chartae = frame->regs[11];
    u64 user_index_ad_locum_quo_chartam_positus_est = frame->regs[12];


    frame->regs[10] = 0;
    {
        rwlock_acquire_read(&process->process_lock);
        ProgrammatisChartaMedia* chartae = process->adsignatio_lineae_chartarum_mediarum.memory;

        u8 found_match = 0;
        ProgrammatisLocusPonendiChartaeMediae match;
        if(user_ansa_chartae < process->magnitudo_lineae_chartarum_mediarum)
        {
            spinlock_acquire(&chartae[user_ansa_chartae].sera_versandi);

            if(chartae[user_ansa_chartae].si_creata_et_numerus_ponendi > 0)
            {
                ProgrammatisChartaMedia* charta = chartae + user_ansa_chartae;
                ProgrammatisLocusPonendiChartaeMediae* loca = charta->adsignatio_lineae_locorum_ponendi.memory;

                u64 index_of_the_found = U64_MAX;
                for(u64 i = 0; i + 1 < charta->si_creata_et_numerus_ponendi; i++)
                {
                    if(loca[i].vaddr == user_index_ad_locum_quo_chartam_positus_est)
                    {
                        found_match = 1;
                        index_of_the_found = i;
                        break;
                    }
                }
                if(found_match)
                {
                    match = loca[index_of_the_found];
                    for(u64 i = index_of_the_found; i + 2 < charta->si_creata_et_numerus_ponendi; i++)
                    {
                        loca[i] = loca[i+1];
                    }
                    charta->si_creata_et_numerus_ponendi--;
                }
            }

            spinlock_release(&chartae[user_ansa_chartae].sera_versandi);
        }
        rwlock_release_read(&process->process_lock);

        if(found_match) // then unmap the buffer at that location
        {
            rwlock_acquire_write(&process->process_lock);                   // TODO allow memory allocation without locking all threads

            Kallocation dummy;
            dummy.memory = 0;
            dummy.page_count = match.numerus_paginae;

            mmu_map_kallocation(process->mmu_table, dummy, match.vaddr, 0);
            frame->regs[10] = 1;

            rwlock_release_write(&process->process_lock);
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_chartam_mediam_da(u64 hart)
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

    u64 user_ansa_chartae_inferae = frame->regs[11];
    u64 user_ansa_programmatis_alieni = frame->regs[12];
    u64 user_index_ad_ansam_alienam = frame->regs[13];

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_index_ad_ansam_alienam, sizeof(u64))

    if(mmu_virt_to_phys_buffer_return_value(my_buffer) || (user_index_ad_ansam_alienam % sizeof(u64)) != 0)
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        u8 ansa_chartae_rata_est = 0;
        u64 ansa_chartae_superae;

        rwlock_acquire_read(&process->process_lock);
        ProgrammatisChartaMedia* chartae = process->adsignatio_lineae_chartarum_mediarum.memory;
        if(user_ansa_chartae_inferae < process->magnitudo_lineae_chartarum_mediarum)
        {
            ProgrammatisChartaMedia* charta = chartae + user_ansa_chartae_inferae;
            spinlock_acquire(&charta->sera_versandi);

            if(charta->si_creata_et_numerus_ponendi)
            {
                ansa_chartae_rata_est = 1;
                ansa_chartae_superae = charta->ansa_chartae_mediae_superae;
                charta_media_calculum_possessorum_augmenta(ansa_chartae_superae); // increment reference counter so it won't get freed in the mean time
            }

            spinlock_release(&charta->sera_versandi);
        }

        u8 ansa_programmatis_alieni_rata_est = 0;
        u64 foreign_pid;

        OwnedProcess* ops = process->owned_process_alloc.memory;
        if(user_ansa_programmatis_alieni < process->owned_process_count &&
           ops[user_ansa_programmatis_alieni].is_initialized &&
           ops[user_ansa_programmatis_alieni].is_alive)
        {
            ansa_programmatis_alieni_rata_est = 1;
            foreign_pid = ops[user_ansa_programmatis_alieni].pid;
        }
        rwlock_release_read(&process->process_lock);

        if(!ansa_chartae_rata_est || !ansa_programmatis_alieni_rata_est)
        {
            frame->regs[10] = 0;
        }
        else
        {
            Process* foreign_process = KERNEL_PROCESS_ARRAY[foreign_pid];
            rwlock_acquire_write(&foreign_process->process_lock);
            u64 ansa_programmatis = programmatis_chartam_mediam_crea(foreign_process, ansa_chartae_superae);
            rwlock_release_write(&foreign_process->process_lock);
            frame->regs[10] = 1;
            *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer, 0)) = ansa_programmatis;
        }

        if(ansa_chartae_rata_est)
        {
            chartam_mediam_omitte(ansa_chartae_superae);
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphorum_medium_crea(u64 hart)
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

    u64 user_pretium_primum = frame->regs[11];
    u64 user_pretium_maximum = frame->regs[12];
    u64 user_index_ad_ansam_suscitandi = frame->regs[13];
    u64 user_ansa_programmatis_quod_suscitat = frame->regs[14];
    u64 user_index_ad_ansam_expectandi = frame->regs[15];
    u64 user_ansa_programmatis_quod_expectat = frame->regs[16];

    rwlock_acquire_read(&process->process_lock);

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_index_ad_ansam_suscitandi, sizeof(u64))
    mmu_virt_to_phys_buffer(my_buffer2, process->mmu_table, user_index_ad_ansam_expectandi, sizeof(u64))

    OwnedProcess* ops = process->owned_process_alloc.memory;

    u64 programma_quod_suscitat = U64_MAX; // if it stays like this that indicates failure
    if(user_ansa_programmatis_quod_suscitat == U64_MAX)
    {
        programma_quod_suscitat = process->pid;
    }
    else if(user_ansa_programmatis_quod_suscitat < process->owned_process_count &&
            ops[user_ansa_programmatis_quod_suscitat].is_initialized &&
            ops[user_ansa_programmatis_quod_suscitat].is_alive)
    {
        programma_quod_suscitat = ops[user_ansa_programmatis_quod_suscitat].pid;
    }

    u64 programma_quod_expectat = U64_MAX; // if it stays like this that indicates failure
    if(user_ansa_programmatis_quod_expectat == U64_MAX)
    {
        programma_quod_expectat = process->pid;
    }
    else if(user_ansa_programmatis_quod_expectat < process->owned_process_count &&
            ops[user_ansa_programmatis_quod_expectat].is_initialized &&
            ops[user_ansa_programmatis_quod_expectat].is_alive)
    {
        programma_quod_expectat = ops[user_ansa_programmatis_quod_expectat].pid;
    }

    rwlock_release_read(&process->process_lock);

    if( mmu_virt_to_phys_buffer_return_value(my_buffer)  || (user_index_ad_ansam_suscitandi % sizeof(u64)) != 0 ||
        mmu_virt_to_phys_buffer_return_value(my_buffer2) || (user_index_ad_ansam_expectandi % sizeof(u64)) != 0 ||
        programma_quod_suscitat == U64_MAX || programma_quod_expectat == U64_MAX)
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        u64 ansa_semaphori_superi;
        if(!semaphorum_medium_crea(&ansa_semaphori_superi, user_pretium_primum, user_pretium_maximum))
        {
            frame->regs[10] = 0;
        }
        else
        {
            // we increment up to 3 owners
            semaphorum_medium_calculum_possessorum_augmenta(ansa_semaphori_superi);
            semaphorum_medium_calculum_possessorum_augmenta(ansa_semaphori_superi);

            { // programma_quod_suscitat
                Process* local_process = KERNEL_PROCESS_ARRAY[programma_quod_suscitat];
                rwlock_acquire_write(&local_process->process_lock);

                u64 ansa_semaphori = programmatis_semaphorum_medium_crea(local_process);
                ProgrammatisSemaphorumMediorum* semaphora = local_process->adsignatio_lineae_semaphororum_mediorum.memory;

                semaphora[ansa_semaphori].ansa_semaphori_medii_superi_et_alia_data =
                    ansa_semaphori_superi | (3llu << 62);

                *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer, 0)) = ansa_semaphori;

                rwlock_release_write(&local_process->process_lock);
            }
            { // programma_quod_expectat
                Process* local_process = KERNEL_PROCESS_ARRAY[programma_quod_expectat];
                rwlock_acquire_write(&local_process->process_lock);

                u64 ansa_semaphori = programmatis_semaphorum_medium_crea(local_process);
                ProgrammatisSemaphorumMediorum* semaphora = local_process->adsignatio_lineae_semaphororum_mediorum.memory;

                semaphora[ansa_semaphori].ansa_semaphori_medii_superi_et_alia_data =
                    ansa_semaphori_superi | (2llu << 62);

                *((u64*)mmu_virt_to_phys_buffer_get_address(my_buffer2, 0)) = ansa_semaphori;

                rwlock_release_write(&local_process->process_lock);
            }

            // now we decrement by 1 owner because we are no longer tracking the semaphore
            semaphorum_medium_omitte(ansa_semaphori_superi);

            frame->regs[10] = 1;
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphorum_medium_suscita(u64 hart)
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

    u64 user_ansam_semaphori = frame->regs[11];
    u64 user_numerus_suscitandi = frame->regs[12];
    u64 user_index_ad_pretium_prius = frame->regs[13];

    rwlock_acquire_read(&process->process_lock);

    mmu_virt_to_phys_buffer(my_buffer, process->mmu_table, user_index_ad_pretium_prius, sizeof(s64))

    if((user_index_ad_pretium_prius && mmu_virt_to_phys_buffer_return_value(my_buffer)) || (user_index_ad_pretium_prius % sizeof(s64)) != 0)
    { // failed
        frame->regs[10] = 0;
    }
    else
    {
        ProgrammatisSemaphorumMediorum* semaphora = process->adsignatio_lineae_semaphororum_mediorum.memory;

        if( user_ansam_semaphori >= process->magnitudo_lineae_semaphororum_mediorum ||
            (semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (3llu << 62)) != (3llu << 62))
        {
            frame->regs[10] = 0;
        }
        else
        {
            u64 ansa_semaphori_superi = semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (~(3llu << 62));

            s64 prev_value;
            if(semaphorum_medium_suscita(ansa_semaphori_superi, user_numerus_suscitandi, &prev_value))
            {
                if(user_index_ad_pretium_prius)
                { *((s64*)mmu_virt_to_phys_buffer_get_address(my_buffer, 0)) = prev_value; }
                frame->regs[10] = 1;
            }
            else
            { frame->regs[10] = 0; }
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphorum_medium_expectare_conare(u64 hart)
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

    u64 user_ansam_semaphori = frame->regs[11];

    rwlock_acquire_read(&process->process_lock);

    {
        ProgrammatisSemaphorumMediorum* semaphora = process->adsignatio_lineae_semaphororum_mediorum.memory;

        if( user_ansam_semaphori >= process->magnitudo_lineae_semaphororum_mediorum ||
            (semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (2llu << 62)) != (2llu << 62)) // is this a wait handle?
        {
            frame->regs[10] = 0;
        }
        else
        {
            u64 ansa_semaphori_superi = semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (~(3llu << 62));

            frame->regs[10] = semaphorum_medium_expectare_conare(ansa_semaphori_superi);
        }
    }

    current_thread->program_counter += 4;
    rwlock_release_read(&process->process_lock);
    rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
}

void syscall_semaphorum_medium_expecta(u64 hart, u64 mtime)
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

    u64 user_ansam_semaphori = frame->regs[11];

    rwlock_acquire_read(&process->process_lock);

    current_thread->program_counter += 4;
    {
        ProgrammatisSemaphorumMediorum* semaphora = process->adsignatio_lineae_semaphororum_mediorum.memory;

        if( user_ansam_semaphori >= process->magnitudo_lineae_semaphororum_mediorum ||
            (semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (2llu << 62)) != (2llu << 62)) // is this a wait handle?
        {
            frame->regs[10] = 0;
        }
        else
        {
            u64 ansa_semaphori_superi = semaphora[user_ansam_semaphori].ansa_semaphori_medii_superi_et_alia_data & (~(3llu << 62));

            frame->regs[10] = semaphorum_medium_expectare_conare(ansa_semaphori_superi);

            if(!frame->regs[10]) // we are going to sleep
            {
                frame->regs[10] = 1;
                current_thread->is_running = 0;
                current_thread->is_waiting_on_semaphore_and_semaphore_handle = (1llu << 63) | ansa_semaphori_superi;
                semaphorum_medium_calculum_possessorum_augmenta(ansa_semaphori_superi); // the waiting thread get's to keep the semaphore alive

                rwlock_release_read(&process->process_lock);
                kernel_choose_new_thread(mtime, hart);
                rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
                return;
            }
        }
    }
    rwlock_release_read(&process->process_lock);
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
    else if(call_num == 55)
    { syscall_directory_get_subdirectories(hart); }
    else if(call_num == 56)
    { syscall_directory_get_name(hart); }
    else if(call_num == 57)
    { syscall_directory_get_absolute_ids(hart); }
    else if(call_num == 58)
    { syscall_directory_give(hart); }
    else if(call_num == 59)
    { syscall_process_add_program_argument_string(hart); }
    else if(call_num == 60)
    { syscall_process_get_program_argument_string(hart); }
    else if(call_num == 61)
    { syscall_file_read_blocks(hart); }
    else if(call_num == 62)
    { syscall_file_get_size(hart); }
    else if(call_num == 63)
    { syscall_file_get_block_count(hart); }
    else if(call_num == 64)
    { syscall_file_write_blocks(hart); }
    else if(call_num == 65)
    { syscall_file_give(hart); }
    else if(call_num == 66)
    { syscall_process_add_program_argument_file(hart); }
    else if(call_num == 67)
    { syscall_process_get_program_argument_file(hart); }
    else if(call_num == 68)
    { syscall_file_set_size(hart); }
    else if(call_num == 69)
    { syscall_directory_create_file(hart); }
    else if(call_num == 70)
    { syscall_file_set_name(hart); }
    else if(call_num == 71)
    { syscall_charta_media_crea(hart); }
    else if(call_num == 72)
    { syscall_chartam_mediam_omitte(hart); }
    else if(call_num == 73)
    { syscall_chartae_mediae_magnitudem_disce(hart); }
    else if(call_num == 74)
    { syscall_chartam_mediam_pone(hart); }
    else if(call_num == 75)
    { syscall_chartam_mediam_deme(hart); }
    else if(call_num == 76)
    { syscall_chartam_mediam_da(hart); }
    else if(call_num == 77)
    { syscall_semaphorum_medium_crea(hart); }
    else if(call_num == 78)
    { syscall_semaphorum_medium_suscita(hart); }
    else if(call_num == 79)
    { syscall_semaphorum_medium_expectare_conare(hart); }
    else if(call_num == 80)
    { syscall_semaphorum_medium_expecta(hart, mtime); }
    else
    { uart_printf("invalid syscall, we should handle this case but we don't\n"); while(1) {} }
}
