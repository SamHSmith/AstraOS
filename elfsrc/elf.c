#include "../userland/aos_syscalls.h"

#include "../common/maths.h"

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

    f64 start_time = AOS_time_get_seconds();

while(1)
{
    u64 surface = 0;
    AOS_wait_for_surface_draw(&surface, 1);

    AOS_Framebuffer* fb = 0x424242000; // the three zeroes alignes it to the page boundry
    u64 fb_page_count = AOS_surface_acquire(surface, 0, 0);
    if(AOS_surface_acquire(surface, fb, fb_page_count))
    {
        f32 time = AOS_time_get_seconds() - start_time;
        f32 red = (sineF32((time*M_PI)/2.0) + 1.0) / 2.0;
        f32 green = (sineF32((time*M_PI)/3.0) + 1.0) / 2.0;
        f32 blue = (sineF32((time*M_PI)/5.0) + 1.0) / 2.0;

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + y * fb->width;

            f32 pfx = ((f32)x / (f32)fb->width) * 2.0 - 1.0;
            f32 pfy = ((f32)y / (f32)fb->height) * 2.0 - 1.0;

            f32 s = cosineF32(time * 2*M_PI);
            f32 c = sineF32(time * 2*M_PI);
            f32 fx = pfx*c - pfy*s;
            f32 fy = pfy*c + pfx*s;

            if(fx > -0.5 && fx < 0.5 && fy > -0.5 && fy < 0.5)
            {
                fb->data[i*4 + 0] = red;
                fb->data[i*4 + 1] = green;
                fb->data[i*4 + 2] = blue;
                fb->data[i*4 + 3] = 0.6;
            }
            else
            {
                fb->data[i*4 + 0] = 0.0;
                fb->data[i*4 + 1] = 0.0;
                fb->data[i*4 + 2] = 0.0;
                fb->data[i*4 + 3] = 0.0;
            }
        }
        AOS_surface_commit(surface);
    }
}
}
