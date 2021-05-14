

typedef struct
{
    u32 magic;
    u8 bitsize;
    u8 endian;
    u8 ident_abi_version;
    u8 target_platform;
    u8 abi_version;
    u8 padding[7];
    u16 obj_type;
    u16 machine;
    u32 version;
    u64 entry_addr;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shtrndx;
} ELF_Header;

#define ELF_MAGIC 0x464c457f
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_RISCV 0xf3

#define ELF_PH_SEG_TYPE_LOAD 1

#define ELF_PROG_READ 4
#define ELF_PROG_WRITE 2
#define ELF_PROG_EXECUTE 1


typedef struct
{
    u32 seg_type;
    u32 flags;
    u64 off;
    u64 vaddr;
    u64 paddr;
    u64 filesz;
    u64 memsz;
    u64 align;
} ELF_ProgramHeader;

// returns true on success
u64 create_proccess_from_file(u64 file_id, u64* pid_ret)
{
    if(!is_valid_file_id(file_id)) { return 0; }

    ELF_Header* header;
    {
        u8 block[PAGE_SIZE];
        u64 pair[2];
        pair[0] = 0;
        pair[1] = block;
        if(!kernel_file_read_blocks(file_id, pair, 1)) { return 0; }
        header = block;

        if(header->magic != ELF_MAGIC)
        { printf("NOT ELF\n"); return 0; }
        if(header->machine != ELF_MACHINE_RISCV)
        { printf("WRONG ARCH\n"); return 0; }
        if(header->obj_type != ELF_TYPE_EXEC)
        { printf("NOT AN EXECTUTABLE ELF\n"); return 0; }
    }

    u64 block_count = kernel_file_get_block_count(file_id);
    Kallocation elf_alloc = kalloc_pages(block_count);
    header = elf_alloc.memory;

    u64 pairs[block_count*2];
    for(u64 i = 0; i < block_count; i++)
    { pairs[i*2] = i; pairs[i*2 + 1] = ((u8*)header) + PAGE_SIZE * i; }
    if(!kernel_file_read_blocks(file_id, pairs, block_count)) { kfree_pages(elf_alloc); return 0; }

    ELF_ProgramHeader* ph_tab = ((u8*)header) + header->phoff;
    u64 phnum = header->phnum;

    if(phnum == 0) { kfree_pages(elf_alloc); return 0; }

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

    *pid_ret = pid;
    return 1;
}