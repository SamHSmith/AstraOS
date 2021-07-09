
#define STREAM_PAGE_COUNT 1
#define STREAM_SIZE (PAGE_SIZE*STREAM_PAGE_COUNT - (sizeof(Kallocation) + sizeof(u32)*2 + sizeof(Spinlock)))
typedef struct
{
    Kallocation alloc;
    u32 put_index;
    u32 get_index;
    Spinlock lock;
    u8 buffer[STREAM_SIZE];
} Stream;

Stream* stream_create()
{
    Kallocation alloc = kalloc_pages(STREAM_PAGE_COUNT);
    Stream* stream = alloc.memory;
    stream->alloc = alloc;
    stream->put_index = 0;
    stream->get_index = 0;
    spinlock_create(&stream->lock);
    return stream;
}

// returns the amount of bytes that got pushed into the stream
u64 stream_put(Stream* stream, u8* memory, u64 count)
{
    spinlock_acquire(&stream->lock);
    u64 written_count = 0;
    for(u64 i = 0; i < count; i++)
    {
        u64 new_put = (stream->put_index+1) % STREAM_SIZE;
        if(new_put == stream->get_index)
        { break; }
        stream->buffer[stream->put_index] = memory[i];
        written_count++;
        stream->put_index = new_put;
    }
    spinlock_release(&stream->lock);
    return written_count;
}

// returns the amount of bytes taken out of the stream and put
// in the buffer. Always less then or equal to buffer_size.
// can be called with NULL buffer, 0 buffer_size and a valid bytes_in_stream
// pointer to get the amount of bytes in the stream after the operation has
// completed.
u64 stream_take(Stream* stream, u8* buffer, u64 buffer_size, u64* bytes_in_stream)
{
    spinlock_acquire(&stream->lock);
    u64 taken_count = 0;
    for(u64 i = 0; i < buffer_size; i++)
    {
        if(stream->get_index == stream->put_index)
        { break; }
        buffer[i] = stream->buffer[stream->get_index];
        taken_count++;
        stream->get_index = (stream->get_index+1) % STREAM_SIZE;
    }
    u64 real_put_index = stream->put_index;
    if(stream->get_index > real_put_index)
    { real_put_index += STREAM_SIZE; }
    u64 byte_count = real_put_index - stream->get_index;
    *bytes_in_stream = byte_count;

    spinlock_release(&stream->lock);
    return taken_count;
}
