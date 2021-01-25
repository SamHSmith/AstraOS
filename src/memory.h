
extern u64 HEAP_START;
extern u64 HEAP_SIZE;
 
#define K_HEAP_START (*(((u64*)HEAP_START) + 0))
#define K_PAGE_COUNT (*(((u64*)HEAP_START) + 1))
#define K_TABLE_COUNT (*(((u64*)HEAP_START) + 2))

struct KmemTable
{
    u64 table_len;
    u8 data[];
};
#define K_MEMTABLES ((struct KmemTable**)((u64*)HEAP_START + 3))

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

Kallocation kalloc_pages(u64 page_count)
{
    if(page_count == 0) { Kallocation al = {0}; return al; }
    u64 a_size = 0;
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
    al.memory = (void*)(HEAP_START + (page_address * PAGE_SIZE));
    al.page_count = page_count;
    return al;
}

void kfree_pages(Kallocation a)
{
    u64 addr = ((u64)a.memory);
    if(addr <= HEAP_START) { return; }
    addr -= HEAP_START;
    if((addr % PAGE_SIZE) != 0) { return; } //TODO: some kind of error maybe
    addr /= PAGE_SIZE;

    mem_table_set_taken(addr, a.page_count, 0);
}

void mem_init()
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
}

void* kalloc_single_page()
{
    Kallocation k = kalloc_pages(1);
    return k.memory;
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

void mmu_map(u64* root, u64 vaddr, u64 paddr, u64 bits, s64 level)
{
    assert((bits & 0xe) != 0, "bits are not null");

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
            u64 page = (u64)kalloc_single_page();
            *v = (page >> 2) | 1;
        }
        u64* entry = (u64*)((*v & (~0x3ff)) << 2);
        v = entry + vpn[i];
    }

    u64 entry = (ppn[2] << 28) |
                (ppn[1] << 19) |
                (ppn[0] << 10) |
                bits |
                1;
    *v = entry;
}

void mmu_unmap(u64* root)
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

