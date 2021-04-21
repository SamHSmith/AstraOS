
typedef struct
{
    u64 device;
    u64 size;
    u8 data[];
} OakPacket;

void send_oak_packet(volatile OakPacket* packet)
{
    assert(packet->size <= 256, "oakpacket within size limit");
    u8 scratch[256];
    for(u64 i = 0; i < packet->size; i++)
    {
        scratch[i] = ((u8*)packet)[i];
    }
    volatile u8* viewer = 0x10000100;
    u64 data_send[2];
    data_send[0] = packet->size;
    data_send[1] = scratch;
    for(u64 i = 0; i < 8*2; i++)
    { *viewer = *(((u8*)data_send) + i); }
}

typedef struct
{
    OakPacket base;
    u64 frame_ptr;
    u64 frame_size;
} OakPacketVideo;

void oak_send_video(Framebuffer* framebuffer)
{
    OakPacketVideo packet;
    packet.base.size = sizeof(packet);
    packet.base.device = 9;
    packet.frame_ptr = framebuffer->data;
    packet.frame_size = framebuffer->width * framebuffer->height * 4 * 4;
    send_oak_packet(&packet);
}

typedef struct
{
    OakPacket base;
    u8 scancode;
    u8 was_pressed;
} OakPacketKeyboard;

void oak_recieve_keyboard(OakPacketKeyboard* packet)
{
    KeyboardEventQueue* kbd_event_queue =
        &KERNEL_PROCCESS_ARRAY[vos[current_vo].pid]->kbd_event_queue;
    if(packet->was_pressed)
    {
        keyboard_put_new_event(kbd_event_queue, KEYBOARD_EVENT_PRESSED, packet->scancode);
    }
    else
    {
        keyboard_put_new_event(kbd_event_queue, KEYBOARD_EVENT_RELEASED, packet->scancode);
    }
}

typedef struct
{
    OakPacket base;
    s32 delta_x;
    s32 delta_y;
} OakPacketMouse;

void oak_recieve_mouse(OakPacketMouse* packet)
{
    RawMouse* mouse = &KERNEL_PROCCESS_ARRAY[vos[current_vo].pid]->mouse;
    new_mouse_input_delta(mouse, packet->delta_x, packet->delta_y);
}

typedef struct
{
    OakPacket base;
    u32 width;
    u32 height;
} OakPacketVideoOutRequest;

void oak_recieve_video_out_request(OakPacketVideoOutRequest* packet)
{
    Surface* surface = &((SurfaceSlot*)KERNEL_PROCCESS_ARRAY[vos[current_vo].pid]
                       ->surface_alloc.memory)->surface;

    surface->width = packet->width;
    surface->height = packet->height;

    if(framebuffer == 0)
    { framebuffer = framebuffer_create(packet->width, packet->height); }
    else if(framebuffer->width != packet->width || framebuffer->height != packet->height)
    {
        kfree_pages(framebuffer->alloc);
        framebuffer = framebuffer_create(packet->width, packet->height);
    }
    frame_has_been_requested = 1;
}

typedef struct
{
    OakPacket base;
    u64 transaction_number;
    u8 write;
    u64 block_address_pairs[];
} OakPacketBlockFetch;
 
typedef struct
{
    OakPacket base;
    u64 transaction_number;
} OakPacketBlockFetchComplete;

void oak_send_block_fetch(u8 write, u64* block_address_pairs, u64 pair_count)
{
    assert(sizeof(OakPacketBlockFetch) + pair_count*2*sizeof(u64) <= 256, "block fetch isn't too big");
    u8 scratch[sizeof(OakPacketBlockFetch) + pair_count*2];
    OakPacketBlockFetch* packet = scratch;
    packet->base.size = sizeof(*packet) + pair_count*2*sizeof(u64);
    packet->base.device = 8;

    packet->transaction_number = 0; // temp
    packet->write = write;
    for(u64 i = 0; i < pair_count*2; i++)
    {
        packet->block_address_pairs[i] = block_address_pairs[i];
    }
    send_oak_packet(packet);
}

void oak_recieve_block_fetch_complete(OakPacketBlockFetchComplete* packet)
{
    printf("the block fetch #%lu is complete\n", packet->transaction_number);
}

void recieve_oak_packet()
{
    volatile u8* viewer_read = 0x10000101;
    u8 scratch[256];
    u64 data_send[2];
    data_send[0] = 256;
    data_send[1] = scratch;
    for(u64 i = 0; i < 8*2; i++)
    { *viewer_read = *(((u8*)data_send) + i); }

    OakPacket* packet = scratch;
    if(packet->device == 8)
    {
        oak_recieve_block_fetch_complete(packet);
    }
    else if(packet->device == 9)
    {
        oak_recieve_video_out_request(packet);
    }
    else if(packet->device == 10)
    {
        oak_recieve_keyboard(packet);
    }
    else if(packet->device == 11)
    {
        oak_recieve_mouse(packet);
    }
}
