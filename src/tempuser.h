// to be user code

void user_surface_commit(u64 surface_slot);
u64 user_surface_acquire(u64 surface_slot, Framebuffer* fb, u64 page_count);

void user_thread_sleep(u64 duration);
void user_wait_for_surface_draw(u64 surface_slot);

u64 user_get_raw_mouse(RawMouse* buf, u64 len);
u64 user_get_keyboard_events(KeyboardEvent* buf, u64 len);

u64 user_switch_vo(u64 vo_id);
u64 user_get_vo_id(u64* vo_id);

u64 user_alloc_pages(void* vaddr, u64 page_count);
u64 user_shrink_allocation(void* vaddr, u64 new_page_count);

u64 user_surface_consumer_has_commited(u64 consumer_slot);
u64 user_surface_consumer_get_size(u64 consumer_slot, u32* width, u32* height);
u64 user_surface_consumer_set_size(u64 consumer_slot, u32 width, u32 height);

#include "samorak.h"
#include "font9_12.h"

void thread1_func()
{
u64 font_bitmaps[256*2];
font9_12_get_bitmap(font_bitmaps);

f64 ballx = 0.0;
f64 bally = 0.0;

char textbuffer[4096];
textbuffer[0] = 0;

s64 backspace_timer = -1;
while(1) {
//    user_wait_for_surface_draw(0);
for(u64 surface_slot = 0; surface_slot < 2; surface_slot++)
{
    Framebuffer* fb = 0x424242000;
    u64 fb_page_count = user_surface_acquire(surface_slot, fb, 0);
    if(user_surface_acquire(surface_slot, fb, fb_page_count))
    {
user_surface_consumer_set_size(0, 100, 100);
if(user_surface_consumer_has_commited(0))
{
    printf("hi\n");
    u32 width, height;
    user_surface_consumer_get_size(0, &width, &height);
    printf("The consumer is %u by %u\n", width, height);
}else { printf("no\n"); }
        u64 raw_mouse_count = user_get_raw_mouse(0, 0);
        RawMouse mouses[raw_mouse_count];
        user_get_raw_mouse(mouses, raw_mouse_count);

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

                    if(scancode >= 58 && scancode <= 69)
                    {
                        u64 fkey = scancode - 58;
                        user_switch_vo(fkey);
                    }

                    if(scancode == 40)
                    {
                        strcat(textbuffer, "\n");
                    }

                    append_scancode_to_string(scancode,
                                kbd_events[i].current_state, textbuffer);
                    if(scancode == 42 && strlen(textbuffer) > 0) {
                        backspace_timer = 5;
                        textbuffer[strlen(textbuffer)-1] = 0;
                    }
                }
                else
                {
                    if(kbd_events[i].scancode == 42)
                    { backspace_timer = -1; }
                }

//                printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        } while(more);

        if(strlen(textbuffer) >= 1 && backspace_timer == 0)
        { textbuffer[strlen(textbuffer)-1] = 0; }
        else
        { backspace_timer--; }

//        printf("%s\n", textbuffer);

/*        printf("status: %llx %llx %llx %llx\n", kbd_events[0].current_state.keys_down[0],
                                        kbd_events[0].current_state.keys_down[1],
                                        kbd_events[0].current_state.keys_down[2],
                                        kbd_events[0].current_state.keys_down[3]);
*/

        u64 column_count = (fb->width / 9);
        u64 row_count = (fb->height / 12);

        char bottom_banner[256];
        bottom_banner[0] = 0;
        u64 cvo = 0;
        if(user_get_vo_id(&cvo))
        {
            sprintf(bottom_banner, "Virtual Output #%llu", cvo);
        }
        u64 bottom_banner_len = strlen(bottom_banner);
        if(bottom_banner_len > column_count || row_count <= 1) { bottom_banner_len = column_count; }

        u64 tblen = strlen(textbuffer);

        for(u64 i = 0; i < raw_mouse_count; i++)
        {
            ballx += mouses[i].x / 2.0;
            bally += mouses[i].y / 2.0;
        }

        if(ballx < 0.0) { ballx = 0.0; }
        if(bally < 0.0) { bally = 0.0; }
        if((u64)ballx >= fb->width) { ballx = (f64)(fb->width-1); }
        if((u64)bally >= fb->height) { bally = (f64)(fb->height-1); }

        u64 fontids[row_count * column_count];

        {
            s64 ch_index = strlen(textbuffer) - 1;
            for(s64 r = row_count -1 - (bottom_banner_len > 0); r >= 0; r--)
            {
                s64 row_len = 0;
                s64 orig_ch_index = ch_index;
                while(ch_index >= 0)
                {
                    if(textbuffer[ch_index] == '\n')
                    {
                        ch_index -= 1;
                        break;
                    }
                    else
                    {
                        row_len += 1;
                        ch_index -= 1;
                    }
                }
                if(row_len > column_count)
                {
                    row_len = row_len % column_count;
                    if(row_len == 0) { row_len = column_count; }
                    ch_index = orig_ch_index - row_len;
                }

                u64 skip = textbuffer[ch_index + 1] == '\n';
                for(u64 c = 0; c < row_len; c++)
                {
                    fontids[c + (r*column_count)] = textbuffer[ch_index + skip + 1 + c];
                }
                for(u64 c = row_len; c < column_count; c++)
                {
                    fontids[c + (r*column_count)] = 0;
                }
            }

            if(bottom_banner_len > 0)
            {
                for(u64 c = 0; c < column_count; c++)
                {
                    if(c < bottom_banner_len){ fontids[(row_count-1)* column_count + c] = bottom_banner[c]; }
                    else { fontids[(row_count-1)* column_count + c] = 0; }
                }
            }

            if(ch_index > 0)
            {
                char* dest = textbuffer;
                char* src = textbuffer + ch_index + 1;
                while(src < (textbuffer + 4096))
                {
                    *dest = *src;
                    src++; dest++;
                }
            }
        }

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + (y * fb->width);

            fb->data[i*4 + 0] = 0.0;
            fb->data[i*4 + 1] = 0.0;
            fb->data[i*4 + 2] = 0.0;
            fb->data[i*4 + 3] = 1.0;

            u64 c = x / 9;
            u64 r = y / 12;

            u64 here = 0;

            if(c < column_count && r < row_count)
            {
                u64 font_id = fontids[c + (r*column_count)];
                if(font_id >= 256) { font_id = 255; }
                u64 bitmap_index = (x % 9) + (9 * (y % 12));
                here =
            (font_bitmaps[2*font_id + (bitmap_index >>6)] & (((u64)1) << (bitmap_index & 0x3F)));
                here = here && font_id != 0;
            }

            if(here)
            {
                fb->data[i*4 + 0] = 1.0;
                fb->data[i*4 + 1] = 1.0;
                fb->data[i*4 + 2] = 0.0;
            }

            if(x == (u64)ballx || y == (u64)bally)
            {
                fb->data[i*4 + 0] = 1.0 - fb->data[i*4 + 0];
                fb->data[i*4 + 1] = 1.0 - fb->data[i*4 + 1];
                fb->data[i*4 + 2] = 1.0 - fb->data[i*4 + 2];
            }
        }
 
        user_surface_commit(surface_slot);
    }
}
}
}
 
void thread2_func()
{

    u64* ptr = PAGE_SIZE * 10;
    assert(user_alloc_pages(ptr, 4), "alloc success");
    assert(user_shrink_allocation(ptr, 4), "shrink success");
    printf("number : %llu\n", *(ptr+5 + (PAGE_SIZE/8)*3));
    assert(user_shrink_allocation(ptr, 0), "shrink success2");

    u64 times = 1;
    while(1)
    {
        user_thread_sleep(10000000*5);
        printf(" **** thread2 is ALSO !! ****** #%lld many times!!!!!!!! \n", times);
        times++;
    }
}

void thread3_func()
{
    u64 times = 1;
    while(1)
    {
        user_thread_sleep(10000000*12);
        printf("<> <> <> <> <> <> <> <>  OMG ITS A THIRD THREAD!!!! #%lld times... <> <> <> \n", times);
        times++;
    }
}
