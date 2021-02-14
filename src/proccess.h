
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
printf("REALLOC\n");
    }
    u64 index = KERNEL_PROCCESS_ARRAY_LEN;
    KERNEL_PROCCESS_ARRAY[index] = proccess;
    KERNEL_PROCCESS_ARRAY_LEN += 1;
    return index;
}

u32 proccess_thread_create(u64 pid)
{
    assert(pid < KERNEL_PROCCESS_ARRAY_LEN, "pid is within range");
    assert(KERNEL_PROCCESS_ARRAY[pid]->mmu_table != 0, "pid refers to a valid proccess");
    for(u32 i = 0; i < KERNEL_PROCCESS_ARRAY[pid]->thread_count; i++)
    {
        if(KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state == THREAD_STATE_UNINITIALIZED)
        {
            KERNEL_PROCCESS_ARRAY[pid]->threads[i].thread_state = THREAD_STATE_INITIALIZED;
            return i;
        }
    }

    if(sizeof(Proccess) + (KERNEL_PROCCESS_ARRAY[pid]->thread_count + 1) * sizeof(Thread) > 
        KERNEL_PROCCESS_ARRAY[pid]->proc_alloc.page_count * PAGE_SIZE)
    {
printf("REALLOC\n");
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
    return tid;
}

void proccess_init()
{
    //for(u64 i = 0; i < 600; i++) { printf("creating procces #%ld\n", proccess_create()); }

    u64 pid = proccess_create();
    for(u64 i = 0; i < 25; i++) {
        printf("creating thread #%ld\n", proccess_thread_create(pid));
    }
}
