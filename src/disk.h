
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


/*
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
            ELF_Header* header = block;

            if(header->magic != ELF_MAGIC)
            { printf("NOT ELF\n"); continue; }
            if(header->machine != ELF_MACHINE_RISCV)
            { printf("WRONG ARCH\n"); continue; }
            if(header->obj_type != ELF_TYPE_EXEC)
            { printf("NOT AN EXECTUTABLE ELF\n"); continue; }

            Kallocation elf_alloc = kalloc_pages(next_partition_start - table->entries[i].start_block);
            header = elf_alloc.memory;
            read_blocks(table->entries[i].start_block, next_partition_start - table->entries[i].start_block, header);

            ELF_ProgramHeader* ph_tab = ((u8*)header) + header->phoff;
            u64 phnum = header->phnum;

            if(phnum == 0) { continue; }

            u64 pid = process_create();
#define proc KERNEL_PROCESS_ARRAY[pid]

            for(u64 i = 0; i < phnum; i++)
            {
                ELF_ProgramHeader* ph = ph_tab + i;

                if(ph->seg_type != ELF_PH_SEG_TYPE_LOAD || ph->memsz == 0)
                { continue; }

                u64 bits = 0;
                if(ph->flags & ELF_PROG_READ != 0)  { bits |= 2; }
                if(ph->flags & ELF_PROG_WRITE != 0) { bits |= 4; }
                if(ph->flags & ELF_PROG_EXECUTE !=0){ bits |= 8; }

                Kallocation section_alloc = kalloc_pages((ph->memsz + PAGE_SIZE) / PAGE_SIZE);
                memcpy(section_alloc.memory, ((u64)header) + ph->off, ph->memsz);
                mmu_map_kallocation(proc->mmu_table, section_alloc, ph->vaddr, bits);
            }

            u32 thread1 = process_thread_create(pid);
            proc->threads[thread1].stack_alloc = kalloc_pages(8);
            proc->threads[thread1].frame.regs[2] = U64_MAX & (~0xfff);
            u64 stack_start =
                proc->threads[thread1].frame.regs[2] - (PAGE_SIZE * proc->threads[thread1].stack_alloc.page_count);
            mmu_map_kallocation(
                proc->mmu_table,
                proc->threads[thread1].stack_alloc,
                stack_start,
                2 + 4
            );

            mmu_kernel_map_range(proc->mmu_table, 0x10000000, 0x10000000, 2 + 4); // UART

            proc->threads[thread1].program_counter = header->entry_addr;
            proc->threads[thread1].thread_state = THREAD_STATE_RUNNING;
        }
    }
}
*/
