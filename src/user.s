

.global user_surface_commit
user_surface_commit:
    mv a1, a0
    addi a0, x0, 0
    ecall
    ret

.global user_surface_acquire
user_surface_acquire:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 1
    ecall
    ret

.global user_thread_sleep
user_thread_sleep:
    mv a1, a0
    addi a0, x0, 2
    ecall
    ret

.global user_wait_for_surface_draw
user_wait_for_surface_draw:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 3
    ecall
    ret

.global user_get_rawmouse_events
user_get_rawmouse_events:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 4
    ecall
    ret

.global user_get_keyboard_events
user_get_keyboard_events:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 5
    ecall
    ret

.global user_switch_vo
user_switch_vo:
    mv a1, a0
    addi a0, x0, 6
    ecall
    ret

.global user_get_vo_id
user_get_vo_id:
    mv a1, a0
    addi a0, x0, 7
    ecall
    ret

.global user_alloc_pages
user_alloc_pages:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 8
    ecall
    ret

.global user_shrink_allocation
user_shrink_allocation:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 9
    ecall
    ret

.global user_surface_consumer_has_commited
user_surface_consumer_has_commited:
    mv a1, a0
    addi a0, x0, 10
    ecall
    ret

.global user_surface_consumer_get_size
user_surface_consumer_get_size:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 11
    ecall
    ret

.global user_surface_consumer_set_size
user_surface_consumer_set_size:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 12
    ecall
    ret

.global user_surface_consumer_fetch
user_surface_consumer_fetch:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 13
    ecall
    ret

.global user_time_get_seconds
user_time_get_seconds:
    addi a0, x0, 14
    ecall
    ret

/*
u64 user_file_get_name(u64 file_id, u8* buf, u64 buf_size); // done 15
u64 user_file_git_size(u64 file_id); // not done 16
u64 user_file_get_block_count(u64 file_id); // not done 17
u64 user_file_set_size(u64 file_id, u64 new_size); // not done 18
u64 user_file_read_blocks(u64 file_id, u64* op_array, u64 op_count); // not done 19
u64 user_file_write_blocks(u64 file_id, u64* op_array, u64 op_count); // not done 20
 
u64 user_directory_get_name(u64 dir_id, u8* buf, u64 buf_size); // not done 21
u64 user_directory_get_subdirectories(u64 dir_id, u64* buf, 64 buf_size); // not done 22
u64 user_directory_get_files(u64 dir_id, u64* buf, u64 buf_size); // sorta done 23
u64 user_directory_add_subdirectory(u64 dir_id, u64 subdirectory); // not done 24
u64 user_directory_add_file(u64 dir_id, u64 file_id); // not done 25

u64 user_create_process_from_file(u64 file_id, u64* pid); // done 26
u64 user_surface_consumer_create(u64 foriegn_pid, u64* surface_consumer); // not done 27
*/

.global user_file_get_name
user_file_get_name:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 15
    ecall
    ret

.global user_directory_get_files
user_directory_get_files:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 23
    ecall
    ret

.global user_create_process_from_file
user_create_process_from_file:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 26
    ecall
    ret

.global user_surface_consumer_create
user_surface_consumer_create:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 27
    ecall
    ret

.global user_surface_consumer_fire
user_surface_consumer_fire:
    mv a1, a0
    addi a0, x0, 28
    ecall
    ret
