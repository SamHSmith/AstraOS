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

void kmain()
{
    u64* kernel_mmu_table = mem_init();
    uart_init();
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

Kallocation a1 = kalloc_pages(65);
void* memory = kalloc_single_page();

kfree_pages(a1);

kfree_single_page(memory);

uart_write_string("finished doing mem test\n");

u64* table = kalloc_single_page();

mmu_map(table, 400*4096, 212*4096, 3, 0);
mmu_map(table, 401*4096, 20012*4096, 3, 0);
mmu_map(table, 403*4096, 212*4096, 3, 0);

mmu_map(table, 520*4096, 51200000*4096, 3, 1);

u64 physical = 0;
//assert(mmu_virt_to_phys(table, 400*4096, &physical) == 0, "invalid virtual address");

for(u64 i = 0; i < 300; i++)
{
    if(mmu_virt_to_phys(table, 390*4096 + i * 2000, &physical) == 0)
    { printf("%p -> %p\n", 390*4096 + i * 2000, physical); }
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
