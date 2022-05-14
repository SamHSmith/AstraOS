
#define MACHINE_TIMER_SECOND 10000000
#define KERNEL_MAX_HART_COUNT 64

#include "../userland/aos_syscalls.h"

#include "../common/types.h"
#include "../common/maths.h"
#include "../common/spinlock.h"
#include "../common/atomics.h"
#include "../common/rwlock.h"

#include "log.c"

Spinlock KERNEL_SPINLOCK;
Spinlock KERNEL_MEMORY_SPINLOCK;
Spinlock KERNEL_VIEWER_SPINLOCK;

RWLock KERNEL_TRAP_LOCK;

#include "random.c"

#include "stb_sprintf.h"

#include "uart.c"
#include "uart_printf.c"

void print_stacktrace(u64 start_frame, u64 program_counter, u64 return_address)
{
    u64* fp = start_frame;
    u64* ra = return_address;
    uart_printf("Stacktrace:\n");
    uart_printf("0x%llx\n", program_counter);
    uart_printf("0x%llx\n", return_address - 4);
    if(fp < 0x80000000 || fp > 0x10000000000) { return; }
    for(u64 i = 0; i < 128; i++)
    {
        ra = ((u64)*(fp-1)) - 4;
        fp = *(fp-2);
        if(fp < 0x80000000 || fp > 0x10000000000) { break; }
        uart_printf("0x%llx\n", ra);
    }
}

void assert(u64 stat, char* error)
{
    if(!stat)
    {
        uart_printf("assertion failed: \"");
        uart_printf(error);
        uart_printf("\"\n");
        print_stacktrace(read_fp_register(), read_pc_register(), read_ra_register());
        u8 freeze = 1;
        while(freeze) {};
    }
}

#include "libfuncs.c"

#include "memory.c"
#include "charta_media.c"
#include "semaphorum_medium.c"

#include "libfuncs2.c"
#include "cyclone_crypto/hash/sha512.h"

#include "disk.c"
#include "file.c"

#include "stream.c"

atomic_s64 KERNEL_HART_COUNT;

#include "plic.c"
#include "input.c"
#include "process.c"

#include "video.c"
#include "elf.c"


extern u64 KERNEL_START_OTHER_HARTS;

u64 KERNEL_MMU_TABLE;
u64 KERNEL_SATP_VALUE;


u64 KERNEL_TRAP_STACK_TOP[KERNEL_MAX_HART_COUNT];
Kallocation KERNEL_TRAP_STACK_ALLOCS[KERNEL_MAX_HART_COUNT];
 
u64 KERNEL_STACK_TOP[KERNEL_MAX_HART_COUNT];
Kallocation KERNEL_STACK_ALLOCS[KERNEL_MAX_HART_COUNT-1];
 
Thread KERNEL_THREADS[KERNEL_MAX_HART_COUNT];

// This tracks how many times each hart enters the kernel
// If you trap from having made an error in the kernel, the error
// handler will be able to see that because of this variable.
// And it needs to do so to be able to remove locks from the crashed
// execution.
s64 KERNEL_TRAP_ENTER_ACC[KERNEL_MAX_HART_COUNT];


/*
 * Debug
 */
u64 wait_time_acc[KERNEL_MAX_HART_COUNT];
u64 wait_time_times[KERNEL_MAX_HART_COUNT];
u64 wait_time_print_time[KERNEL_MAX_HART_COUNT];


#include "process_run.c"

#include "syscall.c"


//for rendering
Framebuffer* framebuffer = 0;
u8 frame_has_been_requested = 0;

#include "oak.c"

void uart_write_string(char* str)
{
    uart_write((u8*)str, strlen(str));
}

struct xoshiro256ss_state KERNEL_GLOBAL_RANDO_STATE = {5, 42, 68, 1};
u64 kinit()
{
    uart_init();

    uart_printf("Counting HARTS...");
    KERNEL_HART_COUNT.value = 0;
    atomic_s64_increment(&KERNEL_HART_COUNT);
    for(u64 i = 0; i < 2000000; i++) { __asm__("nop"); } // wait
    KERNEL_START_OTHER_HARTS = 0; // causes other harts to increment and get ready
    for(u64 i = 0; i < 4000000; i++) { __asm__("nop"); } // wait
    uart_printf("    There are %lld HARTS.\n", KERNEL_HART_COUNT.value);

    KERNEL_MMU_TABLE = (u64)mem_init();

    rwlock_create(&THREAD_RUNTIME_ARRAY_LOCK);
    rwlock_create(&KERNEL_TRAP_LOCK);
    for(s64 i = 0; i < KERNEL_HART_COUNT.value; i++)
    {
        Kallocation trap_stack = kalloc_pages(8);
        KERNEL_TRAP_STACK_TOP[i] = trap_stack.memory + (PAGE_SIZE * trap_stack.page_count);
        KERNEL_TRAP_STACK_ALLOCS[i] = trap_stack;

        KERNEL_TRAP_ENTER_ACC[i] = 0;

        if(i == 0)
        {
            KERNEL_STACK_TOP[0] = KERNEL_STACK_END;
        }
        else
        {
            Kallocation stack = kalloc_pages(8);
            KERNEL_STACK_TOP[i] = stack.memory + (PAGE_SIZE * trap_stack.page_count);
            KERNEL_STACK_ALLOCS[i-1] = stack;
        }

        //rando state
        kernel_choose_new_thread_rando_state[i].s[0] = xoshiro256ss(&KERNEL_GLOBAL_RANDO_STATE);
        kernel_choose_new_thread_rando_state[i].s[1] = xoshiro256ss(&KERNEL_GLOBAL_RANDO_STATE);
        kernel_choose_new_thread_rando_state[i].s[2] = xoshiro256ss(&KERNEL_GLOBAL_RANDO_STATE);
        kernel_choose_new_thread_rando_state[i].s[3] = xoshiro256ss(&KERNEL_GLOBAL_RANDO_STATE);

        // debug
        wait_time_acc[i] = 0;
        wait_time_times[i] = 0;
        wait_time_print_time[i] = 0;
    }

    KERNEL_SATP_VALUE = mmu_table_ptr_to_satp((u64*)KERNEL_MMU_TABLE);

    uart_printf("Entering supervisor mode...");
    return KERNEL_SATP_VALUE;
}

void kmain()
{
    uart_printf("done.\n    Successfully entered kmain with supervisor mode enabled.\n\n");
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

    // lock kernel
    spinlock_create(&KERNEL_SPINLOCK);
    spinlock_create(&KERNEL_MEMORY_SPINLOCK);
    spinlock_create(&KERNEL_VIEWER_SPINLOCK);
    spinlock_acquire(&KERNEL_SPINLOCK);
    KERNEL_START_OTHER_HARTS = 0;

    // initialize middle buffers array
    nucleus_lineam_chartarum_mediarum_initia();

    uart_printf("Pre testing: "); mem_debug_dump_table_counts(1);
    /* TESTING */

    //test middle buffers

    u64 ansa_chartae;
    assert(charta_media_crea(1000, &ansa_chartae), "charta_media_crea succeeds");
    uart_printf("post create : "); mem_debug_dump_table_counts(1);
    uart_printf("allocation size is %llu\n", charta_media_calculum_possessorum_augmenta(ansa_chartae).page_count);
    chartam_mediam_omitte(ansa_chartae);
    uart_printf("post free : "); mem_debug_dump_table_counts(1);
    chartam_mediam_omitte(ansa_chartae);
    uart_printf("post second free : "); mem_debug_dump_table_counts(1);



    u64 dir_id = kernel_directory_create_imaginary("Über directory");
    u64 dir_name_len = kernel_directory_get_name(dir_id, 0, 0);
    char dir_name[dir_name_len];
    kernel_directory_get_name(dir_id, dir_name, dir_name_len);
    uart_printf("The directory created is called : %s\n", dir_name);

    u64 smol_dir = kernel_directory_create_imaginary("smõl directory");
    u64 file_id;// = kernel_file_imaginary_create("an imaginary file");
    kernel_file_imaginary_create(&file_id, 1);
    kernel_file_increment_reference_count(file_id);
    kernel_directory_add_files(smol_dir, &file_id, 1);
    kernel_directory_add_subdirectory(dir_id, smol_dir);

    u64 full_dir_id = kernel_directory_create_imaginary("filled directory");
    kernel_directory_increment_reference_count(full_dir_id);
    kernel_directory_add_subdirectory(dir_id, full_dir_id);

    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));
    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));
    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));

    u64 file_to_remove = S64_MAX;

    {
        u64 file_count = 38;
        u64 files[file_count];
        kernel_file_imaginary_create(files, file_count);

        for(u64 i = 0; i < file_count; i++)
        {
            char name[32];
            stbsp_sprintf(name, "file#%lld", i);
            kernel_file_set_name(files[i], name);
        }
        if(!kernel_directory_add_files(full_dir_id, files, file_count))
        {
            uart_printf("there was an error when adding to the directory\n");
        }
    }

    debug_print_directory_tree(dir_id);
    kernel_directory_free(dir_id);

    kernel_file_imaginary_destroy(file_to_remove);
    debug_print_directory_tree(full_dir_id);
    kernel_directory_free(full_dir_id);

    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    mem_debug_dump_table_counts(1);
    kernel_file_set_size(file_id, 100);
    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    mem_debug_dump_table_counts(1);
    kernel_file_set_size(file_id, 10000);
    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    mem_debug_dump_table_counts(1);
    kernel_file_set_size(file_id, 6100);
    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    mem_debug_dump_table_counts(1);
    kernel_file_set_size(file_id, 100000);
    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    mem_debug_dump_table_counts(1);
    u8 _block[PAGE_SIZE*2];
    u8* block = (((u64)_block + PAGE_SIZE) / PAGE_SIZE) * PAGE_SIZE;
    u64 op_arr[2];
    op_arr[0] = 0;
    op_arr[1] = block;
    kernel_file_write_blocks(file_id, op_arr, 1);
    u8* compare = kalloc_single_page();
    op_arr[1] = compare;
    kernel_file_read_blocks(file_id, op_arr, 1);
    for(u64 i = 0; i < PAGE_SIZE; i++)
    {
        assert(
            block[i] == compare[i],
            "what we wrote to the imaginary file is the same as what we read back"
        );
    }
    kfree_single_page(compare);
    kernel_file_imaginary_destroy(file_id);
    uart_printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    kernel_file_free(file_id);
    uart_printf("Post file testing: "); mem_debug_dump_table_counts(1);

    load_drive_partitions();

    // temp
    u64 secret_dir = kernel_directory_create_imaginary("testdir");
    { // test file
        u64 test_file;
        kernel_file_imaginary_create(&test_file, 1);
        kernel_file_set_name(test_file, "testfile");
        kernel_directory_add_files(secret_dir, &test_file, 1);
        u64 file_size = 20;
        kernel_file_set_size(test_file, file_size);
        u64 op[2];
        op[0] = 0;
        op[1] = ((u64)"This is a string" / PAGE_SIZE) * PAGE_SIZE;
        kernel_file_write_blocks(test_file, op, 1);
        u8 _block[PAGE_SIZE*2];
        u8* block = (((u64)_block + PAGE_SIZE) / PAGE_SIZE) * PAGE_SIZE;
        op[0] = 0;
        op[1] = block;
        kernel_file_read_blocks(test_file, op, 1);
        uart_printf("Start print testfile\n");
        for(u64 i = 0; i < file_size; i++)
        { uart_printf("%c", block[i]); }
        uart_printf("\nAbove is testfile\n");
    }
    kernel_directory_add_subdirectory(secret_dir, drive1_partition_directory);
    kernel_directory_add_subdirectory(secret_dir, kernel_directory_create_imaginary("dave directory"));
    kernel_directory_add_subdirectory(drive1_partition_directory, secret_dir);
    kernel_directory_add_subdirectory(drive1_partition_directory, kernel_directory_create_imaginary("the other secret dir"));

    debug_print_directory_tree(drive1_partition_directory);

/*
    for(u64 b = 0; b < K_TABLE_COUNT; b++)
    {
        for(u64 i = 0; i < K_MEMTABLES[b]->table_len; i++)
        {
            for(u64 j = 0; j < 8; j++)
            {
                if((K_MEMTABLES[b]->data[i] & (1 << j)) != 0)
                { uart_write_string("*"); }
                else
                { uart_write_string("_"); }
            }
        }
        uart_write_string("\n");
    }
*/
/*
    while(1)
    {
        u8 r = 0;
        if(uart_read(&r, 1) == 1)
        {
            uart_write(&r, 1);
        }
    }
*/

    plic_interrupt_set_threshold(0);
    plic_interrupt_enable(10);
    plic_interrupt_set_priority(10, 1);

    process_init();
    spinlock_release(&KERNEL_SPINLOCK);

    for(u64 i = 0; i < 10000; i++) { __asm__("nop"); } // wait
    KERNEL_START_OTHER_HARTS = 1;

    u64* mtimecmp = ((u64*)0x02004000) + 0; // hartid is 0
    u64* mtime = (u64*)0x0200bff8;
    *mtimecmp = *mtime;

    while(1)
    { __asm__("wfi"); }
}

void kmain_hart(u64 hartid)
{
    spinlock_acquire(&KERNEL_SPINLOCK);
    uart_printf("initialized hart#%llu\n", hartid);
    spinlock_release(&KERNEL_SPINLOCK);

    u64* mtimecmp = ((u64*)0x02004000) + hartid;
    u64* mtime = (u64*)0x0200bff8;
    *mtimecmp = *mtime;
    while(1)
    { __asm__("wfi"); }
}

void trap_hang_kernel(
    u64 epc,
    u64 tval,
    u64 cause,
    u64 hart,
    u64 status,
    TrapFrame* frame
    )
{
    atomic_s64_increment(&KERNEL_TRAP_LOCK.write);
    for(u64 i = 0; i < 1000000; i++) { __asm__("nop"); }
    uart_printf("args:\n  epc: %llx\n  tval: %llx\n  cause: %llx\n  hart: %llx\n  status: %llx\n  frame: %llx\n",
            epc, tval, cause, hart, status, frame);
    uart_printf("frame:\n regs:\n");
    for(u64 i = 0; i < 32; i++) { uart_printf("  x%lld: %llx\n", i, frame->regs[i]); }
    uart_printf(" fregs:\n");
    for(u64 i = 0; i < 32; i++) { uart_printf("  f%lld: %llx\n", i, frame->fregs[i]); }
    uart_printf(" satp: %lx, trap_stack: %lx\n", frame->satp, frame);
    uart_printf("Kernel has hung.\n");
    u64 ptr = frame->regs[8];
    print_stacktrace(ptr, epc, frame->regs[1]);
    uart_printf("This is CPU#%llu\n", hart);
    uart_printf("Wide cpu status:\n");
    for(s64 i = 0; i < KERNEL_HART_COUNT.value; i++)
    {
        if(kernel_current_thread_has_thread[hart])
        {
            Process* p = KERNEL_PROCESS_ARRAY[kernel_current_threads[hart].process_pid];
            uart_printf("CPU#%llu is running (PID=%llu - TID#%llu)\n",
                   i,
                   kernel_current_threads[i].process_pid,
                   kernel_current_thread_tid[hart]
            );
        }
        else
        {
            uart_printf("CPU#%llu is without a user thread to run\n", i);
        }
    }
    while(1) {}
}

u64 m_trap(
    u64 epc,
    u64 tval,
    u64 cause,
    u64 hart,
    u64 status,
    TrapFrame* frame
    )
{
    u64 async = (cause >> 63) & 1 == 1;
    u64 cause_num = cause & 0xfff;

    KERNEL_TRAP_ENTER_ACC[hart]++;

    rwlock_acquire_read(&KERNEL_TRAP_LOCK);

    if(async)
    {
             if(cause_num == 0) {
                uart_printf("User software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 1) {
                uart_printf("Supervisor software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 3) {
                uart_printf("Machine software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 4) {
                uart_printf("User timer interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 5) {
                uart_printf("Supervisor timer interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 7) {

            kernel_log_kernel(hart, "hart got a timer interrupt");

            // Store thread
            if(kernel_current_thread_has_thread[hart])
            {
                kernel_current_threads[hart].frame = *frame;
                kernel_current_threads[hart].program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREADS[hart].frame = *frame;
                KERNEL_THREADS[hart].program_counter = epc;
            }

            {
                volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
                volatile u64* mtime = (u64*)0x0200bff8;
                u64 start_wait = *mtime;
                rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
                u64 end_wait = *mtime;

                wait_time_acc[hart] += end_wait - start_wait;
                wait_time_times[hart] += 1;
            }

            if(spinlock_try_acquire(&KERNEL_VIEWER_SPINLOCK))
            {
                kernel_log_kernel(hart, "hart start viewer talk block");
                spinlock_acquire(&KERNEL_SPINLOCK);
                kernel_log_kernel(hart, "hart has kernel spinlock for viewer talk");

                // massive time loss in talking to the viewer
                // either the slowness is in here or in qemu

                // talk to viewer
                volatile u8* viewer = 0x10000100;
                volatile u8* viewer_should_read = 0x10000101;

                SurfaceSlot* surface = (SurfaceSlot*)KERNEL_PROCESS_ARRAY[vos[current_vo].pid]
                                ->surface_alloc.memory;


                while(*viewer_should_read)
                {
                    recieve_oak_packet();
                }
                if(frame_has_been_requested)
                {
                    Process* process = KERNEL_PROCESS_ARRAY[vos[current_vo].pid];
                    rwlock_acquire_write(&process->process_lock);
                    if(surface_slot_has_commited(process, 0, 1))
                    {
                        framebuffer = surface_slot_swap_present_buffer(
                                        process,
                                        0,
                                        framebuffer
                        );
                        oak_send_video(framebuffer);

                        frame_has_been_requested = 0;
                    }
                    else
                    {
                        surface_slot_fire(process, 0, 0);
                    }
                    rwlock_release_write(&process->process_lock);
                }

                // Output VO stream out to serial
                if(KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->out_stream_count > 0)
                {
                    Stream* out_stream = *((Stream**)KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->out_stream_alloc.memory);
                    u64 byte_count = 0;
                    stream_take(out_stream, 0, 0, &byte_count);
                    while(byte_count)
                    {
                        u8 buffer[32];
                        u64 count = stream_take(out_stream, buffer, 32, &byte_count);
                        uart_printf("%.*s", count, buffer);
                    }
                }
                spinlock_release(&KERNEL_SPINLOCK);
                spinlock_release(&KERNEL_VIEWER_SPINLOCK);
                kernel_log_kernel(hart, "hart end viewer talk block");
            }

            volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
            volatile u64* mtime = (u64*)0x0200bff8;
#if 0
            if(*mtime >= wait_time_print_time[hart] && 1)
            {
                spinlock_acquire(&KERNEL_SPINLOCK);
                uart_printf("average wait time on PROC_ARRAY_RWLOCK for hart%llu is %lf μs\n", hart, ((f64)(wait_time_acc[hart]) / (f64)wait_time_times[hart]) / (f64)(MACHINE_TIMER_SECOND/1000000));
                uart_printf("total wait time on PROC_ARRAY_RWLOCK for hart%llu is %lf ms\n", hart, (f64)wait_time_acc[hart] / (f64)(MACHINE_TIMER_SECOND/1000));
                uart_printf("percentage of time spent waiting on PROC_ARRAY_RWLOCK for hart%llu is %lf %%\n", hart, 100.0*((f64)wait_time_acc[hart] / (f64)(MACHINE_TIMER_SECOND*10)));
                wait_time_acc[hart] = 0;
                wait_time_times[hart] = 0;
                wait_time_print_time[hart] = *mtime + (MACHINE_TIMER_SECOND*10);
                spinlock_release(&KERNEL_SPINLOCK);
            }
#endif

            if(kernel_current_thread_has_thread[hart])
            { KERNEL_PROCESS_ARRAY[kernel_current_thread_pid[hart]]->threads[kernel_current_thread_tid[hart]] = kernel_current_threads[hart]; }

            kernel_choose_new_thread(
                *mtime,
                hart
            );

            if(kernel_current_thread_has_thread[hart])
            { kernel_current_threads[hart] = KERNEL_PROCESS_ARRAY[kernel_current_thread_pid[hart]]->threads[kernel_current_thread_tid[hart]]; }

            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);

            if(kernel_current_thread_has_thread[hart])
            {
                *mtimecmp = *mtime + (MACHINE_TIMER_SECOND / 120);
                // Load thread
                *frame = kernel_current_threads[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return kernel_current_threads[hart].program_counter;
            }
            else // Load kernel thread
            {
                *mtimecmp = *mtime;
                *frame = KERNEL_THREADS[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
        else if(cause_num == 8) {
                uart_printf("User external interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 9) {
                uart_printf("Supervisor external interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 11)
        {
            // Store thread
            if(kernel_current_thread_has_thread[hart])
            {
                kernel_current_threads[hart].frame = *frame;
                kernel_current_threads[hart].program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREADS[hart].frame = *frame;
                KERNEL_THREADS[hart].program_counter = epc;
            }

            if(spinlock_try_acquire(&KERNEL_SPINLOCK))
            {
                {
                volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
                volatile u64* mtime = (u64*)0x0200bff8;
                u64 start_wait = *mtime;
                rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
                u64 end_wait = *mtime;

                wait_time_acc[hart] += end_wait - start_wait;
                wait_time_times[hart] += 1;
                }

                u32 interrupt;
                char character;
                while(plic_interrupt_next(&interrupt) && interrupt == 10)
                {
                    uart_read(&character, 1);

                    //TEMP
                    if(character == 'l')
                    {
                        uart_printf("Printing kernel event log:\n");
                        rwlock_acquire_write(&KERNEL_LOG_LOCK);
                        s64 log_len = KERNEL_LOG_SIZE;
                        s64 log_index = atomic_s64_read(&KERNEL_LOG_INDEX);
                        if(log_index < KERNEL_LOG_SIZE)
                        { log_len = log_index; }

                        u64 log_entry_counter = 0;
                        for(s64 i = log_len; i > 0; i--)
                        {
                            KernelLogEntry entry = KERNEL_LOG[(log_index - i) % KERNEL_LOG_SIZE];
                            if(entry.is_kernel)
                            {
                                uart_printf("%4.4llu) H:%llu - T: %llu | %s:%llu - %s\n",
                                       log_entry_counter++,
                                       entry.hart,
                                       entry.time,
                                       entry.function_name,
                                       entry.line_number,
                                       entry.message);
                            }
                            else
                            {
                                uart_printf("%4.4llu) H:%llu - T: %llu - PID:%llu - TID:%llu | %s:%llu - %s\n",
                                       log_entry_counter++,
                                       entry.hart,
                                       entry.time,
                                       entry.pid,
                                       entry.tid,
                                       entry.function_name,
                                       entry.line_number,
                                       entry.message);
                            }
                        }

                        rwlock_release_write(&KERNEL_LOG_LOCK);
                    }
                    else if(KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->out_stream_count > 0)
                    {
                        Stream* in_stream =
                            *((Stream**)KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->in_stream_alloc.memory);
                        stream_put(in_stream, &character, 1);
                    }
                    plic_interrupt_complete(interrupt);
                }

                rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK); // todo change to read
                spinlock_release(&KERNEL_SPINLOCK);
            }

            if(kernel_current_thread_has_thread[hart])
            {
                // Load thread
                *frame = kernel_current_threads[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return kernel_current_threads[hart].program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREADS[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
    }
    else
    {
             if(cause_num == 0) {
                uart_printf("Interrupt: Instruction address misaligned CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 1) {
                uart_printf("Interrupt: Instruction access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 2) {
                uart_printf("Interrupt: Illegal instruction CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 3) {
                uart_printf("Interrupt: Breakpoint CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 4) {
                uart_printf("Interrupt: Load access misaligned CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 5) {
                uart_printf("Interrupt: Load access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 6) {
                uart_printf("Interrupt: Store/AMO address misaligned CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 7) {
                uart_printf("Interrupt: Store/AMO access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 8) {
                uart_printf("Interrupt: Environment call from U-mode CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 9) {

            // Store thread
            if(kernel_current_thread_has_thread[hart])
            {
                kernel_current_threads[hart].frame = *frame;
                kernel_current_threads[hart].program_counter = epc;
            }
            else // Store kernel thread
            {
                assert(0, "very odd");
            }

            // actually store thread into process array data structure
            {
                rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
                KERNEL_PROCESS_ARRAY[kernel_current_thread_pid[hart]]->threads[kernel_current_thread_tid[hart]] = kernel_current_threads[hart];
                rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            }

            volatile u64* mtime = (u64*)0x0200bff8;
            // locking happens inside do_syscall
            do_syscall(&kernel_current_threads[hart].frame, *mtime, hart);


            // actually load thread from process array data structure
            {
                rwlock_acquire_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
                kernel_current_threads[hart] = KERNEL_PROCESS_ARRAY[kernel_current_thread_pid[hart]]->threads[kernel_current_thread_tid[hart]];
                rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK);
            }

            if(kernel_current_thread_has_thread[hart])
            {
                // Load thread
                *frame = kernel_current_threads[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return kernel_current_threads[hart].program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREADS[hart].frame;
                rwlock_release_read(&KERNEL_TRAP_LOCK);
                KERNEL_TRAP_ENTER_ACC[hart]--;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
        else if(cause_num == 11) {
                uart_printf("Interrupt: Environment call from M-mode CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 12) {
                uart_printf("Interrupt: Instruction page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 13) {
                uart_printf("Interrupt: Load page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 15) {
                uart_printf("Interrupt: Store/AMO page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
    }
    rwlock_release_read(&KERNEL_TRAP_LOCK);
    KERNEL_TRAP_ENTER_ACC[hart]--;
    return 0;
}

#include "tempuser.c"
