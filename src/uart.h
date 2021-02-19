#define UART_BASE_ADDRESS ((u8*)0x10000000)
 
void uart_init()
{
    *(UART_BASE_ADDRESS + 3) = 0b11;
    *(UART_BASE_ADDRESS + 2) = 1;
    *(UART_BASE_ADDRESS + 1) = 1;
 
    u16 divisor = 592;
    u8 divisor_least = divisor & 0xff;
    u8 divisor_most = divisor >> 8;
 
    u8 lcr = *(UART_BASE_ADDRESS + 3);
    *(UART_BASE_ADDRESS + 3) = lcr | (1 << 7);
 
    *(UART_BASE_ADDRESS + 0) = divisor_least;
    *(UART_BASE_ADDRESS + 1) = divisor_most;
 
    *(UART_BASE_ADDRESS + 3) = lcr;
}
 
void uart_write(u8* data, u64 len)
{
    for(u64 i = 0; i < len; i++)
    { *UART_BASE_ADDRESS = data[i]; }
}
 
u64 uart_read(u8* data, u64 len)
{
    for(u64 i = 0; i < len; i++)
    {
        if((*(UART_BASE_ADDRESS + 5) & 1) == 0)
        { return i; }
        else
        {
            data[i] = *UART_BASE_ADDRESS;
        }
    }
    return len;
}

void uart_read_blocking(u8* data, u64 len)
{
    for(u64 i = 0; i < len;)
    {
        if((*(UART_BASE_ADDRESS + 5) & 1) == 0)
        { continue; }
        data[i] = *UART_BASE_ADDRESS;
        i++;
    }
}
