
#include "../../src/types.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


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

    close(drive_fd);
}
