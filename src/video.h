

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
    u8 is_initialized;
} Surface;

typedef struct
{
    u64 vaddr;
    u8 has_aquired;
    Surface surface;
} SurfaceSlot;

u64 surface_create(Proccess* p)
{
    for(u64 i = 0; i < p->surface_count; i++)
    {
        SurfaceSlot* s = ((SurfaceSlot*)p->surface_alloc.memory) + i;
        if(!s->surface.is_initialized)
        {
            s->surface.fb_present = framebuffer_create(0, 0);
            s->surface.fb_draw = framebuffer_create(0, 0);
            s->surface.is_initialized = 1;
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

    s->surface.fb_present = framebuffer_create(0, 0);
    s->surface.fb_draw = framebuffer_create(0, 0);
    s->surface.is_initialized = 1;
    s->has_aquired = 0;
    return p->surface_count - 1;
}

u64 surface_has_commited(Surface s)
{
    return s.has_commited && s.fb_present->width == s.width && s.fb_present->height == s.height;
}

u64 surface_acquire(u64 surface_slot, Framebuffer* fb_location, Proccess* proccess)
{
    SurfaceSlot* s = ((SurfaceSlot*)proccess->surface_alloc.memory) + surface_slot;

    if(surface_slot >= proccess->surface_count || surface_has_commited(s->surface)) { return 0; }

    if(proccess_alloc_pages(proccess, fb_location, s->surface.fb_draw->alloc))
    {
        s->has_aquired = 1;
        s->vaddr = fb_location;
        return 1;
    }
    return 0;
}

u64 surface_commit(u64 surface_slot, Proccess* proccess)
{
    SurfaceSlot* s = ((SurfaceSlot*)proccess->surface_alloc.memory) + surface_slot;

    if(surface_slot >= proccess->surface_count || !s->has_aquired) { return 0; }

    Kallocation a = proccess_shrink_allocation(proccess, s->vaddr, 0);
    if(a.memory == 0) { return 0; }

    s->surface.fb_draw->alloc = a; // in case the data was tampered with
    s->has_aquired = 0;

    volatile Framebuffer* temp = s->surface.fb_present;
    s->surface.fb_present = s->surface.fb_draw;

    if(temp->width != s->surface.width || temp->height != s->surface.height)
    {
        kfree_pages(temp->alloc);
        temp = framebuffer_create(s->surface.width, s->surface.height);
    }
    s->surface.fb_draw = temp;
    s->surface.has_commited = 1;
    return 1;
}

typedef struct
{
    u64 pid;
    u8 is_active;
} VirtualOutput;

#define VO_COUNT 4
VirtualOutput vos[VO_COUNT];
u64 current_vo = 0;
