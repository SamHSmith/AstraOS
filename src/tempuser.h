#include "../userland/aos_helper.h"

#include "samorak.h"
//#include "qwerty.h"

#include "font8_16.h"

#define BORDER_SIZE 3

typedef struct
{
    s64 x;
    s64 y;
    u64 pid;
    u64 consumer;
    u64 width;
    u64 height;
    u64 new_width;
    u64 new_height;
    Framebuffer* fb;
    u64 fb_page_count;
    u8 we_have_frame;
    u64 owned_in_stream;
    u64 owned_out_stream;
} Window;

f32 clamp_01(f32 f)
{
    if(f < 0.0) { f = 0.0; }
    else if(f > 1.0) { f = 1.0; }
    return f;
}

Window windows[300];
u64 window_count = 0;

#define THREAD_COUNT 8

volatile Spinlock tempuser_printout_lock;
u64 render_work_semaphore;
void render_thread_entry(u64 thread_number)
{
    spinlock_acquire(&tempuser_printout_lock);
    AOS_H_printf("Hey there you, I am render thread%llu\n", thread_number);
    spinlock_release(&tempuser_printout_lock);

    while(1)
    {
        __asm__("nop");
        assert(AOS_thread_awake_on_semaphore(render_work_semaphore), "actually awaking the semaphore");
        AOS_thread_sleep();
        spinlock_acquire(&tempuser_printout_lock);
        AOS_H_printf("I'm still here, from thread%llu\n", thread_number);
        spinlock_release(&tempuser_printout_lock);
    }
}

void program_loader_program(u64 drive1_partitions_directory)
{
    u8* print_text = "program loader program has started.\n";
    AOS_stream_put(0, print_text, strlen(print_text));

    spinlock_create(&tempuser_printout_lock);
    render_work_semaphore = AOS_semaphore_create(1, THREAD_COUNT-1);
    {
        u64 base_stack_addr = (~(0x1ffffff << 39)) & (~0xfff);
        AOS_alloc_pages(base_stack_addr - 4096*8*(THREAD_COUNT-1), 8*(THREAD_COUNT-1));

        AOS_thread_awake_after_time(1000000); // TODO: investigate memory mapping not right bug
        AOS_thread_sleep();

        for(u64 i = 0; i < THREAD_COUNT-1; i++) // TODO: one day add feature to ask for the "width" of the system
        {
            AOS_TrapFrame frame;
            frame.regs[10] = i;
            frame.regs[8] = base_stack_addr;
            frame.regs[2] = frame.regs[8] - 4 * sizeof(u64);
            base_stack_addr -= 4096*8;
            AOS_thread_new(render_thread_entry, &frame);
        }
    }

    AOS_semaphore_release(render_work_semaphore, 4, 0);

    AOS_thread_awake_after_time(1000000);
    AOS_thread_sleep();

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
        AOS_H_printf("temporary program loader has found %s\n", partition_names[i]);
    }
    u64 slot_index = 0;

    f64 cursor_x = 0.0;
    f64 cursor_y = 0.0;
    f64 new_cursor_x = 0.0;
    f64 new_cursor_y = 0.0;

    u8 is_moving_window = 0;
    f64 start_move_x = 0.0;
    f64 start_move_y = 0.0;

    u8 is_resizing_window = 0;
    f64 start_resize_cursor_x = 0.0;
    f64 start_resize_cursor_y = 0.0;
    u8 resize_x_invert = 0;
    u8 resize_y_invert = 0;
    u64 start_resize_window_width = 0;
    u64 start_resize_window_height = 0;
    s64 start_resize_window_x = 0;
    s64 start_resize_window_y = 0;

    u8 is_fullscreen_mode = 0;

    f64 last_frame_time = 0.0;
    f64 rolling_time_passed = 0.0;
    f64 rolling_frame_time = 0.0;

while(1) {

    { // Read from stdin
        while(1)
        {
            u64 byte_count;
            AOS_stream_take(AOS_STREAM_STDIN, 0, 0, &byte_count);
            char character;
            if(byte_count && AOS_stream_take(AOS_STREAM_STDIN, &character, 1, &byte_count))
            {
                if(window_count)
                {
                    AOS_stream_put(windows[window_count-1].owned_out_stream, &character, 1);
                }
                else
                { AOS_H_printf("you typed the character %c on stdin\n    If you had a window in focus this would get passed through to that program's stdin.\n", character); }
            }
            else { break; }
        }
    }

    { // Read stdout of windows
        for(u64 i = 0; i < window_count; i++)
        {
            u64 byte_count = 0;
            do {
                u8 b;
                if(AOS_stream_take(windows[i].owned_in_stream, &b, 1, &byte_count))
                {
                    AOS_stream_put(AOS_STREAM_STDOUT, &b, 1);
                }
            } while(byte_count);
        }
    }

    { // Mouse events
        u64 mouse_event_count = AOS_get_rawmouse_events(0, 0);
        AOS_RawMouseEvent mouse_events[mouse_event_count];
        mouse_event_count = AOS_get_rawmouse_events(mouse_events, mouse_event_count);
        for(u64 i = 0; i < mouse_event_count; i++)
        {
            new_cursor_x += mouse_events[i].delta_x;
            new_cursor_y += mouse_events[i].delta_y;

            if(mouse_events[i].pressed && mouse_events[i].button == 0 && !is_resizing_window)
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
            else if(mouse_events[i].pressed && mouse_events[i].button == 2 && !is_moving_window)
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
                    is_resizing_window = 1;
                    start_resize_window_width = temp.width;
                    start_resize_window_height = temp.height;
                    start_resize_window_x = temp.x;
                    start_resize_window_y = temp.y;
                    start_resize_cursor_x = cursor_x;
                    start_resize_cursor_y = cursor_y;
                    resize_x_invert = temp.x + ((s64)temp.width / 2) > (s64)cursor_x;
                    resize_y_invert = temp.y + ((s64)temp.height / 2) > (s64)cursor_y;
                }
            }
            else if(mouse_events[i].released && mouse_events[i].button == 2)
            {
                is_resizing_window = 0;
            }
        }
    }

    { // Keyboard events
        u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
        AOS_KeyboardEvent kbd_events[kbd_event_count];
        kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
        for(u64 i = 0; i < kbd_event_count; i++)
        {
            if(kbd_events[i].event == KEYBOARD_EVENT_NOTHING)
            { continue; }

            if(is_fullscreen_mode)
            {
                if( kbd_events[i].event == KEYBOARD_EVENT_PRESSED &&
                    kbd_events[i].scancode == 39 &&
                    /*ctrl*/ kbd_events[i].current_state.keys_down[0] & (1 << 5) &&
                    window_count)
                {
                    if(AOS_surface_stop_forwarding_to_consumer(0))
                    { is_fullscreen_mode = 0; }
                }
                else
                {
                    AOS_forward_keyboard_events(&kbd_events[i], 1, windows[window_count-1].pid);
                }
                continue;
            }

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
                else if(scancode == 39 &&
                    /*ctrl*/ kbd_events[i].current_state.keys_down[0] & (1 << 5) &&
                    window_count)
                {
                    if(!is_fullscreen_mode)
                    {
                        if(AOS_surface_forward_to_consumer(0, windows[window_count-1].consumer))
                        {
                            is_fullscreen_mode = 1;
                            is_moving_window = 0;
                            is_resizing_window = 0;
                        }
                    }
                }

                if(scancode == 35 && slot_index < slot_count)
                {
                    for(u64 i = 0; window_count + 1 < 300 && i < 2000; i++)
                    {
                    u64 pid = 0;
                    if(AOS_create_process_from_file(partitions[slot_index], &pid))
                    {
                        AOS_H_printf("PROCESS CREATED, PID=%llu\n", pid);
                        u64 con = 0;
                        if(AOS_surface_consumer_create(pid, &con))
                        {
                            windows[window_count].pid = pid;
                            windows[window_count].consumer = con;
                            windows[window_count].x = 20 + window_count*7;
                            windows[window_count].y = 49*window_count;
                            if(windows[window_count].y > 400) { windows[window_count].y = 400; }
                            windows[window_count].width = 0;
                            windows[window_count].height = 0;
                            windows[window_count].new_width = 64;
                            windows[window_count].new_height = 64;
                            windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                            windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                            windows[window_count].we_have_frame = 0;
                            AOS_process_create_out_stream(pid, 0, &windows[window_count].owned_in_stream);
                            AOS_process_create_in_stream(pid, &windows[window_count].owned_out_stream, 0);
                            AOS_process_start(pid);
                            window_count++;

                            if(is_moving_window)
                            {
                                Window temp = windows[window_count-2];
                                windows[window_count-2] = windows[window_count-1];
                                windows[window_count-1] = temp;
                            }
                        }
                        else { AOS_H_printf("Failed to create consumer for PID: %llu\n", pid); }
                    }
                    else { AOS_H_printf("failed to create process."); }
                    }
                }
            }
            else
            {
            }
//            AOS_H_printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
        }
    }
//f64 pre_sleep = AOS_time_get_seconds();
    u64 AOS_wait_surface = 0;
    AOS_thread_awake_on_surface(&AOS_wait_surface, 1);
    AOS_thread_awake_on_mouse();
    AOS_thread_awake_on_keyboard();
    AOS_thread_sleep();
//AOS_H_printf("temp slept for %lf seconds\n", AOS_time_get_seconds() - pre_sleep);

    Framebuffer* fb = 0x54000;
    u64 fb_page_count = AOS_surface_acquire(0, 0, 0);
    if(AOS_surface_acquire(0, fb, fb_page_count))
    {
        double time_frame_start = AOS_time_get_seconds();

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

        u64 column_count = (fb->width / 8);
        u64 row_count = (fb->height / 16);

        u8 bottom_banner[256];
        bottom_banner[0] = 0;
        u64 cvo = 0;
        if(AOS_get_vo_id(&cvo))
        {
            AOS_H_sprintf(bottom_banner, "Virtual Output #%llu", cvo);
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
            u64 start_x = 0; if(windows[j].x < 0) { start_x = (s64)(-windows[j].x); }
            u64 start_y = 0; if(windows[j].y < 0) { start_y = (s64)(-windows[j].y); }

            u64 end_x = windows[j].width;
            if(windows[j].x >= fb->width) { end_x = 0; }
            else if(windows[j].x + (s64)windows[j].width > fb->width)
            { end_x = (u64)((s64)fb->width - windows[j].x); }

            u64 end_y = windows[j].height;
            if(windows[j].y >= fb->height) { end_y = 0; }
            else if(windows[j].y + (s64)windows[j].height > fb->height)
            { end_y = (u64)((s64)fb->height - windows[j].y); }

            f32 red = 0.2;
            f32 green = 0.2;
            f32 blue = 70.0/255.0;
            if(j + 1 == window_count && (is_moving_window || is_resizing_window))
            { blue = 180.0/255.0; }
            else if(j + 1 == window_count)
            { blue = 140.0/255.0; }

            if(!windows[j].we_have_frame ||
                windows[j].width <= 2*BORDER_SIZE || windows[j].height <= 2*BORDER_SIZE)
            {
                for(u64 y = start_y; y < end_y; y++)
                for(u64 x = start_x; x < end_x; x++)
                {
                    if((s64)x + windows[j].x < 0 || (s64)y + windows[j].y < 0 ||
                       (s64)x + windows[j].x >= fb->width || (s64)y + windows[j].y >= fb->height)
                    { continue; }
                    u64 external_x = (u64)((s64)x + windows[j].x);
                    u64 external_y = (u64)((s64)y + windows[j].y);
                    u64 i = external_x + (external_y * fb->width);

                    fb->data[i*4 + 0] = red;
                    fb->data[i*4 + 1] = green;
                    fb->data[i*4 + 2] = blue;
                    fb->data[i*4 + 3] = 1.0;
                }
                continue;
            }

            for(u64 y = start_y; y < end_y; y++)
            for(u64 x = start_x; x < end_x; x++)
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

#if 0
// this is the good version with clamping and alpha
                    float cover = 1.0 - clamp_01(windows[j].fb->data[k*4 + 3]);
        fb->data[i*4 + 0] = clamp_01(fb->data[i*4 + 0] * cover + clamp_01(windows[j].fb->data[k*4 + 0]));
        fb->data[i*4 + 1] = clamp_01(fb->data[i*4 + 1] * cover + clamp_01(windows[j].fb->data[k*4 + 1]));
        fb->data[i*4 + 2] = clamp_01(fb->data[i*4 + 2] * cover + clamp_01(windows[j].fb->data[k*4 + 2]));
#else
// this is the trash version with no clamping and no alpha
        fb->data[i*4 + 0] = windows[j].fb->data[k*4 + 0];
        fb->data[i*4 + 1] = windows[j].fb->data[k*4 + 1];
        fb->data[i*4 + 2] = windows[j].fb->data[k*4 + 2];
#endif
        fb->data[i*4 + 3] = 1.0;
                }
                else
                {
                    fb->data[i*4 + 0] = red;
                    fb->data[i*4 + 1] = green;
                    fb->data[i*4 + 2] = blue;
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

        // frame time, bottom right
        f64 time_frame_end = AOS_time_get_seconds();
        f64 frame_time = (time_frame_end-time_frame_start) *1000.0;
        rolling_frame_time = rolling_frame_time * 0.9 + frame_time * 0.1;
        frame_time = rolling_frame_time;
        u8 frame_counter_string[16];
        AOS_H_sprintf(frame_counter_string, "%4.4lf ms", frame_time);
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

        // time since last frame, bottom right, above frametime
        f64 time_right_now = AOS_time_get_seconds();
        f64 time_passed = (time_right_now-last_frame_time) *1000.0;
        rolling_time_passed = rolling_time_passed *0.9 + time_passed *0.1;
        time_passed = rolling_time_passed;
        last_frame_time = time_right_now;
        AOS_H_sprintf(frame_counter_string, "%4.4lf ms", time_passed);
        u64 time_passed_counter_width = strlen(frame_counter_string) * 8;
        u64 passed_xoff = fb->width - time_passed_counter_width;
        s64 bottom_banner_up = bottom_banner_y - 16;
        if(bottom_banner_up < 0) { bottom_banner_y = 0; }
        u64 bottom_banner_end = bottom_banner_up + 16;
        if(bottom_banner_end > fb->height) { bottom_banner_end = fb->height; }
        for(u64 y = bottom_banner_up; y < bottom_banner_end; y++)
        for(u64 x = 0; x < time_passed_counter_width; x++)
        {
            u64 font_id = frame_counter_string[x/8];
            if(font8_16_pixel_filled(font_id, x%8, y - bottom_banner_up))
            {
                u64 i = (x+passed_xoff) + (y * fb->width);
                fb->data[i*4 + 0] = (f32) (fb->data[i*4 + 0] < 0.5);
                fb->data[i*4 + 1] = (f32) (fb->data[i*4 + 1] < 0.5);
                fb->data[i*4 + 2] = (f32) (fb->data[i*4 + 2] < 0.5);
                fb->data[i*4 + 3] = 1.0;
            }
        }

        assert(AOS_surface_commit(0), "commited successfully");

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

                // do resize, this is releated to consumers
                if(is_resizing_window && i + 1 == window_count)
                {
                    s64 new_width = (s64)(cursor_x - start_resize_cursor_x);
                    s64 new_height = (s64)(cursor_y - start_resize_cursor_y);
                    if(resize_x_invert) {
                        windows[i].x = start_resize_window_x + new_width;
            if(windows[i].x > start_resize_window_x + (s64)start_resize_window_width - 2*BORDER_SIZE)
            { windows[i].x = start_resize_window_x + (s64)start_resize_window_width - 2*BORDER_SIZE; }
                        new_width *= -1;
                    }
                    if(resize_y_invert) {
                        windows[i].y = start_resize_window_y + new_height;
            if(windows[i].y > start_resize_window_y + (s64)start_resize_window_height - 2*BORDER_SIZE)
            { windows[i].y = start_resize_window_y + (s64)start_resize_window_height - 2*BORDER_SIZE; }
                        new_height *= -1;
                    }
                    new_width  += start_resize_window_width;
                    new_height += start_resize_window_height;
                    if(new_width < 2*BORDER_SIZE) { new_width = 2*BORDER_SIZE; }
                    if(new_height < 2*BORDER_SIZE) { new_height = 2*BORDER_SIZE; }
                    windows[i].new_width = (u64)new_width;
                    windows[i].new_height = (u64)new_height;
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
    }
}
}

