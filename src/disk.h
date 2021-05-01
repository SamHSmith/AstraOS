
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

void load_elf_from_partition()
{
    assert(sizeof(RAD_PartitionTable) == PAGE_SIZE*2, "partition table struct is the right size");

    Kallocation table_alloc = kalloc_pages(2);
    RAD_PartitionTable* table = table_alloc.memory;
    assert(table != 0, "table alloc failed");

    u64 reference_table = U64_MAX;
    for(s64 i = TABLE_COUNT-1; i >= 0; i--)
    {
        read_blocks(i*2, 2, table);

        u8 hash[64];
        assert(sha512Compute(((u8*)table) + 64, sizeof(RAD_PartitionTable) -64, hash) == 0, "sha stuff");

        u8 is_valid = 1;
        for(u64 i = 0; i < 64; i++) { if(hash[i] != table->sha512sum[i]) { is_valid = 0; } }

        if(!is_valid)
        {
            printf("Table#%ld is not valid\n", i);
        }
        else
        {
            printf("Table#%ld is valid \n", i);
            reference_table = i;
        }
    }
    if(reference_table == U64_MAX)
    {
        printf("There are no valid tables. Either the drive is not formatted or you are in a very unfortunate situation.\n");
        return;
    }

    read_blocks(reference_table*2, 2, table);
    printf("Using table#%ld as reference\n", reference_table);
u8 block[PAGE_SIZE];
    for(u64 i = 0; i < 63; i++)
    {
        u64 next_partition_start = U64_MAX; // drive_block_count; do this later
        if(i < 62) { next_partition_start = table->entries[i+1].start_block; }

        if(table->entries[i].partition_type != 0)
        {
            printf("Partition, type = %u, start = %llu, size = %llu, name = %s\n",
                    table->entries[i].partition_type,
                    table->entries[i].start_block,
                    next_partition_start - table->entries[i].start_block,
                    table->entries[i].name
            );

            read_blocks(table->entries[i].start_block, 1, block);
            block[PAGE_SIZE-1] = 0;
            printf("%s", block);
            printf("\n");
        }
    }
}
