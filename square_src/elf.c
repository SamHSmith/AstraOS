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

    f64 square_x = 0.0;
    f64 square_y = 0.0;
    f64 last_frame_time = start_time;
    u8 input = 0;

while(1)
{
    u64 surface = 0;
    AOS_wait_for_surface_draw(&surface, 1);

    AOS_Framebuffer* fb = 0x424242000; // the three zeroes alignes it to the page boundry
    u64 fb_page_count = AOS_surface_acquire(surface, 0, 0);
    if(AOS_surface_acquire(surface, fb, fb_page_count))
    {
        f64 frame_start = AOS_time_get_seconds();
        f64 delta_time = frame_start - last_frame_time;
        last_frame_time = frame_start;

        { // Keyboard events
            u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
            AOS_KeyboardEvent kbd_events[kbd_event_count];
            kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
            for(u64 i = 0; i < kbd_event_count; i++)
            {
                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_NOTHING)
                { continue; }

                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_PRESSED)
                {
                    u64 scancode = kbd_events[i].scancode;
                    if(scancode == 99)
                    { input = input | 1; }
                    else if(scancode == 100)
                    { input = input | 2; }
                    else if(scancode == 101)
                    { input = input | 4; }
                    else if(scancode == 102)
                    { input = input | 8; }
                }
                else
                {
                    u64 scancode = kbd_events[i].scancode;
                    if(scancode == 99)
                    { input = input & ~1; }
                    else if(scancode == 100)
                    { input = input & ~2; }
                    else if(scancode == 101)
                    { input = input & ~4; }
                    else if(scancode == 102)
                    { input = input & ~8; }
                }
                printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        }

        if(input & 1)
        { square_x -= delta_time; }
        if(input & 2)
        { square_y += delta_time; }
        if(input & 4)
        { square_y -= delta_time; }
        if(input & 8)
        { square_x += delta_time; }

        f32 time = frame_start - start_time;
        f32 red = (sineF32((time*M_PI)/2.0) + 1.0) / 2.0;
        f32 green = (sineF32((time*M_PI)/3.0) + 1.0) / 2.0;
        f32 blue = (sineF32((time*M_PI)/5.0) + 1.0) / 2.0;

        f32 s = cosineF32(time/10.0 * 2*M_PI);
        f32 c = sineF32(time/10.0 * 2*M_PI);
        f32 p1x = -0.25 * c -  0.25 * s;
        f32 p1y =  0.25 * c + -0.25 * s;
        f32 p2x =  0.25 * c -  0.25 * s;
        f32 p2y =  0.25 * c +  0.25 * s;

        f32 d1x = p2x - p1x;
        f32 d1y = p2y - p1y;
        f32 d2x = -p1x - p2x;
        f32 d2y = -p1y - p2y;

        f32 dpfx = 2.0 / (f32)fb->width;
        f32 dpfy = 2.0 / (f32)fb->height;
        f32 alias_by = (dpfx + dpfy) / 2.0;
        f32 pfx = -1.0;
        f32 pfy = -1.0;
        for(u64 y = 0; y < fb->height; y++)
        {
            for(u64 x = 0; x < fb->width; x++)
            {
                u64 i = x + y * fb->width;

                f32 e1 = (pfx-square_x) * d1y - (pfy+square_y) * d1x;
                f32 e2 = (pfx-square_x) * d2y - (pfy+square_y) * d2x;

                if(
                e1 <  0.125 &&
                e2 <  0.125 &&
                e1 > -0.125 &&
                e2 > -0.125
                )
                {
                    f32 cover1 = 1.0;
                    f32 cover2 = 1.0;
                    if(e1 > 0.0 && 0.125 - e1 < alias_by)
                    { cover1 = (0.125 - e1) / alias_by; }
                    else if(e1 < 0.0 && 0.125 + e1 < alias_by)
                    { cover1 = (0.125 + e1) / alias_by; }
                    if(e2 > 0.0 && 0.125 - e2 < alias_by)
                    { cover2 = (0.125 - e2) / alias_by; }
                    else if(e2 < 0.0 && 0.125 + e2 < alias_by)
                    { cover2 = (0.125 + e2) / alias_by; }

                    f32 cover = cover1;
                    if(cover2 < cover1) { cover = cover2; }

                    fb->data[i*4 + 0] = red * cover;
                    fb->data[i*4 + 1] = green * cover;
                    fb->data[i*4 + 2] = blue * cover;
                    fb->data[i*4 + 3] = 0.6 * cover;
                }
                else
                {
                    fb->data[i*4 + 0] = 0.0;
                    fb->data[i*4 + 1] = 0.0;
                    fb->data[i*4 + 2] = 0.0;
                    fb->data[i*4 + 3] = 0.0;
                }
                pfx += dpfx;
            }
        pfx = -1.0;
        pfy += dpfy;
        }
        AOS_surface_commit(surface);
        f64 frame_end = AOS_time_get_seconds();
//        printf("elf time : %10.10lf ms\n", (frame_end - frame_start) * 1000.0);
    }
}
}
