#include "../userland/aos_syscalls.h"

#include "samorak.h"
//#include "qwerty.h"

#include "font8_16.h"

#define BORDER_SIZE 3

typedef struct
{
    s64 x;
    s64 y;
    u64 consumer;
    f64 creation_time;
    u64 target_width;
    u64 width;
    u64 height;
    u64 new_width;
    u64 new_height;
    Framebuffer* fb;
    u64 fb_page_count;
    u8 we_have_frame;
} Window;

f32 clamp_01(f32 f)
{
    if(f < 0.0) { f = 0.0; }
    else if(f > 1.0) { f = 1.0; }
    return f;
}

f32 botched_sin(f32 t)
{
    t /= 3.14;
    s64 off = (s64)(t/2.0);
    t -= (f32)(off*2);
    if(t < 1.0)
    {
        return 1.0 - (2.0*t);
    }
    else
    {
        return (2.0*(t-1.0)) - 1.0;
    }
}

void program_loader_program(u64 drive1_partitions_directory)
{
    u64 slot_count = AOS_directory_get_files(drive1_partitions_directory, 0, 0);
    u64 partitions[slot_count];
    u8 partition_names[slot_count][64];
    u64 partition_name_lens[slot_count];
    slot_count = AOS_directory_get_files(drive1_partitions_directory, partitions, slot_count);
    for(u64 i = 0; i < slot_count; i++)
    {
        partition_names[i][0] = 0;
        AOS_file_get_name(partitions[i], partition_names[i], 64);
        partition_name_lens[i] = strlen(partition_names[i]);
        printf("tempAOS has found %s\n", partition_names[i]);
    }
    u64 slot_index = 0;

    Window windows[256];
    u64 window_count = 0;

    f64 cursor_x = 0.0;
    f64 cursor_y = 0.0;
    f64 new_cursor_x = 0.0;
    f64 new_cursor_y = 0.0;

    u8 is_moving_window = 0;
    f64 start_move_x = 0.0;
    f64 start_move_y = 0.0;

while(1) {

    cursor_x = new_cursor_x;
    cursor_y = new_cursor_y;

    // Set up consumers
    {
        for(u64 i = 0; i < window_count; i++)
        {
            // do move, not related to consumers
            if(is_moving_window && i + 1 == window_count)
            {
                windows[i].x = (s64)(cursor_x - start_move_x);
                windows[i].y = (s64)(cursor_y - start_move_y);
            }

            windows[i].width = windows[i].new_width;
            windows[i].height = windows[i].new_height;
            AOS_surface_consumer_set_size(
                windows[i].consumer,
                windows[i].width  -2*BORDER_SIZE,
                windows[i].height -2*BORDER_SIZE
            );
            AOS_surface_consumer_fire(windows[i].consumer);
        }
    }

    u64 AOS_wait_surface = 0;
    AOS_wait_for_surface_draw(&AOS_wait_surface, 1);

    // Fetch from consumers
    {
        for(u64 i = 0; i < window_count; i++)
        {
            if(AOS_surface_consumer_fetch(windows[i].consumer, 1, 0)) // Poll
            {
                // allocate address space
                windows[i].fb_page_count = AOS_surface_consumer_fetch(windows[i].consumer, 0, 0);
                if(AOS_surface_consumer_fetch(
                    windows[i].consumer,
                    windows[i].fb,
                    windows[i].fb_page_count
                ))
                {
                    windows[i].we_have_frame = 1;
                }
            }
        }
    }

    Framebuffer* fb = 0x54000;
    u64 fb_page_count = AOS_surface_acquire(0, 0, 0);
    if(AOS_surface_acquire(0, fb, fb_page_count))
    {
        double time_frame_start = AOS_time_get_seconds();

        { // Mouse events
            u64 mouse_event_count = AOS_get_rawmouse_events(0, 0);
            RawMouseEvent mouse_events[mouse_event_count];
            mouse_event_count = AOS_get_rawmouse_events(mouse_events, mouse_event_count);
            for(u64 i = 0; i < mouse_event_count; i++)
            {
                new_cursor_x += mouse_events[i].delta_x;
                new_cursor_y += mouse_events[i].delta_y;

                if(mouse_events[i].pressed && mouse_events[i].button == 0)
                {
                    u64 window = 0;
                    u8 any_window = 0;
                    for(s64 j = window_count-1; j >= 0; j--)
                    {
                        if( (s64)cursor_x >= windows[j].x &&
                            (s64)cursor_y >= windows[j].y &&
                            (s64)cursor_x < windows[j].x + (s64)windows[j].width &&
                            (s64)cursor_y < windows[j].y + (s64)windows[j].height
                        )
                        {
                            window = (u64)j;
                            any_window = 1;
                            break;
                        }
                    }
                    if(any_window)
                    {
                        Window temp = windows[window];
                        for(u64 j = window; j + 1 < window_count; j++)
                        { windows[j] = windows[j+1]; }
                        windows[window_count-1] = temp;
                        is_moving_window = 1;
                        start_move_x = cursor_x - (f64)temp.x;
                        start_move_y = cursor_y - (f64)temp.y;
                    }
                }
                else if(mouse_events[i].released && mouse_events[i].button == 0)
                {
                    is_moving_window = 0;
                }
            }
        }

        { // Keyboard events
        u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
        KeyboardEvent kbd_events[kbd_event_count];
        u64 more = 0;
        do {
            more = 0;
            kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
            for(u64 i = 0; i < kbd_event_count; i++)
            {
                if(kbd_events[i].event == KEYBOARD_EVENT_NOTHING)
                { continue; }
                more = 1;

                if(kbd_events[i].event == KEYBOARD_EVENT_PRESSED)
                {
                    u64 scancode = kbd_events[i].scancode;

                    if(scancode >= 62 && scancode <= 71)
                    {
                        u64 fkey = scancode - 62;
                        AOS_switch_vo(fkey);
                    }

                    if(scancode == 100 && slot_index > 0)
                    { slot_index--; }
                    else if(scancode == 101 && slot_index + 1 < slot_count)
                    { slot_index++; }
                    else if(scancode == 102)
                    {
                        for(u64 i = 0; i < window_count; i++)
                        {
                            windows[i].target_width += 10;
                        }
                    }
                    else if(scancode == 99)
                    {
                        for(u64 i = 0; i < window_count; i++)
                        {
                            if(windows[i].target_width > 2*BORDER_SIZE) { windows[i].target_width -= 10; }
                        }
                    }

                    if(scancode == 35 && slot_index < slot_count && window_count + 1 < 256)
                    {
                        u64 pid = 0;
                        if(AOS_create_process_from_file(partitions[slot_index], &pid))
                        {
                            printf("PROCESS CREATED, PID=%llu\n", pid);
                            u64 con = 0;
                            if(AOS_surface_consumer_create(pid, &con))
                            {
                                windows[window_count].consumer = con;
                                windows[window_count].x = 20 + window_count*7;
                                windows[window_count].y = 49*window_count;
                                if(windows[window_count].y > 1000) { windows[window_count].y = 1000; }
                                windows[window_count].width = 0;
                                windows[window_count].height = 0;
                                windows[window_count].new_width = 1;
                                windows[window_count].new_height = 1;
                                windows[window_count].target_width = 2*BORDER_SIZE;
                                windows[window_count].creation_time = AOS_time_get_seconds();
                                windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                                windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                                windows[window_count].we_have_frame = 0;
                                window_count++;

                                if(is_moving_window)
                                {
                                    Window temp = windows[window_count-2];
                                    windows[window_count-2] = windows[window_count-1];
                                    windows[window_count-1] = temp;
                                }
                            }
                            else { printf("Failed to create consumer for PID: %llu\n", pid); }
                        }
                        else { printf("failed to create process."); }
                    }
                }
                else
                {
                }

                printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        } while(more);
        }

        for(u64 i = 0; i < window_count; i++)
        {
            windows[i].new_width = (s64)windows[i].target_width +
                (s64)(125.0 * (1.0+botched_sin((time_frame_start - windows[i].creation_time))));
            windows[i].new_height = 70;
        }

        u64 column_count = (fb->width / 8);
        u64 row_count = (fb->height / 16);

        u8 bottom_banner[256];
        bottom_banner[0] = 0;
        u64 cvo = 0;
        if(AOS_get_vo_id(&cvo))
        {
            sprintf(bottom_banner, "Virtual Output #%llu", cvo);
        }
        u64 bottom_banner_len = strlen(bottom_banner);
        if(bottom_banner_len > column_count || row_count <= 1) { bottom_banner_len = column_count; }
        u64 bottom_banner_y = 0;
        if(fb->height > 16) { bottom_banner_y = fb->height - 16; }

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + (y * fb->width);

            fb->data[i*4 + 0] = 17.0/255.0;
            fb->data[i*4 + 1] = 80.0/255.0;
            fb->data[i*4 + 2] = 128.0/255.0;
            fb->data[i*4 + 3] = 1.0;

            u64 c = x / 8;
            u64 r = y / 16;

            if(r == slot_index)
            {
                fb->data[i*4 + 0] = 28.0/255.0;
                fb->data[i*4 + 1] = 133.0/255.0;
                fb->data[i*4 + 2] = 213.0/255.0;
                fb->data[i*4 + 3] = 1.0;
            }
            else if(r < slot_count)
            {
                fb->data[i*4 + 0] = 8.0/255.0;
                fb->data[i*4 + 1] = 37.0/255.0;
                fb->data[i*4 + 2] = 57.0/255.0;
                fb->data[i*4 + 3] = 1.0;
            }

            u64 here = 0;
            if(r < slot_count && c < partition_name_lens[r])
            {
                u64 font_id = partition_names[r][c];
                here = font8_16_pixel_filled(font_id, x - c*8, y - r*16);
            }

            if(y >= bottom_banner_y && c < bottom_banner_len)
            {
                u64 font_id = bottom_banner[c];
                here = font8_16_pixel_filled(font_id, x - (c*8), y - bottom_banner_y);
            }

            if(here)
            {
                fb->data[i*4 + 0] = 0.909;
                fb->data[i*4 + 1] = 0.89;
                fb->data[i*4 + 2] = 0.772;
                fb->data[i*4 + 3] = 1.0;
            }
        }

        for(s64 j = 0; j < window_count; j++)
        {
            if(!windows[j].we_have_frame)
            {
                for(u64 y = 0; y < windows[j].height; y++)
                for(u64 x = 0; x < windows[j].width; x++)
                {
                    if((s64)x + windows[j].x < 0 || (s64)y + windows[j].y < 0 ||
                       (s64)x + windows[j].x >= fb->width || (s64)y + windows[j].y >= fb->height)
                    { continue; }
                    u64 external_x = (u64)((s64)x + windows[j].x);
                    u64 external_y = (u64)((s64)y + windows[j].y);
                    u64 i = external_x + (external_y * fb->width);

                    fb->data[i*4 + 0] = (f32)x / (f32)windows[j].width;
                    fb->data[i*4 + 1] = (f32)y / (f32)windows[j].height;
                    fb->data[i*4 + 2] = 180.0/255.0;
                    fb->data[i*4 + 3] = 1.0;
                }
                continue;
            }

            for(u64 y = 0; y < windows[j].height; y++)
            for(u64 x = 0; x < windows[j].width; x++)
            {
                if((s64)x + windows[j].x < 0 || (s64)y + windows[j].y < 0 ||
                   (s64)x + windows[j].x >= fb->width || (s64)y + windows[j].y >= fb->height)
                { continue; }
                u64 external_x = (u64)((s64)x + windows[j].x);
                u64 external_y = (u64)((s64)y + windows[j].y);
                u64 i = external_x + (external_y * fb->width);

                if(x >= BORDER_SIZE && y >= BORDER_SIZE &&
                   x - BORDER_SIZE < windows[j].fb->width && y - BORDER_SIZE < windows[j].fb->height)
                {
                    u64 internal_x = x - BORDER_SIZE;
                    u64 internal_y = y - BORDER_SIZE;
                    u64 k = internal_x + (internal_y * windows[j].fb->width);

                    float cover = 1.0 - clamp_01(windows[j].fb->data[k*4 + 3]);
        fb->data[i*4 + 0] = clamp_01(fb->data[i*4 + 0] * cover + clamp_01(windows[j].fb->data[k*4 + 0]));
        fb->data[i*4 + 1] = clamp_01(fb->data[i*4 + 1] * cover + clamp_01(windows[j].fb->data[k*4 + 1]));
        fb->data[i*4 + 2] = clamp_01(fb->data[i*4 + 2] * cover + clamp_01(windows[j].fb->data[k*4 + 2]));
        fb->data[i*4 + 3] = 1.0;
                }
                else
                {
                    fb->data[i*4 + 0] = (f32)x / (f32)windows[j].width;
                    fb->data[i*4 + 1] = (f32)y / (f32)windows[j].height;
                    fb->data[i*4 + 2] = 180.0/255.0;
                    fb->data[i*4 + 3] = 1.0;
                }
            }
        }

        { // draw cursor
            if(cursor_x < 0.0) { cursor_x = 0.0; }
            if(cursor_y < 0.0) { cursor_y = 0.0; }
            if(cursor_x >= (f64)fb->width) { cursor_x = (f64)(fb->width - 1); }
            if(cursor_y >= (f64)fb->height){ cursor_y = (f64)(fb->height -1); }
            if(new_cursor_x < 0.0) { new_cursor_x = 0.0; }
            if(new_cursor_y < 0.0) { new_cursor_y = 0.0; }
            if(new_cursor_x >= (f64)fb->width) { new_cursor_x = (f64)(fb->width - 1); }
            if(new_cursor_y >= (f64)fb->height){ new_cursor_y = (f64)(fb->height -1); }

            for(u64 y = (u64)cursor_y; y < (u64)cursor_y + 3; y++)
            for(u64 x = (u64)cursor_x; x < (u64)cursor_x + 3; x++)
            {
                if(x >= fb->width || y >= fb->height)
                { continue; }
                u64 i = x + (y * fb->width);
                fb->data[i*4 + 0] = 1.0;
                fb->data[i*4 + 1] = 1.0;
                fb->data[i*4 + 2] = 1.0;
                fb->data[i*4 + 3] = 1.0;
            }
        }

        double time_frame_end = AOS_time_get_seconds();
        double frame_time = (time_frame_end-time_frame_start) *1000.0;
        u8 frame_counter_string[16];
        sprintf(frame_counter_string, "%4.4lf ms", frame_time);
        u64 frame_counter_width = strlen(frame_counter_string) * 8;
        u64 xoff = fb->width - frame_counter_width;
        for(u64 y = bottom_banner_y; y < fb->height; y++)
        for(u64 x = 0; x < frame_counter_width; x++)
        {
            u64 font_id = frame_counter_string[x/8];
            if(font8_16_pixel_filled(font_id, x%8, y - bottom_banner_y))
            {
                u64 i = (x + xoff) + (y * fb->width);
                fb->data[i*4 + 0] = (f32) (fb->data[i*4 + 0] < 0.5);
                fb->data[i*4 + 1] = (f32) (fb->data[i*4 + 1] < 0.5);
                fb->data[i*4 + 2] = (f32) (fb->data[i*4 + 2] < 0.5);
                fb->data[i*4 + 3] = 1.0;
            }
        }
        assert(AOS_surface_commit(0), "commited successfully");
    }
}
}

