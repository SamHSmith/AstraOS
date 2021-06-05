#include "../common/types.h"
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

#include "plic.h"
#include "input.h"
#include "process.h"

#include "video.h"
#include "elf.h"

#include "process_run.h"

#include "syscall.h"


//for rendering
Framebuffer* framebuffer = 0;
u8 frame_has_been_requested = 0;

#include "oak.h"

void uart_write_string(char* str)
{
    uart_write((u8*)str, strlen(str));
}

u64 KERNEL_MMU_TABLE;
Thread KERNEL_THREAD;
u64 KERNEL_TRAP_STACK = 0; // will need more when we have multicore
u64 kinit()
{
    uart_init();
    KERNEL_MMU_TABLE = (u64)mem_init();
    Kallocation stack = kalloc_pages(8);
    KERNEL_TRAP_STACK = stack.memory + (PAGE_SIZE * stack.page_count);

    u64 satp_val = mmu_table_ptr_to_satp((u64*)KERNEL_MMU_TABLE);

    printf("Entering supervisor mode...");
    return satp_val;
}

void kmain()
{
    printf("done.\n    Successfully entered kmain with supervisor mode enabled.\n\n");
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

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
    {
        u64 partition_count = kernel_directory_get_files(drive1_partition_directory, 0, 0);
        u64 partitions[partition_count];
        kernel_directory_get_files(drive1_partition_directory, partitions, partition_count);

        for(u64 i = 0; i < 0/*partition_count*/; i++)
        {
            u64 pid;
            if(!create_proccess_from_file(partitions[i], &pid)) { continue; }
            u64 name_len = kernel_file_get_name(partitions[i], 0, 0);
            u8 name[name_len];
            kernel_file_get_name(partitions[i], name, name_len);
            printf("Loaded elf file from partition \"%s\"\n", name);
        }
    }

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

    for(s64 i = 0; i < 5; i++) { uart_write_string("\n"); } //tells the viewer we have initialized

    process_init();

    while(1)
    {
//        printf("Kernel Bored :(\n");
//        for(u64 i = 0; i < 640000000; i++) {}
    }
}

void kinit_hart(u64 hartid)
{

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

#define VIEWBUFF_SIZE (4096*1024)

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
            if(kernel_current_thread != 0)
            {
                kernel_current_thread->frame = *frame;
                kernel_current_thread->program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREAD.frame = *frame;
                KERNEL_THREAD.program_counter = epc;
            }

            // talk to viewer
            volatile u8* viewer = 0x10000100;
            volatile u8* viewer_should_read = 0x10000101;

            Surface* surface = &((SurfaceSlot*)KERNEL_PROCESS_ARRAY[vos[current_vo].pid]
                               ->surface_alloc.memory)->surface;

            while(*viewer_should_read)
            {
                recieve_oak_packet();
            }
            if(frame_has_been_requested && surface_has_commited(*surface))
            {
                Framebuffer* temp = framebuffer;
                framebuffer = surface->fb_present;
                surface->fb_present = temp;
                surface->has_commited = 0;
                oak_send_video(framebuffer);

                frame_has_been_requested = 0;
            }
            surface->has_been_fired = 1;


            // Reset the Machine Timer
            volatile u64* mtimecmp = (u64*)0x02004000;
            volatile u64* mtime = (u64*)0x0200bff8;

            kernel_current_thread = kernel_choose_new_thread(*mtime, kernel_current_thread != 0);
            *mtimecmp = *mtime + (10000000 / 1000);

            if(kernel_current_thread != 0)
            {
                // Load thread
                *frame = kernel_current_thread->frame;
                return kernel_current_thread->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREAD.frame;
                return KERNEL_THREAD.program_counter;
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
//            printf("Machine external interrupt CPU%lld\n", hart);
            // Store thread
            if(kernel_current_thread != 0)
            {
                kernel_current_thread->frame = *frame;
                kernel_current_thread->program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREAD.frame = *frame;
                KERNEL_THREAD.program_counter = epc;
            }

            u32 interrupt;
            char character;
            while(plic_interrupt_next(&interrupt))
            {
                if(interrupt == 10 && uart_read(&character, 1))
                {
                    if(character == 'a')
                    {
                        uart_write("new;;_;;frame", 13);

                        u32 width, height;
                        uart_read_blocking(&width, 4);
                        uart_read_blocking(&height, 4);

                        s32 mouse_data[3];
                        uart_read_blocking(mouse_data, 3*4);
                        RawMouse* mouse = &KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->mouse;
                        new_mouse_input_from_serial(mouse, mouse_data);

                        for(u64 i = 0; i < (width * height) >> 3; i++)
                        {
                            u8 r=0;
                            uart_write(&r, 1);
                        }
                    }
                    else if(character == 'd') // Key down
                    {
                        u8 scode;
                        uart_read_blocking(&scode, 1);
                        KeyboardEventQueue* kbd_event_queue = 
                            &KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->kbd_event_queue;
                        keyboard_put_new_event(kbd_event_queue, KEYBOARD_EVENT_PRESSED, scode);
                    }
                    else if(character == 'u') // Key up
                    {
                        u8 scode;
                        uart_read_blocking(&scode, 1);
                        KeyboardEventQueue* kbd_event_queue =
                            &KERNEL_PROCESS_ARRAY[vos[current_vo].pid]->kbd_event_queue;
                        keyboard_put_new_event(kbd_event_queue, KEYBOARD_EVENT_RELEASED, scode);
                    }
                    else {
                        printf("you typed the character: %c\n", character);
                    }
                }
                plic_interrupt_complete(interrupt);
            }
            if(kernel_current_thread != 0)
            {
                // Load thread
                *frame = kernel_current_thread->frame;
                return kernel_current_thread->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREAD.frame;
                return KERNEL_THREAD.program_counter;
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
            if(kernel_current_thread != 0)
            {
                kernel_current_thread->frame = *frame;
                kernel_current_thread->program_counter = epc;
            }
            else // Store kernel thread
            {
                KERNEL_THREAD.frame = *frame;
                KERNEL_THREAD.program_counter = epc;
            }

            volatile u64* mtime = (u64*)0x0200bff8;
            do_syscall(&kernel_current_thread, *mtime);

            if(kernel_current_thread != 0)
            {
                // Load thread
                *frame = kernel_current_thread->frame;
                return kernel_current_thread->program_counter;
            }
            else // Load kernel thread
            {
                *frame = KERNEL_THREAD.frame;
                return KERNEL_THREAD.program_counter;
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
