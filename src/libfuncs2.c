// for cyclone

//Fill block of memory
#define osMemset(p, value, length) (void) memset(p, value, length)
 
//Copy block of memory
#define osMemcpy(dest, src, length) (void) memcpy(dest, src, length)
 
//Move block of memory
#define osMemmove(dest, src, length) (void) memmove(dest, src, length)
 
//Compare two blocks of memory
#define osMemcmp(p1, p2, length) memcmp(p1, p2, length)
 
//Get string length
#define osStrlen(s) strlen(s)
 
//Compare strings
#define osStrcmp(s1, s2) strcmp(s1, s2)
 
//Compare substrings
#define osStrncmp(s1, s2, length) strncmp(s1, s2, length)
 
//Compare strings without case
#define osStrcasecmp(s1, s2) strcasecmp(s1, s2)
 
//Compare substrings without case
#define osStrncasecmp(s1, s2, length) strncasecmp(s1, s2, length)
 
//Search for the first occurrence of a given character
#define osStrchr(s, c) strchr(s, c)
 
//Search for the first occurrence of a substring
#define osStrstr(s1, s2) strstr(s1, s2)

//Copy string
#define osStrcpy(s1, s2) (void) strcpy(s1, s2)
 
//Copy characters from string
#define osStrncpy(s1, s2, length) (void) strncpy(s1, s2, length)
 
//Concatenate strings
#define osStrcat(s1, s2) (void) strcat(s1, s2)
 
//Extract tokens from string
#define osStrtok_r(s, delim, last) strtok_r(s, delim, last)
 
//Format string
#define osSprintf(dest, ...) stbsp_sprintf(dest, __VA_ARGS__)
 
//Format string
#define osVsnprintf(dest, size, format, ap) vsnprintf(dest, size, format, ap)
 
//Convert string to unsigned long integer
#define osStrtoul(s, endptr, base) strtoul(s, endptr, base)
 
//Convert string to unsigned long long integer
#define osStrtoull(s, endptr, base) strtoull(s, endptr, base)
 
//Convert a character to lowercase
#define osTolower(c) tolower((uint8_t) (c))
 
//Convert a character to uppercase
#define osToupper(c) toupper((uint8_t) (c))
 
//Check if a character is an uppercase letter
#define osIsupper(c) isupper((c))

//Check if a character is a decimal digit
#define osIsdigit(c) isdigit((uint8_t) (c))
 
//Check if a character is a whitespace character
#define osIsspace(c) isspace((uint8_t) (c))

//Check if a character is a blank character
#ifndef osIsblank
   #define osIsblank(c) ((c) == ' ' || (c) == '\t')
#endif


Kallocation libfuncs_allocation_array_alloc = {0};
#define libfuncs_allocation_array ((Kallocation*)libfuncs_allocation_array_alloc.memory)
u64 libfuncs_allocation_array_count = 0;
 
void* osAllocMem(u64 size)
{
    if(size == 0) { return 0; }
    Kallocation* k = 0;
    for(u64 i = 0; i < libfuncs_allocation_array_count; i++)
    {
        if(libfuncs_allocation_array[i].memory = 0)
        { k = libfuncs_allocation_array + i; break; }
    }
 
    if(k == 0)
    {
        if((libfuncs_allocation_array_count + 1) *sizeof(Kallocation) <= libfuncs_allocation_array_alloc.page_count *PAGE_SIZE)
        {
            k = libfuncs_allocation_array + libfuncs_allocation_array_count;
            libfuncs_allocation_array_count++;
        }
        else
        {
            Kallocation new_alloc = kalloc_pages(libfuncs_allocation_array_alloc.page_count + 1);
            Kallocation* arr = (Kallocation*) new_alloc.memory;
            for(u64 i = 0; i < libfuncs_allocation_array_count; i++)
            {
                arr[i] = libfuncs_allocation_array[i];
            }
            if(libfuncs_allocation_array_alloc.page_count != 0)
            {
                kfree_pages(libfuncs_allocation_array_alloc);
            }
            libfuncs_allocation_array_alloc = new_alloc;

            k = libfuncs_allocation_array + libfuncs_allocation_array_count;
            libfuncs_allocation_array_count++;
        }
    }

    u64 page_count = size / PAGE_SIZE;
    page_count += (size % PAGE_SIZE) != 0;
    *k = kalloc_pages(page_count);
    return k->memory;
}

void osFreeMem(void* ptr)
{
    for(u64 i = 0; i < libfuncs_allocation_array_count; i++)
    {
        if(libfuncs_allocation_array[i].memory == ptr)
        {
            kfree_pages(libfuncs_allocation_array[i]);
            libfuncs_allocation_array[i].memory = 0;
            return;
        }
    }
    return;
}

