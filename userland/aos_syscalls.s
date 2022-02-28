

.global AOS_surface_commit
AOS_surface_commit:
    mv a1, a0
    addi a0, x0, 0
    ecall
    ret

.global AOS_surface_acquire
AOS_surface_acquire:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 1
    ecall
    ret

.global AOS_thread_awake_after_time
AOS_thread_awake_after_time:
    mv a1, a0
    addi a0, x0, 2
    ecall
    ret

.global AOS_thread_awake_on_surface
AOS_thread_awake_on_surface:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 3
    ecall
    ret

.global AOS_get_rawmouse_events
AOS_get_rawmouse_events:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 4
    ecall
    ret

.global AOS_get_keyboard_events
AOS_get_keyboard_events:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 5
    ecall
    ret

.global AOS_switch_vo
AOS_switch_vo:
    mv a1, a0
    addi a0, x0, 6
    ecall
    ret

.global AOS_get_vo_id
AOS_get_vo_id:
    mv a1, a0
    addi a0, x0, 7
    ecall
    ret

.global AOS_alloc_pages
AOS_alloc_pages:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 8
    ecall
    ret

.global AOS_shrink_allocation
AOS_shrink_allocation:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 9
    ecall
    ret

.global AOS_surface_consumer_has_commited
AOS_surface_consumer_has_commited:
    mv a1, a0
    addi a0, x0, 10
    ecall
    ret

.global AOS_surface_consumer_get_size
AOS_surface_consumer_get_size:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 11
    ecall
    ret

.global AOS_surface_consumer_set_size
AOS_surface_consumer_set_size:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 12
    ecall
    ret

.global AOS_surface_consumer_fetch
AOS_surface_consumer_fetch:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 13
    ecall
    ret

.global AOS_get_cpu_time
AOS_get_cpu_time:
    addi a0, x0, 14
    ecall
    ret

/*
u64 AOS_file_get_name(u64 file_id, u8* buf, u64 buf_size); // done 15
u64 AOS_file_git_size(u64 file_id); // not done 16
u64 AOS_file_get_block_count(u64 file_id); // not done 17
u64 AOS_file_set_size(u64 file_id, u64 new_size); // not done 18
u64 AOS_file_read_blocks(u64 file_id, u64* op_array, u64 op_count); // not done 19
u64 AOS_file_write_blocks(u64 file_id, u64* op_array, u64 op_count); // not done 20
 
u64 AOS_directory_get_name(u64 dir_id, u8* buf, u64 buf_size); // not done 21
u64 AOS_directory_get_subdirectories(u64 dir_id, u64* buf, 64 buf_size); // not done 22
u64 AOS_directory_get_files(u64 dir_id, u64* buf, u64 buf_size); // sorta done 23
u64 AOS_directory_add_subdirectory(u64 dir_id, u64 subdirectory); // not done 24
u64 AOS_directory_add_file(u64 dir_id, u64 file_id); // not done 25

u64 AOS_create_process_from_file(u64 file_id, u64* pid); // done 26
u64 AOS_surface_consumer_create(u64 foriegn_pid, u64* surface_consumer); // not done 27
*/

.global AOS_file_get_name
AOS_file_get_name:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 15
    ecall
    ret

.global AOS_directory_get_files
AOS_directory_get_files:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 23
    ecall
    ret

.global AOS_create_process_from_file
AOS_create_process_from_file:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 26
    ecall
    ret

.global AOS_surface_consumer_create
AOS_surface_consumer_create:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 27
    ecall
    ret

.global AOS_surface_consumer_fire
AOS_surface_consumer_fire:
    mv a1, a0
    addi a0, x0, 28
    ecall
    ret

.global AOS_surface_forward_to_consumer
AOS_surface_forward_to_consumer:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 29
    ecall
    ret

.global AOS_surface_stop_forwarding_to_consumer
AOS_surface_stop_forwarding_to_consumer:
    mv a1, a0
    addi a0, x0, 30
    ecall
    ret

.global AOS_forward_keyboard_events
AOS_forward_keyboard_events:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 31
    ecall
    ret

.global AOS_thread_sleep
AOS_thread_sleep:
    addi a0, x0, 32
    ecall
    ret

.global AOS_thread_awake_on_keyboard
AOS_thread_awake_on_keyboard:
    addi a0, x0, 33
    ecall
    ret

.global AOS_thread_awake_on_mouse
AOS_thread_awake_on_mouse:
    addi a0, x0, 34
    ecall
    ret

.global AOS_stream_put
AOS_stream_put:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 35
    ecall
    ret

.global AOS_stream_take
AOS_stream_take:
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 36
    ecall
    ret

.global AOS_process_create_out_stream
AOS_process_create_out_stream:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 37
    ecall
    ret

.global AOS_process_create_in_stream
AOS_process_create_in_stream:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 38
    ecall
    ret

.global AOS_process_start
AOS_process_start:
    mv a1, a0
    addi a0, x0, 39
    ecall
    ret

.global AOS_thread_new
AOS_thread_new:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 40
    ecall
    ret

.global AOS_semaphore_create
AOS_semaphore_create:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 41
    ecall
    ret

.global AOS_semaphore_release
AOS_semaphore_release:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 42
    ecall
    ret

.global AOS_thread_awake_on_semaphore
AOS_thread_awake_on_semaphore:
    mv a1, a0
    addi a0, x0, 43
    ecall
    ret

.global AOS_process_exit
AOS_process_exit:
    addi a0, x0, 44
    ecall
    ret

.global AOS_process_is_alive
AOS_process_is_alive:
    mv a1, a0
    addi a0, x0, 45
    ecall
    ret

.global AOS_process_kill
AOS_process_kill:
    mv a1, a0
    addi a0, x0, 46
    ecall
    ret

.global AOS_out_stream_destroy
AOS_out_stream_destroy:
    mv a1, a0
    addi a0, x0, 47
    ecall
    ret

.global AOS_in_stream_destroy
AOS_in_stream_destroy:
    mv a1, a0
    addi a0, x0, 48
    ecall
    ret

.global AOS_IPFC_handler_create
AOS_IPFC_handler_create:
    mv a7, a6
    mv a6, a5
    mv a5, a4
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 49
    ecall
    ret

.global AOS_IPFC_init_session
AOS_IPFC_init_session:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 50
    ecall
    ret

.global AOS_IPFC_close_session
AOS_IPFC_close_session:
    mv a1, a0
    addi a0, x0, 51
    ecall
    ret

.global AOS_IPFC_call
AOS_IPFC_call:
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 52
    ecall
    ret

.global AOS_IPFC_return
AOS_IPFC_return:
    mv a1, a0
    addi a0, x0, 53
    ecall
    ret

.global AOS_get_cpu_timer_frequency
AOS_get_cpu_timer_frequency:
    addi a0, x0, 54
    ecall
    ret

.global AOS_directory_get_subdirectories
AOS_directory_get_subdirectories:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 55
    ecall
    ret

.global AOS_directory_get_name
AOS_directory_get_name:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 56
    ecall
    ret

.global AOS_directory_get_absolute_ids
AOS_directory_get_absolute_ids:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 57
    ecall
    ret

.global AOS_directory_give
AOS_directory_give:
    mv a5, a4
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    addi a0, x0, 58
    ecall
    ret

.global AOS_process_add_program_argument_string
AOS_process_add_program_argument_string:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    add a0, x0, 59
    ecall
    ret

.global AOS_process_get_program_argument_string
AOS_process_get_program_argument_string:
    mv a3, a2
    mv a2, a1
    mv a1, a0
    add a0, x0, 60
    ecall
    ret
