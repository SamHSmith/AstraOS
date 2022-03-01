#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

#include "../common/maths.h"
#include "../common/spinlock.h"

#include "font8_16.h"
#include "sv_qwerty.h"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}


int strncmp( const char * s1, const char * s2, u64 n )
{
    while ( n && *s1 && ( *s1 == *s2 ) )
    {
        ++s1;
        ++s2;
        --n;
    }
    if ( n == 0 )
    {
        return 0;
    }
    else
    {
        return ( *(unsigned char *)s1 - *(unsigned char *)s2 );
    }
}

u64 strn_str_match(u8* a, u8* b, u64 n)
{
    while(n && *b && (*a == *b))
    {
        a++; b++;
        n--;
    }
    return !n && *b == 0;
}

u64 strn_strn_match(u8* a, u8* b, u64 n, u64 n2)
{
    if(n != n2) { return 0; }
    while(n && (*a == *b))
    {
        a++; b++;
        n--;
    }
    return !n;
}

#include "parsing.c"

typedef struct
{
    s64 start_index;
    s64 end_index;
} TextLine;

u64 dir_id_stack[64];
u64 dir_id_stack_index = 0;

u8* text_buffer = 0x30405000;
u64 text_len = 0;

u8* pre_send_to_stdin = 0x30404000;
u64 pre_send_to_stdin_len = 0;

u8* catting_and_zeroing_buffer = 0x30403000;

#define MAX_PARTITION_COUNT 64
u64 partitions[MAX_PARTITION_COUNT];
u8 partition_names[MAX_PARTITION_COUNT][64];
u64 partition_name_lens[MAX_PARTITION_COUNT];

u16 process_surfaces[1024/sizeof(u16)];
u64 process_surface_count = 0;

u64 process_is_running = 0;
u64 process_pid;
u64 process_stdin;
u64 process_stdout;
u64 consumer_handle;
u8 has_consumer = 0;
u8 surface_visible = 0;
Spinlock surface_visible_lock;
u8 show_console = 0;

const char* EGA_IPFC_API_NAME = "embedded_gui_application_ipfc_api_v1";
// f1 is get surfaces
// f2 is show
// f3 is hide
void embedded_gui_application_ipfc_api_entry(u64 source_pid, u16 function_index, void* static_data_1024b)
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    if(function_index == 0)
    {
        u16* copy_to = static_data_1024b;
        for(u64 i = 0; i < 1024/sizeof(u16) && i < process_surface_count; i++)
        { copy_to[i] = process_surfaces[i]; }
        AOS_IPFC_return(process_surface_count);
    }
    else if(function_index == 1)
    {
        spinlock_acquire(&surface_visible_lock);
        surface_visible = 1;
        spinlock_release(&surface_visible_lock);
        AOS_IPFC_return(0);
    }
    else if(function_index == 2)
    {
        spinlock_acquire(&surface_visible_lock);
        surface_visible = 0;
        spinlock_release(&surface_visible_lock);
        AOS_IPFC_return(0);
    }
    AOS_IPFC_return(0);
}

void dave_term_printf(const char* format, ...)
{
  va_list va;
  va_start(va, format);
  s32 ret = AOS_H_vsnprintf(text_buffer + text_len, (12 * 0x1000) - text_len, format, va);
  va_end(va);
  text_len += ret;

  if(ret < 0)
  {
      u64* nullptr = 0;
      *nullptr = 5;
  }
}

void _start()
{
    u64 root_directory_id = 0;
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    dir_id_stack_index = 0;
    dir_id_stack[dir_id_stack_index] = root_directory_id;

    AOS_H_printf("Welcome to dave's terminal, not-emulator\n");

    spinlock_create(&surface_visible_lock);
    // setting up ega interface
    {
        u64 handler_name_len = strlen(EGA_IPFC_API_NAME);
        u64 handler_stacks_start = 0x3241234000;
        AOS_alloc_pages(handler_stacks_start, 1);
        u64 ipfc_handler;
        if(!
        AOS_IPFC_handler_create(EGA_IPFC_API_NAME, handler_name_len, embedded_gui_application_ipfc_api_entry,
                                handler_stacks_start, 1, 1, &ipfc_handler)
        )
        { AOS_H_printf("failed to init ega ipfc handler. Something is very wrong.\n"); }
    }

    u64 partition_count = AOS_directory_get_files(root_directory_id, 0, 0);
    if(partition_count > MAX_PARTITION_COUNT)
    { partition_count = MAX_PARTITION_COUNT; }
    partition_count = AOS_directory_get_files(root_directory_id, partitions, partition_count);
    for(u64 i = 0; i < partition_count; i++)
    {
        partition_names[i][0] = 0;
        partition_name_lens[i] = AOS_file_get_name(partitions[i], partition_names[i], 64);
        partition_name_lens[i]--;
        AOS_H_printf("dave's terminal loader has found %.*s\n", partition_name_lens[i], partition_names[i]);
    }

    AOS_alloc_pages(text_buffer, 12);
    text_len = 0;

    text_buffer[text_len + 0] = ')';
    text_buffer[text_len + 1] = '>';
    text_buffer[text_len + 2] = ' ';
    text_len += 3;

    AOS_alloc_pages(pre_send_to_stdin, 1);
    pre_send_to_stdin_len = 0;

    AOS_alloc_pages(catting_and_zeroing_buffer, 1);

    u8 is_running_as_twa = 0;
    u64 twa_session_id;
    u64 twa_window_handle = 0;
    {
        u8* name = "thunder_windowed_application_ipfc_api_v1";
        u64 name_len = strlen(name);
        if(AOS_IPFC_init_session(name, name_len, &twa_session_id))
        {
            is_running_as_twa = 1;
            // create window
            u64 scratch[1024/8];
            if(AOS_IPFC_call(twa_session_id, 0, 0, &scratch))
            {
                twa_window_handle = scratch[0];
                AOS_H_printf("Created a thunder window! handle = %llu\n", twa_window_handle);
            }
            else
            {
                AOS_IPFC_close_session(twa_session_id);
                is_running_as_twa = 0;
                AOS_H_printf("Failed to create thunder window!\n");
            }
        }
        else
        { AOS_H_printf("Failed to init thunder session\n"); }
    }

    spinlock_acquire(&surface_visible_lock);
    while(1)
    {
        spinlock_release(&surface_visible_lock);

#define DEBUG_IPFC_TIME 0

        u16 surfaces[512];
        u16 surface_count = 0;

        if(is_running_as_twa)
        {
#if DEBUG_IPFC_TIME
            f64 sec_before_call = AOS_H_time_get_seconds();
#endif
            u64 scratch[1024/8];
            scratch[0] = twa_window_handle;
            surface_count = AOS_IPFC_call(twa_session_id, 2, scratch, surfaces);
#if DEBUG_IPFC_TIME
            f64 sec_after_call = AOS_H_time_get_seconds();
            AOS_H_printf("time to get surfaces via ipfc : %5.5lf ms\n", (sec_after_call - sec_before_call) * 1000.0);
#endif
        }

        AOS_thread_awake_on_surface(&surfaces, surface_count);
        AOS_thread_awake_after_time(100000);  // haha, this sleep causes frame drops
        AOS_thread_sleep();
        spinlock_acquire(&surface_visible_lock);

        if(process_is_running && !AOS_process_is_alive(process_pid)) // running process has exited
        {
            // read remaining stdout
            {
                u64 byte_count;
                AOS_stream_take(process_stdout, 0, 0, &byte_count);
                u8 scratch[byte_count];
                u64 taken_count = AOS_stream_take(process_stdout, scratch, byte_count, &byte_count);
                if(text_len + taken_count >= 4096 * 12)
                {
                    for(u64 i = 0; i < text_len - taken_count; i++)
                    {
                        text_buffer[i] = text_buffer[i + taken_count];
                    }
                    text_len -= taken_count;
                }
                for(u64 i = 0; i < taken_count; i++)
                {
                    text_buffer[text_len + i] = scratch[i];
                }
                text_len += taken_count;
            }

            AOS_out_stream_destroy(process_stdin);
            AOS_in_stream_destroy(process_stdout);
            process_is_running = 0;
            has_consumer = 0;
            text_buffer[text_len + 0] = ')';
            text_buffer[text_len + 1] = '>';
            text_buffer[text_len + 2] = ' ';
            text_len += 3;
            pre_send_to_stdin_len = 0;
        }

        if(!process_is_running)
        {
            show_console = 0;
            surface_visible = 0;
        }

        {
            if(!has_consumer)
            { surface_visible = 0; }

            if(show_console)
            { AOS_surface_stop_forwarding_to_consumer(surfaces[0]); }
            else
            {
                if(surface_visible)
                { AOS_surface_forward_to_consumer(surfaces[0], consumer_handle); }
                else
                { AOS_surface_stop_forwarding_to_consumer(surfaces[0]); }
            }
        }

        { // Keyboard events
            u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
            AOS_KeyboardEvent kbd_events[kbd_event_count];
            kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
            for(u64 i = 0; i < kbd_event_count; i++)
            {
                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_NOTHING)
                { continue; }

                u8 is_forwarding_input = !show_console && surface_visible;

                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_PRESSED)
                {
                    u64 scancode = kbd_events[i].scancode;

                    if(scancode == 21)
                    {
                        if(text_len > 0 && pre_send_to_stdin_len > 0) { text_len--; }
                        if(pre_send_to_stdin_len > 0) { pre_send_to_stdin_len--; }
                    }
                    else if(scancode == 8 && (kbd_events[i].current_state.keys_down[0] & 0x2) == 0x2)
                    {
                        AOS_process_exit();
                    }
                    else if(scancode == 50 && (kbd_events[i].current_state.keys_down[0] & 0x22) == 0x22)
                    {
                        if(!AOS_process_kill(process_pid))
                        {
                            dave_term_printf("KILL FAILED\n");
                        }
                        else
                        {
                            dave_term_printf("KILLED PROCESS\n");
                        }
                    }
                    else if(!is_forwarding_input && scancode == 35)
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
                            Expression expressions[64];
                            u64 expression_count = parse_string_into_expressions(pre_send_to_stdin, pre_send_to_stdin_len, expressions, 64, dir_id_stack[dir_id_stack_index]);

                            for(u64 i = 0; i < expression_count; i++)
                            {
                                dave_term_printf("Exp: \"%.*s\" : ", expressions[i].text_len, expressions[i].text);
                                if(expressions[i].is_file)
                                { dave_term_printf("file\n"); }
                                else
                                { dave_term_printf("string\n"); }
                            }

                            if(expression_count && strn_str_match(expressions[0].text, "exit", expressions[0].text_len))
                            {
                                AOS_process_exit();
                            }
                            else if(expression_count && strn_str_match(expressions[0].text, "ls", expressions[0].text_len))
                            {
                                u8 name_buffer[64];
                                u64 dir_id = dir_id_stack[dir_id_stack_index];
                                {
                                    u64 file_count = AOS_directory_get_files(dir_id, 0, 0);
                                    u64 files[file_count];
                                    file_count = AOS_directory_get_files(dir_id, files, file_count);
                                    for(u64 i = 0; i < file_count; i++)
                                    {
                                        AOS_file_get_name(files[i], name_buffer, 64);
                                        dave_term_printf("F: %s\n", name_buffer);
                                    }
                                }

                                {
                                    u64 dir_count = AOS_directory_get_subdirectories(dir_id, 0, 0);
                                    u64 dirs[dir_count];
                                    dir_count = AOS_directory_get_subdirectories(dir_id, dirs, dir_count);

                                    for(u64 i = 0; i < dir_count; i++)
                                    {
                                        AOS_directory_get_name(dirs[i], name_buffer, 64);
                                        dave_term_printf("D: %s\n", name_buffer);
                                    }
                                }
                            }
                            else if(expression_count && strn_str_match(expressions[0].text, "pwd", expressions[0].text_len))
                            {
                                u8 name_buffer[64];
                                for(u64 i = 0; i <= dir_id_stack_index; i++)
                                {
                                    if(i != 0)
                                    { dave_term_printf("/"); }
                                    AOS_directory_get_name(dir_id_stack[i], name_buffer, 64);
                                    dave_term_printf("%s", name_buffer);
                                }
                                dave_term_printf("\n");
                            }
                            else if(expression_count && strn_str_match(expressions[0].text, "cd", expressions[0].text_len))
                            {
                                if(expression_count != 2)
                                { dave_term_printf("usage: cd %%directory_to_change_into%%\n"); }
                                else
                                {
                                    if(strn_str_match(expressions[1].text, "..", expressions[1].text_len))
                                    {
                                        if(dir_id_stack_index)
                                        { dir_id_stack_index--; }
                                        else
                                        { dave_term_printf("Already in root directory\n"); }
                                    }
                                    else
                                    {
                                        u8 name_buffer[64];
                                        u64 dir_id = dir_id_stack[dir_id_stack_index];
                                        u64 dir_count = AOS_directory_get_subdirectories(dir_id, 0, 0);
                                        u64 dirs[dir_count];
                                        dir_count = AOS_directory_get_subdirectories(dir_id, dirs, dir_count);

                                        u8 found_directory = 0;
                                        for(u64 i = 0; i < dir_count; i++)
                                        {
                                            AOS_directory_get_name(dirs[i], name_buffer, 64);
                                            if(strn_str_match(expressions[1].text, name_buffer, expressions[1].text_len))
                                            {
                                                found_directory = 1;
                                                dir_id_stack_index++;
                                                dir_id_stack[dir_id_stack_index] = dirs[i];
                                                break;
                                            }
                                        }
                                        if(!found_directory)
                                        { dave_term_printf("\"%.*s\", no such directory.\n", expressions[1].text_len, expressions[1].text); }
                                    }
                                }
                            }
                            else if(expression_count && strn_str_match(expressions[0].text, "cat_intrin", expressions[0].text_len))
                            {
                                if(expression_count != 2)
                                { dave_term_printf("usage: cat_intrin %%file%%\n"); }
                                else
                                {
                                    {
                                        u8 name_buffer[64];
                                        u64 dir_id = dir_id_stack[dir_id_stack_index];
                                        u64 file_count = AOS_directory_get_files(dir_id, 0, 0);
                                        u64 files[file_count];
                                        file_count = AOS_directory_get_files(dir_id, files, file_count);

                                        u8 found_file = 0;
                                        u64 file_id;
                                        for(u64 i = 0; i < file_count; i++)
                                        {
                                            AOS_file_get_name(files[i], name_buffer, 64);
                                            if(strn_str_match(expressions[1].text, name_buffer, expressions[1].text_len))
                                            {
                                                found_file = 1;
                                                file_id = files[i];
                                                break;
                                            }
                                        }
                                        if(!found_file)
                                        { dave_term_printf("\"%.*s\", no such file.\n", expressions[1].text_len, expressions[1].text); }
                                        else
                                        {
                                            u64 bytes_left = AOS_file_get_size(file_id);
                                            u64 block_count = AOS_file_get_block_count(file_id);

                                            u64 op[2];
                                            op[1] = catting_and_zeroing_buffer;
                                            dave_term_printf("##FILE START##\n");
                                            u8 failed = 0;
                                            for(u64 i = 0; i < block_count; i++)
                                            {
                                                op[0] = i;
                                                if(AOS_file_read_blocks(file_id, op, 1))
                                                {
                                                    u64 copy_count = PAGE_SIZE;
                                                    if(copy_count > bytes_left) { copy_count = bytes_left; }
                                                    bytes_left -= copy_count;

                                                    if(text_len + copy_count >= 4096 * 12)
                                                    {
                                                        for(u64 i = 0; i < text_len - copy_count; i++)
                                                        { text_buffer[i] = text_buffer[i + copy_count]; }
                                                        text_len -= copy_count;
                                                    }

                                                    for(u64 j = 0; j < copy_count; j++)
                                                    { text_buffer[text_len++] = catting_and_zeroing_buffer[j]; }
                                                }
                                                else
                                                { dave_term_printf("Failed to read file...\n"); failed = 1; break; }
                                            }
                                            if(!failed) { dave_term_printf("\n##FILE END##\n"); }
                                            dave_term_printf("File Size: %llu B, Block Count: %llu\n", AOS_file_get_size(file_id), block_count);
                                        }
                                    }
                                }
                            }
                            else if(expression_count && strn_str_match(expressions[0].text, "zero_file", expressions[0].text_len))
                            {
                                if(expression_count != 2)
                                { dave_term_printf("usage: zero_file %%file%%\n"); }
                                else
                                {
                                    {
                                        u8 name_buffer[64];
                                        u64 dir_id = dir_id_stack[dir_id_stack_index];
                                        u64 file_count = AOS_directory_get_files(dir_id, 0, 0);
                                        u64 files[file_count];
                                        file_count = AOS_directory_get_files(dir_id, files, file_count);

                                        u8 found_file = 0;
                                        u64 file_id;
                                        for(u64 i = 0; i < file_count; i++)
                                        {
                                            AOS_file_get_name(files[i], name_buffer, 64);
                                            if(strn_str_match(expressions[1].text, name_buffer, expressions[1].text_len))
                                            {
                                                found_file = 1;
                                                file_id = files[i];
                                                break;
                                            }
                                        }
                                        if(!found_file)
                                        { dave_term_printf("\"%.*s\", no such file.\n", expressions[1].text_len, expressions[1].text); }
                                        else
                                        {
                                            u64 bytes_left = AOS_file_get_size(file_id);
                                            u64 block_count = AOS_file_get_block_count(file_id);

                                            u64 op[2];
                                            op[1] = catting_and_zeroing_buffer;
                                            for(u64 i = 0; i < PAGE_SIZE; i++)
                                            { catting_and_zeroing_buffer[i] = 0; }
                                            u8 failed = 0;
                                            for(u64 i = 0; i < block_count; i++)
                                            {
                                                op[0] = i;
                                                if(!AOS_file_write_blocks(file_id, op, 1))
                                                { dave_term_printf("Failed to zero file...\n"); failed = 1; break; }
                                            }
                                            dave_term_printf("File Size: %llu B, Block Count: %llu\n", AOS_file_get_size(file_id), block_count);
                                        }
                                    }
                                }
                            }
                            else if(expression_count)
                            {
                            for(u64 i = 0; i < partition_count + 1; i++)
                            {
                                if(i == partition_count)
                                {
                                    dave_term_printf("Program does not exist\n");
                                    break;
                                }
                                u64 is_equal = expressions[0].text_len == partition_name_lens[i];
                                if(is_equal)
                                {
                                for(u64 j = 0; j < partition_name_lens[i]; j++)
                                {
                                    if(expressions[0].text[j] != partition_names[i][j])
                                    { is_equal = 0; break; }
                                }
                                }
                                if(is_equal)
                                {
                                    dave_term_printf("Starting %.*s...", partition_name_lens[i], partition_names[i]);
                                    u64 pid;
                                    if(AOS_create_process_from_file(partitions[i], &pid))
                                    {
                                        process_is_running = 1;
                                        process_pid = pid;
                                        AOS_process_create_in_stream(pid, &process_stdin, 0);
                                        AOS_process_create_out_stream(pid, 0, &process_stdout);
                                        {
                                            u64 foriegn_dir_id;
                                            AOS_directory_give(pid, &dir_id_stack[dir_id_stack_index], &foriegn_dir_id, 1, 1);
                                            AOS_H_printf("dave terminal is giving access to working directory via handle = %llu.\n", foriegn_dir_id);
                                        }
                                        { // program arguments
                                            for(u64 i = 1; i < expression_count; i++)
                                            {
                                                if(expressions[i].is_file)
                                                {
                                                    u64 foreign_file_id;
                                                    AOS_file_give(pid, &expressions[i].file_id, &foreign_file_id, 1, 1);
                                                    AOS_H_printf("dave terminal is giving the program access a file via handle %llu.\n", foreign_file_id);
                                                    AOS_process_add_program_argument_file(pid, foreign_file_id);
                                                }
                                                else
                                                {
                                                    AOS_process_add_program_argument_string(pid, expressions[i].text, expressions[i].text_len);
                                                }
                                            }
                                        }
                                        AOS_process_start(pid);
                                        dave_term_printf("SUCCESS\n");

                                        u64 con, surface_slot;
                                        if(AOS_surface_consumer_create(pid, &con, &surface_slot))
                                        {
                                            has_consumer = 1;
                                            consumer_handle = con;
                                            process_surfaces[0] = surface_slot;
                                            process_surface_count = 1;
                                        }
                                    }
                                    else
                                    {
                                        dave_term_printf("FAILED.\n");
                                    }
                                    break;
                                }
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
                    else if(scancode == 22 && kbd_events[i].current_state.keys_down[0] == 0x20)
                    {
                        show_console = !show_console;
                    }
                    else if(!is_forwarding_input && pre_send_to_stdin_len < 4096)
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
                else // key released
                {
                    u64 scancode = kbd_events[i].scancode;

                }

                if(is_forwarding_input)
                {
                    AOS_forward_keyboard_events(&kbd_events[i], 1, process_pid);
                }
                else
                {
                    AOS_H_printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
                }
            }
        }

        // Handle stdin/stdout for process
        if(process_is_running)
        {
            u64 byte_count;
            AOS_stream_take(process_stdout, 0, 0, &byte_count);
            u8 scratch[byte_count];
            u64 taken_count = AOS_stream_take(process_stdout, scratch, byte_count, &byte_count);
            if(text_len + taken_count >= 4096 * 12)
            {
                for(u64 i = 0; i < text_len - taken_count; i++)
                {
                    text_buffer[i] = text_buffer[i + taken_count];
                }
                text_len -= taken_count;
            }
            for(u64 i = 0; i < taken_count; i++)
            {
                text_buffer[text_len + i] = scratch[i];
            }
            text_len += taken_count;
        }

        AOS_Framebuffer* fb = 0x696969000;
        u64 fb_page_count = 9001;
        if(surface_count && AOS_surface_acquire(surfaces[0], fb, fb_page_count))
        {
            u64 column_count = fb->width / 8;
            u64 row_count = (fb->height / 16) + 1;
            u64 text_offset = 16 - (fb->height % 16);

            u64 practical_column_count = 1;
            if(column_count > practical_column_count) { practical_column_count = column_count; }

            f64 time_frame_start = AOS_H_time_get_seconds();
            f64 when_in_second_frame_start = modulo(time_frame_start, 1.0);

            u8 draw_text_buffer[row_count * practical_column_count];
            s64 draw_text_buffer_index = (s64)(row_count * practical_column_count);

            TextLine lines[row_count];
            s64 line_index = (s64)row_count - 1;
            u8 should_create_new_line = 0;
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
                            s64 shift_amount = practical_column_count - line_length;
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

                if(should_create_new_line)
                {
                    line_index--;
                    lines[line_index].start_index = draw_text_buffer_index;
                    lines[line_index].end_index = draw_text_buffer_index - 1;
                    should_create_new_line = 0;
                }

                draw_text_buffer_index--;
                if(draw_text_buffer_index < 0)
                {
                    new_zero = i;
                    break;
                }
                draw_text_buffer[draw_text_buffer_index] = text_buffer[i];

                lines[line_index].start_index = draw_text_buffer_index;
                if(lines[line_index].end_index - lines[line_index].start_index + 1 >= practical_column_count)
                {
                    if(line_index > 0)
                    {
                        should_create_new_line = 1;
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
                s64 line_length = lines[line_index].end_index - lines[line_index].start_index + 1;
                s64 shift_amount = practical_column_count - line_length;
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

            new_zero -= 2*4096;
            if(new_zero < 0) { new_zero = 0; }
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
                    if(column >= practical_column_count || row >= row_count ||
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
            AOS_surface_commit(surfaces[0]);
        }
    }
}
