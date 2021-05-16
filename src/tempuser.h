// to be user code

void user_surface_commit(u64 surface_slot);
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

// give file to proccess
// give directory to proccess

// create file
// create directory

#include "samorak.h"
//#include "qwerty.h"

#include "font8_16.h"

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

while(1) {
    u64 user_wait_surface = 0;
    user_wait_for_surface_draw(&user_wait_surface, 1);

    Framebuffer* fb = 0x424242000;
    u64 fb_page_count = user_surface_acquire(0, fb, 0);
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
                }
                else
                {
                }

                printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        } while(more);

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
                fb->data[i*4 + 0] = 8.0/255.0;
                fb->data[i*4 + 1] = 37.0/255.0;
                fb->data[i*4 + 2] = 57.0/255.0;
                fb->data[i*4 + 3] = 1.0;
            }
            else if(r < slot_count)
            {
                fb->data[i*4 + 0] = 28.0/255.0;
                fb->data[i*4 + 1] = 133.0/255.0;
                fb->data[i*4 + 2] = 213.0/255.0;
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
 
        user_surface_commit(0);
    }
}
}

