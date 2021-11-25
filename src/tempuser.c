#include "../userland/aos_helper.h"

#include "samorak.c"
//#include "qwerty.h"

#include "font8_16.c"

#define BORDER_SIZE 3

typedef struct
{
    Spinlock lock;
    u16 other_surface_slot;
    u64 window_handle;
    s64 x;
    s64 y;
    s64 new_x;
    s64 new_y;
    u64 pid;
    u64 consumer;
    u64 width;
    u64 height;
    u64 new_width;
    u64 new_height;
    AOS_Framebuffer* fb;
    u64 fb_page_count;
    u8 we_have_frame;
} Window;

f32 clamp_01(f32 f)
{
    if(f < 0.0) { f = 0.0; }
    else if(f > 1.0) { f = 1.0; }
    return f;
}

Window windows[84];
u64 window_count = 0;
u64 window_handle_counter = 0;

typedef struct
{
    u64 pid;
    u64 owned_in_stream;
    u64 owned_out_stream;
} Program;

Program programs[84];
u64 program_count;

volatile u64 slot_count, slot_index;
#define MAX_SLOT_COUNT 128
u64 partitions[MAX_SLOT_COUNT];
u8 partition_names[MAX_SLOT_COUNT][64];
u64 partition_name_lens[MAX_SLOT_COUNT];


u8 bottom_banner[256];
u64 bottom_banner_len;
u64 bottom_banner_y;


u8 is_moving_window = 0;
u8 is_resizing_window = 0;

volatile AOS_Framebuffer* render_buffer;

volatile Spinlock tempuser_printout_lock;
atomic_s64 render_work_started;
atomic_s64 render_work_done;
u64 render_work_done_semaphore;

f64 cursor_x = 0.0;
f64 cursor_y = 0.0;
f64 new_cursor_x = 0.0;
f64 new_cursor_y = 0.0;

f64 start_move_x = 0.0;
f64 start_move_y = 0.0;

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

typedef struct
{
    u64 canvas_start;
    u64 canvas_end;
    AOS_Framebuffer* buffer;
    u32 xoffset;
    u32 y_index;
    f32 red; f32 green; f32 blue;
} RenderArea;
void render_function(u64 threadid, u64 total_threads)
{
    RenderArea areas[window_count*2*2];
    for(u64 y = threadid; y < render_buffer->height; y+=total_threads)
    {
        u64 area_count = 0;

        for(u64 i = 0; i < window_count; i++)
        {
            {
            if(!(windows[i].y <= (s64)y && windows[i].y + (s64)windows[i].height > (s64)y))
            { continue; }
            u64 start, end;
            {
                s64 _start = windows[i].x;
                if(_start < 0) { _start = 0; }
                s64 _end = windows[i].x + (s64)windows[i].width - 1;
                if(_end > (s64)render_buffer->width - 1) { _end = (s64)render_buffer->width - 1; }
                start = (u64)_start;
                end = (u64)_end;
            }
            u64 insert_index = area_count;
            s64 push_count = 0;
            for(u64 i = 0; i < area_count; i++)
            {
                if(areas[i].canvas_start >= start)
                { insert_index = i; break; }
            }

            {
                push_count = 1;
                for(u64 j = insert_index; j < area_count; j++)
                {
                    if(areas[j].canvas_end >= end)
                    { break; }
                    push_count--;
                }
                if(insert_index)
                {
                    if(areas[insert_index-1].canvas_end > end)
                    { push_count++; }
                }
                if(push_count > 0)
                {
                    for(s64 i = ((s64)area_count)-1; i >= (s64)insert_index; i--)
                    { areas[i+push_count] = areas[i]; }
                    area_count += push_count;
                }
                else if(push_count < 0)
                {
                    for(u64 i = insert_index + (u64)(-push_count); i < area_count; i++)
                    {
                        areas[i - (u64)(-push_count)] = areas[i];
                    }
                    area_count -= (u64)(-push_count);
                }
            }
            if(insert_index)
            {
                u64 prev_index = insert_index - 1;

                if(areas[prev_index].canvas_end > end)
                {
                    u64 new_start = end + 1;
                    areas[insert_index + 1] = areas[prev_index];
                    areas[insert_index + 1].xoffset += new_start - areas[prev_index].canvas_start;
                    areas[insert_index + 1].canvas_start = new_start;
                }
            }
            for(u64 i = 0; i < insert_index; i++)
            {
                if(areas[i].canvas_end >= start)
                { areas[i].canvas_end = start-1; }
            }

            f32 red = 0.2;
            f32 green = 0.2;
            f32 blue = 70.0/255.0;
            if(i + 1 == window_count && (is_moving_window || is_resizing_window))
            { blue = 180.0/255.0; }
            else if(i + 1 == window_count)
            { blue = 140.0/255.0; }

            areas[insert_index].canvas_start = start;
            areas[insert_index].canvas_end = end;
            areas[insert_index].buffer = 0;
            areas[insert_index].red = red;
            areas[insert_index].green = green;
            areas[insert_index].blue = blue;
            }
            {
            if(!windows[i].we_have_frame || !(windows[i].y + BORDER_SIZE <= (s64)y && windows[i].y + BORDER_SIZE + windows[i].fb->height > (s64)y))
            { continue; }
            u64 start, end;
            {
                s64 _start = windows[i].x + BORDER_SIZE;
                if(windows[i].width <= 2*BORDER_SIZE) { continue; }
                if(windows[i].height <= 2*BORDER_SIZE) { continue; }
                if((s64)y >= windows[i].y + (s64)windows[i].height - BORDER_SIZE) { continue; }
                s64 _end = windows[i].x + (s64)windows[i].width - BORDER_SIZE;
                if(_end > render_buffer->width) { _end = render_buffer->width; }
                if((u64)(_end - _start) >= windows[i].fb->width)
                { _end = _start + (s64)windows[i].fb->width - 1; }
                if(_start < 0) { _start = 0; }
                if(_end > windows[i].x + (s64)windows[i].width - 1 - BORDER_SIZE)
                { _end = windows[i].x + (s64)windows[i].width - 1 - BORDER_SIZE; }
                if(_end <= _start) { continue; }
                start = (u64)_start;
                end = (u64)_end;
            }
            u64 insert_index = area_count;
            s64 push_count = 0;
            for(u64 i = 0; i < area_count; i++)
            {
                if(areas[i].canvas_start >= start)
                { insert_index = i; break; }
            }

            {
                push_count = 1;
                for(u64 j = insert_index; j < area_count; j++)
                {
                    if(areas[j].canvas_end >= end)
                    { break; }
                    push_count--;
                }
                if(insert_index)
                {
                    if(areas[insert_index-1].canvas_end > end)
                    { push_count++; }
                }
                if(push_count > 0)
                {
                    for(s64 i = ((s64)area_count)-1; i >= (s64)insert_index; i--)
                    { areas[i+push_count] = areas[i]; }
                    area_count += push_count;
                }
                else if(push_count < 0)
                {
                    for(u64 i = insert_index + (u64)(-push_count); i < area_count; i++)
                    {
                        areas[i - (u64)(-push_count)] = areas[i];
                    }
                    area_count -= (u64)(-push_count);
                }
            }
            if(insert_index)
            {
                u64 prev_index = insert_index - 1;

                if(areas[prev_index].canvas_end > end)
                {
                    u64 new_start = end + 1;
                    areas[insert_index + 1] = areas[prev_index];
                    areas[insert_index + 1].xoffset += new_start - areas[prev_index].canvas_start;
                    areas[insert_index + 1].canvas_start = new_start;
                }
            }
            for(u64 i = 0; i < insert_index; i++)
            {
                if(areas[i].canvas_end >= start)
                { areas[i].canvas_end = start-1; }
            }

            areas[insert_index].canvas_start = start;
            areas[insert_index].canvas_end = end;
            areas[insert_index].buffer = windows[i].fb;
            areas[insert_index].xoffset = 0;
            if(windows[i].x + BORDER_SIZE < 0) { areas[insert_index].xoffset = (u32)(-(windows[i].x + BORDER_SIZE)); }
            areas[insert_index].y_index = ((s64)y - windows[i].y - BORDER_SIZE);
            }
        }

    u64 area_index = 0;
    for(u64 x = 0; x < render_buffer->width; x++)
    {
        u64 i = x + (y * render_buffer->width);

        render_buffer->data[i*4 + 0] = 17.0/255.0;
        render_buffer->data[i*4 + 1] = 80.0/255.0;
        render_buffer->data[i*4 + 2] = 128.0/255.0;
        render_buffer->data[i*4 + 3] = 1.0;

        u64 c = x / 8;
        u64 r = y / 16;

        if(r == slot_index)
        {
            render_buffer->data[i*4 + 0] = 28.0/255.0;
            render_buffer->data[i*4 + 1] = 133.0/255.0;
            render_buffer->data[i*4 + 2] = 213.0/255.0;
            render_buffer->data[i*4 + 3] = 1.0;
        }
        else if(r < slot_count)
        {
            render_buffer->data[i*4 + 0] = 8.0/255.0;
            render_buffer->data[i*4 + 1] = 37.0/255.0;
            render_buffer->data[i*4 + 2] = 57.0/255.0;
            render_buffer->data[i*4 + 3] = 1.0;
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
            render_buffer->data[i*4 + 0] = 0.909;
            render_buffer->data[i*4 + 1] = 0.89;
            render_buffer->data[i*4 + 2] = 0.772;
            render_buffer->data[i*4 + 3] = 1.0;
        }

        if(area_index < area_count && areas[area_index].canvas_end < x)
        {
            area_index++;
        }

        RenderArea a;
        u8 has_area = 0;
        if(area_index < area_count && areas[area_index].canvas_start <= x && areas[area_index].canvas_end >= x)
        {
            a = areas[area_index];
            has_area = 1;
        }
        if(has_area)
        {
            if(a.buffer)
            {
                u64 local_x = (x - (u64)a.canvas_start) + a.xoffset;
                u64 j = local_x + (a.y_index * a.buffer->width);
                render_buffer->data[i*4 + 0] = a.buffer->data[j*4 + 0];
                render_buffer->data[i*4 + 1] = a.buffer->data[j*4 + 1];
                render_buffer->data[i*4 + 2] = a.buffer->data[j*4 + 2];
                render_buffer->data[i*4 + 3] = 1.0;
            }
            else
            {
                render_buffer->data[i*4 + 0] = a.red;
                render_buffer->data[i*4 + 1] = a.green;
                render_buffer->data[i*4 + 2] = a.blue;
                render_buffer->data[i*4 + 3] = 1.0;
            }
        }
    }
    }

    if(atomic_s64_increment(&render_work_done) + 1 >= total_threads)
    { AOS_semaphore_release(render_work_done_semaphore, 1, 0); }
}

#define WORKER_THREAD_STACK_SIZE (8+((sizeof(RenderArea)*300*2)/4096))
#define THREAD_COUNT 8
#define JOBS_PER_THREAD 8

u64 render_work_semaphore;
void render_thread_entry(u64 thread_number)
{
    while(1)
    {
        assert(AOS_thread_awake_on_semaphore(render_work_semaphore), "actually awaking the semaphore");
        AOS_thread_sleep();
        while(1) {
            s64 jobid = atomic_s64_increment(&render_work_started);
            if(jobid >= THREAD_COUNT * JOBS_PER_THREAD) { break; }
            render_function(jobid, THREAD_COUNT * JOBS_PER_THREAD);
        }
    }
}

RWLock thunder_lock;
const char* TWA_IPFC_API_NAME = "thunder_windowed_application_ipfc_api_v1";
// f0 is create window
// f1 is destroy window
// f2 is get surfaces
void thunder_windowed_application_ipfc_api_entry(u64 source_pid, u16 function_index, void* static_data_1024b)
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, _global_pointer");
    __asm__(".option relax");

    rwlock_acquire_read(&thunder_lock);

    if(function_index == 0)
    {
        rwlock_release_read(&thunder_lock);
        rwlock_acquire_write(&thunder_lock);
        AOS_H_printf("new window! from pid %llu\n", source_pid);
        if(window_count + 1 < 84) // can allocate new window
        {
            u64* window_handle = static_data_1024b;
            *window_handle = window_handle_counter++;
            {
                u64 con = 0;
                u64 surface_slot;
                if(AOS_surface_consumer_create(source_pid, &con, &surface_slot))
                {
                    windows[window_count].pid = source_pid;
                    windows[window_count].consumer = con;
                    windows[window_count].x = 20 + window_count*7;
                    windows[window_count].y = 49*window_count;
                    if(windows[window_count].y > 400) { windows[window_count].y = 400; }
                    windows[window_count].new_x = windows[window_count].x;
                    windows[window_count].new_y = windows[window_count].y;
                    windows[window_count].width = 0;
                    windows[window_count].height = 0;
                    windows[window_count].new_width = 64;
                    windows[window_count].new_height = 64;
                    windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                    windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                    windows[window_count].we_have_frame = 0;
                    windows[window_count].window_handle = *window_handle;
                    windows[window_count].other_surface_slot = surface_slot;
                    window_count++;

                    if(is_moving_window)
                    {
                        Window temp = windows[window_count-2];
                        windows[window_count-2] = windows[window_count-1];
                        windows[window_count-1] = temp;
                    }
                }
                else { AOS_H_printf("Failed to create consumer for PID: %llu\n", source_pid); }
            }
            rwlock_release_write(&thunder_lock);
            AOS_IPFC_return(1);
        }
        else
        {
            rwlock_release_write(&thunder_lock);
            AOS_IPFC_return(0);
        }
    }
    else if(function_index == 1)
    {
        u64* window_handle_pointer = static_data_1024b;
        u64 window_handle = *window_handle_pointer;
        AOS_H_printf("destroy window with handle=%llu! from pid %llu\n", window_handle, source_pid);
        // destroy thingy

        rwlock_release_read(&thunder_lock);
        rwlock_acquire_write(&thunder_lock);

        u64 destroyed = 0;

        for(u64 i = 0; i < window_count; i++)
        {
            if(windows[i].pid != source_pid || windows[i].window_handle != window_handle)
            { continue; }

            destroyed = 1;

            //TODO cleanup
            if(i + 1 == window_count && is_fullscreen_mode)
            {
                AOS_surface_stop_forwarding_to_consumer(0);
                is_fullscreen_mode = 0;
            }

            for(; i + 1 < window_count; i++)
            {
                windows[i] = windows[i+1];
            }
            window_count--;
        }

        rwlock_release_write(&thunder_lock);
        AOS_IPFC_return(destroyed);
    }
    else if(function_index == 2)
    {
        AOS_H_printf("get window surfaces! from pid %llu\n", source_pid);
        u64 window_handle;
        {
            u64* window_handle_pointer = static_data_1024b;
            window_handle = *window_handle_pointer;
        }

        u16* copy_to = static_data_1024b;

        for(u64 i = 0; i < window_count; i++)
        {
            if(windows[i].pid != source_pid || windows[i].window_handle != window_handle)
            { continue; }

            copy_to[0] = windows[i].other_surface_slot;
            rwlock_release_read(&thunder_lock);
            AOS_IPFC_return(1);
        }
        rwlock_release_read(&thunder_lock);
        AOS_IPFC_return(0);
    }

    rwlock_release_read(&thunder_lock);
    AOS_IPFC_return(0);
}

void program_loader_program(u64 drive1_partitions_directory)
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, _global_pointer");
    __asm__(".option relax");

    window_count = 0;
    u8* print_text = "program loader program has started.\n";
    AOS_stream_put(0, print_text, strlen(print_text));

    spinlock_create(&tempuser_printout_lock);
    spinlock_create(&thunder_lock);
    rwlock_acquire_read(&thunder_lock);
    render_work_semaphore = AOS_semaphore_create(0, THREAD_COUNT * JOBS_PER_THREAD);
    render_work_done_semaphore = AOS_semaphore_create(0, 1);
    {
        u64 base_stack_addr = (~(0x1ffffff << 39)) & (~0xfff);
        AOS_alloc_pages(base_stack_addr - (4096*WORKER_THREAD_STACK_SIZE*(THREAD_COUNT+1)), WORKER_THREAD_STACK_SIZE*(THREAD_COUNT+1));

        for(u64 i = 0; i < THREAD_COUNT; i++) // TODO: one day add feature to ask for the "width" of the system
        {
            AOS_TrapFrame frame;
            frame.regs[10] = i;
            frame.regs[8] = base_stack_addr;
            frame.regs[2] = frame.regs[8] - 4 * sizeof(u64);
            base_stack_addr -= 4096*WORKER_THREAD_STACK_SIZE;
            AOS_thread_new(render_thread_entry, &frame, 2);
        }
    }

    slot_count = AOS_directory_get_files(drive1_partitions_directory, 0, 0);
    if(slot_count > MAX_SLOT_COUNT)
    { slot_count = MAX_SLOT_COUNT; }
    slot_count = AOS_directory_get_files(drive1_partitions_directory, partitions, slot_count);
    for(u64 i = 0; i < slot_count; i++)
    {
        partition_names[i][0] = 0;
        AOS_file_get_name(partitions[i], partition_names[i], 64);
        partition_name_lens[i] = strlen(partition_names[i]);
        AOS_H_printf("temporary program loader has found %s\n", partition_names[i]);
    }
    slot_index = 0;

    // setting up twa interface
    {
        u64 handler_name_len = strlen(TWA_IPFC_API_NAME);
        u64 handler_stacks_start = 0x3241234000;
        AOS_alloc_pages(handler_stacks_start, 2);
        if(!AOS_IPFC_handler_create(TWA_IPFC_API_NAME, handler_name_len,
                                    thunder_windowed_application_ipfc_api_entry,
                                    handler_stacks_start, 2, 1, 0)
        )
        { AOS_H_printf("failed to init twa ipfc handler. Something is very wrong.\n"); }
    }

while(1) {

    { // Check for program not alive's
        rwlock_release_read(&thunder_lock);
        rwlock_acquire_write(&thunder_lock);
        for(u64 i = 0; i < window_count; i++)
        {
            if(AOS_process_is_alive(windows[i].pid))
            { continue; }

            //TODO cleanup
            if(i + 1 == window_count && is_fullscreen_mode)
            {
                AOS_surface_stop_forwarding_to_consumer(0);
                is_fullscreen_mode = 0;
            }

            for(u64 j = i; j + 1 < window_count; j++)
            {
                windows[j] = windows[j+1];
            }
            window_count--;
        }
        rwlock_release_write(&thunder_lock);
        rwlock_acquire_read(&thunder_lock);
    }

#if 0
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
#endif

    { // Read stdout of programs
        spinlock_acquire(&tempuser_printout_lock);
        for(u64 i = 0; i < program_count; i++)
        {
            u64 byte_count = 0;
            AOS_stream_take(programs[i].owned_in_stream, 0, 0, &byte_count);
            u8 scratch[512];
            do {
                u64 dummy_byte_count;
                u64 read_count = byte_count;
                if(read_count > 512) { read_count = 512; }
                if(AOS_stream_take(programs[i].owned_in_stream, scratch, read_count, &dummy_byte_count))
                {
                    AOS_stream_put(AOS_STREAM_STDOUT, scratch, read_count);
                    byte_count-=read_count;
                }
            } while(byte_count);
        }
        spinlock_release(&tempuser_printout_lock);
    }

    { // Mouse events
        rwlock_release_read(&thunder_lock);
        rwlock_acquire_write(&thunder_lock);
        u64 mouse_event_count = AOS_get_rawmouse_events(0, 0);
        AOS_RawMouseEvent mouse_events[mouse_event_count];
        mouse_event_count = AOS_get_rawmouse_events(mouse_events, mouse_event_count);
        for(u64 i = 0; i < mouse_event_count; i++)
        {
            new_cursor_x += mouse_events[i].delta_x / 2.0;
            new_cursor_y += mouse_events[i].delta_y / 2.0;

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
                    start_resize_window_x = temp.new_x;
                    start_resize_window_y = temp.new_y;
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
        rwlock_release_write(&thunder_lock);
        rwlock_acquire_read(&thunder_lock);
    }

    { // Keyboard events
        u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
        AOS_KeyboardEvent kbd_events[kbd_event_count];
        kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
        for(u64 i = 0; i < kbd_event_count; i++)
        {
            if(kbd_events[i].event == KEYBOARD_EVENT_NOTHING)
            { continue; }

            if(kbd_events[i].current_state.keys_down[0] & 0x20)
            {
                u8 key_consumed = 1;

                if(kbd_events[i].event == KEYBOARD_EVENT_PRESSED)
                {
                    u64 scancode = kbd_events[i].scancode;

                    if(scancode >= 62 && scancode <= 71)
                    {
                        u64 fkey = scancode - 62;
                        AOS_switch_vo(fkey);
                    }
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
                        else if(AOS_surface_stop_forwarding_to_consumer(0))
                        { is_fullscreen_mode = 0; }
                    }
                    else if(scancode == 100 && slot_index > 0)
                    { slot_index--; }
                    else if(scancode == 101 && slot_index + 1 < slot_count)
                    { slot_index++; }
                    else if(scancode == 35 && slot_index < slot_count)
                    {
                        for(u64 i = 0; program_count + 1 < 84 && window_count + 1 < 84 && i < 1; i++)
                        {
                        u64 pid = 0;
                        if(AOS_create_process_from_file(partitions[slot_index], &pid))
                        {
                            AOS_H_printf("PROCESS CREATED, PID=%llu\n", pid);
                            u64 con = 0;
                            u64 surface_slot;
                            if(AOS_surface_consumer_create(pid, &con, &surface_slot))
                            {
                                windows[window_count].pid = pid;
                                windows[window_count].consumer = con;
                                windows[window_count].x = 20 + window_count*7;
                                windows[window_count].y = 49*window_count;
                                if(windows[window_count].y > 400) { windows[window_count].y = 400; }
                                windows[window_count].new_x = windows[window_count].x;
                                windows[window_count].new_y = windows[window_count].y;
                                windows[window_count].width = 0;
                                windows[window_count].height = 0;
                                windows[window_count].new_width = 64;
                                windows[window_count].new_height = 64;
                                windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                                windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                                windows[window_count].we_have_frame = 0;
                                windows[window_count].window_handle = window_handle_counter++;
                                AOS_process_create_out_stream(pid, 0, &programs[program_count].owned_in_stream);
                                AOS_process_create_in_stream(pid, &programs[program_count].owned_out_stream, 0);
                                AOS_process_start(pid);
                                window_count++;
                                program_count++;

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
                    else
                    { key_consumed = 0; }
                    if(key_consumed) { continue; }
                }
                else
                {
                }
            }

            AOS_forward_keyboard_events(&kbd_events[i], 1, windows[window_count-1].pid);
//            AOS_H_printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
        }
    }
//f64 pre_sleep = AOS_H_time_get_seconds();
    u64 AOS_wait_surface = 0;
    AOS_thread_awake_on_surface(&AOS_wait_surface, 1);
    AOS_thread_awake_on_mouse();
    AOS_thread_awake_on_keyboard();
    AOS_thread_awake_after_time(1000000);
    rwlock_release_read(&thunder_lock);
    AOS_thread_sleep();
    rwlock_acquire_read(&thunder_lock);
//AOS_H_printf("temp slept for %lf seconds\n", AOS_H_time_get_seconds() - pre_sleep);

    Framebuffer* fb = 0x54000;
    u64 fb_page_count = AOS_surface_acquire(0, 0, 0);
    if(AOS_surface_acquire(0, fb, fb_page_count))
    {
        double time_frame_start = AOS_H_time_get_seconds();

        // Fetch from consumers and setup
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
                else
                { AOS_H_printf("frame has been dropped\n"); }

                // do move, not related to consumers
                if(is_moving_window && i + 1 == window_count)
                {
                    windows[i].new_x = (s64)(new_cursor_x - start_move_x);
                    windows[i].new_y = (s64)(new_cursor_y - start_move_y);
                }

                // do resize, this is releated to consumers
                if(is_resizing_window && i + 1 == window_count)
                {
                    s64 new_width = (s64)(new_cursor_x - start_resize_cursor_x);
                    s64 new_height = (s64)(new_cursor_y - start_resize_cursor_y);
                    if(resize_x_invert) {
                        windows[i].new_x = start_resize_window_x + new_width;
            if(windows[i].new_x > start_resize_window_x + (s64)start_resize_window_width - 2*BORDER_SIZE)
            { windows[i].new_x = start_resize_window_x + (s64)start_resize_window_width - 2*BORDER_SIZE; }
                        new_width *= -1;
                    }
                    if(resize_y_invert) {
                        windows[i].new_y = start_resize_window_y + new_height;
            if(windows[i].new_y > start_resize_window_y + (s64)start_resize_window_height - 2*BORDER_SIZE)
            { windows[i].new_y = start_resize_window_y + (s64)start_resize_window_height - 2*BORDER_SIZE; }
                        new_height *= -1;
                    }
                    new_width  += start_resize_window_width;
                    new_height += start_resize_window_height;
                    if(new_width < 2*BORDER_SIZE) { new_width = 2*BORDER_SIZE; }
                    if(new_height < 2*BORDER_SIZE) { new_height = 2*BORDER_SIZE; }
                    windows[i].new_width = (u64)new_width;
                    windows[i].new_height = (u64)new_height;
                }

                AOS_surface_consumer_set_size(
                    windows[i].consumer,
                    windows[i].new_width  -2*BORDER_SIZE,
                    windows[i].new_height -2*BORDER_SIZE
                );
                AOS_surface_consumer_fire(windows[i].consumer);
            }
        }

        u64 column_count = (fb->width / 8);
        u64 row_count = (fb->height / 16);

        bottom_banner[0] = 0;
        u64 cvo = 0;
        if(AOS_get_vo_id(&cvo))
        {
            AOS_H_sprintf(bottom_banner, "Virtual Output #%llu", cvo);
        }
        bottom_banner_len = strlen(bottom_banner);
        if(bottom_banner_len > column_count || row_count <= 1) { bottom_banner_len = column_count; }
        bottom_banner_y = 0;
        if(fb->height > 16) { bottom_banner_y = fb->height - 16; }

        f64 time_render_start = AOS_H_time_get_seconds();
#if 1
        render_buffer = fb;
        atomic_s64_set(&render_work_done, 0);
        atomic_s64_set(&render_work_started, 0);
        AOS_semaphore_release(render_work_semaphore, THREAD_COUNT * JOBS_PER_THREAD, 0);

        assert(AOS_thread_awake_on_semaphore(render_work_done_semaphore), "actually awaking semaphore");
        AOS_thread_sleep();
#else
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
#endif
        f64 time_render_end = AOS_H_time_get_seconds();
        //printf("it took %lf ms to render\n", (time_render_end - time_render_start)*1000.0);

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
        f64 time_frame_end = AOS_H_time_get_seconds();
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
        f64 time_right_now = AOS_H_time_get_seconds();
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

        // move windows and resize them
        {
            rwlock_release_read(&thunder_lock);
            rwlock_acquire_write(&thunder_lock);
            for(u64 i = 0; i < window_count; i++)
            {
                windows[i].x = windows[i].new_x;
                windows[i].y = windows[i].new_y;
                windows[i].width = windows[i].new_width;
                windows[i].height = windows[i].new_height;
            }
            rwlock_release_write(&thunder_lock);
            rwlock_acquire_read(&thunder_lock);
        }

        cursor_x = new_cursor_x;
        cursor_y = new_cursor_y;
    }
}
}

