#ifndef _AOS_HELPER
#define _AOS_HELPER

#include "../common/types.h"
#include "aos_syscalls.h"

#define STB_SPRINTF_DECORATE(name) AOS_H_##name
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

char* _AOS_H_printf_flush(const char* buf, void* user, int len)
{
    int to_flush_count = len;
    char* temp_buf = buf;
    while(to_flush_count)
    {
        int put_count = AOS_stream_put(AOS_STREAM_STDOUT, temp_buf, to_flush_count);
        to_flush_count -= put_count;
        temp_buf += put_count;
    }
    return buf;
}

s32 AOS_H_printf(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    char buffer[STB_SPRINTF_MIN];
    const int ret = AOS_H_vsprintfcb(_AOS_H_printf_flush, 0, buffer, format, va);
    va_end(va);
    return ret;
}

f64 AOS_H_time_get_seconds()
{
    return (f64)AOS_get_cpu_time() / (f64)AOS_get_cpu_timer_frequency();
}

#endif // _AOS_HELPER
