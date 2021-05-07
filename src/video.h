

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
    u32 width;
    u32 height;
    volatile Framebuffer* fb_present;
    volatile Framebuffer* fb_draw;
    u8 has_commited;

    u64 consumer_pid;
    u64 consumer_slot;
    u8 has_consumer;
    u8 is_initialized;
} Surface;

typedef struct
{
    u64 vaddr;
    u8 has_aquired;
    Framebuffer fb_draw_control;
    Surface surface;
} SurfaceSlot;

typedef struct
{
    u64 surface_pid;
    u64 surface_slot;
    u8 has_surface;
    u8 is_initialized;

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
        if(!s->surface.is_initialized)
        {
            s->surface.width = 0;
            s->surface.height = 0;
            s->surface.fb_present = framebuffer_create(0, 0);
            s->surface.fb_draw = framebuffer_create(0, 0);
            s->surface.is_initialized = 1;
            s->surface.has_consumer = 0;
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

    s->surface.width = 0;
    s->surface.height = 0;
    s->surface.fb_present = framebuffer_create(0, 0);
    s->surface.fb_draw = framebuffer_create(0, 0);
    s->surface.is_initialized = 1;
    s->surface.has_consumer = 0;
    s->has_aquired = 0;
    return p->surface_count - 1;
}

u64 surface_consumer_create(u64 pid, u64 surface_pid, u64* consumer)
{
    if(pid == surface_pid) { return 0; }
    Process* p = KERNEL_PROCESS_ARRAY[pid];
    if(!(surface_pid < KERNEL_PROCESS_ARRAY_LEN && KERNEL_PROCESS_ARRAY[surface_pid]->mmu_table))
    { return 0; }
    u64 surface_slot = surface_create(KERNEL_PROCESS_ARRAY[surface_pid]);

    for(u64 i = 0; i < p->surface_consumer_count; i++)
    {
        SurfaceConsumer* con = ((SurfaceConsumer*)p->surface_consumer_alloc.memory) + i;
        if(!con->is_initialized)
        {
            con->surface_pid = surface_pid;
            con->surface_slot = surface_slot;
            con->has_surface = 1;
            con->is_initialized = 1;
            con->has_fetched = 0;
            con->fb_fetched = framebuffer_create(0,0);
            Surface* surface = &(((SurfaceSlot*)KERNEL_PROCESS_ARRAY[surface_pid]->surface_alloc.memory)
                                + surface_slot)->surface;
            surface->consumer_pid = pid;
            surface->consumer_slot = i;
            surface->has_consumer = 1;
            *consumer = i;
            return 1;
        }
    }

    if((p->surface_consumer_count+1)*sizeof(SurfaceConsumer) >p->surface_consumer_alloc.page_count*PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(p->surface_consumer_alloc.page_count + 1);
        SurfaceConsumer* new_array = (SurfaceConsumer*)new_alloc.memory;
        for(u64 i = 0; i < p->surface_consumer_count; i++)
        {
            new_array[i] = ((SurfaceConsumer*)p->surface_consumer_alloc.memory)[i];
        }
        if(p->surface_consumer_alloc.page_count != 0)
        {
            kfree_pages(p->surface_consumer_alloc);
        }
        p->surface_consumer_alloc = new_alloc;
    }
    u64 cs = p->surface_consumer_count;
    p->surface_consumer_count += 1;
    SurfaceConsumer* con = ((SurfaceConsumer*)p->surface_consumer_alloc.memory) + cs;
    con->surface_pid = surface_pid;
    con->surface_slot = surface_slot;
    con->has_surface = 1;
    con->is_initialized = 1;
    con->has_fetched = 0;
    con->fb_fetched = framebuffer_create(0,0);
    Surface* surface = &(((SurfaceSlot*)KERNEL_PROCESS_ARRAY[surface_pid]->surface_alloc.memory)
                        + surface_slot)->surface;
    surface->consumer_pid = pid;
    surface->consumer_slot = cs;
    surface->has_consumer = 1;
    *consumer = cs;
    return 1;
}

u64 surface_has_commited(Surface s)
{
    return s.width == 0 || s.height == 0 ||
        (s.has_commited && s.fb_present->width == s.width && s.fb_present->height == s.height);
}

void surface_prepare_draw_framebuffer(u64 surface_slot, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    Framebuffer* draw = s->surface.fb_draw;

    if(draw->width != s->surface.width || draw->height != s->surface.height)
    {
        kfree_pages(draw->alloc);
        draw = framebuffer_create(s->surface.width, s->surface.height);
    }
    s->surface.fb_draw = draw;
}

u64 surface_acquire(u64 surface_slot, Framebuffer* fb_location, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    if(surface_slot >= process->surface_count || surface_has_commited(s->surface)) { return 0; }

    s->fb_draw_control = *s->surface.fb_draw;
    if(process_alloc_pages(process, fb_location, s->surface.fb_draw->alloc))
    {
        s->has_aquired = 1;
        s->vaddr = fb_location;
        return 1;
    }
    return 0;
}

u64 surface_commit(u64 surface_slot, Process* process)
{
    SurfaceSlot* s = ((SurfaceSlot*)process->surface_alloc.memory) + surface_slot;

    if(surface_slot >= process->surface_count || !s->has_aquired) { return 0; }

    Kallocation a = process_shrink_allocation(process, s->vaddr, 0);
    if(a.memory == 0) { return 0; }

    *s->surface.fb_draw = s->fb_draw_control;
    s->has_aquired = 0;

    volatile Framebuffer* temp = s->surface.fb_present;
    s->surface.fb_present = s->surface.fb_draw;

    s->surface.fb_draw = temp;
    s->surface.has_commited = 1;
    return 1;
}

u64 get_consumer_and_surface(u64 pid, u64 consumer_slot, SurfaceConsumer** c, Surface** surface)
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
    Surface* s = &(((SurfaceSlot*)KERNEL_PROCESS_ARRAY[con->surface_pid]->surface_alloc.memory)
                    + con->surface_slot)->surface;
    if(!s->is_initialized || !s->has_consumer || s->consumer_pid != pid || s->consumer_slot != consumer_slot)
    { return 0; }

    *c = con;
    *surface = s;
    return 1;
}

u64 surface_consumer_has_commited(u64 pid, u64 consumer_slot)
{
    SurfaceConsumer* con; Surface* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    return surface_has_commited(*s);
}

u64 surface_consumer_get_size(u64 pid, u64 consumer_slot, u32* width, u32* height)
{
    SurfaceConsumer* con; Surface* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    *width = s->width;
    *height = s->height;
    return 1;
}

u64 surface_consumer_set_size(u64 pid, u64 consumer_slot, u32 width, u32 height)
{
    SurfaceConsumer* con; Surface* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    s->width = width;
    s->height = height;
    return 1;
}

u64 surface_consumer_fetch(u64 pid, u64 consumer_slot, Framebuffer* fb_location, u64 page_count)
{
    Process* process = KERNEL_PROCESS_ARRAY[pid];
    SurfaceConsumer* con; Surface* s;
    if(!get_consumer_and_surface(pid, consumer_slot, &con, &s))
    { return 0; }

    if(!s->has_commited)
    { return 0; }

    if(page_count == 0)
    { return s->fb_present->alloc.page_count; }

    if(((u64)fb_location % PAGE_SIZE) != 0 || page_count < s->fb_present->alloc.page_count)
    { return 0; }

    if(con->has_fetched)
    {
        Kallocation a = process_shrink_allocation(process, con->vaddr, 0);
        con->has_fetched = 0;
        *con->fb_fetched = con->fb_fetched_control;
    }
    Framebuffer* temp = s->fb_present;
    s->fb_present = con->fb_fetched;
    con->fb_fetched = temp;
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
