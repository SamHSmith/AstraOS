
typedef struct
{
    u64 destination;
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
    packet.base.destination = 0;
    packet.frame_ptr = framebuffer->data;
    packet.frame_size = framebuffer->width * framebuffer->height * 4 * 4;
    send_oak_packet(&packet);
}
