
typedef struct TrapFrame
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
} TrapFrame;

#define THREAD_STATE_UNINITIALIZED 0
#define THREAD_STATE_INITIALIZED 1
#define THREAD_STATE_RUNNING 2
typedef struct Thread
{
    TrapFrame frame;
    Kallocation stack_alloc;
    u64 program_counter;
    u64 thread_state;
} Thread;

typedef struct Proccess
{
    Kallocation proc_alloc;
    u64* mmu_table;
    u32 thread_count;
    Thread threads[];
} Proccess;

Kallocation KERNEL_PROCCESS_ARRAY_ALLOCATION = {0};
#define KERNEL_PROCCESS_ARRAY ((Proccess**)KERNEL_PROCCESS_ARRAY_ALLOCATION.memory)
u64 KERNEL_PROCCESS_ARRAY_LEN = 0;

u64 proccess_create()
{
    Kallocation _proc = kalloc_pages(1);
    Proccess* proccess = (Proccess*)_proc.memory;
    proccess->proc_alloc = _proc;

    proccess->mmu_table = (u64*)kalloc_single_page();
    for(u64 i = 0; i < 512; i++) { proccess->mmu_table[i] = 0; }
    proccess->thread_count = 0;

    for(u64 i = 0; i < KERNEL_PROCCESS_ARRAY_LEN; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[i]->mmu_table == 0)
        {
            KERNEL_PROCCESS_ARRAY[i] = proccess;
            return i;
        }
    }
    if(KERNEL_PROCCESS_ARRAY_LEN % 512 == 0)
    {
        Kallocation new_alloc=kalloc_pages(KERNEL_PROCCESS_ARRAY_ALLOCATION.page_count+1);
        Proccess** new_array = (Proccess**)new_alloc.memory;
        for(u64 i = 0; i < KERNEL_PROCCESS_ARRAY_LEN; i++)
        {
            new_array[i] = KERNEL_PROCCESS_ARRAY[i];
        }
        if(KERNEL_PROCCESS_ARRAY_ALLOCATION.page_count != 0) //at init this is false
        {
            kfree_pages(KERNEL_PROCCESS_ARRAY_ALLOCATION);
        }
        KERNEL_PROCCESS_ARRAY_ALLOCATION = new_alloc;
    }
    u64 index = KERNEL_PROCCESS_ARRAY_LEN;
    KERNEL_PROCCESS_ARRAY[index] = proccess;
    KERNEL_PROCCESS_ARRAY_LEN += 1;
    return index;
}

u64 mmu_table_ptr_to_satp(u64* mmu_table)
{
    u64 root_ppn = ((u64)mmu_table) >> 12;
    u64 satp_val = (((u64)8) << 60) | root_ppn;
    return satp_val;
}

u32 proccess_thread_create(u64 pid)
{
    assert(pid < KERNEL_PROCCESS_ARRAY_LEN, "pid is within range");
    assert(KERNEL_PROCCESS_ARRAY[pid]->mmu_table != 0, "pid refers to a valid proccess");

    u64 thread_satp = mmu_table_ptr_to_satp(KERNEL_PROCCESS_ARRAY[pid]->mmu_table);

    for(u32 i = 0; i < KERNEL_PROCCESS_ARRAY[pid]->thread_count; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state == THREAD_STATE_UNINITIALIZED)
        {
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state = THREAD_STATE_INITIALIZED;
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].frame.satp = thread_satp;
            return i;
        }
    }

    if(sizeof(Proccess) + (KERNEL_PROCCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread) > 
        KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.page_count * PAGE_SIZE)
    {
        Kallocation new_alloc = kalloc_pages(KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.page_count + 1);
        for(u64 i = 0; i < (new_alloc.page_count - 1) * (PAGE_SIZE / 8); i++)
        {
            *(((u64*)new_alloc.memory) + i) =
                *(((u64*)KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.memory) + i);
        }
        kfree_pages(KERNEL_PROCCESS_ARRAY[pid]->proc_alloc);
        KERNEL_PROCCESS_ARRAY[pid] = (Proccess*)new_alloc.memory;
        KERNEL_PROCCESS_ARRAY[pid]->proc_alloc = new_alloc;
    }
    u32 tid = KERNEL_PROCCESS_ARRAY[pid]->thread_count;
    KERNEL_PROCCESS_ARRAY[pid]->thread_count += 1;

    KERNEL_PROCCESS_ARRAY[pid]->threads[tid].thread_state = THREAD_STATE_INITIALIZED;
    KERNEL_PROCCESS_ARRAY[pid]->threads[tid].frame.satp = thread_satp;
    return tid;
}

void thread1_func();
void thread2_func();

void proccess_init()
{
    //for(u64 i = 0; i < 600; i++) { printf("creating procces #%ld\n", proccess_create()); }

    u64 pid = proccess_create();

    u64* table = KERNEL_PROCCESS_ARRAY[pid]->mmu_table;

    mmu_kernel_map_range(table, (u64*)TEXT_START, (u64*)TEXT_END,                   2 + 8); //read + execute
    mmu_kernel_map_range(table, (u64*)RODATA_START, (u64*)RODATA_END,               2    ); //readonly
    mmu_kernel_map_range(table, (u64*)DATA_START, (u64*)DATA_END,                   2 + 4); //read + write
    mmu_kernel_map_range(table, (u64*)BSS_START, (u64*)BSS_END,                     2 + 4);

    mmu_kernel_map_range(table, 0x10000000, 0x10000000, 2 + 4);

    u32 thread1 = proccess_thread_create(pid);
    u32 thread2 = proccess_thread_create(pid);

    Thread* tarr = KERNEL_PROCCESS_ARRAY[pid]->threads;

    tarr[thread1].stack_alloc = kalloc_pages(4);
    tarr[thread1].frame.regs[2] = 
        ((u64)tarr[thread1].stack_alloc.memory) + tarr[thread1].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread1].stack_alloc.memory,
            (u64*)tarr[thread1].frame.regs[2],
            2 + 4
        );

    tarr[thread2].stack_alloc = kalloc_pages(4);
    tarr[thread2].frame.regs[2] = 
        ((u64)tarr[thread2].stack_alloc.memory) + tarr[thread2].stack_alloc.page_count * PAGE_SIZE;
    mmu_kernel_map_range(
            table,
            (u64*)tarr[thread2].stack_alloc.memory,
            (u64*)tarr[thread2].frame.regs[2],
            2 + 4
        );

    tarr[thread1].program_counter = (u64)thread1_func;
    tarr[thread2].program_counter = (u64)thread2_func;

    u64* mtimecmp = (u64*)0x02004000;
    u64* mtime = (u64*)0x0200bff8;

    *mtimecmp = *mtime;
}

Thread* kernel_current_thread;

u64 current_thread = 55;
Thread* kernel_choose_new_thread()
{
    if(current_thread != 0) { current_thread = 0; }
    else { current_thread = 1; }
    return &KERNEL_PROCCESS_ARRAY[0]->threads[current_thread];
}


void thread1_func()
{
    u64 times = 1;
    while(1)
    {
        for(u64 i = 0; i < 90000000; i++) {}
        printf("thread1 is doing stuff. #%lld times!\n", times);
        times++;
    }
}

void thread2_func()
{
    u64 times = 1;
    while(1)
    {
        for(u64 i = 0; i < 500000000; i++) {}
        printf(" ******** thread2 is ALSO doing stuff!!! ********* #%lld many times!!!!!!!! \n", times);
        times++;
    }
}
