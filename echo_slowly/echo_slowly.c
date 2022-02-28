#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

void _start()
{
    //temp
    u64 arg_len = AOS_process_get_program_argument_string(0, 0, 0);
    u8 arg_buf[arg_len];
    AOS_process_get_program_argument_string(0, arg_buf, arg_len);
    AOS_H_printf("ECHO: %.*s\n", arg_len, arg_buf);

    AOS_process_exit();
}
