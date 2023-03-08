#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

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
{ /* Minix Version 3 Superblock
   * this structure found in fs/super.h
   * in minix 3.1.1
   */
    /* on disk. These fields and orientation are non–negotiable */
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

int printContents(inode *dir, char *data, int zonesize)
{
    if ((dir->mode & 0170000) == 040000)
    {
        int containedFiles = dir->size / 64;

        char *zone1 = data + (dir->zone[0] * zonesize);
        dirent *contents = (dirent *)zone1;

        for (int i = 0; i < containedFiles; i++)
        {
            dirent *current = (contents + i);

            if (current->inode != 0)
            {
                printf("file: %s, fd: %d\n", current->name, current->inode);

                // TODO: print permissions/size of each file
            }
        }

        return 0;
    }
    else
    {
        printf("ERROR: file is not a directory!\n");

        return -1;
    }
}

/* Prints program usage information */
void printUsage()
{
    printf("usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n"
           "Options:\n"
           "-p part    --- select partition for filesystem (default: none)\n"
           "-s sub     --- select subpartition for filesystem (default: none)\n"
           "-h help    --- print usage information and exit\n"
           "-v verbose --- increase verbosity level\n");
}

int main(int argc, char **argv)
{
    /* If no arguments specified, print usage */
    if (argc < 2)
    {
        printUsage();
        return 0;
    }

    char *filename = NULL;
    char *path = "/";

    int v_flag = 0;
    int partition = -1;
    int subpart = -1;

    /* Loop through argument flags */
    int c;
    while ((c = getopt(argc, argv, "hvp:s:")) != -1)
    {
        switch (c)
        {
        /* Partition is specified */
        case 'p':
            partition = atoi(optarg);
            break;
        /* Subpartition is specified */
        case 's':
            if (partition == -1)
            {
                printf("ERROR: cannot set subpartition"
                       " unless main partition is specified.\n");
                return -1;
            }
            subpart = atoi(optarg);
            break;
        /* Help flag */
        case 'h':
            printUsage();
            break;
        /* Verbose mode enabled */
        case 'v':
            v_flag = 1;
            break;
        }
    }

    /* Get image filename */
    if (optind < argc)
    {
        filename = argv[optind];
    }
    else
    {
        printf("ERROR: image name required.\n");
        printUsage();
        exit(-1);
    }
    /* Get path (if specified) */
    if (optind + 1 < argc)
    {
        path = argv[optind + 1];
    }

    printf("opening file: %s\n", filename);

    /* Open image file descriptor */
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("ERROR: file not found!\n");
        return -1;
    }

    /* Find total length of file, in bytes */
    fseek(fp, 0L, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    /* Read contents of file into memory */
    char *data = malloc(filesize);
    fread(data, 1, filesize, fp);
    fclose(fp);

    // uint8_t test = *data;
    // printf("%d\n", test);

    // partition * firstPart = (partition *) (data + 0x1BE);

    // printf("type: %x\n", firstPart->type);

    superblock *sb = (superblock *)(data + 1024);

    // printf("inodes: %d\n", sb->ninodes);

    int zonesize = sb->blocksize << sb->log_zone_size;

    // printf("zone size: %d\n", zonesize);

    inode *root = (inode *)(data + ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize));

    // printf("first inode size: %d\n", root->size);

    printf("%s:\n", path);
    printContents(root, data, zonesize);

    free(data);
}