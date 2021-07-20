
typedef struct
{
    Kallocation alloc;
    u32 width;
    u32 height;
    float data[];
} Framebuffer;

Framebuffer* framebuffer_create(u32 width, u32 height)
{
    u64 byte_count = sizeof(Framebuffer) + (width*height*4*4);
    byte_count += PAGE_SIZE - (byte_count % PAGE_SIZE);
    Kallocation k = kalloc_pages(byte_count >> 12);
    assert(k.memory != 0, "the allocation for the framebuffer was successful");
    Framebuffer* fb = (Framebuffer*)k.memory;

    fb->alloc = k;
    fb->width = width;
    fb->height = height;

    return fb;
}

typedef struct
{
    u64 vaddr;
    u8 has_acquired;
    Framebuffer fb_draw_control;

    u8 is_defering_to_consumer_slot;
    u64 defer_consumer_slot;

    u32 width;
    u32 height;
    volatile Framebuffer* fb_present;
    volatile Framebuffer* fb_draw;
    u8 has_commited;
    u8 has_been_fired;
 
    u64 consumer_pid;
    u64 consumer_slot;
    u8 has_consumer;
    u8 is_initialized;
} SurfaceSlot;

typedef struct
{
    u64 surface_pid;
    u64 surface_slot;
    u8 has_surface;
    u8 is_initialized;

    u32 not_yet_fired_width;
    u32 not_yet_fired_height;

    volatile Framebuffer* fb_fetched;
    Framebuffer fb_fetched_control;
    u8 has_fetched;
    u64 vaddr;
} SurfaceConsumer;

u64 surface_create(Process* p)
{
    for(u64 i = 0; i < p->surface_count; i++)
    {
        SurfaceSlot* s = ((SurfaceSlot*)p->surface_alloc.memory) + i;
        if(!s->is_initialized)
        {
            s->width = 0;
            s->height = 0;
            s->fb_present = framebuffer_create(0, 0);
            s->fb_draw = framebuffer_create(0, 0);
            s->is_initialized = 1;
            s->has_consumer = 0;
            return i;
        }
    }

    if((p->surface_count + 1) * sizeof(SurfaceSlot) > p->surface_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(p->surface_alloc.page_count + 1);
        SurfaceSlot* new_array = (SurfaceSlot*)new_alloc.memory;
        for(u64 i = 0; i < p->surface_count; i++)
        {
            new_array[i] = ((SurfaceSlot*)p->surface_alloc.memory)[i];
        }
        if(p->surface_alloc.page_count != 0)
        {
            kfree_pages(p->surface_alloc);
        }
        p->surface_alloc = new_alloc;
    }
    SurfaceSlot* s = ((SurfaceSlot*)p->surface_alloc.memory) + p->surface_count;
    p->surface_count += 1;

    s->width = 0;
    s->height = 0;
    s->fb_present = framebuffer_create(0, 0);
    s->fb_draw = framebuffer_create(0, 0);
    s->is_initialized = 1;
    s->has_commited = 0;
    s->has_been_fired = 0;
    s->has_consumer = 0;
    s->is_defering_to_consumer_slot = 0;
    s->has_acquired = 0;
    return p->surface_count - 1;
}

u64 surface_consumer_create(Process* process, Process* surface_process, u64* consumer)
{
    if(process == surface_process) { return 0; }
    u64 surface_slot = surface_create(surface_process);

    for(u64 i = 0; i < process->surface_consumer_count; i++)
    {
        SurfaceConsumer* con = ((SurfaceConsumer*)process->surface_consumer_alloc.memory) + i;
        if(!con->is_initialized)
        {
            con->surface_pid = surface_process->pid;
            con->surface_slot = surface_slot;
            con->has_surface = 1;
            con->is_initialized = 1;
            con->has_fetched = 0;
            con->fb_fetched = framebuffer_create(0,0);
            SurfaceSlot* surface = ((SurfaceSlot*)surface_process->surface_alloc.memory) + surface_slot;
            surface->consumer_pid = process->pid;
            surface->consumer_slot = i;
            surface->has_consumer = 1;
            *consumer = i;
            return 1;
        }
    }

    if((process->surface_consumer_count+1)*sizeof(SurfaceConsumer)
        > process->surface_consumer_alloc.page_count*PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(process->surface_consumer_alloc.page_count + 1);
        SurfaceConsumer* new_array = (SurfaceConsumer*)new_alloc.memory;
        for(u64 i = 0; i < process->surface_consumer_count; i++)
        {
            new_array[i] = ((SurfaceConsumer*)process->surface_consumer_alloc.memory)[i];
        }
        if(process->surface_consumer_alloc.page_count != 0)
        {
            kfree_pages(process->surface_consumer_alloc);
        }
        process->surface_consumer_alloc = new_alloc;
    }
    u64 cs = process->surface_consumer_count;
    process->surface_consumer_count += 1;
    SurfaceConsumer* con = ((SurfaceConsumer*)process->surface_consumer_alloc.memory) + cs;
    con->surface_pid = surface_process->pid;
    con->surface_slot = surface_slot;
    con->has_surface = 1;
    con->is_initialized = 1;
    con->has_fetched = 0;
    con->fb_fetched = framebuffer_create(0,0);
    SurfaceSlot* surface = ((SurfaceSlot*)surface_process->surface_alloc.memory) + surface_slot;
    surface->consumer_pid = process->pid;
    surface->consumer_slot = cs;
    surface->has_consumer = 1;
    *consumer = cs;
    return 1;
}

// This function expects that you have a read lock on KERNEL_PROCESS_ARRAY_RWLOCK and
// has a read lock on process->process_lock, unless you pass true in arg3, then you can have
// a write lock
u64 surface_slot_has_commited(Process* process, u64 surface_slot, u64 has_write_lock)
{
    u64 process1_pid = process->pid;
    SurfaceSlot* surface_slot_array = (SurfaceSlot*)process->surface_alloc.memory;
    assert(surface_slot < process->surface_count && surface_slot_array[surface_slot].is_initialized,
        "this is a valid surface slot");

    SurfaceSlot* slot = surface_slot_array + surface_slot;
    u64 defer_slot = slot->defer_consumer_slot;

    SurfaceConsumer* consumer_array = (SurfaceConsumer*)process->surface_consumer_alloc.memory;
    if( slot->is_defering_to_consumer_slot && defer_slot < process->surface_consumer_count &&
        consumer_array[defer_slot].is_initialized &&
        consumer_array[defer_slot].has_surface &&
        consumer_array[defer_slot].surface_pid < KERNEL_PROCESS_ARRAY_LEN)
    {
        u64 surface_pid2 = consumer_array[defer_slot].surface_pid;
        u64 surface_slot2 = consumer_array[defer_slot].surface_slot;
        Process* process2 = KERNEL_PROCESS_ARRAY[surface_pid2];
        if(has_write_lock)
        { rwlock_release_write(&process->process_lock); }
        else
        { rwlock_release_read(&process->process_lock); }
        rwlock_acquire_write(&process2->process_lock);
        if( process2->mmu_table &&
            surface_slot2 < process2->surface_count)
        {
            SurfaceSlot* slot2 = ((SurfaceSlot*)process2->surface_alloc.memory) +
                surface_slot2;
            if( slot2->is_initialized && slot2->has_consumer &&
                slot2->consumer_pid == process1_pid &&
                slot2->consumer_slot == defer_slot)
            {
                // successfull forward
                slot2->width = slot->width;
                slot2->height = slot->height;
                u64 result = surface_slot_has_commited(process2, surface_slot2, 1);
                rwlock_release_write(&process2->process_lock);
                if(has_write_lock)
                { rwlock_acquire_write(&process->process_lock); }
                else
                { rwlock_acquire_read(&process->process_lock); }
                return result;
            }
        }
        rwlock_release_write(&process2->process_lock);
        if(has_write_lock)
        { rwlock_acquire_write(&process->process_lock); }
        else
        { rwlock_acquire_read(&process->process_lock); }
    }
    // otherwise
    surface_slot_array = (SurfaceSlot*)process->surface_alloc.memory;
    slot = surface_slot_array + surface_slot;

    return slot->width == 0 || slot->height == 0 ||
        (slot->has_commited && slot->fb_present->width == slot->width &&
            slot->fb_present->height == slot->height);
}

// This function expects that you have a write lock on process->process_lock
void surface_prepare_draw_framebuffer(u64 surface_slot, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    Framebuffer* draw = s->fb_draw;

    if(draw->width != s->width || draw->height != s->height)
    {
        kfree_pages(draw->alloc);
        draw = framebuffer_create(s->width, s->height);
    }
    s->fb_draw = draw;
}

// This function expects that you have a write lock on process->process_lock
u64 surface_acquire(u64 surface_slot, Framebuffer* fb_location, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    if(surface_slot >= process->surface_count) { return 0; }

    if(surface_slot_has_commited(process, surface_slot, 1) ||
        !s->has_been_fired) { return 0; }

    s->fb_draw_control = *s->fb_draw;
    if(process_alloc_pages(process, fb_location, s->fb_draw->alloc))
    {
        s->has_acquired = 1;
        s->has_been_fired = 0;
        s->vaddr = fb_location;
        return 1;
    }
    return 0;
}

u64 surface_commit(u64 surface_slot, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    if(surface_slot >= process->surface_count || !s->has_acquired) { return 0; }

    Kallocation a = process_shrink_allocation(process, s->vaddr, 0);
    if(a.memory == 0) { return 0; }

    *s->fb_draw = s->fb_draw_control;
    s->has_acquired = 0;

    volatile Framebuffer* temp = s->fb_present;
    s->fb_present = s->fb_draw;

    s->fb_draw = temp;
    s->has_commited = 1;
    return 1;
}
// This function expects that you have a read lock on KERNEL_PROCESS_ARRAY_RWLOCK and
// has a write lock on process->process_lock
void surface_slot_fire(Process* process, u64 surface_slot)
{
    u64 process1_pid = process->pid;
    SurfaceSlot* surface_slot_array = (SurfaceSlot*)process->surface_alloc.memory;
    assert(surface_slot < process->surface_count && surface_slot_array[surface_slot].is_initialized,
        "this is a valid surface slot");

    SurfaceSlot* slot = surface_slot_array + surface_slot;
    u64 defer_slot = slot->defer_consumer_slot;

    SurfaceConsumer* consumer_array = (SurfaceConsumer*)process->surface_consumer_alloc.memory;
    if( slot->is_defering_to_consumer_slot && defer_slot < process->surface_consumer_count &&
        consumer_array[defer_slot].is_initialized &&
        consumer_array[defer_slot].has_surface &&
        consumer_array[defer_slot].surface_pid < KERNEL_PROCESS_ARRAY_LEN)
    {
        u64 surface_pid2 = consumer_array[defer_slot].surface_pid;
        u64 surface_slot2 = consumer_array[defer_slot].surface_slot;
        Process* process2 = KERNEL_PROCESS_ARRAY[surface_pid2];
        rwlock_release_write(&process->process_lock);
        rwlock_acquire_write(&process2->process_lock);
        if( process2->mmu_table &&
            surface_slot2 < process2->surface_count)
        {
            SurfaceSlot* slot2 = ((SurfaceSlot*)process2->surface_alloc.memory) +
                surface_slot2;
            if( slot2->is_initialized && slot2->has_consumer &&
                slot2->consumer_pid == process1_pid &&
                slot2->consumer_slot == defer_slot)
            {
                // successfull forward
                slot2->width = slot->width;
                slot2->height = slot->height;
                surface_slot_fire(process2, surface_slot2);
                rwlock_release_write(&process2->process_lock);
                rwlock_acquire_write(&process->process_lock);
                return;
            }
        }
        rwlock_release_write(&process2->process_lock);
        rwlock_acquire_write(&process->process_lock);
    }
    surface_slot_array = (SurfaceSlot*)process->surface_alloc.memory;
    slot = surface_slot_array + surface_slot;
    // otherwise
    if(!surface_slot_has_commited(process, surface_slot, 1))
    { slot->has_been_fired = 1; }
}

// You must pass a valid replacement buffer. It does not have to be the right size though.
// This function expects that you have a read lock on KERNEL_PROCESS_ARRAY_RWLOCK and
// has a write lock on process->process_lock
Framebuffer* surface_slot_swap_present_buffer(Process* process, u64 surface_slot, Framebuffer* replacement)
{
    u64 process1_pid = process->pid;
    SurfaceSlot* surface_slot_array = (SurfaceSlot*)process->surface_alloc.memory;
    assert(surface_slot < process->surface_count && surface_slot_array[surface_slot].is_initialized,
        "this is a valid surface slot");
 
    SurfaceSlot* slot = surface_slot_array + surface_slot;
    u64 defer_consumer_slot = slot->defer_consumer_slot;
 
    SurfaceConsumer* consumer_array = (SurfaceConsumer*)process->surface_consumer_alloc.memory;
    if( slot->is_defering_to_consumer_slot && defer_consumer_slot < process->surface_consumer_count &&
        consumer_array[defer_consumer_slot].is_initialized &&
        consumer_array[defer_consumer_slot].has_surface &&
        consumer_array[defer_consumer_slot].surface_pid < KERNEL_PROCESS_ARRAY_LEN)
    {
        u64 surface_pid2 = consumer_array[defer_consumer_slot].surface_pid;
        u64 surface_slot2 = consumer_array[defer_consumer_slot].surface_slot;
        Process* process2 = KERNEL_PROCESS_ARRAY[surface_pid2];
        rwlock_release_write(&process->process_lock);
        rwlock_acquire_write(&process2->process_lock);

        if( process2->mmu_table &&
            surface_slot2 < process2->surface_count)
        {
            SurfaceSlot* slot2 = ((SurfaceSlot*)process2->surface_alloc.memory) +
                surface_slot2;
            if( slot2->is_initialized && slot2->has_consumer &&
                slot2->consumer_pid == process1_pid &&
                slot2->consumer_slot == defer_consumer_slot)
            {
                // successfull forward
                Framebuffer* result = surface_slot_swap_present_buffer(
                        process2,
                        surface_slot2,
                        replacement
                    );
                rwlock_release_write(&process2->process_lock);
                rwlock_acquire_write(&process->process_lock);
                return result;
            }
        }
        rwlock_release_write(&process2->process_lock);
        rwlock_acquire_write(&process->process_lock);
    }
    // otherwise
    Framebuffer* temp = replacement;
    replacement = slot->fb_present;
    slot->fb_present = temp;
    slot->has_commited = 0;
    return replacement;
}

u64 get_consumer_and_surface(u64 pid, u64 consumer_slot, SurfaceConsumer** c, SurfaceSlot** surface)
{
    if(pid >= KERNEL_PROCESS_ARRAY_LEN ||
              consumer_slot >= KERNEL_PROCESS_ARRAY[pid]->surface_consumer_count)
    { return 0; }
    SurfaceConsumer* con = ((SurfaceConsumer*)KERNEL_PROCESS_ARRAY[pid]->surface_consumer_alloc.memory)
                            + consumer_slot;
    if(!con->is_initialized || !con->has_surface ||
        con->surface_pid >= KERNEL_PROCESS_ARRAY_LEN ||
        con->surface_slot >= KERNEL_PROCESS_ARRAY[con->surface_pid]->surface_count
    ) { return 0; }
    SurfaceSlot* s = ((SurfaceSlot*)KERNEL_PROCESS_ARRAY[con->surface_pid]->surface_alloc.memory)
                        + con->surface_slot;
    if(!s->is_initialized || !s->has_consumer || s->consumer_pid != pid || s->consumer_slot != consumer_slot)
    { return 0; }

    *c = con;
    *surface = s;
    return 1;
}

u64 surface_consumer_has_commited(u64 pid, u64 consumer_slot)
{
    SurfaceConsumer* con; SurfaceSlot* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    Process* process = KERNEL_PROCESS_ARRAY[con->surface_pid];
    rwlock_acquire_read(&process->process_lock);
    u64 result = surface_slot_has_commited(process, con->surface_slot, 0);
    rwlock_release_read(&process->process_lock);
}

u64 surface_consumer_get_size(u64 pid, u64 consumer_slot, u32* width, u32* height)
{
    SurfaceConsumer* con; SurfaceSlot* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    *width = s->width;
    *height = s->height;
    return 1;
}

u64 surface_consumer_set_size(u64 pid, u64 consumer_slot, u32 width, u32 height)
{
    SurfaceConsumer* con; SurfaceSlot* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    con->not_yet_fired_width = width;
    con->not_yet_fired_height = height;
    return 1;
}

u64 surface_consumer_fire(u64 pid, u64 consumer_slot)
{
    SurfaceConsumer* con; SurfaceSlot* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    s->width = con->not_yet_fired_width;
    s->height = con->not_yet_fired_height;
    Process* process = KERNEL_PROCESS_ARRAY[con->surface_pid];
    rwlock_acquire_write(&process->process_lock);
    surface_slot_fire(process, con->surface_slot);
    rwlock_release_write(&process->process_lock);
    return 1;
}

/*
If fb_location is not null and page_count is zero the function returns *surface_has_commited* status.
If not the previous option the function will return 0 if *surface_has_commited* is false.
Otherwise the function will return the page count required for a fetch if fb_location and page_count
are null.
If the surface has commited and you pass a page aligned fb_location and a big enough page_count
a successful consumer fetch is performed.

expects a write lock on pid process_lock
*/
u64 surface_consumer_fetch(u64 pid, u64 consumer_slot, Framebuffer* fb_location, u64 page_count)
{
    Process* process = KERNEL_PROCESS_ARRAY[pid];
    SurfaceConsumer* con; SurfaceSlot* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    if(fb_location && page_count == 0)
    { return s->has_commited; }

    if(!s->has_commited)
    { return 0; }

    if(page_count == 0 && !fb_location)
    { return s->fb_present->alloc.page_count; }

    if(((u64)fb_location % PAGE_SIZE) != 0 || page_count < s->fb_present->alloc.page_count)
    { return 0; }

    if(con->has_fetched)
    {
        Kallocation a = process_shrink_allocation(process, con->vaddr, 0);
        con->has_fetched = 0;
        *con->fb_fetched = con->fb_fetched_control;
    }
    Process* process2 = KERNEL_PROCESS_ARRAY[con->surface_pid];
    rwlock_release_write(&process->process_lock);
    rwlock_acquire_write(&process2->process_lock);
    con->fb_fetched = surface_slot_swap_present_buffer(
        process2,
        con->surface_slot,
        con->fb_fetched
    );
    rwlock_release_write(&process2->process_lock);
    rwlock_acquire_write(&process->process_lock);
    con->fb_fetched_control = *con->fb_fetched;
    s->has_commited = 0;
    if(process_alloc_pages(process, fb_location, con->fb_fetched->alloc))
    {
        con->has_fetched = 1;
        con->vaddr = fb_location;
        return 1;
    }
    return 0;
}

typedef struct
{
    u64 pid;
    u8 is_active;
} VirtualOutput;

#define VO_COUNT 4
VirtualOutput vos[VO_COUNT];
u64 current_vo = 0;
