
#include <stdarg.h>

char* _uart_printf_flush(const char* buf, void* user, int len)
{
    uart_write(buf, len);
    return buf;
}

s32 uart_printf(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    char buffer[STB_SPRINTF_MIN];
    const int ret = stbsp_vsprintfcb(_uart_printf_flush, 0, buffer, format, va);
    va_end(va);
    return ret;
}
