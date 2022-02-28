
#ifndef __AOS_SYSCALLS_H
#define __AOS_SYSCALLS_H

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
u64 AOS_thread_awake_on_surface(u16* surface_slots, u64 count);

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
 
#define _AOS_KeyboardEvent \
struct \
{ \
    u8 event; \
    u8 scancode; \
    AOS_KeyboardState current_state; \
}
typedef struct
{
    _AOS_KeyboardEvent;
    u8 _padding[NEXT_POWER_OF_2(sizeof(_AOS_KeyboardEvent)) - sizeof(_AOS_KeyboardEvent)];
} __attribute__((aligned (NEXT_POWER_OF_2(sizeof(_AOS_KeyboardEvent))))) AOS_KeyboardEvent;
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
 
u64 AOS_get_cpu_time();
u64 AOS_get_cpu_timer_frequency();
 
u64 AOS_is_valid_file_id(u64 file_id);
u64 AOS_is_valid_dir_id(u64 directory_id);
 
u64 AOS_file_get_name(u64 file_id, volatile u8* buf, u64 buf_size);
u64 AOS_file_get_size(u64 file_id); // not done
u64 AOS_file_get_block_count(u64 file_id); // not done
u64 AOS_file_set_size(u64 file_id, u64 new_size); // not done
u64 AOS_file_read_blocks(u64 file_id, volatile u64* op_array, u64 op_count); // not done
u64 AOS_file_write_blocks(u64 file_id, volatile u64* op_array, u64 op_count); // not done
 
u64 AOS_directory_get_name(u64 dir_id, volatile u8* buf, u64 buf_size);
u64 AOS_directory_get_subdirectories(u64 dir_id, volatile u64* buf, u64 buf_size);
u64 AOS_directory_get_files(u64 dir_id, volatile u64* buf, u64 buf_size);
u64 AOS_directory_add_subdirectory(u64 dir_id, u64 subdirectory); // not done
u64 AOS_directory_add_file(u64 dir_id, u64 file_id); // not done
 
u64 AOS_create_process_from_file(u64 file_id, volatile u64* pid);

void AOS_directory_get_absolute_ids(u64* local_id_buffer, u64* absolute_id_buffer, u64 count);
 
u64 AOS_surface_consumer_create(u64 foriegn_pid, u64* surface_consumer, u64* surface_slot);
u64 AOS_surface_consumer_fire(u64 consumer_slot);

u64 AOS_surface_forward_to_consumer(u64 surface_slot, u64 consumer_slot);
u64 AOS_surface_stop_forwarding_to_consumer(u64 surface_slot);

#define AOS_FORWARD_KEYBOARD_EVENTS_MAX_COUNT 20
u64 AOS_forward_keyboard_events(AOS_KeyboardEvent* buf, u64 len, u64 pid);

u64 AOS_thread_sleep();
u64 AOS_thread_awake_on_keyboard();
u64 AOS_thread_awake_on_mouse();

#define AOS_STREAM_STDOUT 0
#define AOS_STREAM_STDIN 0
u64 AOS_stream_put(u64 out_stream, u8* memory, u64 count);
u64 AOS_stream_take(u64 in_stream, u8* buffer, u64 buffer_size, u64* byte_count_in_stream);

// out and in refer to the *other* process. So create_out_stream creates a stream from you to the other process
u64 AOS_process_create_out_stream(u64 process_handle, u64* foreign_out_stream, u64* owned_in_stream);
u64 AOS_process_create_in_stream(u64 process_handle, u64* owned_out_stream, u64* foreign_in_stream);

void AOS_process_start(u64 process_handle);

typedef struct
{
    u64 regs[32];
    u64 fregs[32];
} AOS_TrapFrame;

// all ipfc threads use thread_group 0, the main thread is always 1.
// If you want to go wide over all cores create as many threads as cores
// in a unique thread group. That ensures they don't end up on the same core.
u64 AOS_thread_new(u64 program_counter, AOS_TrapFrame* register_values, u32 thread_group);

// returns a semaphore handle
u64 AOS_semaphore_create(u32 initial_value, u32 max_value);
// returns true if success
// if you don't care about previous_value you can pass null.
u64 AOS_semaphore_release(u64 semaphore, u32 release_count, u32* previous_value);
// returns true if the awake was set on the thread successfully.
// when the thread is awoken by the semaphore's value being > 0,
// the semaphore's value is decremented by 1.
u64 AOS_thread_awake_on_semaphore(u64 semaphore);

void AOS_process_exit();

u64 AOS_process_is_alive(u64 pid);

u64 AOS_process_kill(u64 pid);

u64 AOS_out_stream_destroy(u64 out_stream);
u64 AOS_in_stream_destroy(u64 in_stream);

// BEGIN IPFC's
// BEGIN CALLEE
// returns true on success
// name_len can not be greater than 64
// handler_id_ptr can be 0 if you don't want to know
// the handler_id of the created IPFC handler.
u64 AOS_IPFC_handler_create(
        u8* name,
        u64 name_len,
        u64(*ipfc_entry_point)(u64 source_pid, u16 function_index, u64* ipfc_static_data_1024_bytes),
        void* stack_pages_start, // page aligned
        u64 pages_per_stack,
        u64 stack_count,
        u64* handler_id_ptr);

u64 AOS_IPFC_return(u64 return_value);
// END CALLEE

// BEGIN CALLER
u64 AOS_IPFC_init_session(u8* name, u64 name_len, u64* session_id);
void AOS_IPFC_close_session(u64 session_id);

u64 AOS_IPFC_call(  u64 session_id,
                    u16 function_index,
                    void* ipfc_static_data_1024_bytes_in,
                    void* ipfc_static_data_1024_bytes_out);
// END CALLER
// END IPFC's


// give file to proccess
// give directory to proccess
 
// create file
// create directory

#endif
