#ifndef PARSE_LFS_H
#define PARSE_LFS_H

// See `man inode` for more on bitmasks.
#include <sys/stat.h> // Provides mode_t, S_ISDIR, etc.

#include <time.h> // Provides time_t

#include "../lib/lib.h" // Provides ls_print_file(...)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// You may need more includes!

// Can add more structs, change, extend, or delete these.
typedef struct{
    uint file_cursor;
    uint size;
    mode_t permissions;
    time_t mtime;

    uint num_direct_blocks;

    // Could have one or more entries pointing to data block 
    // Each entry is:
        // uint direct_block_disk_offset (in bytes)
    // ...

} Inode;

typedef struct{
    uint num_entries;

    // Could have one or more entries pointing to Inode 
    // Each entry has:
        // uint inumber 
        // uint inode_disk_offset
    // ...

} Imap;

typedef struct{
    uint image_offset;
    uint num_entries;

    // Could have one or more entries pointing to Imaps
    // Each entry has:
        // uint inumber_start 
        // uint inumber_end
        // uint imap_disk_offset
    // ...
} CR;

#endif
