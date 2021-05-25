// to be user code
#include "../src/types.h"

void user_surface_commit(u64 surface_slot);
//u64 user_surface_acquire(u64 surface_slot, Framebuffer* fb, u64 page_count);

void user_thread_sleep(u64 duration);
void user_wait_for_surface_draw(u64* surface_slots, u64 count);

//u64 user_get_raw_mouse(RawMouse* buf, u64 len);
//u64 user_get_keyboard_events(KeyboardEvent* buf, u64 len);

u64 user_switch_vo(u64 vo_id);
u64 user_get_vo_id(u64* vo_id);

u64 user_alloc_pages(void* vaddr, u64 page_count);
u64 user_shrink_allocation(void* vaddr, u64 new_page_count);

u64 user_surface_consumer_has_commited(u64 consumer_slot);
u64 user_surface_consumer_get_size(u64 consumer_slot, u32* width, u32* height);
u64 user_surface_consumer_set_size(u64 consumer_slot, u32 width, u32 height);
//u64 user_surface_consumer_fetch(u64 consumer_slot, Framebuffer* fb, u64 page_count);

f64 user_time_get_seconds();

#include "../src/uart.h"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void _start()
{
    char* dave = "Hi I'm dave and I live in an elf file on a partition on the RADICAL PARTITION SYSTEM\n";
    while(1)
    {
        uart_write(dave, strlen(dave));
        user_thread_sleep(990000);
    }
}
