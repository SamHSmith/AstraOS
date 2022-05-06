
extern u64 HEAP_START;
extern u64 HEAP_SIZE;
extern u64 TEXT_START;
extern u64 TEXT_END;
extern u64 DATA_START;
extern u64 DATA_END;
extern u64 RODATA_START;
extern u64 RODATA_END;
extern u64 BSS_START;
extern u64 BSS_END;
extern u64 KERNEL_STACK_START;
extern u64 KERNEL_STACK_END;
 
#define K_HEAP_START (*(((volatile u64*)HEAP_START) + 0))
#define K_PAGE_COUNT (*(((volatile u64*)HEAP_START) + 1))
#define K_TABLE_COUNT (*(((volatile u64*)HEAP_START) + 2))

struct KmemTable
{
    u64 table_len;
    u8 data[];
};
#define K_MEMTABLES ((volatile struct KmemTable**)((u64*)HEAP_START + 3))

#define PAGE_SIZE 4096
#define ALLOCATION_SPLIT_COUNT 3

void mem_table_set_taken(u64 start, u64 count, u8 taken)
{
    if(count == 0) { return; }
    count -= 1;
    for(s64 k = K_TABLE_COUNT - 1; k >= 0; k--)
    {
        struct KmemTable* last_table = K_MEMTABLES[k+1]; // Be careful with this one
        struct KmemTable* table = K_MEMTABLES[k];
        u64 start_byte = start >> 3;
        u64 end_byte = (start+count) >> 3;
    
        u64 start_bit = start & 0b111;
        u64 end_bit = (start+count) & 0b111;

        u64 i = start_byte;
        u64 j = start_bit;
        while(i <= end_byte)
        {
            u64 e_b = 7;
            if(i == end_byte) { e_b = end_bit; }
            while(j <= e_b)
            {
                if(k == K_TABLE_COUNT - 1)
                {
                    if(taken)
                    {
                        table->data[i] |= 1 << j;
                    }
                    else
                    {
                        table->data[i] &= ~(1 << j);
                    }
                }
                else
                {
                    table->data[i] &= ~(1 << j);

                    u64 current = (i << 3) | j;
                    current = current << 1;

                    u64 yte = current >> 3;
                    u64 it = current & 0b111;
                    table->data[i] |= ((last_table->data[yte] & (1 << it)) != 0) << j;

                    current++;
                    yte = current >> 3;
                    it = current & 0b111;
                    table->data[i] |= ((last_table->data[yte] & (1 << it)) != 0) << j;
                }
                j++;
            }
            j = 0;
            i++;
        }
        {
            u64 e = start + count;
            start = start >> 1;
            e = e >> 1;
            count = e - start;
        }
    }
}

typedef struct Kallocation
{
    void* memory;
    u64 page_count; // DO NOT TOUCH
} Kallocation;

// if the allocation fails Kallocation.memory will be zero.
Kallocation kalloc_pages(u64 page_count)
{
    spinlock_acquire(&KERNEL_MEMORY_SPINLOCK); // TODO maybe move this down so we don't lock local work?
    if(page_count == 0)
    {
        spinlock_release(&KERNEL_MEMORY_SPINLOCK);
        Kallocation al = {0};
        return al;
    }
    s64 a_size = 0;
    for(u64 i = 0; i < 64; i++)
    {
        u64 temp = (((u64)1) << (63 - i));
        if((page_count & temp) != 0)
        {
            if(temp < page_count) { i--; } // go up to the next power of 2
            a_size = K_TABLE_COUNT -1 - (63 - i);
            break;
        }
    }

    if(a_size < 0)
    {
        spinlock_release(&KERNEL_MEMORY_SPINLOCK);
        Kallocation al = {0};
        return al;
    }

    u64 allocation_splits = 0;
    for(u64 i = 0; i < ALLOCATION_SPLIT_COUNT; i++)
    {
        if((K_TABLE_COUNT - a_size) > 1) { a_size += 1; allocation_splits++; }
    }
    struct KmemTable* table = K_MEMTABLES[a_size];
    u64 local_size_shift = K_TABLE_COUNT -1 - a_size;

    u64 page_address = 0;

    u64 last_count = 0;
    u64 last_i = 0; u8 last_j = 0;
    for(u64 i = 0; i < table->table_len; i++)
    {
        if(page_address != 0) { break; }
        for(u8 j = 0; j < 8; j++)
        {
            if(page_address != 0) { break; }
            if(allocation_splits == 0)
            {
                last_i = i; last_j = j;
            }

            if((table->data[i] & (1 << j)) == 0) // Page free
            {
                if(((last_count + 1) << local_size_shift) >= page_count) // last is also free
                {
                    page_address = (last_i * 8 + last_j) << local_size_shift;
                    mem_table_set_taken(page_address, page_count, 1);
                }
                else
                {
                    last_count++;
                    if(last_count == 1)
                    { last_i = i; last_j = j; }
                }
            }
            else // Page is not free
            {
                last_count = 0;
            }
        }
    }
    Kallocation al = {0};
    // HEAP_START isn't always 4096 aligned so the first page will be smaller in some cases.
    al.memory = (void*)((HEAP_START - (HEAP_START % PAGE_SIZE)) + (page_address * PAGE_SIZE));
    al.page_count = page_count;
    spinlock_release(&KERNEL_MEMORY_SPINLOCK);
    return al;
}

void kfree_pages(Kallocation a)
{
    if(!a.page_count)
    { return; }
    u64 addr = ((u64)a.memory);
    if(addr <= HEAP_START) { return; }
    addr -= HEAP_START;
    if((addr % PAGE_SIZE) != 0) { return; } //TODO: some kind of error maybe
    addr /= PAGE_SIZE;

    spinlock_acquire(&KERNEL_MEMORY_SPINLOCK);
    mem_table_set_taken(addr, a.page_count, 0);
    spinlock_release(&KERNEL_MEMORY_SPINLOCK);
}

void* kalloc_single_page()
{
    Kallocation k = kalloc_pages(1);
    return k.memory;
}

void mem_debug_dump_table_counts(u64 table_count)
{
    if(table_count > K_TABLE_COUNT) { table_count = K_TABLE_COUNT; }
    for(u64 b = K_TABLE_COUNT - table_count; b < K_TABLE_COUNT; b++)
    {
        u64 count = 0;
        u64 total = 0;
        for(u64 i = 0; i < K_MEMTABLES[b]->table_len; i++)
        {
            for(u64 j = 0; j < 8; j++)
            {
                if((K_MEMTABLES[b]->data[i] & (1 << j)) != 0)
                { count++; }
                total++;
            }
        }
        uart_printf("Memtable#%lld %lld/%lld used.\n", b, count, total);
    }
}

void kfree_single_page(void* page)
{
    Kallocation k = {0};
    k.memory = page;
    k.page_count = 1;
    kfree_pages(k);
}

u64 mmu_is_entry_valid(u64 entry)
{
    return (entry & 1) != 0;
}
 
u64 mmu_is_entry_leaf(u64 entry)
{
    return (entry & 0xe) != 0;
}

u64* create_mmu_table()
{
    u64* page = (u64)kalloc_single_page();
    for(u64 i = 0; i < 512; i++) { page[i] = 0; }
    return page;
}

void mmu_map(u64* root, u64 vaddr, u64 paddr, u64 bits, s64 level)
{

    u64 vpn[3];
    vpn[0] = (vaddr >> 12) & 0x1ff;
    vpn[1] = (vaddr >> 21) & 0x1ff;
    vpn[2] = (vaddr >> 30) & 0x1ff;

    u64 ppn[3];
    ppn[0] = (paddr >> 12) & 0x1ff;
    ppn[1] = (paddr >> 21) & 0x1ff;
    ppn[2] = (paddr >> 30) & 0x3ffffff;

    u64* v = root + vpn[2];

    for(s64 i = 1; i >= level; i--)
    {
        if(!mmu_is_entry_valid(*v))
        {
            u64* page = (u64)kalloc_single_page();
            for(u64 i = 0; i < 512; i++) { page[i] = 0; }
            *v = ((u64)page >> 2) | 1;
        }
        u64* entry = (u64*)((*v & (~0x3ff)) << 2);
        v = entry + vpn[i];
    }

    u64 entry = (ppn[2] << 28) |
                (ppn[1] << 19) |
                (ppn[0] << 10) |
                bits |
                1;
    if(bits) { *v = entry; }
    else     { *v = 0; }
}

void mmu_kernel_map_range(u64* root, void* start, void* end, u64 bits)
{
    assert((u64)start % PAGE_SIZE == 0, "map range start is page aligned");
    assert((u64)end % PAGE_SIZE == 0, "map range end is page aligned");
    u64 num_kb_pages = ((u64)end - (u64)start) / PAGE_SIZE;
 
    u64 memaddr = start;
    for(u64 i = 0; i < num_kb_pages; i++)
    {
        mmu_map(root, memaddr, memaddr, bits, 0);
        memaddr += PAGE_SIZE;
    }
}
 
void mmu_unmap_table(u64* root)
{
    for(u64 lv2 = 0; lv2 < 512; lv2++)
    {
        u64 entry_lv2 = root[lv2];
        if(mmu_is_entry_valid(entry_lv2) && !mmu_is_entry_leaf(entry_lv2))
        {
            u64* table_lv1 = (u64*)((entry_lv2 & ~0x3ff) << 2);
 
            for(u64 lv1 = 0; lv1 < 512; lv1++)
            {
                u64 entry_lv1 = table_lv1[lv1];
                if(mmu_is_entry_valid(entry_lv1) && !mmu_is_entry_leaf(entry_lv1))
                {
                    u64* table_lv0 = (u64*)((entry_lv1 & ~0x3ff) << 2);
 
                    kfree_single_page(table_lv0);
                }
            }
            kfree_single_page(table_lv1);
        }
    }
}

void mmu_map_kallocation(u64* root, Kallocation k, void* vaddr, u64 bits)
{
    u64 vmemaddr = ((u64)vaddr) & ~(PAGE_SIZE - 1);
    u64 memaddr = (u64)k.memory;
    u64 num_kb_pages = k.page_count;

    for(u64 i = 0; i < num_kb_pages; i++)
    {
        mmu_map(root, vmemaddr, memaddr, bits, 0); // TODO: Auto larger pages
        memaddr += PAGE_SIZE;
        vmemaddr += PAGE_SIZE;
    }
}

/*
    returns 0 on success.
*/
u64 mmu_virt_to_phys(u64* root, u64 vaddr, u64* paddr)
{
    u64 vpn[3];
    vpn[0] = (vaddr >> 12) & 0x1ff;
    vpn[1] = (vaddr >> 21) & 0x1ff;
    vpn[2] = (vaddr >> 30) & 0x1ff;

    u64* v = root + vpn[2];
    for(s64 i = 2; i >= 0; i--)
    {
        if(!mmu_is_entry_valid(*v))
        { return 1; }
        else if(mmu_is_entry_leaf(*v))
        {
            u64 off_mask = (1 << (12 + i * 9)) - 1;
            u64 vaddr_pgoff = vaddr & off_mask;
            u64 addr = (*v << 2) & ~off_mask;
            u64 res = addr | vaddr_pgoff;
            *paddr = res;
            return 0;
        }
        u64* entry = (u64*)((*v & ~0x3ff) << 2);
        v = entry + vpn[i - 1];
    }

    return 2;
}

/* return 0 on success. start_page_vaddr should be page aligned */
u64 mmu_virt_to_phys_pages(u64* root, u64 start_page_vaddr, u64* paddrs, u64 page_count)
{
    for(u64 i = 0; i < page_count; i++)
    {
        u64 ret = mmu_virt_to_phys(root, start_page_vaddr + (i * PAGE_SIZE), paddrs + i);
        if(ret != 0)
        {
            return ret;
        }
    }
    return 0;
}

#define mmu_virt_to_phys_buffer(name, table, start_address, byte_length) \
u64 name##_buf_start = start_address; \
u64 name##_page_offset = name##_buf_start & 0xfffllu; \
u64 name##_total_byte_size = byte_length + name##_page_offset; \
u64 name##_total_page_count = (name##_total_byte_size + PAGE_SIZE - 1) / PAGE_SIZE; \
u64 name##_pages[name##_total_page_count]; \
u64 name##_return_value = mmu_virt_to_phys_pages(table, name##_buf_start & (~0xfffllu), name##_pages, name##_total_page_count);

#define mmu_virt_to_phys_buffer_get_address(name, byte_index) \
    (name##_pages[(byte_index + name##_page_offset) / PAGE_SIZE] + (byte_index + name##_page_offset) % PAGE_SIZE)

#define mmu_virt_to_phys_buffer_page_count(name) \
    (name##_total_page_count)

#define mmu_virt_to_phys_buffer_pages(name) \
    (name##_pages)

#define mmu_virt_to_phys_buffer_return_value(name) \
    (name##_return_value)


u64* mem_init()
{
    K_PAGE_COUNT = HEAP_SIZE / PAGE_SIZE;

    K_TABLE_COUNT = 63;
    while(1)
    {
        if((K_PAGE_COUNT & (((u64)1) << K_TABLE_COUNT)) != 0) { break; }
        K_TABLE_COUNT -= 1;
        if(K_TABLE_COUNT == 1) { break; }
    }
    if(((u64)1) << K_TABLE_COUNT < K_PAGE_COUNT) { K_TABLE_COUNT += 1; }

    // But remember, the smallest buddy stores a byte of bitmap
    for(u64 i = 0; i < 2 && K_TABLE_COUNT > 1; i++) { K_TABLE_COUNT -= 1; }

    K_HEAP_START = HEAP_START + (3 * 8); // we have variables at the heap start see above
    // Heap starts with memtables
    K_HEAP_START += K_TABLE_COUNT * sizeof(u8*);

    for(u64 i = 0; i < K_TABLE_COUNT; i++)
    {
        K_MEMTABLES[i] = (void*)K_HEAP_START;
        s64 buddy_byte_count = ((u64)1) << i;
        while((buddy_byte_count << (K_TABLE_COUNT - (i+1))) * 8 >= K_PAGE_COUNT)
        { buddy_byte_count -= 1; }
        buddy_byte_count += 1;
        K_MEMTABLES[i]->table_len = buddy_byte_count;

        K_HEAP_START += sizeof(struct KmemTable);
        K_HEAP_START += K_MEMTABLES[i]->table_len;
    }
    K_HEAP_START += PAGE_SIZE - (K_HEAP_START % PAGE_SIZE);
    for(u64 i = 0; i < K_TABLE_COUNT; i++)
    {
        u64 table_page_size = ((u64)PAGE_SIZE) << (K_TABLE_COUNT - (i+1));
        u64 c = HEAP_START;
        for(u64 j = 0; j < K_MEMTABLES[i]->table_len; j++)
        {
            K_MEMTABLES[i]->data[j] = 0;
            for(u64 k = 0; k < 8; k++)
            {
                if(c < K_HEAP_START)
                { K_MEMTABLES[i]->data[j] |= 1 << k; }
                c += table_page_size;
            }
        }
    }

    uart_printf("Memory has been initialized:\n\n");
    uart_printf("TEXT:        0x%x <-> 0x%x\n", TEXT_START, TEXT_END);
    uart_printf("RODATA:      0x%x <-> 0x%x\n", RODATA_START, RODATA_END);
    uart_printf("DATA:        0x%x <-> 0x%x\n", DATA_START, DATA_END);
    uart_printf("BSS:         0x%x <-> 0x%x\n", BSS_START, BSS_END);
    uart_printf("STACK:       0x%x <-> 0x%x\n", KERNEL_STACK_START, KERNEL_STACK_END);
    uart_printf("HEAP META:   0x%x <-> 0x%x\n", HEAP_START, K_HEAP_START);
    uart_printf("HEAP:        0x%x <-> 0x%x\n", K_HEAP_START, HEAP_START + HEAP_SIZE);
    uart_printf("\n\n");


    // Initialize MMU table for the kernel
    u64* table = kalloc_single_page();

    mmu_kernel_map_range(table, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);
    mmu_kernel_map_range(table, (u64*)KERNEL_STACK_START, (u64*)KERNEL_STACK_END,     2 + 4);
    mmu_kernel_map_range(table, (u64*)HEAP_START, (u64*)(HEAP_START + HEAP_SIZE),   2 + 4);

    //Map the uart
    mmu_kernel_map_range(table, 0x10000000, 0x10001000, 2 + 4);


    /* Test code */
    {
        mmu_map(table, 0x10000000 + 1*0x1000, 0x10000000, 2 + 4, 0);
        mmu_map(table, 0x10000000 + 2*0x1000, 0x10000000, 2 + 4, 0);
        mmu_map(table, 0x10000000 + 3*0x1000, 0x10000000, 2 + 4, 0);
        mmu_map(table, 0x10000000 + 4*0x1000, 0x20000000, 2 + 4, 0);
        mmu_map(table, 0x10000000 + 5*0x1000, 0x20000000, 2 + 4, 0);
        for(u64 i = 0; i < 10*2; i++)
        {
            void* test_addr = 0x10000000 - 0x1300 + (i * 0x1000);
            void* result_addr;
            u64 res = mmu_virt_to_phys(table, test_addr, &result_addr);
            if(!res)
            {
                uart_printf("0x%llx -> 0x%llx\n", test_addr, result_addr);
            }
            else
            {
                uart_printf("0x%llx is not mapped.\n", test_addr);
            }
        }

        u64 element_size = 512;
        u64 element_count = 50;

        mmu_virt_to_phys_buffer(my_buffer_mapping, table, 0x10001800, element_size * element_count)
        u64 is_good = mmu_virt_to_phys_buffer_return_value(my_buffer_mapping);

        uart_printf("%llu\n", is_good);

        for(u64 i = 0; i < mmu_virt_to_phys_buffer_page_count(my_buffer_mapping); i++)
        {
            uart_printf("page%llu is 0x%llx\n", i, mmu_virt_to_phys_buffer_pages(my_buffer_mapping)[i]);
        }
        for(u64 i = 0; i < element_count; i++)
        {
            u64 addr = mmu_virt_to_phys_buffer_get_address(my_buffer_mapping, i * element_size);
            uart_printf("%llu : 0x%llx\n", i, addr);
        }

    }

    //Map the clint
    mmu_kernel_map_range(table, 0x2000000, 0x2010000, 2 + 4);

    //Map the plic
    mmu_kernel_map_range(table, (u64*)0x0c000000, (u64*)0x0c003000, 2 + 4);
    mmu_kernel_map_range(table, (u64*)0x0c200000, (u64*)0x0c209000, 2 + 4);

    return table;
}

