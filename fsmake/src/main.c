
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
    if(argc < 3) { printf("usage: fsmake drive_file source_directory\n"); return; }

    int drive_fd = open(argv[1], O_EXCL | O_CREAT | O_RDWR, S_IRUSR|S_IWUSR | S_IRGRP|S_IWGRP);
    if(drive_fd < 0) { printf("%s already exists or there was an error opening the file\n"); return; }

    u8 block[4096];
    for(u64 i = 0; i < 2048*32; i++)
    {
        write(drive_fd, block, 4096);
    }

    assert(sizeof(RAD_PartitionTable) == 4096*2);

    RAD_PartitionTable* table = calloc(sizeof(RAD_PartitionTable));
    assert(table != 0);

    SHA512_CTX sha_ctx;

    // fill out table


    assert(SHA512_Init(&sha_ctx));
    assert(SHA512_Update(&sha_ctx, ((u8*)table) + 64, sizeof(RAD_PartitionTable) - 64));
    assert(SHA512_Final((u8*)table, &sha_ctx));

    // write table
    lseek(drive_fd, 0, SEEK_SET);
    for(u64 i = 0; i < TABLE_COUNT; i++)
    {
        write(drive_fd, table, sizeof(*table));
    }

    close(drive_fd);
}
