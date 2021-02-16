#include "types.h"

#include "uart.h"
#include "printf.h"
void _putchar(char c)
{
    uart_write(&c, 1);
}

#include "memory.h"
#include "plic.h"
#include "proccess.h"

// --- Lib maybe? ---
u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void* memcpy(void* dest, const void* src, u64 n)
{
    void* orig_dest = dest;
    u64 n8 = (n >> 3) << 3;
    for(u64 i = 0; i < (n8 >> 3); i++)
    { ((u64*)dest)[i] = ((u64*)src)[i]; }

    dest += n8;
    src += n8;
    n -= n8;
    for(u64 i = 0; i < n; i++)
    { ((u8*)dest)[i] = ((u8*)src)[i]; }
    return orig_dest;
}

void uart_write_string(char* str)
{
    uart_write((u8*)str, strlen(str));
}

void assert(u64 stat, char* error)
{
    if(!stat)
    {
        uart_write_string("assertion failed: \"");
        uart_write_string(error);
        uart_write_string("\"\n");
        while(1) {};
    }
}

u64 KERNEL_MMU_TABLE;

u64 kinit()
{
    uart_init();
    KERNEL_MMU_TABLE = (u64)mem_init();

    u64 satp_val = mmu_table_ptr_to_satp((u64*)KERNEL_MMU_TABLE);

    printf("Entering supervisor mode...");
    return satp_val;
}

void kmain()
{
    printf("done.\n    Successfully entered kmain with supervisor mode enabled.\n\n");
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

Kallocation a1 = kalloc_pages(65);
void* memory = kalloc_single_page();

kfree_pages(a1);

float f = 0.4;
//printf("%f + 4.2 = %f\n", f, f + 4.2);
float b = f + 2.3;

char* dave = "davey";
printf("writing to readonly memory: %p\n", dave +2);
// dave[2] = 'p'; // this causes a kernel trap
printf("%s\n", dave);

kfree_single_page(memory);

uart_write_string("finished doing mem test\n");

u64* table = kalloc_single_page();

mmu_map(table, 401*4096, 20012*4096, 2+4, 0);
mmu_map(table, 403*4096, 212*4096, 2+4, 0);

mmu_map(table, 520*4096, 51200000*4096, 2+4, 1);

u64 physical = 0;
//assert(mmu_virt_to_phys(table, 400*4096, &physical) == 0, "invalid virtual address");

for(u64 i = 799; i < 1100; i++)
{
    if(mmu_virt_to_phys(table, i << 11, &physical) == 0)
    { printf("%p -> %p\n", i << 11, physical); }
    else
    { printf("segv"); }
}

mmu_unmap(table);

/*
    for(u64 b = 0; b < K_TABLE_COUNT; b++)
    {
        for(u64 i = 0; i < K_MEMTABLES[b]->table_len; i++)
        {
            for(u64 j = 0; j < 8; j++)
            {
                if((K_MEMTABLES[b]->data[i] & (1 << j)) != 0)
                { uart_write_string("*"); }
                else
                { uart_write_string("_"); }
            }
        }
        uart_write_string("\n");
    }
*/
/*
    while(1)
    {
        u8 r = 0;
        if(uart_read(&r, 1) == 1)
        {
            uart_write(&r, 1);
        }
    }
*/

    plic_interrupt_set_threshold(0);
    plic_interrupt_enable(10);
    plic_interrupt_set_priority(10, 1);

    proccess_init();

    while(1)
    {
        printf("doing stuff");
        for(u64 i = 0; i < 120000000; i++) {}
    }
}

void kinit_hart(u64 hartid)
{

}

u64 m_trap(
    u64 epc,
    u64 tval,
    u64 cause,
    u64 hart,
    u64 status,
    TrapFrame* frame
    )
{
    u64 async = (cause >> 63) & 1 == 1;
    u64 cause_num = cause & 0xfff;
    u64 return_pc = epc;

    if(async)
    {
             if(cause_num == 0) {
                printf("User software interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 1) {
                printf("Supervisor software interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 3) {
                printf("Machine software interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 4) {
                printf("User timer interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 5) {
                printf("Supervisor timer interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 7) {
            // Store thread
            if(kernel_current_thread != 0)
            {
                kernel_current_thread->frame = *frame;
                kernel_current_thread->program_counter = return_pc;
            }

            kernel_current_thread = kernel_choose_new_thread();

            // Load thread
            *frame = kernel_current_thread->frame;
            return_pc = kernel_current_thread->program_counter;

            // Reset the Machine Timer
            u64* mtimecmp = (u64*)0x02004000;
            u64* mtime = (u64*)0x0200bff8;
            *mtimecmp = *mtime + 10000000 / 600;

            return return_pc;
        }
        else if(cause_num == 8) {
                printf("User external interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 9) {
                printf("Supervisor external interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 11)
        {
            printf("Machine external interrupt CPU%lld\n", hart);
            u32 interrupt;
            char character;
            while(plic_interrupt_next(&interrupt))
            {
                if(interrupt == 10 && uart_read(&character, 1))
                {
                    printf("you typed the character: %c\n", character);
                }
                plic_interrupt_complete(interrupt);
            }
            return return_pc;
        }
    }
    else
    {
             if(cause_num == 0) {
                printf("Interrupt: Instruction address misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 1) {
                printf("Interrupt: Instruction access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 2) {
                printf("Interrupt: Illegal instruction CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 3) {
                printf("Interrupt: Breakpoint CPU%lld -> 0x%x\n", hart, epc);
        }
        else if(cause_num == 4) {
                printf("Interrupt: Load access misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 5) {
                printf("Interrupt: Load access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 6) {
                printf("Interrupt: Store/AMO address misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 7) {
                printf("Interrupt: Store/AMO access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 8) {
                printf("Interrupt: Environment call from U-mode CPU%lld -> 0x%x\n", hart, epc);
        }
        else if(cause_num == 9) {
                printf("Interrupt: Environment call from S-mode CPU%lld -> 0x%x\n", hart, epc);
        }
        else if(cause_num == 11) {
                printf("Interrupt: Environment call from M-mode CPU%lld -> 0x%x\n", hart, epc);
        }
        else if(cause_num == 12) {
                printf("Interrupt: Instruction page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 13) {
                printf("Interrupt: Load page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
        else if(cause_num == 15) {
                printf("Interrupt: Store/AMO page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
        }
    }

    printf("args:\n  epc: %x\n  tval: %x\n  cause: %x\n  hart: %x\n  status: %x\n  frame: %x\n",
            epc, tval, cause, hart, status, frame);
    printf("frame:\n regs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  x%lld: %lx\n", i, frame->regs[i]); }
    printf(" fregs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  f%lld: %lx\n", i, frame->fregs[i]); }
    printf(" satp: %lx, trap_stack: %lx\n", frame->satp, frame);

    while(1) {}
}
