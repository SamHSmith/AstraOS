
void write_blocks(u64 block, u64 count, void* memory_start)
{
    u64 pair[2];
    for(u64 i = 0; i < count; i++)
    {
        pair[0] = block+i;
        pair[1] = memory_start + i*PAGE_SIZE;
        oak_send_block_fetch(1, pair, 1);
    }
}

void read_blocks(u64 block, u64 count, void* memory_start)
{
    u64 pair[2];
    for(u64 i = 0; i < count; i++)
    {
        pair[0] = block+i;
        pair[1] = memory_start + i*PAGE_SIZE;
        oak_send_block_fetch(0, pair, 1);
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

#define TABLE_COUNT 8

