

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
u64 create_process_from_file(u64 file_id, u64* pid_ret, u64* parents, u64 parent_count)
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

    u64 pid = process_create(parents, parent_count);
    Process* process = KERNEL_PROCESS_ARRAY[pid];
#define proc process

    for(u64 i = 0; i < phnum; i++)
    {
        ELF_ProgramHeader* ph = ph_tab + i;

        if(ph->seg_type != ELF_PH_SEG_TYPE_LOAD || ph->memsz == 0)
        { continue; }

        u64 bits = 0;
        if((ph->flags & ELF_PROG_READ) != 0)  { bits |= 2; }
        if((ph->flags & ELF_PROG_WRITE) != 0) { bits |= 4; }
        if((ph->flags & ELF_PROG_EXECUTE) !=0){ bits |= 8; }

        u64 virtual_start = ph->paddr & (~0xfff);
        u64 virtual_paddr = ph->paddr - virtual_start;
        u64 virtual_size =  ph->paddr + ph->memsz - virtual_start;

        Kallocation section_alloc = kalloc_pages((virtual_size + PAGE_SIZE - 1) / PAGE_SIZE);
        memset(section_alloc.memory, 0, section_alloc.page_count * PAGE_SIZE);
        memcpy(section_alloc.memory + virtual_paddr, ((u64)header) + ph->off, ph->filesz);
        mmu_map_kallocation(proc->mmu_table, section_alloc, virtual_start, bits);

        if((process->allocations_count + 1) * sizeof(Kallocation) > process->allocations_alloc.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc = kalloc_pages(process->allocations_alloc.page_count + 1);
            Kallocation* new_array = new_alloc.memory;
            Kallocation* old_array = process->allocations_alloc.memory;
            for(u64 i = 0; process->allocations_count; i++)
            {
                new_array[i] = old_array[i];
            }
            if(process->allocations_alloc.page_count)
            {
                kfree_pages(process->allocations_alloc);
            }
            process->allocations_alloc = new_alloc;
        }

        ((Kallocation*)process->allocations_alloc.memory)[process->allocations_count] = section_alloc;
        process->allocations_count++;
    }

    u32 thread1 = process_thread_create(pid, 1, 0);
    process = KERNEL_PROCESS_ARRAY[pid];
    proc->threads[thread1].stack_alloc = kalloc_pages(8);
    proc->threads[thread1].frame.regs[8] = (~(0x1ffffff << 39)) & (~0xfff); // frame pointer
    proc->threads[thread1].frame.regs[2] = proc->threads[thread1].frame.regs[8] - 4 * sizeof(u64);
    u64 stack_start =
        proc->threads[thread1].frame.regs[8] - (PAGE_SIZE * proc->threads[thread1].stack_alloc.page_count);
    proc->threads[thread1].frame.regs[8] = proc->threads[thread1].frame.regs[8] - 2 * sizeof(u64);
    mmu_map_kallocation(
        proc->mmu_table,
        proc->threads[thread1].stack_alloc,
        stack_start,
        2 + 4
    );
    { // Mark end of stack for stacktrace
        u64* frame = (u64)proc->threads[thread1].stack_alloc.memory +
                        proc->threads[thread1].stack_alloc.page_count * PAGE_SIZE;
        *(frame-2) = 0;
    }

    mmu_kernel_map_range(proc->mmu_table, 0x10000000, 0x10000000, 2 + 4); // UART

    proc->threads[thread1].program_counter = header->entry_addr;

    *pid_ret = pid;
    kfree_pages(elf_alloc);
    return 1;
}
