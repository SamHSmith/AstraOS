#include "../userland/aos_syscalls.h"
#include "../userland/aos_helper.h"

void* memcpy(void *dst0, const void *src0, u64 length)
{
    char *dst = dst0;
    const char *src = src0;
    u64 t;

    if (length == 0 || dst == src)        /* nothing to do */
        goto done;

    /*
     * Macros: loop-t-times; and loop-t-times, t>0
     */
#define    TLOOP(s) if (t) TLOOP1(s)
#define    TLOOP1(s) do { s; } while (--t)
#define wmask (u64)(~0b111)

    if ((unsigned long)dst < (unsigned long)src) {
        /*
         * Copy forward.
         */
        t = (u64)src;    /* only need low bits */
        if ((t | (u64)dst) & wmask) {
            /*
             * Try to align operands.  This cannot be done
             * unless the low bits match.
             */
            if ((t ^ (u64)dst) & wmask || length < 8)
                t = length;
            else
                t =  - (t & wmask);
            length -= t;
            TLOOP1(*dst++ = *src++);
        }
        /*
         * Copy whole words, then mop up any trailing bytes.
         */
        t = length / 8;
        TLOOP(*(u64 *)dst = *(u64 *)src; src += 8; dst += 8);
        t = length & wmask;
        TLOOP(*dst++ = *src++);
    } else {
        /*
         * Copy backwards.  Otherwise essentially the same.
         * Alignment works as before, except that it takes
         * (t&wmask) bytes to align, not wsize-(t&wmask).
         */
        src += length;
        dst += length;
        t = (u64)src;
        if ((t | (u64)dst) & wmask) {
            if ((t ^ (u64)dst) & wmask || length <= 8)
                t = length;
            else
                t &= wmask;
            length -= t;
            TLOOP1(*--dst = *--src);
        }
        t = length / 8;
        TLOOP(src -= 8; dst -= 8; *(u64 *)dst = *(u64 *)src);
        t = length & wmask;
        TLOOP(*--dst = *--src);
    }
done:
    return (dst0);
}

void* memset(void * dest, int c, u64 n)
{
    unsigned char *s = dest;
    u64 k;

    /* Fill head and tail with minimal branching. Each
     * conditional ensures that all the subsequently used
     * offsets are well-defined and in the dest region. */

    if (!n) return dest;
    s[0] = s[n-1] = c;
    if (n <= 2) return dest;
    s[1] = s[n-2] = c;
    s[2] = s[n-3] = c;
    if (n <= 6) return dest;
    s[3] = s[n-4] = c;
    if (n <= 8) return dest;

    /* Advance pointer to align it at a 4-byte boundary,
     * and truncate n to a multiple of 4. The previous code
     * already took care of any head/tail that get cut off
     * by the alignment. */

    k = -(u64)s & 3;
    s += k;
    n -= k;
    n &= -4;
    n /= 4;

    u32 *ws = (u32 *)s;
    u32 wc = c & 0xFF;
    wc |= ((wc << 8) | (wc << 16) | (wc << 24));

    /* Pure C fallback with no aliasing violations. */
    for (; n; n--, ws++) *ws = wc;

    return dest;
}

u64 strlen(char* str)
{
    u64 i = 0;
    while(str[i] != 0) { i++; }
    return i;
}

u64 strnlen_s(char* str, u64 max_len)
{
    u64 i = 0;
    while(str[i] != 0 && i < max_len) { i++; }
    return i;
}

char* strcpy(char* dest, char* src)
{
    memcpy(dest, src, strlen(src));
    return dest;
}

char* strncpy(char* dest, char* src, u64 max_len)
{
    memcpy(dest, src, strnlen_s(src, max_len));
    return dest;
}

int strcmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp( const char * s1, const char * s2, u64 n )
{
    while ( n && *s1 && ( *s1 == *s2 ) )
    {
        ++s1;
        ++s2;
        --n;
    }
    if ( n == 0 )
    {
        return 0;
    }
    else
    {
        return ( *(unsigned char *)s1 - *(unsigned char *)s2 );
    }
}

void strcat(char* dest, char* src)
{
    char* d = dest;
    while(*d) { d++; }
    char* s = src;
    do {
        *d = *s;
        d++;
        s++;
    } while(*s);
}


u8 name_buffer[64];

void print_directory(u64 dir_id, u8* prefix, u64 levels_left)
{
    AOS_directory_get_name(dir_id, name_buffer, 64);
    AOS_H_printf("%s%s:\n", prefix, name_buffer);

    {
        u64 file_count = AOS_directory_get_files(dir_id, 0, 0);
        u64 files[file_count];
        file_count = AOS_directory_get_files(dir_id, files, file_count);
        for(u64 i = 0; i < file_count; i++)
        {
            AOS_file_get_name(files[i], name_buffer, 64);
            AOS_H_printf("%s |  %s\n", prefix, name_buffer);
        }
    }

    if(levels_left)
    {
        u64 dir_count = AOS_directory_get_subdirectories(dir_id, 0, 0);
        u64 dirs[dir_count];
        dir_count = AOS_directory_get_subdirectories(dir_id, dirs, dir_count);

        u8 new_prefix[strlen(prefix) + strlen(" \\- ") + 1];
        memset(new_prefix, 0, strlen(prefix) + strlen(" \\- ") + 1);
        strcat(new_prefix, prefix);
        strcat(new_prefix, " \\- ");
        for(u64 i = 0; i < dir_count; i++)
        {
            print_directory(dirs[i], new_prefix, levels_left - 1);
        }
    }
    else
    {
        AOS_H_printf("%s \\-| MAX DEPTH REACHED\n", prefix);
    }
}

void _start(u64 root_dir_id)
{
    AOS_H_printf("Starting tree on root directory.\n");

    print_directory(root_dir_id, "", 8);

    AOS_process_exit();
}
