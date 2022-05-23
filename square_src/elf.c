#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

#include "../userland/aso_tonitrus.h"

#include "../common/maths.h"

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

void _start()
{
    // this enables the use of global variables
    __asm__(".option norelax");
    __asm__("la gp, __global_pointer$");
    __asm__(".option relax");

    AOS_H_printf(
        "Hi I'm square-dave and I live in an elf file on a partition on the RADICAL PARTITION SYSTEM\n"
    );

    u8 is_running_as_ega = 0;
    u64 ega_session_id;
    {
        u8* name = "embedded_gui_application_ipfc_api_v1";
        u64 name_len = strlen(name);
        if(AOS_IPFC_init_session(name, name_len, &ega_session_id))
        {
            is_running_as_ega = 1;
            AOS_IPFC_call(ega_session_id, 1, 0, 0); // show ega
        }
    }

    u8 is_running_as_twa = 0;
    u64 twa_session_id;
    u64 twa_window_handle = 0;
    if(!is_running_as_ega)
    {
        u8* name = "thunder_windowed_application_ipfc_api_v1";
        u64 name_len = strlen(name);
        if(AOS_IPFC_init_session(name, name_len, &twa_session_id))
        {
            is_running_as_twa = 1;
            // create window
            u64 scratch[1024/8];
            if(AOS_IPFC_call(twa_session_id, 0, 0, &scratch))
            {
                twa_window_handle = scratch[0];
                AOS_H_printf("Created a thunder window! handle = %llu\n", twa_window_handle);
            }
            else
            {
                AOS_IPFC_close_session(twa_session_id);
                is_running_as_twa = 0;
                AOS_H_printf("Failed to create thunder window!\n");
            }

#if 0
            // for testing destroy window
            if(!AOS_IPFC_call(twa_session_id, 1, &scratch, 0))
            {
                AOS_H_printf("Failed to destroy window...\n");
            }
#endif
        }
        else
        { AOS_H_printf("Failed to init thunder session\n"); }
    }

    f64 start_time = AOS_H_time_get_seconds();

    f64 square_x_control = 0.0;
    f64 square_y_control = 0.0;
    f64 last_frame_time = start_time;
    u8 input = 0;

    f64 cursor_x = 0.0;
    f64 cursor_y = 0.0;

    u8 lock_square_to_cursor = 0;

while(1)
{
#define DEBUG_IPFC_TIME 0

    u16 surfaces[512];
    u16 surface_count = 0;
    if(is_running_as_ega)
    {
#if DEBUG_IPFC_TIME
        f64 sec_before_call = AOS_H_time_get_seconds();
#endif
        surface_count = AOS_IPFC_call(ega_session_id, 0, 0, surfaces);
#if DEBUG_IPFC_TIME
        f64 sec_after_call = AOS_H_time_get_seconds();
        AOS_H_printf("time to get surfaces via ipfc : %5.5lf ms\n", (sec_after_call - sec_before_call) * 1000.0);
#endif
    }
    else if(is_running_as_twa)
    {
#if DEBUG_IPFC_TIME
        f64 sec_before_call = AOS_H_time_get_seconds();
#endif
        u64 scratch[1024/8];
        scratch[0] = twa_window_handle;
        surface_count = AOS_IPFC_call(twa_session_id, 2, scratch, surfaces);
#if DEBUG_IPFC_TIME
        f64 sec_after_call = AOS_H_time_get_seconds();
        AOS_H_printf("time to get surfaces via ipfc : %5.5lf ms\n", (sec_after_call - sec_before_call) * 1000.0);
#endif

        {
            f64 fscratch[1024/8];
            if(AOS_IPFC_call(twa_session_id, 3, scratch, fscratch))
            {
//                AOS_H_printf("       cursor position is %3.3lf %3.3lf\n", fscratch[0], fscratch[1]);
                cursor_x = fscratch[0];
                cursor_y = fscratch[1];
            }
            else
            {
                AOS_H_printf("failed to get cursor location\n");
            }
        }
    }

    //AOS_thread_awake_on_surface(&surfaces, surface_count);
    //AOS_thread_sleep();

    { // read from stdin
        while(1)
        {
            u64 byte_count;
            AOS_stream_take(AOS_STREAM_STDIN, 0, 0, &byte_count);
            char character;
            if(byte_count && AOS_stream_take(AOS_STREAM_STDIN, &character, 1, &byte_count))
            {
                AOS_stream_put(AOS_STREAM_STDOUT, &character, 1);
            }
            else
            { break; }
        }
    }

static last_middle_buffer_handle = U64_MAX;

    u64* middle_buffer_ptr = 0x424242000; // the three zeroes alignes it to the page boundry
    u64 commit_semaphore_handle = 0;
    u64 acquire_semaphore_handle = 0;
    {
        u64 scratch[1024/8];
        scratch[0] = twa_window_handle;

        u64 before_wait_time = AOS_get_cpu_time();
        AOS_IPFC_call(twa_session_id, 4, scratch, scratch);
        u64 after_wait_time = AOS_get_cpu_time();

        //AOS_H_printf("ipfc took for %llu \u03BCs\n", ((after_wait_time - before_wait_time) * 1000000) / AOS_get_cpu_timer_frequency());

        if(scratch[0] != last_middle_buffer_handle)
        {
            AOS_H_printf("OMG I was given a new middle buffer!\n");
            aso_chartam_mediam_pone(scratch[0], middle_buffer_ptr, 0, aso_chartae_mediae_magnitudem_disce(scratch[0]));
            last_middle_buffer_handle = scratch[0];
        }
        commit_semaphore_handle = scratch[1];
        acquire_semaphore_handle = scratch[2];
    }

#if 0
    while(!aso_semaphorum_medium_expectare_conare(acquire_semaphore_handle))
    { __asm__("nop"); } // TODO actually wait for semaphore instead
#endif
#if 1
    u64 before_wait_time = AOS_get_cpu_time();
    aso_semaphorum_medium_expecta(acquire_semaphore_handle);
    u64 after_wait_time = AOS_get_cpu_time();
    //AOS_H_printf("waited for %llu \u03BCs\n", ((after_wait_time - before_wait_time) * 1000000) / AOS_get_cpu_timer_frequency());
#endif


    PicturaAnimata* pictura = middle_buffer_ptr;
    u8* data_picturae;
    if(pictura->dispositio_elementorum)
    {
        data_picturae = index_absolutus_ad_picturam_primam(pictura);
    }
    else
    {
        data_picturae = index_absolutus_ad_picturam_secundam(pictura);
    }

//    u64 fb_page_count;
//    if(surface_count) { fb_page_count = AOS_surface_acquire(surfaces[0], 0, 0); }
//    if(surface_count && AOS_surface_acquire(surfaces[0], fb, fb_page_count))
    {
        f64 frame_start = AOS_H_time_get_seconds();
        f64 delta_time = frame_start - last_frame_time;
        last_frame_time = frame_start;

        delta_time = 1.0 / 60.0;

        // unsquishing the square
        f64 x_scale = 1.0;
        f64 y_scale = 1.0;
        if(pictura->altitudo > pictura->latitudo)
        {
            y_scale = (f64)pictura->altitudo / (f64)pictura->latitudo;
        }
        else
        {
            x_scale = (f64)pictura->latitudo / (f64)pictura->altitudo;
        }

        { // Keyboard events
            u64 kbd_event_count = AOS_get_keyboard_events(0, 0);
            AOS_KeyboardEvent kbd_events[kbd_event_count];
            kbd_event_count = AOS_get_keyboard_events(kbd_events, kbd_event_count);
            for(u64 i = 0; i < kbd_event_count; i++)
            {
                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_NOTHING)
                { continue; }

                if(kbd_events[i].event == AOS_KEYBOARD_EVENT_PRESSED)
                {
                    u64 scancode = kbd_events[i].scancode;
                    if(scancode == 99)
                    { input = input | 1; }
                    else if(scancode == 100)
                    { input = input | 2; }
                    else if(scancode == 101)
                    { input = input | 4; }
                    else if(scancode == 102)
                    { input = input | 8; }
                    else if(scancode == 60)
                    { lock_square_to_cursor = !lock_square_to_cursor; }
                    else if(scancode == 8)
                    { AOS_process_exit(); }
                }
                else
                {
                    u64 scancode = kbd_events[i].scancode;
                    if(scancode == 99)
                    { input = input & ~1; }
                    else if(scancode == 100)
                    { input = input & ~2; }
                    else if(scancode == 101)
                    { input = input & ~4; }
                    else if(scancode == 102)
                    { input = input & ~8; }
                }
                AOS_H_printf("kbd event: %u, scancode: %u\n", kbd_events[i].event, kbd_events[i].scancode);
            }
        }

        if(input & 1)
        { square_x_control -= delta_time; }
        if(input & 2)
        { square_y_control += delta_time; }
        if(input & 4)
        { square_y_control -= delta_time; }
        if(input & 8)
        { square_x_control += delta_time; }

        f32 time = frame_start - start_time;

        f32 square_x = (f32)square_x_control + sineF32(3.0*time)/2.0;
        f32 square_y = (f32)square_y_control + sineF32(3.0*time/M_PI)/2.0;

        if(is_running_as_twa && lock_square_to_cursor)
        {
            square_x = ( cursor_x - 0.5) * 2.0;
            square_y = (-cursor_y + 0.5) * 2.0;
        }

        f32 red =   255.0 * (sineF32((time*M_PI)/2.0) + 1.0) / 2.0;
        f32 green = 255.0 * (sineF32((time*M_PI)/3.0) + 1.0) / 2.0;
        f32 blue =  255.0 * (sineF32((time*M_PI)/5.0) + 1.0) / 2.0;

        u8 background_red = AOS_get_cpu_time();
        u8 background_green = AOS_get_cpu_time();
        u8 background_blue = AOS_get_cpu_time();

        f32 s = cosineF32(time/10.0 * 2*M_PI);
        f32 c = sineF32(time/10.0 * 2*M_PI);
        f32 p1x = -0.25 * c -  0.25 * s;
        f32 p1y =  0.25 * c + -0.25 * s;
        f32 p2x =  0.25 * c -  0.25 * s;
        f32 p2y =  0.25 * c +  0.25 * s;

        f32 d1x = p2x - p1x;
        f32 d1y = p2y - p1y;
        f32 d2x = -p1x - p2x;
        f32 d2y = -p1y - p2y;

        f32 dpfx = 2.0 / (f32)pictura->latitudo;
        f32 dpfy = 2.0 / (f32)pictura->altitudo;
        f32 alias_by = (dpfx + dpfy) / 2.0;
        f32 pfx = -1.0;
        f32 pfy = -1.0;

        u64 start_y = (1.0 - square_y - 0.35)/2.0 * (f32)pictura->altitudo;
        u64 end_y = (1.0 - square_y + 0.35)/2.0 * (f32)pictura->altitudo;

        u64 start_x = (1.0 + square_x - 0.4)/2.0 * (f32)pictura->latitudo;
        u64 end_x = (1.0 + square_x + 0.4)/2.0 * (f32)pictura->latitudo;

        for(u64 i = 0; i < pictura->latitudo * pictura->altitudo * 3 + 24; i += 24)
        {
            *((u64*)&data_picturae[i]) = 0;
            *((u64*)&data_picturae[i+8]) = 0;
            *((u64*)&data_picturae[i+16]) = 0;
        }

        for(u64 y = start_y; y < end_y; y++)
        {
            for(u64 x = start_x; x < end_x; x++)
            {
                u64 i = x + y * pictura->latitudo;

                f32 e1 = (-1.0 + dpfx * (f32)x - square_x) * d1y * x_scale - (-1.0 + dpfy * (f32)y + square_y) * d1x * y_scale;
                f32 e2 = (-1.0 + dpfx * (f32)x - square_x) * d2y * x_scale - (-1.0 + dpfy * (f32)y + square_y) * d2x * y_scale;

                if(
                e1 <  0.125 &&
                e2 <  0.125 &&
                e1 > -0.125 &&
                e2 > -0.125
                )
                {
                    data_picturae[i*3 + 0] = red;
                    data_picturae[i*3 + 1] = green;
                    data_picturae[i*3 + 2] = blue;
                }
                pfx += dpfx;
            }
        pfx = -1.0;
        pfy += dpfy;
        }
        {
            if(!aso_semaphorum_medium_suscita(commit_semaphore_handle, 1, 0))
            { AOS_H_printf("My frames are not being read...\n"); }
        }
//        AOS_surface_commit(surfaces[0]);
        f64 frame_end = AOS_H_time_get_seconds();
        //AOS_H_printf("elf time : %10.10lf ms\n", (frame_end - frame_start) * 1000.0);
    }
}
}
