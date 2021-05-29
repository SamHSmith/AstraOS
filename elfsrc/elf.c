#include "../userland/aos_syscalls.h"

#include "../src/uart.h"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void _start()
{
    char* dave = "Hi I'm dave and I live in an elf file on a partition on the RADICAL PARTITION SYSTEM\n";
    while(1)
    {
        uart_write(dave, strlen(dave));
        AOS_thread_sleep(990000);
    }
}
