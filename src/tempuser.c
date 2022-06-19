#include "../userland/aos_helper.h"

#include "../userland/aso_tonitrus.h"

#include "samorak.c"
//#include "qwerty.h"

#include "font8_16.c"

void user_assert(u64 condition, u8* message)
{
    if(condition) { return; }

    AOS_H_printf("User assertion failed: \"%s\"\n", message);
    for(u64 i = 0; i < 100000000; i++) { __asm__("nop"); }
    u64* ptr = 0;
    *ptr = 5;
}

#define BORDER_SIZE 4
#define WINDOW_BANNER_HEIGHT 19

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
    f64 cursor_x;
    f64 cursor_y;
    AOS_Framebuffer* fb;
    u64 fb_page_count;
    u8 we_have_frame;

    u64 middle_buffer_handle;
    u64 foreign_middle_buffer_handle;
    PicturaAnimata* pictura;
    u8 displaying_primary;

    u64 foreign_commit_semaphore_handle;
    u64 own_commit_semaphore_handle;

    u64 foreign_acquire_semaphore_handle;
    u64 own_acquire_semaphore_handle;

    u8 dropped_frame;
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

            u8 red = 51;
            u8 green = 51;
            u8 blue = 70;
            if(i + 1 == window_count && (is_moving_window || is_resizing_window))
            { blue = 180; }
            else if(i + 1 == window_count)
            { blue = 140; }

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

        render_buffer->data[i*3 + 0] = 17;
        render_buffer->data[i*3 + 1] = 80;
        render_buffer->data[i*3 + 2] = 128;

        u64 c = x / 8;
        u64 r = y / 16;

        if(r == slot_index)
        {
            render_buffer->data[i*3 + 0] = 28;
            render_buffer->data[i*3 + 1] = 133;
            render_buffer->data[i*3 + 2] = 213;
        }
        else if(r < slot_count)
        {
            render_buffer->data[i*3 + 0] = 8;
            render_buffer->data[i*3 + 1] = 37;
            render_buffer->data[i*3 + 2] = 57;
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
            render_buffer->data[i*3 + 0] = 232;
            render_buffer->data[i*3 + 1] = 227;
            render_buffer->data[i*3 + 2] = 197;
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
                render_buffer->data[i*3 + 0] = a.buffer->data[j*3 + 0];
                render_buffer->data[i*3 + 1] = a.buffer->data[j*3 + 1];
                render_buffer->data[i*3 + 2] = a.buffer->data[j*3 + 2];
            }
            else
            {
                render_buffer->data[i*3 + 0] = a.red;
                render_buffer->data[i*3 + 1] = a.green;
                render_buffer->data[i*3 + 2] = a.blue;
                render_buffer->data[i*3 + 3] = 1.0;
            }
        }
    }
    }

    if(atomic_s64_increment(&render_work_done) + 1 >= total_threads)
    { AOS_semaphore_release(render_work_done_semaphore, 1, 0); }
}

#define WORKER_THREAD_STACK_SIZE (8+((sizeof(RenderArea)*300*2)/4096))
#define THREAD_COUNT 0
#define JOBS_PER_THREAD 8

u64 render_work_semaphore;
void render_thread_entry(u64 thread_number)
{
    while(1)
    {
        user_assert(AOS_thread_awake_on_semaphore(render_work_semaphore), "actually awaking the semaphore");
        AOS_thread_sleep();
        while(1) {
            s64 jobid = atomic_s64_increment(&render_work_started);
            if(jobid >= THREAD_COUNT * JOBS_PER_THREAD) { break; }
            render_function(jobid, THREAD_COUNT * JOBS_PER_THREAD);
        }
    }
}

u64 thunder_lock_mutex_semaphore_handle_signal;
u64 thunder_lock_mutex_semaphore_handle_wait;
const char* TWA_IPFC_API_NAME = "thunder_windowed_application_ipfc_api_v1";
// f0 is create window
// f1 is destroy window
// f2 is get surfaces
// f3 is get cursor pos
void thunder_windowed_application_ipfc_api_entry(u64 source_pid, u16 function_index, void* static_data_1024b)
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, _global_pointer");
    __asm__(".option relax");

//    f64 before = AOS_H_time_get_seconds();
    u64 before_lock_time = kernel_rdtime();
    aso_semaphorum_medium_expecta(thunder_lock_mutex_semaphore_handle_wait);
    u64 post_lock_time = kernel_rdtime();
    //AOS_H_printf("thunder lock took %llu \u03BCs\n", ((post_lock_time - before_lock_time) * 1000000) / MACHINE_TIMER_SECOND);

    if(function_index == 0)
    {
        spinlock_acquire(&tempuser_printout_lock);
        AOS_H_printf("new window! from pid %llu\n", source_pid);
        spinlock_release(&tempuser_printout_lock);
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
                    windows[window_count].new_width = 100;
                    windows[window_count].new_height = 100;
                    windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                    windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                    windows[window_count].we_have_frame = 0;
                    windows[window_count].window_handle = *window_handle;
                    windows[window_count].other_surface_slot = surface_slot;

                    PicturaAnimata* pictura = 0x379342000 + magnitudo_picturae_animatae_disce(300, 300) * 4096 * windows[window_count].window_handle;
                    if(!pictura_animata_crea(300, 300, &windows[window_count].middle_buffer_handle, pictura))
                    { AOS_H_printf("I am sad\n"); }
                    windows[window_count].displaying_primary = 1;
                    windows[window_count].pictura = pictura;

                    if(!aso_chartam_mediam_da(
                        windows[window_count].middle_buffer_handle,
                        windows[window_count].pid,
                        &windows[window_count].foreign_middle_buffer_handle))
                    {
                        AOS_H_printf("failed to give middle buffer\n");
                    }

                    if(!aso_semaphorum_medium_crea(
                        0,
                        1,
                        &windows[window_count].foreign_commit_semaphore_handle,
                        windows[window_count].pid,
                        &windows[window_count].own_commit_semaphore_handle,
                        U64_MAX))
                    {
                        AOS_H_printf("failed to create commit semaphore\n");
                    }

                    if(!aso_semaphorum_medium_crea(
                        1,
                        1,
                        &windows[window_count].own_acquire_semaphore_handle,
                        U64_MAX,
                        &windows[window_count].foreign_acquire_semaphore_handle,
                        windows[window_count].pid))
                    {
                        AOS_H_printf("failed to create acquire semaphore\n");
                    }

                    windows[window_count].dropped_frame = 1;

                    window_count++;

                    if(is_moving_window)
                    {
                        Window temp = windows[window_count-2];
                        windows[window_count-2] = windows[window_count-1];
                        windows[window_count-1] = temp;
                    }
                }
                else
                {
                    spinlock_acquire(&tempuser_printout_lock);
                    AOS_H_printf("Failed to create consumer for PID: %llu\n", source_pid);
                    spinlock_release(&tempuser_printout_lock);
                }
            }
            aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
            AOS_IPFC_return(1);
        }
        else
        {
            aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
            AOS_IPFC_return(0);
        }
    }
    else if(function_index == 1)
    {
        u64* window_handle_pointer = static_data_1024b;
        u64 window_handle = *window_handle_pointer;
        spinlock_acquire(&tempuser_printout_lock);
        AOS_H_printf("destroy window with handle=%llu! from pid %llu\n", window_handle, source_pid);
        spinlock_release(&tempuser_printout_lock);
        // destroy thingy

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

        aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
        AOS_IPFC_return(destroyed);
    }
    else if(function_index == 2)
    {
//        spinlock_acquire(&tempuser_printout_lock);
//        AOS_H_printf("get window surfaces! from pid %llu\n", source_pid);
//        spinlock_release(&tempuser_printout_lock);
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
            aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
//            f64 after = AOS_H_time_get_seconds();
//            spinlock_acquire(&tempuser_printout_lock);
//            AOS_H_printf("total ipfc time is %lf ms\n", (after-before) * 1000.0);
//            spinlock_release(&tempuser_printout_lock);
            AOS_IPFC_return(1);
        }
        aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
        AOS_IPFC_return(0);
    }
    else if(function_index == 3)
    {
//        spinlock_acquire(&tempuser_printout_lock);
//        AOS_H_printf("get cursor pos! from pid %llu\n", source_pid);
//        spinlock_release(&tempuser_printout_lock);
        u64 window_handle;
        {
            u64* window_handle_pointer = static_data_1024b;
            window_handle = *window_handle_pointer;
        }

        f64* copy_to = static_data_1024b;

        for(u64 i = 0; i < window_count; i++)
        {
            if(windows[i].pid != source_pid || windows[i].window_handle != window_handle)
            { continue; }

            copy_to[0] = windows[i].cursor_x;
            copy_to[1] = windows[i].cursor_y;
            aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
            AOS_IPFC_return(1);
        }
        aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
        AOS_IPFC_return(0);
    }
    else if(function_index == 4)
    {
        u64 window_handle;
        {
            u64* window_handle_pointer = static_data_1024b;
            window_handle = *window_handle_pointer;
        }

        u64* copy_to = static_data_1024b;

        for(u64 i = 0; i < window_count; i++)
        {
            if(windows[i].pid != source_pid || windows[i].window_handle != window_handle)
            { continue; }

            copy_to[0] = windows[i].foreign_middle_buffer_handle;
            copy_to[1] = windows[i].foreign_commit_semaphore_handle;
            copy_to[2] = windows[i].foreign_acquire_semaphore_handle;
            aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
            AOS_IPFC_return(1);
        }
        aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
        AOS_IPFC_return(0);
    }

    aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
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
    aso_semaphorum_medium_crea(0, 1,
                               &thunder_lock_mutex_semaphore_handle_signal, U64_MAX,
                               &thunder_lock_mutex_semaphore_handle_wait, U64_MAX);
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

    {
        AOS_H_printf("the temporary program loader is now going to look for *secret* directories...\n");
        u64 dirs[64];
        u64 dir_count = AOS_directory_get_subdirectories(drive1_partitions_directory, dirs, 64);
        u8 name_buf[64];
        for(u64 i = 0; i < dir_count; i++)
        {
            AOS_directory_get_name(dirs[i], name_buf, 64);
            AOS_H_printf("  \"I have found id=%llu, %s!\"\n", dirs[i], name_buf);
        }
    }

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

        for(u64 i = 0; i < window_count; i++)
        {
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

            f64 dx = (f64)new_cursor_x - (f64)(windows[i].new_x + BORDER_SIZE);
            f64 dy = (f64)new_cursor_y - (f64)(windows[i].new_y + BORDER_SIZE);

            dx /= (f64)(windows[i].new_width - 2*BORDER_SIZE);
            dy /= (f64)(windows[i].new_height - 2*BORDER_SIZE);

            windows[i].cursor_x = dx;
            windows[i].cursor_y = dy;
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

            if(kbd_events[i].current_state.keys_down[0] & 0x20)
            {
                u8 key_consumed = 1;

                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_PRESSED)
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
                            AOS_process_create_out_stream(pid, 0, &programs[program_count].owned_in_stream);
                            AOS_process_create_in_stream(pid, &programs[program_count].owned_out_stream, 0);

                            // give access to root directory
                            u64 foriegn_root_directory_id;
                            user_assert(AOS_directory_give(pid, &drive1_partitions_directory, &foriegn_root_directory_id, 1, 1), "I can give directory");
                            AOS_H_printf("Giving process access to root directory via handle = %llu.\n", foriegn_root_directory_id);

                            // add dumb program argument
                            user_assert(AOS_process_add_program_argument_string(pid, "How ya doin bro?", strlen("How ya doin bro?")), "I can add string argument");

                            AOS_process_start(pid);
                            program_count++;
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
    //AOS_thread_awake_on_mouse();
    //AOS_thread_awake_on_keyboard();
    //AOS_thread_awake_after_time(1000000);
    aso_semaphorum_medium_suscita(thunder_lock_mutex_semaphore_handle_signal, 1, 0);
    AOS_thread_sleep();
    aso_semaphorum_medium_expecta(thunder_lock_mutex_semaphore_handle_wait);
//AOS_H_printf("temp slept for %lf seconds\n", AOS_H_time_get_seconds() - pre_sleep);

    AOS_Framebuffer* fb = 0x54000;
    u64 fb_page_count = AOS_surface_acquire(0, 0, 0);
    if(AOS_surface_acquire(0, fb, fb_page_count))
    {
        double time_frame_start = AOS_H_time_get_seconds();

        // Fetch from consumers and setup
        {
            for(u64 i = 0; i < window_count; i++)
            {
                if(aso_semaphorum_medium_expectare_conare(windows[i].own_commit_semaphore_handle)) // aka new frame
                {
                    windows[i].displaying_primary = !windows[i].displaying_primary;

                    windows[i].dropped_frame = 0;

                    // HACK to tell client what buffer to render into
                    windows[i].pictura->dispositio_elementorum = !windows[i].displaying_primary;

                    aso_semaphorum_medium_suscita(windows[i].own_acquire_semaphore_handle, 1, 0);
                }
                else
                { windows[i].dropped_frame = 1; }

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
                {
                    spinlock_acquire(&tempuser_printout_lock);
                    //AOS_H_printf("frame has been dropped\n");
                    spinlock_release(&tempuser_printout_lock);
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

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + (y * fb->width);

            fb->data[i*3 + 0] = 17;
            fb->data[i*3 + 1] = 80;
            fb->data[i*3 + 2] = 128;

            u64 c = x / 8;
            u64 r = y / 16;

            if(r == slot_index)
            {
                fb->data[i*3 + 0] = 28;
                fb->data[i*3 + 1] = 133;
                fb->data[i*3 + 2] = 213;
            }
            else if(r < slot_count)
            {
                fb->data[i*3 + 0] = 8;
                fb->data[i*3 + 1] = 37;
                fb->data[i*3 + 2] = 57;
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
                fb->data[i*3 + 0] = 232;
                fb->data[i*3 + 1] = 227;
                fb->data[i*3 + 2] = 197;
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

            u8 red = 51;
            u8 green = 51;
            u8 blue = 70;
            if(j + 1 == window_count && (is_moving_window || is_resizing_window))
            { blue = 180; }
            else if(j + 1 == window_count)
            { blue = 140.0; }

            u64 latitudo_picturae = windows[j].pictura->latitudo;
            u64 altitudo_picturae = windows[j].pictura->altitudo;
            u8* data_picturae;
            if(windows[j].displaying_primary)
            {
                data_picturae = index_absolutus_ad_picturam_primam(windows[j].pictura);
            }
            else
            {
                data_picturae = index_absolutus_ad_picturam_secundam(windows[j].pictura);
            }

            u8 border_column_colours[end_x - start_x]; // this is an optimization
            for(u64 x = 0; x < end_x - start_x; x++)
            {
                f32 lerp_factor = (f32)x / (f32)windows[j].width;
                border_column_colours[x] = 106 + (u8)(120.0f * lerp_factor);
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

                if( x < BORDER_SIZE || x + BORDER_SIZE > windows[j].width ||
                    y < WINDOW_BANNER_HEIGHT || y + BORDER_SIZE > windows[j].height)  // aka this is a border
                {
                    fb->data[i*3 + 0] = 54;
                    fb->data[i*3 + 1] = 160;
                    fb->data[i*3 + 2] = border_column_colours[x - start_x];

                    if(j + 1 == window_count && (is_moving_window || is_resizing_window))
                    {
                        fb->data[i*3 + 0] += 10;
                        fb->data[i*3 + 1] += 10;
                        fb->data[i*3 + 2] += 10;
                    }
                    else if(j + 1 == window_count)
                    {
                        fb->data[i*3 + 0] += 30;
                        fb->data[i*3 + 1] += 30;
                        fb->data[i*3 + 2] += 30;
                    }

                    u8* temp_title = "Title of window";
                    u64 temp_title_len = strlen(temp_title);
                    if( x >= BORDER_SIZE && x + BORDER_SIZE < windows[j].width &&
                        y >= 3 && y + 3 < WINDOW_BANNER_HEIGHT)
                    {
                        if(x < BORDER_SIZE + temp_title_len*8 && font8_16_pixel_filled(temp_title[(x-BORDER_SIZE)/8], (x-BORDER_SIZE)%8, y - 3))
                        {
                            fb->data[i*3 + 0] = 230;
                            fb->data[i*3 + 1] = 230;
                            fb->data[i*3 + 2] = 230;
                        }
                        else
                        {
                            fb->data[i*3 + 0] -= 20;
                            fb->data[i*3 + 1] -= 20;
                            fb->data[i*3 + 2] -= 20;
                        }
                    }

                    continue;
                }

                if(x >= BORDER_SIZE && y >= WINDOW_BANNER_HEIGHT &&
                   x - BORDER_SIZE < latitudo_picturae && y - BORDER_SIZE < altitudo_picturae)
                {
                    u64 internal_x = x - BORDER_SIZE;
                    u64 internal_y = y - BORDER_SIZE;
                    u64 k = internal_x + (internal_y * latitudo_picturae);

                    if(!windows[j].dropped_frame)
                    {
                        fb->data[i*3 + 0] = data_picturae[k*3 + 0];
                        fb->data[i*3 + 1] = data_picturae[k*3 + 1];
                        fb->data[i*3 + 2] = data_picturae[k*3 + 2];
                    }
                    else
                    {
                        fb->data[i*3 + 0] = 200;
                        fb->data[i*3 + 1] = 200;
                        fb->data[i*3 + 2] = 200;
                    }
                }
                else
                {
                    fb->data[i*3 + 0] = 60; // Application has failed to give us data for this part.
                    fb->data[i*3 + 1] = 60; // so the correct response is eye-sore grey.
                    fb->data[i*3 + 2] = 60;
                }
            }
        }

        f64 time_render_end = AOS_H_time_get_seconds();
        //AOS_H_printf("it took %lf ms to render\n", (time_render_end - time_render_start)*1000.0);

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
                fb->data[i*3 + 0] = 255;
                fb->data[i*3 + 1] = 255;
                fb->data[i*3 + 2] = 255;
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
                fb->data[i*3 + 0] = 255 * (fb->data[i*3 + 0] < 127);
                fb->data[i*3 + 1] = 255 * (fb->data[i*3 + 1] < 127);
                fb->data[i*3 + 2] = 255 * (fb->data[i*3 + 2] < 127);
            }
        }

        // time since last frame, bottom right, above frametime
        f64 time_right_now = AOS_H_time_get_seconds();
        f64 time_passed = (time_right_now-last_frame_time) *1000.0;
        //AOS_H_printf("Time passed since last frame : %5.5lf s\n", time_passed);
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
                fb->data[i*3 + 0] = 255 * (fb->data[i*3 + 0] < 127);
                fb->data[i*3 + 1] = 255 * (fb->data[i*3 + 1] < 127);
                fb->data[i*3 + 2] = 255 * (fb->data[i*3 + 2] < 127);
            }
        }
        user_assert(AOS_surface_commit(0), "commited successfully");

        // move windows and resize them
        {
            for(u64 i = 0; i < window_count; i++)
            {
                windows[i].x = windows[i].new_x;
                windows[i].y = windows[i].new_y;
                windows[i].width = windows[i].new_width;
                windows[i].height = windows[i].new_height;
            }
        }

        cursor_x = new_cursor_x;
        cursor_y = new_cursor_y;
    }
}
}

