#include "../userland/aos_syscalls.h"

#include "../src/uart.h"
#include "../src/printf.h"
void _putchar(char c)
{
    uart_write(&c, 1);
}

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void _start()
{
    printf("Hi I'm dave and I live in an elf file on a partition on the RADICAL PARTITION SYSTEM\n");

    double start_time = AOS_time_get_seconds();

while(1)
{
    u64 surface = 0;
    AOS_wait_for_surface_draw(&surface, 1);

    AOS_Framebuffer* fb = 0x424242000; // the three zeroes alignes it to the page boundry
    u64 fb_page_count = AOS_surface_acquire(surface, 0, 0);
    if(AOS_surface_acquire(surface, fb, fb_page_count))
    {
        double time = AOS_time_get_seconds() - start_time;
        float red = time/2.0 - (f64)((u64)(time/2.0));
        if(red > 0.5) { red = 1.0 - red; }
        red *= 2.0;
        float green = time/3.0 - (f64)((u64)(time/3.0));
        if(green > 0.5) { green = 1.0 - green; }
        green *= 2.0;
        float blue =  time/5.0 - (f64)((u64)(time/5.0));
        if(blue > 0.5) { blue = 1.0 - blue; }
        blue *= 2.0;

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + y * fb->width;
            fb->data[i*4 + 0] = red;
            fb->data[i*4 + 1] = green;
            fb->data[i*4 + 2] = blue;
            fb->data[i*4 + 3] = 0.6;
        }
        AOS_surface_commit(surface);
    }
}
}
