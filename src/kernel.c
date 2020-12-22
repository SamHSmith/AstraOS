#include "types.h"

// --- Lib maybe? ---
u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

#include "uart.h"

void kmain()
{
    uart_init();
    char* welcome_msg = "Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen";
    uart_write(welcome_msg, strlen(welcome_msg));

    while(1)
    {
        char r = 0;
        if(uart_read(&r, 1) == 1)
        {
            uart_write(&r, 1);
        }
    }
}
