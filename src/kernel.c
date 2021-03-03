#include "types.h"
#include "random.h"

#include "uart.h"
#include "printf.h"
void _putchar(char c)
{
    uart_write(&c, 1);
}

#include "memory.h"
#include "plic.h"
#include "input.h"
#include "proccess.h"
#include "video.h"
#include "proccess_run.h"
#include "syscall.h"

// --- Lib maybe? ---
u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void* memcpy(void* dest, const void* src, u64 n)
{
    void* orig_dest = dest;
    u64 n8 = (n >> 3) << 3;
    for(u64 i = 0; i < (n8 >> 3); i++)
    { ((u64*)dest)[i] = ((u64*)src)[i]; }

    dest += n8;
    src += n8;
    n -= n8;
    for(u64 i = 0; i < n; i++)
    { ((u8*)dest)[i] = ((u8*)src)[i]; }
    return orig_dest;
}

void uart_write_string(char* str)
{
    uart_write((u8*)str, strlen(str));
}

void assert(u64 stat, char* error)
{
    if(!stat)
    {
        uart_write_string("assertion failed: \"");
        uart_write_string(error);
        uart_write_string("\"\n");
        while(1) {};
    }
}

u64 KERNEL_MMU_TABLE;
Thread KERNEL_THREAD;

u64 kinit()
{
    uart_init();
    KERNEL_MMU_TABLE = (u64)mem_init();

    u64 satp_val = mmu_table_ptr_to_satp((u64*)KERNEL_MMU_TABLE);

    printf("Entering supervisor mode...");
    return satp_val;
}

void kmain()
{
    printf("done.\n    Successfully entered kmain with supervisor mode enabled.\n\n");
    uart_write_string("Hello there, welcome to the ROS operating system\nYou have no idea the pain I went through to make these characters you type appear on screen\n\n");

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

    surface = surface_create();

    for(s64 i = 0; i < 5; i++) { uart_write_string("\n"); } //tells the viewer we have initialized

    proccess_init();

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
    printf("args:\n  epc: %x\n  tval: %x\n  cause: %x\n  hart: %x\n  status: %x\n  frame: %x\n",
            epc, tval, cause, hart, status, frame);
    printf("frame:\n regs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  x%lld: %lx\n", i, frame->regs[i]); }
    printf(" fregs:\n");
    for(u64 i = 0; i < 32; i++) { printf("  f%lld: %lx\n", i, frame->fregs[i]); }
    printf(" satp: %lx, trap_stack: %lx\n", frame->satp, frame);
    printf("Kernel has hung.");
    while(1) {}
}

Framebuffer* framebuffer = 0;

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
            // Reset the Machine Timer
            volatile u64* mtimecmp = (u64*)0x02004000;
            volatile u64* mtime = (u64*)0x0200bff8;

            kernel_current_thread = kernel_choose_new_thread(*mtime, kernel_current_thread != 0);
            *mtimecmp = *mtime + (10000000 / 1000);
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
                        new_mouse_input_from_serial(mouse_data);

                        surface.width = width;
                        surface.height = height;
                        u8 frame_dropped = 1;

                        if(framebuffer == 0)
                        { framebuffer = framebuffer_create(width, height); }
                        else if(framebuffer->width != width || framebuffer->height != height)
                        {
                            kfree_pages(framebuffer->alloc);
                            framebuffer = framebuffer_create(width, height);
                        }

                        if(surface_has_commited(surface))
                        {
                            Framebuffer* temp = framebuffer;
                            framebuffer = surface.fb_present;
                            surface.fb_present = temp;
                            surface.has_commited = 0;
                            frame_dropped = 0;
                        }

                        for(u64 i = 0; i < (width * height) >> 3; i++)
                        {
                            u8 r=0;
                            for(u64 j = 0; j < 8; j++)
                            {
                                r |= (framebuffer->data[4*(i*8 + j) + 0] > 0.0) << j;
                                r |= (framebuffer->data[4*(i*8 + j) + 1] > 0.0) << j;
                                r |= (framebuffer->data[4*(i*8 + j) + 2] > 0.0) << j;
                            }
                            uart_write(&r, 1);
                        }
                        if(frame_dropped) { printf("KERNEL: A frame was dropped.\n"); }
                    }
                    else if(character == 'd') // Key down
                    {
                        u8 scode;
                        uart_read_blocking(&scode, 1);
                        printf("scancode %u down\n", scode);
                    }
                    else if(character == 'u') // Key up
                    {
                        u8 scode;
                        uart_read_blocking(&scode, 1);
                        printf("scancode %u up\n", scode);
                    }
                    else {
                        printf("you typed the character: %c\n", character);
                    }
                }
                plic_interrupt_complete(interrupt);
            }
        }
    }
    else
    {
             if(cause_num == 0) {
                printf("Interrupt: Instruction address misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 1) {
                printf("Interrupt: Instruction access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 2) {
                printf("Interrupt: Illegal instruction CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 3) {
                printf("Interrupt: Breakpoint CPU%lld -> 0x%x\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 4) {
                printf("Interrupt: Load access misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 5) {
                printf("Interrupt: Load access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 6) {
                printf("Interrupt: Store/AMO address misaligned CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 7) {
                printf("Interrupt: Store/AMO access fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 8) {
                printf("Interrupt: Environment call from U-mode CPU%lld -> 0x%x\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 9) {
            volatile u64* mtime = (u64*)0x0200bff8;
            do_syscall(&kernel_current_thread, *mtime);
        }
        else if(cause_num == 11) {
                printf("Interrupt: Environment call from M-mode CPU%lld -> 0x%x\n", hart, epc);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 12) {
                printf("Interrupt: Instruction page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 13) {
                printf("Interrupt: Load page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
        else if(cause_num == 15) {
                printf("Interrupt: Store/AMO page fault CPU%lld -> 0x%x: 0x%x\n", hart, epc, tval);
                trap_hang_kernel(epc, tval, cause, hart, status, frame);
        }
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

// To be user code

void user_surface_commit(u64 surface_slot);
u64 user_surface_acquire(u64 surface_slot, Framebuffer** fb);
void user_thread_sleep(u64 duration);
void user_wait_for_surface_draw(u64 surface_slot);
u64 user_get_raw_mouse(RawMouse* buf, u64 len);
 
void thread1_func()
{
f64 ballx = 0.0;
f64 bally = 0.0;
while(1) {
    user_wait_for_surface_draw(42);
    Framebuffer* fb;
    if(user_surface_acquire(42, &fb))
    {
        u64 raw_mouse_count = user_get_raw_mouse(0, 0);
        RawMouse mouses[raw_mouse_count];
        user_get_raw_mouse(mouses, raw_mouse_count);

        for(u64 i = 0; i < raw_mouse_count; i++)
        {
            ballx += mouses[i].x / 5.0;
            bally += mouses[i].y / 5.0;
        }

        if(ballx < 0.0) { ballx = 0.0; }
        if(bally < 0.0) { bally = 0.0; }
        if((u64)ballx >= fb->width) { ballx = (f64)(fb->width-1); }
        if((u64)bally >= fb->height) { bally = (f64)(fb->height-1); }

        for(u64 y = 0; y < fb->height; y++)
        for(u64 x = 0; x < fb->width; x++)
        {
            u64 i = x + (y * fb->width);
            if(x == (u64)ballx || y == (u64)bally)
            {
                fb->data[i*4 + 0] = 0.0;
                fb->data[i*4 + 1] = 1.0;
                fb->data[i*4 + 2] = 1.0;
                fb->data[i*4 + 3] = 1.0;
            }
            else
            {
                fb->data[i*4 + 0] = 0.0;
                fb->data[i*4 + 1] = 0.0;
                fb->data[i*4 + 2] = 0.0;
                fb->data[i*4 + 3] = 1.0;
            }
        }
 
        user_surface_commit(42);
    }
    else
    {
        printf("there was nothing to render\n");
        user_thread_sleep(10000000/1000);
    }
}
}
 
void thread2_func()
{
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
