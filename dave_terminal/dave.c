#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

#include "font8_16.h"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

typedef struct
{
    s64 start_index;
    s64 end_index;
} TextLine;

void _start()
{
    AOS_H_printf("Welcome to dave's terminal, not-emulator\n");

    u8* text_buffer = "Dave is the coolest guy in the world!\n\nDave\nDouble Dave\n\nIs this rendering well?";
    //text_buffer = "aaaabbbbccccffff\nadbdcded";
    u64 text_len = strlen(text_buffer);

    while(1)
    {
        u64 surface_handle = 0;
        AOS_thread_awake_on_surface(&surface_handle, 1);
        AOS_thread_sleep();

        AOS_Framebuffer* fb = 0x696969000;
        u64 fb_page_count = 9001;
        if(AOS_surface_acquire(surface_handle, fb, fb_page_count))
        {
            u64 column_count = fb->width / 8;
            u64 row_count = (fb->height / 16) + 1;
            u64 text_offset = 16 - (fb->height % 16);

            u8 draw_text_buffer[row_count * column_count];
            s64 draw_text_buffer_index = (s64)(row_count * column_count);

            TextLine lines[row_count];
            s64 line_index = (s64)row_count - 1;
            s64 current_chunk_start = line_index;
            lines[line_index].end_index = draw_text_buffer_index - 1;
            for(s64 i = text_len - 1; i >= 0; i--)
            {
                if(text_buffer[i] == '\n')
                {
                    if(text_buffer[i+1] == '\n')
                    {
                        lines[line_index].start_index = 1;
                        lines[line_index].end_index = 0;
                    }
                    else
                    {
                        if(current_chunk_start > line_index)
                        {
                            s64 line_length = lines[line_index].end_index + 1 - lines[line_index].start_index;
                            s64 shift_amount = column_count - line_length;
                            lines[line_index].end_index += shift_amount;
                            for(s64 r = line_index+1; r < current_chunk_start; r++)
                            {
                                lines[r].start_index += shift_amount;
                                lines[r].end_index += shift_amount;
                            }
                            lines[current_chunk_start].start_index += shift_amount;
                        }
                    }
                    if(line_index <= 0) { break; }
                    line_index--;
                    lines[line_index].start_index = draw_text_buffer_index;
                    lines[line_index].end_index = draw_text_buffer_index - 1;

                    current_chunk_start = line_index;
                    continue;
                }

                draw_text_buffer_index--;
                draw_text_buffer[draw_text_buffer_index] = text_buffer[i];

                lines[line_index].start_index = draw_text_buffer_index;
                if(lines[line_index].end_index + 1 - lines[line_index].start_index > column_count)
                {
                    if(line_index > 0)
                    {
                        line_index--;
                        lines[line_index].start_index = draw_text_buffer_index;
                        lines[line_index].end_index = draw_text_buffer_index - 1;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if(current_chunk_start > line_index)
            {
                s64 line_length = lines[line_index].end_index + 1 - lines[line_index].start_index;
                s64 shift_amount = column_count - line_length;
                lines[line_index].end_index += shift_amount;
                for(s64 r = line_index+1; r < current_chunk_start; r++)
                {
                    lines[r].start_index += shift_amount;
                    lines[r].end_index += shift_amount;
                }
                lines[current_chunk_start].start_index += shift_amount;
            }
            line_index--;
            while(line_index >= 0)
            {
                lines[line_index].start_index = 1;
                lines[line_index].end_index = 0;
                line_index--;
            }

            for(u64 y = 0; y < fb->height; y++)
            {
                for(u64 x = 0; x < fb->width; x++)
                {
                    u64 pixel = x + y * fb->width;
                    fb->data[pixel*4 + 0] = 80.0/255.0;
                    fb->data[pixel*4 + 1] = 6.0/255.0;
                    fb->data[pixel*4 + 2] = 96.0/255.0;
                    fb->data[pixel*4 + 3] = 1.0;

                    u64 column = x/8;
                    u64 row = (y + text_offset)/16;
                    s64 line_length = lines[row].end_index + 1 - lines[row].start_index;
                    if(column >= column_count || row >= row_count ||
                        (s64)column >= line_length)
                    { continue; }
                    {
                        u64 local_x = x % 8;
                        u64 local_y = (y+text_offset) % 16;
                        u8 character = draw_text_buffer[lines[row].start_index + (s64)column];
                        if(font8_16_pixel_filled(character, local_x, local_y))
                        {
                            fb->data[pixel*4 + 0] = 240.0/255.0;
                            fb->data[pixel*4 + 1] = 230.0/255.0;
                            fb->data[pixel*4 + 2] = 230.0/255.0;
                            fb->data[pixel*4 + 3] = 1.0;
                        }
                    }
                }
            }
            AOS_surface_commit(surface_handle);
        }
    }
}
