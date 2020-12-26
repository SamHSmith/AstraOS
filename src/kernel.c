#include "types.h"

#include "memory.h"
#include "uart.h"

// --- Lib maybe? ---
u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void kmain()
{
    mem_init();
    uart_init();
    char* welcome_msg = "Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n";
    uart_write(welcome_msg, strlen(welcome_msg));


Kallocation a1 = kalloc_pages(8);

kfree_pages(a1);

    for(u64 b = 0; b < K_TABLE_COUNT; b++)
    {
        for(u64 i = 0; i < K_MEMTABLES[b]->table_len; i++)
        {
            char f = '_'; char u = '*';
            for(u64 j = 0; j < 8; j++)
            {
                if((K_MEMTABLES[b]->data[i] & (1 << j)) != 0)
                { uart_write(&u, 1); }
                else
                { uart_write(&f, 1); }
            }
        }
        char r = '\n';
        uart_write(&r, 1);
    }

    while(1)
    {
        char r = 0;
        if(uart_read(&r, 1) == 1)
        {
            uart_write(&r, 1);
        }
    }
}
