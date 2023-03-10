#include <stdint.h>
#include <stdio.h>

#define DIRECT_ZONES 7

typedef struct partition
{
    uint8_t bootint;
    uint8_t start_head;
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sec;
    uint32_t lFirst;
    uint32_t size;
} partition;

typedef struct superblock
{
    /* Minix Version 3 Superblock
        this structure found in fs/super.h in minix 3.1.1 on disk.
        These fields and orientation are non–negotiable */
    uint32_t ninodes;      /* number of inodes in this filesystem */
    uint16_t pad1;         /* make things line up properly */
    int16_t i_blocks;      /* # of blocks used by inode bit map */
    int16_t z_blocks;      /* # of blocks used by zone bit map */
    uint16_t firstdata;    /* number of first data zone */
    int16_t log_zone_size; /* log2 of blocks per zone */
    int16_t pad2;          /* make things line up again */
    uint32_t max_file;     /* maximum file size */
    uint32_t zones;        /* number of zones on disk */
    int16_t magic;         /* magic number */
    int16_t pad3;          /* make things line up again */
    uint16_t blocksize;    /* block size in bytes */
    uint8_t subversion;    /* filesystem sub–version */
} superblock;

typedef struct inode
{
    uint16_t mode;  /* mode */
    uint16_t links; /* number or links */
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[7];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
} inode;

typedef struct dirent
{
    uint32_t inode;
    unsigned char name[60];
} dirent;

/* Reads bytes from the filesystem image at a specified location */
unsigned char *getData(uint32_t start, uint32_t size, FILE *file,
                       int partitionStart);

/* Gets a data zone from an inode at a given index (starting from zero) */
char *getZoneByIndex(int index, inode *file, FILE *image, superblock *sb,
                     int partitionStart, int verbose);

/* Retrieves the contents of a file */
char *getFileContents(inode *file, FILE *image, superblock *sb,
                      int partitionStart, int verbose);

/* Gets an inode struct given its index */
inode *getInode(int number, FILE *image, superblock *sb, int partitionStart,
                int verbose);

/* Checks if a file is a directory */
int isDirectory(inode *file);

/* Checks if a file is a regular file */
int isRegularFile(inode *file);

/* Gets the directory entry at a certain index */
dirent getDirEntByIndex(int index, inode *dir, FILE *image, superblock *sb,
                        int partitionStart, int verbose);

/* Gets the directory entry with a certain name */
dirent getDirEntByName(char *name, inode *dir, FILE *image, superblock *sb,
                       int partitionStart, int verbose);

/* Finds a file inode given the path */
inode *findFile(char *path, FILE *image, superblock *sb, int partitionStart,
                int verbose);

/* Gets the location on disk of a specified partition */
uint32_t enterPartition(FILE *file, uint32_t currentStart, int partIndex);
