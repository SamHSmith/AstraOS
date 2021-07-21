#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

void _start()
{
    u64 counter = 0;
    AOS_H_printf("You are running a 100%% GPLv3 Operating System.\nrms would be proud.\n");
    while(1)
    {
        AOS_H_printf("Do you wish to exit? [Y/n] :");
        u64 is_exiting = 1;
        while(1)
        {
            AOS_thread_awake_after_time(1000);
            AOS_thread_sleep();
            u64 byte_count;
            AOS_stream_take(AOS_STREAM_STDIN, 0, 0, &byte_count);
            u8 scratch[byte_count];
            u64 read_count = AOS_stream_take(AOS_STREAM_STDIN, scratch, byte_count, &byte_count);
            if(!read_count)
            { continue; }
            if(scratch[0] == '\n' || scratch[0] == 'y' || scratch[0] == 'Y')
            { break; }
            else
            { is_exiting = 0; break; }
        }
        if(is_exiting)
        { break; }

        counter++;
        if(counter == 1)
        { AOS_H_printf("Why are you staying there is so much freesoftware to enjoy!\n"); }
    }

    AOS_process_exit();
}
