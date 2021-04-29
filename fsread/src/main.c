
#include "../../src/types.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

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
    if(argc < 3) { printf("usage: fsmake drive_file output_directory\n"); return; }

    int drive_fd = open(argv[1], O_RDWR, S_IRUSR|S_IWUSR | S_IRGRP|S_IWGRP);
    if(drive_fd < 0) { printf("%s does not exists or there was an error opening the file\n"); return; }
    u64 drive_block_count = lseek(drive_fd, 0, SEEK_END);
    assert(drive_block_count % 4096 == 0);
    drive_block_count /= 4096;

    assert(sizeof(RAD_PartitionTable) == 4096*2);

    RAD_PartitionTable* table = calloc(1, sizeof(RAD_PartitionTable));
    assert(table != 0);

    SHA512_CTX sha_ctx;

    u64 reference_table = U64_MAX;
    for(s64 i = 7; i >= 0; i--)
    {
        lseek(drive_fd, sizeof(*table) * i, SEEK_SET);
        read(drive_fd, table, sizeof(*table));

        u8 hash[64];
        assert(SHA512_Init(&sha_ctx));
        assert(SHA512_Update(&sha_ctx, ((u8*)table) + 64, sizeof(RAD_PartitionTable) - 64));
        assert(SHA512_Final(hash, &sha_ctx));

        u8 is_valid = 1;
        for(u64 i = 0; i < 64; i++) { if(hash[i] != table->sha512sum[i]) { is_valid = 0; } }

        if(!is_valid)
        {
            printf("Table#%ld is not valid\n", i);
        }
        else
        {
            printf("Table#%ld is valid\n", i);
            reference_table = i;
        }
    }
    if(reference_table == U64_MAX)
    {
        printf("There are no valid tables. Either the drive is not formatted or you are in a very unfortunate situation.\n");
        return;
    }
    lseek(drive_fd, sizeof(*table) * reference_table, SEEK_SET);
    read(drive_fd, table, sizeof(*table));
    printf("Using table#%lu as reference\n", reference_table);

    for(u64 i = 0; i < 63; i++)
    {
        u64 next_partition_start = drive_block_count; // temp should be disk size
        if(i < 62) { next_partition_start = table->entries[i+1].start_block; }

        if(table->entries[i].partition_type != 0)
        {
            printf("Partition, type = %u, start = %llu, size = %llu, name = %s\n",
                    table->entries[i].partition_type,
                    table->entries[i].start_block,
                    next_partition_start - table->entries[i].start_block,
                    table->entries[i].name
            );
        }
    }

    close(drive_fd);
}
