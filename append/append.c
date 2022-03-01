#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

void _start()
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    u64 file_to_append = AOS_process_get_program_argument_file(0);

    u64 append_string_len = AOS_process_get_program_argument_string(1, 0, 0);
    u8 append_string_buf[append_string_len];
    u8* append_string = append_string_buf;
    AOS_process_get_program_argument_string(1, append_string, append_string_len);

    u64 original_file_size = AOS_file_get_size(file_to_append);
    u64 new_file_size = original_file_size + append_string_len;

    AOS_file_set_size(file_to_append, new_file_size);

    u64 page_offset = original_file_size & 0xfff;
    u64 write_block_count = (page_offset + append_string_len) / PAGE_SIZE;
    u8* scratch = 0x2344000;
    AOS_alloc_pages(scratch, 1);

    u64 start_block = original_file_size / PAGE_SIZE;
    u64 final_block_count = (new_file_size + PAGE_SIZE - 1) / PAGE_SIZE;

    for(u64 i = start_block; i < final_block_count; i++)
    {
        u64 op[2];
        op[0] = i;
        op[1] = scratch;
        AOS_file_read_blocks(file_to_append, op, 1);

        for(u64 j = page_offset; j < PAGE_SIZE && append_string_len; j++)
        {
            scratch[j] = *(append_string++);
            append_string_len--;
        }
        page_offset = 0;

        AOS_file_write_blocks(file_to_append, op, 1);
    }

    AOS_process_exit();
}
