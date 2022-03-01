#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

void _start()
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    u64 file_to_cat = AOS_process_get_program_argument_file(0);

    u8 name[64];
    AOS_file_get_name(file_to_cat, name, 64);
    AOS_H_printf("File is called %s?\n", name);

    AOS_H_printf("Begin catting...\n");
    u8* scratch = 0x2344000;
    AOS_alloc_pages(scratch, 1);

    u64 block_count = AOS_file_get_block_count(file_to_cat);
    u64 bytes_left = AOS_file_get_size(file_to_cat);
    for(u64 i = 0; i < block_count; i++)
    {
        u64 op[2];
        op[0] = i;
        op[1] = scratch;
        AOS_file_read_blocks(file_to_cat, op, 1);
        u64 write_amount = PAGE_SIZE;
        if(write_amount > bytes_left) { write_amount = bytes_left; }
        bytes_left -= write_amount;

        AOS_stream_put(AOS_STREAM_STDOUT, scratch, write_amount);
    }

    AOS_process_exit();
}
