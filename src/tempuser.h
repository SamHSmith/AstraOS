// to be user code

u64 user_surface_commit(u64 surface_slot);
u64 user_surface_acquire(u64 surface_slot, Framebuffer* fb, u64 page_count);

void user_thread_sleep(u64 duration);
void user_wait_for_surface_draw(u64* surface_slots, u64 count);

u64 user_get_raw_mouse(RawMouse* buf, u64 len);
u64 user_get_keyboard_events(KeyboardEvent* buf, u64 len);

u64 user_switch_vo(u64 vo_id);
u64 user_get_vo_id(u64* vo_id);

u64 user_alloc_pages(void* vaddr, u64 page_count);
u64 user_shrink_allocation(void* vaddr, u64 new_page_count);

u64 user_surface_consumer_has_commited(u64 consumer_slot);
u64 user_surface_consumer_get_size(u64 consumer_slot, u32* width, u32* height);
u64 user_surface_consumer_set_size(u64 consumer_slot, u32 width, u32 height);
u64 user_surface_consumer_fetch(u64 consumer_slot, Framebuffer* fb, u64 page_count);

f64 user_time_get_seconds();

u64 user_is_valid_file_id(u64 file_id);
u64 user_is_valid_dir_id(u64 directory_id);

u64 user_file_get_name(u64 file_id, u8* buf, u64 buf_size);
u64 user_file_git_size(u64 file_id); // not done
u64 user_file_get_block_count(u64 file_id); // not done
u64 user_file_set_size(u64 file_id, u64 new_size); // not done
u64 user_file_read_blocks(u64 file_id, u64* op_array, u64 op_count); // not done
u64 user_file_write_blocks(u64 file_id, u64* op_array, u64 op_count); // not done

u64 user_directory_get_name(u64 dir_id, u8* buf, u64 buf_size); // not done
u64 user_directory_get_subdirectories(u64 dir_id, u64* buf, u64 buf_size); // not done
u64 user_directory_get_files(u64 dir_id, u64* buf, u64 buf_size);
u64 user_directory_add_subdirectory(u64 dir_id, u64 subdirectory); // not done
u64 user_directory_add_file(u64 dir_id, u64 file_id); // not done

u64 user_create_process_from_file(u64 file_id, u64* pid);

u64 user_surface_consumer_create(u64 foriegn_pid, u64* surface_consumer);

// give file to proccess
// give directory to proccess

// create file
// create directory

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
    u64 slot_count = user_directory_get_files(drive1_partitions_directory, 0, 0);
    u64 partitions[slot_count];
    u8 partition_names[slot_count][64];
    u64 partition_name_lens[slot_count];
    slot_count = user_directory_get_files(drive1_partitions_directory, partitions, slot_count);
    for(u64 i = 0; i < slot_count; i++)
    {
        partition_names[i][0] = 0;
        user_file_get_name(partitions[i], partition_names[i], 64);
        partition_name_lens[i] = strlen(partition_names[i]);
        printf("tempuser has found %s\n", partition_names[i]);
    }
    u64 slot_index = 0;

    Window windows[256];
    u64 window_count = 0;

while(1) {
    u64 user_wait_surface = 0;
    user_wait_for_surface_draw(&user_wait_surface, 1);

    Framebuffer* fb = 0x54000;
    u64 fb_page_count = user_surface_acquire(0, 0, 0);
    if(user_surface_acquire(0, fb, fb_page_count))
    {
        double time_frame_start = user_time_get_seconds();

        u64 kbd_event_count = user_get_keyboard_events(0, 0);
        KeyboardEvent kbd_events[kbd_event_count];
        u64 more = 0;
        do {
            more = 0;
            kbd_event_count = user_get_keyboard_events(kbd_events, kbd_event_count);
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
                        user_switch_vo(fkey);
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
                            if(windows[i].target_width > 90) { windows[i].target_width -= 10; }
                        }
                    }

                    if(scancode == 35 && slot_index < slot_count && window_count + 1 < 256)
                    {
                        u64 pid = 0;
                        if(user_create_process_from_file(partitions[slot_index], &pid))
                        {
                            printf("PROCESS CREATED, PID=%llu\n", pid);
                            u64 con = 0;
                            if(user_surface_consumer_create(pid, &con))
                            {
                                windows[window_count].consumer = con;
                                windows[window_count].x = 20 + window_count*7;
                                windows[window_count].y = 49*window_count;
                                if(windows[window_count].y > 1000) { windows[window_count].y = 1000; }
                                windows[window_count].new_width = 100;
                                windows[window_count].target_width = 100;
                                windows[window_count].creation_time = user_time_get_seconds();
                                windows[window_count].new_height = 100;
                                windows[window_count].width = windows[window_count].new_width;
                                windows[window_count].height = windows[window_count].new_height;
                                user_surface_consumer_set_size(
                                    windows[window_count].consumer,
                                    windows[window_count].new_width - 2*BORDER_SIZE,
                                    windows[window_count].new_height - 2*BORDER_SIZE
                                );
                                windows[window_count].fb = 0x54000 + (6900*6900*4*4 * (window_count+1));
                                windows[window_count].fb = (u64)windows[window_count].fb & ~0xfff;
                                windows[window_count].we_have_frame = 0;
                                window_count++;
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

        // Fetch from consumers
        {
            for(u64 i = 0; i < window_count; i++)
            {
                windows[i].new_width = (s64)windows[i].target_width +
                    (s64)(75.0 * botched_sin((time_frame_start - windows[i].creation_time)));

                if(user_surface_consumer_fetch(windows[i].consumer, 1, 0)) // Poll
                {
                    // allocate address space
                    windows[i].fb_page_count = user_surface_consumer_fetch(windows[i].consumer, 0, 0);
                    if(user_surface_consumer_fetch(
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
        if(user_get_vo_id(&cvo))
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

        double time_frame_end = user_time_get_seconds();
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
        assert(user_surface_commit(0), "commited successfully");

        { // prepare consumers for the next frame
            for(u64 i = 0; i < window_count; i++)
            {
                user_surface_consumer_set_size(
                    windows[i].consumer,
                    windows[i].new_width  -2*BORDER_SIZE,
                    windows[i].new_height -2*BORDER_SIZE
                );
                windows[i].width = windows[i].new_width;
                windows[i].height = windows[i].new_height;
            }
        }
    }
}
}

