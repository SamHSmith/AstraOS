
typedef struct
{
    u8 name[56];
    u64 subdirs[8];
    u64 subdir_count;
} KernelDirectoryImaginary;
#define KERNEL_DIRECTORY_TYPE_IMAGINARY 0

typedef struct
{
    u64 type;
    u64 reference_count; // zero is uninitialized
    union {
        KernelDirectoryImaginary imaginary;
    };
} KernelDirectory;

Kallocation KERNEL_DIRECTORY_ARRAY_ALLOC = {0};
#define KERNEL_DIRECTORY_ARRAY ((KernelDirectory*)KERNEL_DIRECTORY_ARRAY_ALLOC.memory)
u64 KERNEL_DIRECTORY_ARRAY_LEN = 0;

u64 kernel_directory_create()
{
    u64 dir_id = 0;
    u8 found = 0;
    for(u64 i = 0; i < KERNEL_DIRECTORY_ARRAY_LEN; i++)
    {
        if(KERNEL_DIRECTORY_ARRAY[i].reference_count == 0)
        {
            dir_id = i;
            found = 1;
            break;
        }
    }
    if(!found)
    {
        if((KERNEL_DIRECTORY_ARRAY_LEN+1) * sizeof(KernelDirectory) > KERNEL_DIRECTORY_ARRAY_ALLOC.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc =
                kalloc_pages(((KERNEL_DIRECTORY_ARRAY_LEN+1) * sizeof(KernelDirectory) + PAGE_SIZE) / PAGE_SIZE);
            KernelDirectory* new_array = new_alloc.memory;
            for(u64 i = 0; i < KERNEL_DIRECTORY_ARRAY_LEN; i++)
            {
                new_array[i] = KERNEL_DIRECTORY_ARRAY[i];
            }
            if(KERNEL_DIRECTORY_ARRAY_ALLOC.page_count != 0)
            {
                kfree_pages(KERNEL_DIRECTORY_ARRAY_ALLOC);
            }
            KERNEL_DIRECTORY_ARRAY_ALLOC = new_alloc;
        }
        dir_id = KERNEL_DIRECTORY_ARRAY_LEN;
        KERNEL_DIRECTORY_ARRAY_LEN += 1;
    }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    dir->reference_count = 1;
    return dir_id;
}

u64 kernel_directory_create_imaginary(char* name)
{
    u64 dir_id = kernel_directory_create();
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    dir->type = KERNEL_DIRECTORY_TYPE_IMAGINARY;
    KernelDirectoryImaginary* imaginary = &dir->imaginary;
    strncpy(imaginary->name, name, 56);
    return dir_id;
}

/* where there is variable data to be returned we fill submitted buffer as much as possible and return real size*/
u64 kernel_directory_get_name(u64 dir_id, u8* buf, u64 buf_size)
{
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;
        u64 name_len = strnlen_s(imaginary->name, 56);

        u64 cpy_len = name_len; if(cpy_len > buf_size) { cpy_len = buf_size; }
        strncpy(buf, imaginary->name, cpy_len);
        if(buf_size > 56) { buf[56] = 0; }
        else if(buf_size > 0) { buf[buf_size-1] = 0; }
        return name_len;
    }
    else
    {
        printf("kernel_directory_get_name: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}
