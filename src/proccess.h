
typedef struct TrapFrame
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
} TrapFrame;

#define THREAD_STATE_UNINITIALIZED 0
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

void proccess_init()
{
    for(u64 i = 0; i < 600; i++) { printf("creating procces #%ld\n", proccess_create()); }
}
