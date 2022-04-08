#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

#include "../common/maths.h"

#include "sv_qwerty.c"
#include "font8_16.c"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

u8* text_backing_buffer = 0x55543000;
u64 text_backing_buffer_used_bytes = 0;
u64 text_backing_buffer_pages_allocated = 0;

u8* allocate_in_backing_buffer(u64 num_bytes)
{
    u64 needed_page_count = (text_backing_buffer_used_bytes + num_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    if(needed_page_count > text_backing_buffer_pages_allocated)
    {
        AOS_alloc_pages(text_backing_buffer + text_backing_buffer_pages_allocated * PAGE_SIZE, needed_page_count - text_backing_buffer_pages_allocated);
        text_backing_buffer_pages_allocated = needed_page_count;
//        AOS_H_printf("The text backing buffer is now %llu pages.\n", text_backing_buffer_pages_allocated);
    }
    u8* ptr = text_backing_buffer + text_backing_buffer_used_bytes;
    text_backing_buffer_used_bytes += num_bytes;
    return ptr;
}

u8* text_buffer = "Here is the test text that we will be using.\n\n\n Is It Not Great!";
u64 text_buffer_len;

s64 target_cursor_x = 0;
s64 target_cursor_y = 0;

s64 cursor_x = 0;
s64 cursor_y = 0;

f64 fdynamic_cursor_x = 0;
f64 fdynamic_cursor_y = 0;

u64 last_character_width = 0;

typedef struct
{
    u64 litteral_line_index;
    u64 offset_into_line;
    u64 len;
} VisibleLine;
#define visible_line_buffer(line) (litteral_lines[(line).litteral_line_index].data + (line).offset_into_line)

// start and end refer to the backing buffer
typedef struct
{
    u8* data;
    u64 len;
    u64 display_line_index;
} LitteralLine;

#define LINE_COUNT 20000
LitteralLine litteral_lines[LINE_COUNT];
u64 litteral_line_count = 0;

VisibleLine visible_lines[LINE_COUNT];
u64 visible_line_count = 0;

void parse_into_visible_lines(u64 width)
{
    if(!width)
    { "assert"[0] = 0; }

    visible_line_count = 0;
    for(u64 i = 0; i < litteral_line_count; i++)
    {
        u64 offset = 0;
        u64 count = (litteral_lines[i].len + width - 1) / width;

        for(u64 j = 0; j < count; j++)
        {
            visible_lines[visible_line_count].litteral_line_index = i;
            visible_lines[visible_line_count].offset_into_line = offset;
            visible_lines[visible_line_count].len = litteral_lines[i].len - offset;
            if(visible_lines[visible_line_count].len > width)
            { visible_lines[visible_line_count].len = width; }
            visible_line_count++;
            offset += width;
        }

        if(!count)
        {
            visible_lines[visible_line_count].litteral_line_index = i;
            visible_lines[visible_line_count].offset_into_line = 0;
            visible_lines[visible_line_count].len = 0;
            visible_line_count++;
        }
    }
}

u64 visible_line_of_coordinate(u64 column, u64 line)
{
    u8 found = 0;
    u64 the_one = 0;
    for(u64 i = 0; i < visible_line_count; i++)
    {
        if( visible_lines[i].litteral_line_index == target_cursor_y &&
            target_cursor_x >= visible_lines[i].offset_into_line)
        {
            found = 1;
            the_one = i;
        }
        else
        {
            if(found)
            { return the_one; }
        }
    }
}

void _start()
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    AOS_H_printf("Welcome to the wonderful editor TE. aka Text Editor.\n");

    // init
    text_buffer_len = strlen(text_buffer);

    // parse init text into litteral lines
    {
        u8* current_start;
        u64 current_len;
        u8 new_line = 0;
        for(u64 i = 0; i <= text_buffer_len; i++)
        {
            if(i == 0 || new_line)
            {
                current_start = text_buffer + i;
                current_len = 0;
                new_line = 0;
            }
            if(i == text_buffer_len || text_buffer[i] == '\n')
            {
                if(current_len)
                {
                    u8* buf = allocate_in_backing_buffer(current_len);
                    for(u64 j = 0; j < current_len; j++)
                    { buf[j] = current_start[j]; }
                    litteral_lines[litteral_line_count].data = buf;
                }
                litteral_lines[litteral_line_count].len = current_len;
                litteral_line_count++;
                new_line = 1;
            }
            else
            { current_len++; }
        }
    }

    parse_into_visible_lines(20);

    for(u64 i = 0; i < visible_line_count; i++)
    {
        AOS_H_printf("%.*s\n", visible_lines[i].len, visible_line_buffer(visible_lines[i]));
    }

    u8 is_running_as_ega = 0;
    u64 ega_session_id;
    {
        u8* name = "embedded_gui_application_ipfc_api_v1";
        u64 name_len = strlen(name);
        if(AOS_IPFC_init_session(name, name_len, &ega_session_id))
        {
            is_running_as_ega = 1;
            AOS_IPFC_call(ega_session_id, 1, 0, 0); // show ega
        }
    }
    if(!is_running_as_ega)
    { AOS_H_printf("Failed to start as ega.\n"); AOS_process_exit(); }

    while(1)
    {
        u16 surfaces[512];
        u64 surface_count = AOS_IPFC_call(ega_session_id, 0, 0, surfaces);

        if(!surface_count)
        { AOS_H_printf("There was no surface to render too.\n"); continue; }

        AOS_thread_awake_on_surface(surfaces, 1);
        AOS_thread_sleep();

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
                { target_cursor_x--; }
                else if(scancode == 100)
                { target_cursor_y--; }
                else if(scancode == 101)
                { target_cursor_y++; }
                else if(scancode == 102)
                { target_cursor_x++; }
                else if(scancode == 8)
                { target_cursor_x = 0; }
                else
                { target_cursor_x += 1; }
            }

            if(target_cursor_x < 0) { target_cursor_x = 0; }
            if(target_cursor_y < 0) { target_cursor_y = 0; }

            if(target_cursor_y > litteral_line_count)
            { target_cursor_y = litteral_line_count; }
            if(target_cursor_x > litteral_lines[target_cursor_y].len)
            { target_cursor_x = litteral_lines[target_cursor_y].len; }
        }

        AOS_Framebuffer* fb = 0x1234123000;
        if(!AOS_surface_acquire(surfaces[0], fb, 5000))
        { continue; }
        // RENDER START
        f64 start_frame_time = AOS_H_time_get_seconds();

        u64 character_width = fb->width / 8;
        u64 character_height = (fb->height + 16 - 1) / 16;

        last_character_width = character_width;

        parse_into_visible_lines(character_width);

        // setup on screen cursor coordinates
        {
            u64 visible_line_index = visible_line_of_coordinate(target_cursor_x, target_cursor_y);
            cursor_x = target_cursor_x - (s64)visible_lines[visible_line_index].offset_into_line;
            cursor_y = visible_line_index;
        }

        u8 display_character_buffer[character_width*character_height];

        for(u64 y = 0; y < character_height; y++)
        {
            for(u64 x = 0; x < character_width; x++)
            {
                u8* current_display_char = display_character_buffer + x + y * character_width;

                if(y >= visible_line_count || x >= visible_lines[y].len)
                { *current_display_char = 0; continue; }

                *current_display_char = visible_line_buffer(visible_lines[y])[x];
            }
        }

#define CURSOR_SPEED_FACTOR 0.4
#define CURSOR_SPEED_POWER 0.86

        //s64 wrap_times = target_cursor_x / character_width;
        //cursor_x = target_cursor_x % character_width;
        //cursor_y = target_cursor_y + wrap_times;

        {
            f64 diffx = fdynamic_cursor_x - (f64)cursor_x;
            f64 diffy = fdynamic_cursor_y - (f64)cursor_y;
            f64 dx = -CURSOR_SPEED_FACTOR * sign(diffx) * power(abs(diffx), CURSOR_SPEED_POWER);
            f64 dy = -CURSOR_SPEED_FACTOR * sign(diffy) * power(abs(diffy), CURSOR_SPEED_POWER);
            if(abs(dx) > abs(diffx))
            { dx = -diffx; }
            if(abs(dy) > abs(diffy))
            { dy = -diffy; }
            fdynamic_cursor_x += dx;
            fdynamic_cursor_y += dy;
        }

        s64 dynamic_cursor_x = (s64)(fdynamic_cursor_x * 8.0 + 0.5);
        s64 dynamic_cursor_y = (s64)(fdynamic_cursor_y * 16.0 + 0.5);

        for(u64 y = 0; y < fb->height; y++)
        {
            for(u64 x = 0; x < fb->width; x++)
            {
                f32* pixel = fb->data + (y * fb->width + x) * 4;

                pixel[0] = 84.0f / 255.0f;
                pixel[1] = 47.0f / 255.0f;
                pixel[2] = 76.0f / 255.0f;
                pixel[3] = 1.0f;

                u64 text_local_x = x;
                u64 text_local_y = y;

                u64 character_coord_x = text_local_x / 8;
                u64 character_coord_y = text_local_y / 16;

                u8 fill_here = x < character_width*8 &&
                font8_16_pixel_filled(
                    display_character_buffer[character_coord_x + character_coord_y * character_width],
                    text_local_x % 8,
                    text_local_y % 16);


                u8 is_cursor = 0;
                if( (s64)x - dynamic_cursor_x < 8 && (s64)y - dynamic_cursor_y < 16 &&
                    (s64)x - dynamic_cursor_x >= 0 && (s64)y - dynamic_cursor_y >= 0 )
                { is_cursor = 1; }

                if(is_cursor)
                { fill_here = !fill_here; }

                if(fill_here)
                {
                    pixel[0] = 252.0f / 255.0f;
                    pixel[1] = 227.0f / 255.0f;
                    pixel[2] = 227.0f / 255.0f;
                    pixel[3] = 1.0f;
                }
            }
        }

        f64 end_frame_time = AOS_H_time_get_seconds();

        // RENDER FINISH
        AOS_surface_commit(surfaces[0]);

        //AOS_H_printf("Render time : %5.5lf ms\n", (end_frame_time - start_frame_time) * 1000.0);
    }

    AOS_process_exit();
}
