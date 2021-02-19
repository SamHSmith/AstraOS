

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
    Framebuffer* fb_present;
    Framebuffer* fb_draw;
    u8 has_commited;
} Surface;

Surface surface_create()
{
    Surface s = {0};
    s.fb_present = framebuffer_create(0, 0);
    s.fb_draw = framebuffer_create(0, 0);
    return s;
}

u8 surface_has_commited(Surface s)
{
    return s.has_commited && s.fb_present->width == s.width && s.fb_present->height == s.height;
}

void surface_commit(Surface* s)
{
    Framebuffer* temp = s->fb_present;
    s->fb_present = s->fb_draw;

    if(temp->width != s->width || temp->height != s->height)
    {
        kfree_pages(temp->alloc);
        temp = framebuffer_create(s->width, s->height);
    }
    s->fb_draw = temp;
    s->has_commited = 1;
}

    
