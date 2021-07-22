
#define MACHINE_TIMER_SECOND 10000000
#define KERNEL_MAX_HART_COUNT 64

#include "../common/types.h"
#include "../common/maths.h"
#include "../common/spinlock.h"
#include "../common/atomics.h"
#include "../common/rwlock.h"

Spinlock KERNEL_SPINLOCK;
Spinlock KERNEL_MEMORY_SPINLOCK;
Spinlock KERNEL_VIEWER_SPINLOCK;

#include "random.h"

#include "uart.h"
#include "printf.h"
void _putchar(char c)
{
    uart_write(&c, 1);
}

void print_stacktrace(u64 start_frame, u64 program_counter, u64 return_address)
{
    u64* fp = start_frame;
    u64* ra = return_address;
    printf("Stacktrace:\n");
    printf("0x%llx\n", program_counter);
    printf("0x%llx\n", return_address - 4);
    if(fp < 0x80000000 || fp > 0x10000000000) { return; }
    for(u64 i = 0; i < 128; i++)
    {
        ra = ((u64)*(fp-1)) - 4;
        fp = *(fp-2);
        if(fp < 0x80000000 || fp > 0x10000000000) { break; }
        printf("0x%llx\n", ra);
    }
}

void assert(u64 stat, char* error)
{
    if(!stat)
    {
        printf("assertion failed: \"");
        printf(error);
        printf("\"\n");
        print_stacktrace(read_fp_register(), read_pc_register(), read_ra_register());
        u8 freeze = 1;
        while(freeze) {};
    }
}

#include "libfuncs.h"

#include "memory.h"
#include "libfuncs2.h"
#include "cyclone_crypto/hash/sha512.h"

#include "disk.h"
#include "file.h"

#include "stream.h"

#include "plic.h"
#include "input.h"
#include "process.h"

#include "video.h"
#include "elf.h"


extern u64 KERNEL_START_OTHER_HARTS;
atomic_s64 KERNEL_HART_COUNT;
 
u64 KERNEL_MMU_TABLE;
u64 KERNEL_SATP_VALUE;


u64 KERNEL_TRAP_STACK_TOP[KERNEL_MAX_HART_COUNT];
Kallocation KERNEL_TRAP_STACK_ALLOCS[KERNEL_MAX_HART_COUNT];
 
u64 KERNEL_STACK_TOP[KERNEL_MAX_HART_COUNT];
Kallocation KERNEL_STACK_ALLOCS[KERNEL_MAX_HART_COUNT-1];
 
Thread KERNEL_THREADS[KERNEL_MAX_HART_COUNT];



/*
 * Debug
 */
u64 wait_time_acc[KERNEL_MAX_HART_COUNT];
u64 wait_time_times[KERNEL_MAX_HART_COUNT];
u64 wait_time_print_time[KERNEL_MAX_HART_COUNT];


#include "process_run.h"

#include "../userland/aos_syscalls.h"
#include "syscall.h"


//for rendering
Framebuffer* framebuffer = 0;
u8 frame_has_been_requested = 0;

#include "oak.h"

void uart_write_string(char* str)
{
    uart_write((u8*)str, strlen(str));
}

struct xoshiro256ss_state KERNEL_GLOBAL_RANDO_STATE = {5, 42, 68, 1};
u64 kinit()
{
    uart_init();

    printf("Counting HARTS...");
    KERNEL_HART_COUNT.value = 0;
    atomic_s64_increment(&KERNEL_HART_COUNT);
    for(u64 i = 0; i < 2000000; i++) { __asm__("nop"); } // wait
    KERNEL_START_OTHER_HARTS = 0; // causes other harts to increment and get ready
    for(u64 i = 0; i < 4000000; i++) { __asm__("nop"); } // wait
    printf("    There are %lld HARTS.\n", KERNEL_HART_COUNT.value);

    KERNEL_MMU_TABLE = (u64)mem_init();

    thread_runtime_commons.memory_alloc.page_count = 0;
    thread_runtime_commons.len = 0;
    spinlock_create(&thread_runtime_commons_lock);
    for(s64 i = 0; i < KERNEL_HART_COUNT.value; i++)
    {
        Kallocation trap_stack = kalloc_pages(8);
        KERNEL_TRAP_STACK_TOP[i] = trap_stack.memory + (PAGE_SIZE * trap_stack.page_count);
        KERNEL_TRAP_STACK_ALLOCS[i] = trap_stack;

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

        // thread runtimes
        local_thread_runtimes[i].memory_alloc.page_count = 0;
        local_thread_runtimes[i].len = 0;

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

    printf("Entering supervisor mode...");
    return KERNEL_SATP_VALUE;
}

void kmain()
{
    printf("done.\n    Successfully entered kmain with supervisor mode enabled.\n\n");
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

    // lock kernel
    spinlock_create(&KERNEL_SPINLOCK);
    spinlock_create(&KERNEL_MEMORY_SPINLOCK);
    spinlock_create(&KERNEL_VIEWER_SPINLOCK);
    spinlock_acquire(&KERNEL_SPINLOCK);
    KERNEL_START_OTHER_HARTS = 0;

    /* TESTING */
    u64 dir_id = kernel_directory_create_imaginary("Über directory");
    u64 dir_name_len = kernel_directory_get_name(dir_id, 0, 0);
    char dir_name[dir_name_len];
    kernel_directory_get_name(dir_id, dir_name, dir_name_len);
    printf("The directory created is called : %s\n", dir_name);

    u64 smol_dir = kernel_directory_create_imaginary("smõl directory");
    u64 file_id = kernel_file_create_imaginary("an imaginary file");
    kernel_file_increment_reference_count(file_id);
    kernel_directory_add_file(smol_dir, file_id);
    kernel_directory_add_subdirectory(dir_id, smol_dir);

    u64 full_dir_id = kernel_directory_create_imaginary("filled directory");
    kernel_directory_increment_reference_count(full_dir_id);
    kernel_directory_add_subdirectory(dir_id, full_dir_id);

    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));
    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));
    kernel_directory_add_subdirectory(full_dir_id, kernel_directory_create_imaginary("another small directory"));

    for(u64 i = 0; i < 10; i++)
    {
        char name[32];
        sprintf(name, "file#%lld", 10-i);
        u64 file = kernel_file_create_imaginary(name);
        if(!kernel_directory_add_file(full_dir_id, file))
        {
//            kernel_file_free(file);
            if(is_valid_file_id(file)) { printf("file free failed for %s\n", name); }
        }
    }

    debug_print_directory_tree(dir_id, "");
    kernel_directory_free(dir_id);
    debug_print_directory_tree(full_dir_id, "");
    kernel_directory_free(full_dir_id);

    printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    kernel_file_set_size(file_id, 100);
    printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    kernel_file_set_size(file_id, 10000);
    printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    kernel_file_set_size(file_id, 6100);
    printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
    kernel_file_set_size(file_id, 100000);
    printf("file : %llu, %llu\n", kernel_file_get_block_count(file_id), kernel_file_get_size(file_id));
mem_debug_dump_table_counts(1);
    u8 block[PAGE_SIZE];
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
    kernel_file_free(file_id);
mem_debug_dump_table_counts(1);

    load_drive_partitions();
    debug_print_directory_tree(drive1_partition_directory, "");

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
    printf("initialized hart#%llu\n", hartid);
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
    printf("args:\n  epc: %llx\n  tval: %llx\n  cause: %llx\n  hart: %llx\n  status: %llx\n  frame: %llx\n",
            epc, tval, cause, hart, status, frame);
    printf("frame:\n regs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  x%lld: %llx\n", i, frame->regs[i]); }
    printf(" fregs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  f%lld: %llx\n", i, frame->fregs[i]); }
    printf(" satp: %lx, trap_stack: %lx\n", frame->satp, frame);
    printf("Kernel has hung.\n");
    u64 ptr = frame->regs[8];
    u64 r = mmu_virt_to_phys(frame->satp << 12, ptr, (u64*)&ptr);
    print_stacktrace(ptr, epc, frame->regs[1]);
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

    if(async)
    {
             if(cause_num == 0) {
                printf("User software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 1) {
                printf("Supervisor software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 3) {
                printf("Machine software interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 4) {
                printf("User timer interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 5) {
                printf("Supervisor timer interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 7) {

            // Store thread
            if(kernel_current_threads[hart] != 0)
            {
                kernel_current_threads[hart]->frame = *frame;
                kernel_current_threads[hart]->program_counter = epc;
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
                spinlock_acquire(&KERNEL_SPINLOCK);
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
                    surface_slot_fire(process, 0);
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
                        printf("%.*s", count, buffer);
                    }
                }
                spinlock_release(&KERNEL_SPINLOCK);
                spinlock_release(&KERNEL_VIEWER_SPINLOCK);
            }

            volatile u64* mtimecmp = ((u64*)0x02004000) + hart;
            volatile u64* mtime = (u64*)0x0200bff8;
#if 0
            if(*mtime >= wait_time_print_time[hart] && 1)
            {
                spinlock_acquire(&KERNEL_SPINLOCK);
                printf("average wait time on PROC_ARRAY_RWLOCK for hart%llu is %lf μs\n", hart, ((f64)(wait_time_acc[hart]) / (f64)wait_time_times[hart]) / (f64)(MACHINE_TIMER_SECOND/1000000));
                printf("total wait time on PROC_ARRAY_RWLOCK for hart%llu is %lf ms\n", hart, (f64)wait_time_acc[hart] / (f64)(MACHINE_TIMER_SECOND/1000));
                printf("percentage of time spent waiting on PROC_ARRAY_RWLOCK for hart%llu is %lf %%\n", hart, 100.0*((f64)wait_time_acc[hart] / (f64)(MACHINE_TIMER_SECOND*10)));
                wait_time_acc[hart] = 0;
                wait_time_times[hart] = 0;
                wait_time_print_time[hart] = *mtime + (MACHINE_TIMER_SECOND*10);
                spinlock_release(&KERNEL_SPINLOCK);
            }
#endif
            kernel_choose_new_thread(
                &kernel_current_threads[hart],
                *mtime,
                hart
            );
            *mtimecmp = *mtime + (MACHINE_TIMER_SECOND / 1000);

            rwlock_release_read(&KERNEL_PROCESS_ARRAY_RWLOCK); // todo change to read

            if(kernel_current_threads[hart] != 0)
            {
                // Load thread
                *frame = kernel_current_threads[hart]->frame;
                return kernel_current_threads[hart]->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREADS[hart].frame;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
        else if(cause_num == 8) {
                printf("User external interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 9) {
                printf("Supervisor external interrupt CPU%lld\n", hart);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 11)
        {
            if(kernel_current_threads[hart] != 0)
            {
                kernel_current_threads[hart]->frame = *frame;
                kernel_current_threads[hart]->program_counter = epc;
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
                    if(KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->out_stream_count > 0)
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

            if(kernel_current_threads[hart] != 0)
            {
                // Load thread
                *frame = kernel_current_threads[hart]->frame;
                return kernel_current_threads[hart]->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREADS[hart].frame;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
    }
    else
    {
             if(cause_num == 0) {
                printf("Interrupt: Instruction address misaligned CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 1) {
                printf("Interrupt: Instruction access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 2) {
                printf("Interrupt: Illegal instruction CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 3) {
                printf("Interrupt: Breakpoint CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 4) {
                printf("Interrupt: Load access misaligned CPU%lld -> 0x%llx: 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 5) {
                printf("Interrupt: Load access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 6) {
                printf("Interrupt: Store/AMO address misaligned CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 7) {
                printf("Interrupt: Store/AMO access fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 8) {
                printf("Interrupt: Environment call from U-mode CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 9) {

            // Store thread
            if(kernel_current_threads[hart] != 0)
            {
                kernel_current_threads[hart]->frame = *frame;
                kernel_current_threads[hart]->program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREADS[hart].frame = *frame;
                KERNEL_THREADS[hart].program_counter = epc;
            }

            volatile u64* mtime = (u64*)0x0200bff8;
            // locking happens inside do_syscall
            do_syscall(&kernel_current_threads[hart], *mtime, hart);

            if(kernel_current_threads[hart] != 0)
            {
                // Load thread
                *frame = kernel_current_threads[hart]->frame;
                return kernel_current_threads[hart]->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREADS[hart].frame;
                return KERNEL_THREADS[hart].program_counter;
            }
        }
        else if(cause_num == 11) {
                printf("Interrupt: Environment call from M-mode CPU%lld -> 0x%llx\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 12) {
                printf("Interrupt: Instruction page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 13) {
                printf("Interrupt: Load page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 15) {
                printf("Interrupt: Store/AMO page fault CPU%lld -> 0x%llx : 0x%llx\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
    }
    return 0;
}

#include "tempuser.h"
