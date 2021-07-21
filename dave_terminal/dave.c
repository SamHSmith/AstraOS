#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

#include "../common/maths.h"

#include "font8_16.h"
#include "ANSI_dvorak.h"

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

#define MAX_PARTITION_COUNT 64
u64 partitions[MAX_PARTITION_COUNT];
u8 partition_names[MAX_PARTITION_COUNT][64];
u64 partition_name_lens[MAX_PARTITION_COUNT];

void _start()
{
    AOS_H_printf("Welcome to dave's terminal, not-emulator\n");

    u64 drive1_partitions_directory = 0;
    u64 partition_count = AOS_directory_get_files(drive1_partitions_directory, 0, 0);
    if(partition_count > MAX_PARTITION_COUNT)
    { partition_count = MAX_PARTITION_COUNT; }
    partition_count = AOS_directory_get_files(drive1_partitions_directory, partitions, partition_count);
    for(u64 i = 0; i < partition_count; i++)
    {
        partition_names[i][0] = 0;
        AOS_file_get_name(partitions[i], partition_names[i], 64);
        partition_name_lens[i] = strlen(partition_names[i]);
        AOS_H_printf("dave's terminal loader has found %s\n", partition_names[i]);
    }

    u64 process_is_running = 0;
    u64 process_pid;
    u64 process_stdin;
    u64 process_stdout;

    u8* text_buffer = 0x30405000;
    AOS_alloc_pages(text_buffer, 10);
    u64 text_len = 0;

    text_buffer[text_len + 0] = ')';
    text_buffer[text_len + 1] = '>';
    text_buffer[text_len + 2] = ' ';
    text_len += 3;

    u8* pre_send_to_stdin = 0x30404000;
    AOS_alloc_pages(pre_send_to_stdin, 1);
    u64 pre_send_to_stdin_len = 0;

    while(1)
    {
        u64 surface_handle = 0;
        AOS_thread_awake_on_surface(&surface_handle, 1);
        AOS_thread_sleep();

        if(process_is_running && !AOS_process_is_alive(process_pid))
        {
            AOS_H_printf("process is not alive!\n");
            process_is_running = 0;
            text_buffer[text_len + 0] = ')';
            text_buffer[text_len + 1] = '>';
            text_buffer[text_len + 2] = ' ';
            text_len += 3;
            // TODO stream cleanup
        }

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
                    if(scancode == 21)
                    {
                        if(text_len > 0 && pre_send_to_stdin_len > 0) { text_len--; }
                        if(pre_send_to_stdin_len > 0) { pre_send_to_stdin_len--; }
                    }
                    else if(scancode == 35)
                    {
                        text_buffer[text_len] = '\n';
                        text_len++;
                        if(process_is_running)
                        {
                            pre_send_to_stdin[pre_send_to_stdin_len] = '\n';
                            pre_send_to_stdin_len++;
                            AOS_stream_put(process_stdin, pre_send_to_stdin, pre_send_to_stdin_len);
                        }
                        else
                        {
                            for(u64 i = 0; i < partition_count + 1; i++)
                            {
                                if(i == partition_count)
                                {
                                    s32 written_count = AOS_H_sprintf(text_buffer + text_len, "Program does not exist\n");
                                    if(written_count < 0)
                                    {
                                        u64* nullptr = 0;
                                        *nullptr = 5;
                                    }
                                    text_len += (u64)written_count;
                                    break;
                                }
                                u64 is_equal = pre_send_to_stdin_len == partition_name_lens[i];
                                if(is_equal)
                                {
                                for(u64 j = 0; j < partition_name_lens[i]; j++)
                                {
                                    if(pre_send_to_stdin[j] != partition_names[i][j])
                                    { is_equal = 0; break; }
                                }
                                }
                                if(is_equal)
                                {
                                    s32 written_count = AOS_H_sprintf(text_buffer + text_len, "Starting %.*s...", partition_name_lens[i], partition_names[i]);
                                    if(written_count < 0)
                                    {
                                        u64* nullptr = 0;
                                        *nullptr = 5;
                                    }
                                    text_len += (u64)written_count;
                                    u64 pid;
                                    if(AOS_create_process_from_file(partitions[i], &pid))
                                    {
                                        process_is_running = 1;
                                        process_pid = pid;
                                        AOS_process_create_in_stream(pid, &process_stdin, 0);
                                        AOS_process_create_out_stream(pid, 0, &process_stdout);
                                        AOS_process_start(pid);
                                        {
                                            written_count = AOS_H_sprintf(text_buffer + text_len, "SUCCESS\n");
                                            if(written_count < 0)
                                            {
                                                u64* nullptr = 0;
                                                *nullptr = 5;
                                            }
                                            text_len += (u64)written_count;
                                        }
                                    }
                                    else
                                    {
                                        written_count = AOS_H_sprintf(text_buffer + text_len, "FAILED\n");
                                        if(written_count < 0)
                                        {
                                            u64* nullptr = 0;
                                            *nullptr = 5;
                                        }
                                        text_len += (u64)written_count;
                                    }
                                    break;
                                }
                            }
                        }
                        pre_send_to_stdin_len = 0;
                        if(!process_is_running)
                        {
                            text_buffer[text_len + 0] = ')';
                            text_buffer[text_len + 1] = '>';
                            text_buffer[text_len + 2] = ' ';
                            text_len += 3;
                        }
                    }
                    else if(pre_send_to_stdin_len < 4096)
                    {
                        u8 character = scancode_to_u8(scancode, kbd_events[i].current_state);
                        if(character != 0)
                        {
                            text_buffer[text_len] = character;
                            text_len++;
                            pre_send_to_stdin[pre_send_to_stdin_len] = character;
                            pre_send_to_stdin_len++;
                        }
                    }
                }
                else
                {
                    u64 scancode = kbd_events[i].scancode;

                }
                //AOS_H_printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        }

        // Handle stdin/stdout for process
        if(process_is_running)
        {
            u64 byte_count;
            AOS_stream_take(process_stdout, 0, 0, &byte_count);
            u8 scratch[byte_count];
            u64 taken_count = AOS_stream_take(process_stdout, scratch, byte_count, &byte_count);
            for(u64 i = 0; i < taken_count; i++)
            {
                text_buffer[text_len + i] = scratch[i];
            }
            text_len += taken_count;
        }


        AOS_Framebuffer* fb = 0x696969000;
        u64 fb_page_count = 9001;
        if(AOS_surface_acquire(surface_handle, fb, fb_page_count))
        {
            u64 column_count = fb->width / 8;
            u64 row_count = (fb->height / 16) + 1;
            u64 text_offset = 16 - (fb->height % 16);

            f64 time_frame_start = AOS_time_get_seconds();
            f64 when_in_second_frame_start = modulo(time_frame_start, 1.0);

            u8 draw_text_buffer[row_count * column_count];
            s64 draw_text_buffer_index = (s64)(row_count * column_count);

            TextLine lines[row_count];
            s64 line_index = (s64)row_count - 1;
            s64 current_chunk_start = line_index;
            lines[line_index].start_index = draw_text_buffer_index;
            lines[line_index].end_index = draw_text_buffer_index - 1;

            if(when_in_second_frame_start < 0.5)
            { text_buffer[text_len] = 219; }
            else
            { text_buffer[text_len] = ' '; }
            text_len++;

            s64 new_zero = 0;
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
                    if(line_index <= 0)
                    {
                        new_zero = i;
                        break;
                    }
                    line_index--;
                    lines[line_index].start_index = draw_text_buffer_index;
                    lines[line_index].end_index = draw_text_buffer_index - 1;

                    current_chunk_start = line_index;
                    continue;
                }

                draw_text_buffer_index--;
                if(draw_text_buffer_index < 0)
                {
                    new_zero = i;
                    break;
                }
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
                        new_zero = i;
                        break;
                    }
                }
            }
            text_len--; // remove cursor
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

            if(new_zero + 3 > text_len) { new_zero = text_len - 3; }
            for(u64 i = 0; i < text_len - new_zero; i++)
            {
                text_buffer[i] = text_buffer[i+new_zero];
            }
            text_len -= new_zero;

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
