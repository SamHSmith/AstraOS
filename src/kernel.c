#include "types.h"

#include "uart.h"
#include "printf.h"
void _putchar(char c)
{
    uart_write(&c, 1);
}

#include "memory.h"

// --- Lib maybe? ---
u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
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

    u64 root_ppn = ((u64)KERNEL_MMU_TABLE) >> 12;
    u64 satp_val = (((u64)8) << 60) | root_ppn;

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
dave[2] = 'p';
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
    while(1)
    {
        u8 r = 0;
        if(uart_read(&r, 1) == 1)
        {
            uart_write(&r, 1);
        }
    }
}

void kinit_hart(u64 hartid)
{

}

struct TrapFrame
{
    u64 regs[32];
    u64 fregs[32];
    u64 satp;
};

u64 m_trap(
    u64 epc,
    u64 tval,
    u64 cause,
    u64 hart,
    u64 status,
    struct TrapFrame* frame
    )
{
    printf("args:\n  epc: %x\n  tval: %x\n  cause: %x\n  hart: %x\n  status: %x\n  frame: %x\n",
            epc, tval, cause, hart, status, frame);
    printf("frame:\n regs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  x%lld: %lx\n", i, frame->regs[i]); }
    printf(" fregs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  f%lld: %lx\n", i, frame->fregs[i]); }
    printf(" satp: %lx, trap_stack: %lx\n", frame->satp, frame);

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
                printf("Machine timer interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 8) {
                printf("User external interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 9) {
                printf("Supervisor external interrupt CPU%lld\n", hart);
        }
        else if(cause_num == 11) {
                printf("Machine external interrupt CPU%lld\n", hart);
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
return_pc += 4;
return return_pc;
        }
    }

    while(1) {}
}
