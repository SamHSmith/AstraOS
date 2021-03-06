
typedef struct
{
    Kallocation page_array_alloc;
    u64 page_array_len;
    u64 file_size;
    u8 name[64];
    u8 _padding[32];
} KernelFileImaginary;
#define KERNEL_FILE_TYPE_IMAGINARY 0

typedef struct
{
    u8 name[54];
    u64 start_block;
    u64 block_count;
} KernelFileDrivePartition;
#define KERNEL_FILE_TYPE_DRIVE_PARTITION 1

u8 has_loaded_drive1_partitions = 0;
u64 drive1_partition_directory = 0;

typedef struct
{
    u64 type;
    u64 reference_count; // zero is uninitialized
    union {
        KernelFileImaginary imaginary;
        KernelFileDrivePartition drive_partition;
    }
} KernelFile;

Kallocation KERNEL_FILE_ARRAY_ALLOC = {0};
#define KERNEL_FILE_ARRAY ((KernelFile*)KERNEL_FILE_ARRAY_ALLOC.memory)
u64 KERNEL_FILE_ARRAY_LEN = 0;

u64 kernel_file_create()
{
    u64 file_id = 0;
    u8 found = 0;
    for(u64 i = 0; i < KERNEL_FILE_ARRAY_LEN; i++)
    {
        if(KERNEL_FILE_ARRAY[i].reference_count == 0)
        {
            file_id = i;
            found = 1;
            break;
        }
    }
    if(!found)
    {
        if((KERNEL_FILE_ARRAY_LEN+1) * sizeof(KernelFile) > KERNEL_FILE_ARRAY_ALLOC.page_count * PAGE_SIZE)
        {
            Kallocation new_alloc =
                kalloc_pages(((KERNEL_FILE_ARRAY_LEN+1) * sizeof(KernelFile) + PAGE_SIZE) / PAGE_SIZE);
            KernelFile* new_array = new_alloc.memory;
            for(u64 i = 0; i < KERNEL_FILE_ARRAY_LEN; i++)
            {
                new_array[i] = KERNEL_FILE_ARRAY[i];
            }
            if(KERNEL_FILE_ARRAY_ALLOC.page_count != 0)
            {
                kfree_pages(KERNEL_FILE_ARRAY_ALLOC);
            }
            KERNEL_FILE_ARRAY_ALLOC = new_alloc;
        }
        file_id = KERNEL_FILE_ARRAY_LEN;
        KERNEL_FILE_ARRAY_LEN += 1;
    }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    memset(file, 0, sizeof(*file));
    file->reference_count = 1;
    return file_id;
}

u64 kernel_file_create_imaginary(char* name)
{
    u64 file_id = kernel_file_create();
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    file->type = KERNEL_FILE_TYPE_IMAGINARY;
    KernelFileImaginary* imaginary = &file->imaginary;
    memset(imaginary, 0, sizeof(*imaginary));
    strncpy(imaginary->name, name, 64);
    return file_id;
}

u64 is_valid_file_id(u64 file_id)
{
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file_id >= KERNEL_FILE_ARRAY_LEN || file->reference_count == 0)
    {
        return 0;
    }
    return 1;
}

u64 kernel_file_get_name(u64 file_id, u8* buf, u64 buf_size)
{
    if(!is_valid_file_id(file_id) || !buf_size) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        u64 name_len = strnlen_s(imaginary->name, 64);

        u64 cpy_len = name_len; if(cpy_len > buf_size) { cpy_len = buf_size; }
        strncpy(buf, imaginary->name, cpy_len);
        if(buf_size > 64) { buf[64] = 0; }
        else if(buf_size > 0) { buf[buf_size-1] = 0; }
        return name_len + 1;
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    {
        KernelFileDrivePartition* part = &file->drive_partition;
        u64 name_len = strnlen_s(part->name, 54);
 
        u64 cpy_len = name_len; if(cpy_len + 1 > buf_size) { cpy_len = buf_size - 1; }
        strncpy(buf, part->name, cpy_len);
        if(buf_size > cpy_len) { buf[cpy_len] = 0; }
        else { buf[buf_size-1] = 0; }
        return name_len + 1;
    }
    else
    {
        printf("kernel_file_get_name: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

u64 kernel_file_get_size(u64 file_id)
{
    if(!is_valid_file_id(file_id)) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        return imaginary->file_size;
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    {
        KernelFileDrivePartition* part = &file->drive_partition;
        return part->block_count * PAGE_SIZE;
    }
    else
    {
        printf("kernel_file_get_size: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

u64 kernel_file_get_block_count(u64 file_id)
{
    if(!is_valid_file_id(file_id)) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        return imaginary->page_array_len;
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    {
        KernelFileDrivePartition* part = &file->drive_partition;
        return part->block_count;
    }
    else
    {
        printf("kernel_file_get_block_count: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

// returns true if the operation was successful.
u64 kernel_file_set_size(u64 file_id, u64 new_size)
{
    if(!is_valid_file_id(file_id)) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    u64 new_block_count = (new_size + PAGE_SIZE) / PAGE_SIZE;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;

        if(new_block_count < imaginary->page_array_len)
        {
            void** page_array = imaginary->page_array_alloc.memory;
            for(u64 i = new_block_count; i < imaginary->page_array_len; i++)
            {
                kfree_single_page(page_array[i]);
            }
            imaginary->page_array_len = new_block_count;

            u64 needed_pages = (imaginary->page_array_len * sizeof(void*) + PAGE_SIZE) / PAGE_SIZE;
            if(needed_pages < imaginary->page_array_alloc.page_count)
            {
                Kallocation to_free = {0};
                to_free.page_count = imaginary->page_array_alloc.page_count - needed_pages;
                to_free.memory = imaginary->page_array_alloc.memory + needed_pages * PAGE_SIZE;
                kfree_pages(to_free);
                imaginary->page_array_alloc.page_count = needed_pages;
            }
            imaginary->file_size = new_size;
            return 1;
        }
        else
        {
            void** page_array = imaginary->page_array_alloc.memory;
            u64 needed_pages = (new_block_count * sizeof(void*) + PAGE_SIZE) / PAGE_SIZE;
            if(needed_pages > imaginary->page_array_alloc.page_count)
            {
                Kallocation new_alloc = kalloc_pages(needed_pages);
                void** new_array = new_alloc.memory;
                for(u64 i = 0; i < imaginary->page_array_len; i++)
                {
                    new_array[i] = page_array[i];
                }
                if(imaginary->page_array_alloc.page_count != 0) { kfree_pages(imaginary->page_array_alloc); }
                imaginary->page_array_alloc = new_alloc;
                page_array = new_array;
            }
            for(u64 i = imaginary->page_array_len; i < new_block_count; i++)
            { page_array[i] = kalloc_single_page(); }
            imaginary->page_array_len = new_block_count;
            imaginary->file_size = new_size;
            return 1;
        }
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    { return 0; }
    else
    {
        printf("kernel_file_set_size: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

// op_array is filled with (block_num, memory_ptr) pairs.
// op_count is the amount of pairs so for a op_count of N, op_array has N*2 elements
// the function will use op_array as a scratch buffer and potentially mess it up
// return true on success
u64 kernel_file_read_blocks(u64 file_id, u64* op_array, u64 op_count)
{
    if(!is_valid_file_id(file_id)) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        void** page_array = imaginary->page_array_alloc.memory;
        for(u64 i = 0; i < op_count*2; i+=2)
        {
            u64 block_num = op_array[i];
            void* destination = op_array[i+1];
            if(block_num >= imaginary->page_array_len)
            { continue; }
            memcpy(destination, page_array[block_num], PAGE_SIZE);
        }
        return 1;
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    {
        KernelFileDrivePartition* part = &file->drive_partition;
        u64 write_index = 0;
        for(u64 i = 0; i < op_count; i++)
        {
            u64 block_num = op_array[i*2];
            if(block_num >= part->block_count)
            { continue; }
            block_num += part->start_block;
            u64 addr = op_array[i*2 +1];
            op_array[write_index*2] = block_num;
            op_array[write_index*2 + 1] = addr;
            write_index++;
        }
        if(write_index > 0)
        {
            u64 send_times = (write_index + 10 - 1) / 10; // change 10 to something exact later
            for(u64 i = 0; i < send_times - 1; i++)
            {
                oak_send_block_fetch(0, op_array, 10);
                op_array += 10*2;
                write_index -= 10;
            }
            if(write_index > 0) { oak_send_block_fetch(0, op_array, write_index); }
            return 1;
        }
        return 0;
    }
    else
    {
        printf("kernel_file_read_blocks: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

u64 kernel_file_write_blocks(u64 file_id, u64* op_array, u64 op_count)
{
    if(!is_valid_file_id(file_id)) { return 0; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        void** page_array = imaginary->page_array_alloc.memory;
        for(u64 i = 0; i < op_count*2; i+=2)
        {
            u64 block_num = op_array[i];
            void* src = op_array[i+1];
            if(block_num >= imaginary->page_array_len)
            { continue; }
            memcpy(page_array[block_num], src, PAGE_SIZE);
        }
        return 1;
    }
    else if(file->type == KERNEL_FILE_TYPE_DRIVE_PARTITION)
    {
        KernelFileDrivePartition* part = &file->drive_partition;
        u64 write_index = 0;
        for(u64 i = 0; i < op_count; i++)
        {
            u64 block_num = op_array[i*2];
            if(block_num >= part->block_count)
            { continue; }
            block_num += part->start_block;
            u64 addr = op_array[i*2 +1];
            op_array[write_index*2] = block_num;
            op_array[write_index*2 + 1] = addr;
            write_index++;
        }
        if(write_index > 0)
        {
            u64 send_times = (write_index + 10 - 1) / 10; // change 10 to something exact later
            for(u64 i = 0; i < send_times - 1; i++)
            {
                oak_send_block_fetch(1, op_array, 10);
                op_array += 10*2;
                write_index -= 10;
            }
            if(write_index > 0) { oak_send_block_fetch(1, op_array, write_index); }
            return 1;
        }
        return 0;
    }
    else
    {
        printf("kernel_file_write_blocks: Unknown file type: %llu\n", file->type);
        return 0;
    }
}

void kernel_file_increment_reference_count(u64 file_id)
{
    if(!is_valid_file_id(file_id)) { return; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;
    file->reference_count++;
}

void kernel_file_free(u64 file_id)
{
    if(!is_valid_file_id(file_id)) { return; }
    KernelFile* file = KERNEL_FILE_ARRAY + file_id;

    if(file->reference_count > 1) { file->reference_count--; return; }

    if(file->type == KERNEL_FILE_TYPE_IMAGINARY)
    {
        KernelFileImaginary* imaginary = &file->imaginary;
        void** page_array = imaginary->page_array_alloc.memory;
        for(u64 i = 0; i < imaginary->page_array_len; i++)
        {
            kfree_single_page(page_array[i]);
        }
        kfree_pages(imaginary->page_array_alloc);
        file->reference_count = 0;
    }
    else
    {
        printf("kernel_file_free: Unknown file type: %llu\n", file->type);
    }
}

///////////////////////// START DIRECTORY

typedef struct
{
    u8 name[56];
    u64 subdirs[8];
    u64 subdir_count;
    u64 files[8];
    u64 file_count;
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
    imaginary->subdir_count = 0;
    imaginary->file_count = 0;
    strncpy(imaginary->name, name, 56);
    return dir_id;
}

u64 is_valid_dir_id(u64 dir_id)
{
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir_id >= KERNEL_DIRECTORY_ARRAY_LEN || dir->reference_count == 0)
    {
        return 0;
    }
    return 1;
}

// where there is variable data to be returned we fill submitted buffer as much as possible and return real size
u64 kernel_directory_get_name(u64 dir_id, u8* buf, u64 buf_size)
{
    if(!is_valid_dir_id(dir_id)) { return 0; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;
        u64 name_len = strnlen_s(imaginary->name, 56);

        u64 cpy_len = name_len; if(cpy_len > buf_size) { cpy_len = buf_size; }
        strncpy(buf, imaginary->name, cpy_len);
        if(buf_size > 56) { buf[56] = 0; }
        else if(buf_size > 0) { buf[buf_size-1] = 0; }
        return name_len + 1;
    }
    else
    {
        printf("kernel_directory_get_name: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}

u64 kernel_directory_get_subdirectories(u64 dir_id, u64* buf, u64 buf_size)
{
    if(!is_valid_dir_id(dir_id)) { return 0; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;

        if(buf_size > imaginary->subdir_count) { buf_size = imaginary->subdir_count; }
        for(u64 i = 0; i < buf_size; i++)
        { buf[i] = imaginary->subdirs[i]; }

        return imaginary->subdir_count;
    }
    else
    {
        printf("kernel_directory_get_subdirectories: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}

u64 kernel_directory_get_files(u64 dir_id, u64* buf, u64 buf_size)
{
    if(!is_valid_dir_id(dir_id)) { return 0; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;

        if(buf_size > imaginary->file_count) { buf_size = imaginary->file_count; }
        for(u64 i = 0; i < buf_size; i++)
        { buf[i] = imaginary->files[i]; }

        return imaginary->file_count;
    }
    else
    {
        printf("kernel_directory_get_subdirectories: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}

// returns true if the operation was successful.
// You might think this function should increment the reference count of subdirectory
// that is wrong think. The caller could be passing ownership to us or keeping a reference for itself.
// Since it is ambiguous from our side it is *YOUR* responsibility to increment the subdirectory's
// reference counter if you intend to hold on to a local copy of subdirectory.
u64 kernel_directory_add_subdirectory(u64 dir_id, u64 subdirectory)
{
    if(!is_valid_dir_id(dir_id) || !is_valid_dir_id(subdirectory)) { return 0; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;
 
        if(imaginary->subdir_count >= 8) { return 0; }
        for(u64 i = 0; i < imaginary->subdir_count; i++)
        { if(imaginary->subdirs[i] == subdirectory) { return 0; } }
        imaginary->subdirs[imaginary->subdir_count] = subdirectory;
        imaginary->subdir_count += 1;
 
        return 1;
    }
    else
    {
        printf("kernel_directory_add_subdirectory: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}

u64 kernel_directory_add_file(u64 dir_id, u64 file_id)
{
    if(!is_valid_dir_id(dir_id) || !is_valid_file_id(file_id)) { return 0; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        KernelDirectoryImaginary* imaginary = &dir->imaginary;

        if(imaginary->file_count >= 8) { return 0; }
        for(u64 i = 0; i < imaginary->file_count; i++)
        { if(imaginary->files[i] == file_id) { return 0; } }
        imaginary->files[imaginary->file_count] = file_id;
        imaginary->file_count += 1;

        return 1;
    }
    else
    {
        printf("kernel_directory_add_file: Unknown directory type: %llu\n", dir->type);
        return 0;
    }
}

// Silently fails if dir_id is not valid
void kernel_directory_increment_reference_count(u64 dir_id)
{
    if(!is_valid_dir_id(dir_id)) { return; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;
    dir->reference_count++;
}

void kernel_directory_free(u64 dir_id)
{
    if(!is_valid_dir_id(dir_id)) { return; }
    KernelDirectory* dir = KERNEL_DIRECTORY_ARRAY + dir_id;

    if(dir->reference_count > 1) { dir->reference_count--; return; }

    u64 file_count = kernel_directory_get_files(dir_id, 0, 0);
    u64 files[file_count];
    kernel_directory_get_files(dir_id, files, file_count);
    for(u64 i = 0; i < file_count; i++)
    {
        kernel_file_free(files[i]);
    }

    u64 sub_dir_count = kernel_directory_get_subdirectories(dir_id, 0, 0);
    u64 sub_dirs[sub_dir_count];
    kernel_directory_get_subdirectories(dir_id, sub_dirs, sub_dir_count);
    for(u64 i = 0; i < sub_dir_count; i++)
    {
        kernel_directory_free(sub_dirs[i]);
    }

    if(dir->type == KERNEL_DIRECTORY_TYPE_IMAGINARY)
    {
        dir->reference_count = 0;
    }
    else
    {
        printf("kernel_directory_free: Unknown directory type: %llu\n", dir->type);
    }
}



void debug_print_directory_tree(u64 dir_id, char* prefix)
{
    if(!is_valid_dir_id(dir_id)) { printf("%s>] NOT VALID DIR\n", prefix); return; }

    u64 sub_prefix_len = strlen(prefix) + strlen("--") + 1;
    char sub_prefix[sub_prefix_len];
    for(u64 i = 0; i < sub_prefix_len; i++) { sub_prefix[i] = 0; }
    strcat(sub_prefix, prefix);
    strcat(sub_prefix, "--");

    u64 dir_name_len = kernel_directory_get_name(dir_id, 0, 0);
    char dir_name[dir_name_len];
    kernel_directory_get_name(dir_id, dir_name, dir_name_len);
    printf("%s> %s\n", prefix, dir_name);

    u64 file_count = kernel_directory_get_files(dir_id, 0, 0);
    u64 files[file_count];
    kernel_directory_get_files(dir_id, files, file_count);
    u64 longest_name = 0;
    for(u64 i = 0; i < file_count; i++)
    {
        u64 filename_len = kernel_file_get_name(files[i], 0, 0);
        if(filename_len > longest_name) { longest_name = filename_len; }
    }
    if(longest_name > 512) { longest_name = 512; } // in case there's a bad actor
    char filename[longest_name];
    char pad[longest_name];
    for(u64 i = 0; i < file_count; i++)
    {
        u64 filename_len = kernel_file_get_name(files[i], 0, 0);
        if(filename_len > longest_name) { filename_len = longest_name; }
        kernel_file_get_name(files[i], filename, filename_len);

        u64 pad_len = longest_name - filename_len;
        for(u64 i = 0; i < pad_len; i++) { pad[i] = ' '; }
        pad[pad_len] = 0;
        if(is_valid_file_id(files[i]))
        {
            printf("%s|+ file: \"%s\",%s block_count: %llu, file_size: %llu B\n",
                sub_prefix,
                filename,
                pad,
                kernel_file_get_block_count(files[i]),
                kernel_file_get_size(files[i])
            );
        }
        else
        {
            printf("%s!+ invalid file!\n", sub_prefix);
        }
    }

    u64 sub_dir_count = kernel_directory_get_subdirectories(dir_id, 0, 0);
    u64 sub_dirs[sub_dir_count];
    kernel_directory_get_subdirectories(dir_id, sub_dirs, sub_dir_count);
    for(u64 i = 0; i < sub_dir_count; i++)
    {
        debug_print_directory_tree(sub_dirs[i], sub_prefix);
    }
}

void load_drive_partitions()
{
    if(has_loaded_drive1_partitions)
    { printf("drive1 partitions are already loaded!\n"); return; }

    assert(sizeof(RAD_PartitionTable) == PAGE_SIZE*2, "partition table struct is the right size");

    Kallocation table_alloc = kalloc_pages(2);
    RAD_PartitionTable* table = table_alloc.memory;
    assert(table != 0, "table alloc failed");

    u64 reference_table = U64_MAX;
    for(s64 i = TABLE_COUNT-1; i >= 0; i--)
    {
        read_blocks(i*2, 2, table);

        u8 hash[64];
        assert(sha512Compute(((u8*)table) + 64, sizeof(RAD_PartitionTable) -64, hash) == 0, "sha stuff");

        u8 is_valid = 1;
        for(u64 i = 0; i < 64; i++) { if(hash[i] != table->sha512sum[i]) { is_valid = 0; } }

        if(!is_valid)
        {
            printf("Table#%ld is not valid\n", i);
        }
        else
        {
            printf("Table#%ld is valid \n", i);
            reference_table = i;
        }
    }
    if(reference_table == U64_MAX)
    {
        printf("There are no valid tables. Either the drive is not formatted or you are in a very unfortunate situation.\n");
        return;
    }

    read_blocks(reference_table*2, 2, table);
    printf("Using table#%ld as reference\n", reference_table);

    drive1_partition_directory = kernel_directory_create_imaginary("drive1");
    has_loaded_drive1_partitions = 1;

    for(u64 i = 0; i < 63; i++)
    {
        u64 next_partition_start = U64_MAX; // drive_block_count; do this later
        if(i < 62) { next_partition_start = table->entries[i+1].start_block; }

        if(table->entries[i].partition_type != 0)
        {
            printf("Partition, type = %u, start = %llu, size = %llu, name = %s\n",
                    table->entries[i].partition_type,
                    table->entries[i].start_block,
                    next_partition_start - table->entries[i].start_block,
                    table->entries[i].name
            );
            RAD_PartitionTableEntry* entry = table->entries + i;

            u64 file_id = kernel_file_create();
            KernelFile* file = KERNEL_FILE_ARRAY + file_id;
            file->type = KERNEL_FILE_TYPE_DRIVE_PARTITION;
            KernelFileDrivePartition* part = &file->drive_partition;

            for(u64 i = 0; i < 54; i++)
            { part->name[i] = entry->name[i]; }
            part->start_block = entry->start_block;
            part->block_count = next_partition_start - entry->start_block;

            kernel_directory_add_file(drive1_partition_directory, file_id);
        }
    }
}

