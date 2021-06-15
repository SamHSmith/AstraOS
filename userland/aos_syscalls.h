
#include "../common/types.h"

typedef struct
{
    u64 _pad[2];
    u32 width;
    u32 height;
    float data[];
} AOS_Framebuffer;

u64 AOS_surface_commit(u64 surface_slot);
u64 AOS_surface_acquire(u64 surface_slot, AOS_Framebuffer* fb, u64 page_count);
 
u64 AOS_thread_awake_after_time(u64 duration);
u64 AOS_thread_awake_on_surface(u64* surface_slots, u64 count);

typedef struct
{
    f64 delta_x;
    f64 delta_y;
    f64 delta_z;
    u32 buttons_down;
    u8 button;
    u8 pressed;
    u8 released;
} AOS_RawMouseEvent;
u64 AOS_get_rawmouse_events(volatile AOS_RawMouseEvent* buf, u64 len);

typedef struct
{
    u64 keys_down[4];
} AOS_KeyboardState;
 
typedef struct
{
    u8 event;
    u8 scancode;
    AOS_KeyboardState current_state;
} AOS_KeyboardEvent;
#define AOS_KEYBOARD_EVENT_NOTHING 0
#define AOS_KEYBOARD_EVENT_PRESSED 1
#define AOS_KEYBOARD_EVENT_RELEASED 2
u64 AOS_get_keyboard_events(volatile AOS_KeyboardEvent* buf, u64 len);
 
u64 AOS_switch_vo(u64 vo_id);
u64 AOS_get_vo_id(u64* vo_id);
 
u64 AOS_alloc_pages(volatile void* vaddr, u64 page_count);
u64 AOS_shrink_allocation(volatile void* vaddr, u64 new_page_count);
 
u64 AOS_surface_consumer_has_commited(u64 consumer_slot);
u64 AOS_surface_consumer_get_size(u64 consumer_slot, volatile u32* width, volatile u32* height);
u64 AOS_surface_consumer_set_size(u64 consumer_slot, u32 width, u32 height);
u64 AOS_surface_consumer_fetch(u64 consumer_slot, volatile AOS_Framebuffer* fb, u64 page_count);
 
f64 AOS_time_get_seconds();
 
u64 AOS_is_valid_file_id(u64 file_id);
u64 AOS_is_valid_dir_id(u64 directory_id);
 
u64 AOS_file_get_name(u64 file_id, volatile u8* buf, u64 buf_size);
u64 AOS_file_git_size(u64 file_id); // not done
u64 AOS_file_get_block_count(u64 file_id); // not done
u64 AOS_file_set_size(u64 file_id, u64 new_size); // not done
u64 AOS_file_read_blocks(u64 file_id, volatile u64* op_array, u64 op_count); // not done
u64 AOS_file_write_blocks(u64 file_id, volatile u64* op_array, u64 op_count); // not done
 
u64 AOS_directory_get_name(u64 dir_id, volatile u8* buf, u64 buf_size); // not done
u64 AOS_directory_get_subdirectories(u64 dir_id, volatile u64* buf, u64 buf_size); // not done
u64 AOS_directory_get_files(u64 dir_id, volatile u64* buf, u64 buf_size);
u64 AOS_directory_add_subdirectory(u64 dir_id, u64 subdirectory); // not done
u64 AOS_directory_add_file(u64 dir_id, u64 file_id); // not done
 
u64 AOS_create_process_from_file(u64 file_id, volatile u64* pid);
 
u64 AOS_surface_consumer_create(u64 foriegn_pid, volatile u64* surface_consumer);
u64 AOS_surface_consumer_fire(u64 consumer_slot);

u64 AOS_surface_forward_to_consumer(u64 surface_slot, u64 consumer_slot);
u64 AOS_surface_stop_forwarding_to_consumer(u64 surface_slot);

u64 AOS_forward_keyboard_events(AOS_KeyboardEvent* buf, u64 len, u64 pid);

u64 AOS_thread_sleep();
u64 AOS_thread_awake_on_keyboard();
u64 AOS_thread_awake_on_mouse();

// give file to proccess
// give directory to proccess
 
// create file
// create directory
