
#include "../../common/types.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <openssl/sha.h>

typedef struct
{
    u8 uid[64];
    u64 start_block;
    u16 partition_type;
    u8 name[54];
} RAD_PartitionTableEntry;

typedef struct
{
    u8 sha512sum[64];
    u8 padding[64];
    RAD_PartitionTableEntry entries[63];
} RAD_PartitionTable;

#define TABLE_COUNT 8

void main(int argc, char** argv)
{
    if(argc < 3) { printf("usage: fsmake drive_file source_directory\n"); return; }

    if(argv[2][strlen(argv[2]) - 1] != '/') { printf("source_directory must end with /\n"); return; }

    int drive_fd = open(argv[1], O_EXCL | O_CREAT | O_RDWR, S_IRUSR|S_IWUSR | S_IRGRP|S_IWGRP);
    if(drive_fd < 0) { printf("%s already exists or there was an error opening the file\n", argv[1]); return; }

    u8 block[4096];
    for(u64 i = 0; i < 2048*32; i++)
    {
        write(drive_fd, block, 4096);
    }
    u64 drive_block_count = lseek(drive_fd, 0, SEEK_END);
    assert(drive_block_count % 4096 == 0);
    drive_block_count /= 4096;

    assert(sizeof(RAD_PartitionTable) == 4096*2);

    RAD_PartitionTable* table = calloc(1, sizeof(RAD_PartitionTable));
    assert(table != 0);

    SHA512_CTX sha_ctx;

    // fill out table

    char* layout_path = malloc(strlen(argv[2]) + strlen("layout") + 1);
    strcpy(layout_path, argv[2]);
    strcat(layout_path, "layout");
    int layout_fd = open(layout_path, O_RDONLY);

    u8 name_buf[64];
    u8 num_buf[32];
    u8 type_buf[32];
    u8 reading_layout = 1;
    s64 partition_index = -1;
    u64 current_block = (sizeof(RAD_PartitionTable) / 4096)*TABLE_COUNT;
    while(reading_layout && partition_index < 63)
    {
        partition_index += 1;
        u64 name_index = 0;
        while(name_index < 128)
        {
            char c = 0;
            ssize_t result = read(layout_fd, &c, 1);
            if(result != 1) { reading_layout = 0; }
            if(result != 1 || c == ' ') { break; }
            name_buf[name_index] = c;
            name_index += 1;
        }
        name_buf[name_index] = 0;
        u64 num_index = 0;
        while(num_index < 32)
        {
            char c = 0;
            ssize_t result = read(layout_fd, &c, 1);
            if(result != 1) { reading_layout = 0; }
            if(result != 1 || c == ' ') { break; }
            num_buf[num_index] = c;
            num_index += 1;
        }
        num_buf[num_index] = 0;
        u64 type_index = 0;
        while(type_index < 32)
        {
            char c = 0;
            ssize_t result = read(layout_fd, &c, 1);
            if(result != 1) { reading_layout = 0; }
            if(result != 1 || c == '\n') { break; }
            type_buf[type_index] = c;
            type_index += 1;
        }
        type_buf[type_index] = 0;
        if(name_index == 0 || num_index == 0 || type_index == 0) { continue; }

        if(name_index >= 54) { continue; }

        u64 size = strtoull(num_buf, 0, 10);
        u16 type = (u16)strtoul(type_buf, 0, 10);
        printf("creating partition, name: %s, size: %llu, type: %u\n", name_buf, size, type);

        strcpy(table->entries[partition_index].name, name_buf);
        table->entries[partition_index].partition_type = type;
        table->entries[partition_index].start_block = current_block;
        current_block += size;
    }
    if(partition_index < 63)
    { table->entries[partition_index].start_block = current_block; }


    assert(SHA512_Init(&sha_ctx));
    assert(SHA512_Update(&sha_ctx, ((u8*)table) + 64, sizeof(RAD_PartitionTable) - 64));
    assert(SHA512_Final((u8*)table, &sha_ctx));

    // write table
    lseek(drive_fd, 0, SEEK_SET);
    for(u64 i = 0; i < TABLE_COUNT; i++)
    {
        write(drive_fd, table, sizeof(*table));
    }

    // write partition contents
    for(u64 i = 0; i < 63; i++)
    {
        if(table->entries[i].partition_type == 0) { continue; }
        char* partition_filepath = malloc(strlen(argv[2]) + strlen("partitions/") + strlen(table->entries[i].name) + 1);
        strcpy(partition_filepath, argv[2]);
        strcat(partition_filepath, "partitions/");
        mkdir(partition_filepath, ~0);
        strcat(partition_filepath, table->entries[i].name);

        u64 next_partition_start = drive_block_count;
        if(i < 62) { next_partition_start = table->entries[i+1].start_block; }
        u64 partition_block_count = next_partition_start - table->entries[i].start_block;

        int partition_fd = open(partition_filepath, O_RDWR);
        if(partition_fd >0 && lseek(partition_fd, 0, SEEK_END) != partition_block_count * 4096)
        { ftruncate(partition_fd, 4096 * partition_block_count); }

        if(partition_fd < 0)
        {
            partition_fd = open(partition_filepath, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR | S_IRGRP|S_IWGRP);
            if(partition_fd < 0) { printf("failed in handling partition: %s\n", table->entries[i].name); continue; }

            for(u64 i = 0; i < partition_block_count; i++)
            { write(partition_fd, block, 4096); }
        }
        assert(lseek(partition_fd, 0, SEEK_END) % 4096 == 0);
        lseek(partition_fd, 0, SEEK_SET);

        lseek(drive_fd, table->entries[i].start_block * 4096, SEEK_SET);
        for(u64 i = 0; i < partition_block_count; i++)
        {
            read(partition_fd, block, 4096);
            write(drive_fd, block, 4096);
        }
        close(partition_fd);
    }

    close(drive_fd);
}
