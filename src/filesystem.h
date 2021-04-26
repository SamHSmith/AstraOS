
void write_blocks(u64 block, u64 count, void* memory_start)
{
    for(u64 i = 0; i < count; i++)
    {
        oak_send_block_fetch(1, memory_start + i*PAGE_SIZE, i+block);
    }
}

void read_blocks(u64 block, u64 count, void* memory_start)
{
    for(u64 i = 0; i < count; i++)
    {
        oak_send_block_fetch(0, memory_start + i*PAGE_SIZE, i+block);
    }
}

typedef struct
{
    u8 uid[64];
    u64 start_block;
    u16 partition_type;
    u8 name[54];
} RAD_PartitionTableEntry;

typedef struct
{
    u8 sha512sum[64];
    u8 padding[64];
    RAD_PartitionTableEntry entries[63];
} RAD_PartitionTable;

