#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "minutil.h"

inode *getInode(int number, char *data)
{
    superblock *sb = (superblock *)(data + 1024);
    return (inode *)(data + ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize) + ((number - 1) * sizeof(inode)));
}

/* Gets the directory entry at a certain index */
dirent *getDirEntIndex(int index, inode *dir, char *data)
{
    superblock *sb = (superblock *)(data + 1024);
    int zonesize = sb->blocksize << sb->log_zone_size;
    int blocksize = sb->blocksize;
    int direntsPerZone = zonesize / sizeof(dirent);
    int directZoneEntries = direntsPerZone * DIRECT_ZONES;

    // target is inside direct zones
    if (index <= directZoneEntries)
    {
        int zoneIndex = index / direntsPerZone;
        int relativeIndex = index - (zoneIndex * direntsPerZone);

        char *zone = data + (dir->zone[zoneIndex] * zonesize) + (relativeIndex * sizeof(dirent));
        dirent *contents = (dirent *)zone;

        return contents;
    }
    else
    {
        // TODO: read indirect zones
        printf("direct zones exceeded! will be fixed later...\n");
        exit(-1);
    }
}

/* Gets the directory entry with a certain name */
dirent *getDirEntName(char *name, inode *dir, char *data)
{
    int containedFiles = dir->size / 64;

    for (int i = 0; i < containedFiles; i++)
    {
        dirent *current = getDirEntIndex(i, dir, data);

        if (current->inode != 0 && strcmp(name, current->name) == 0)
        {
            return current;
        }
    }

    return NULL;
}

/* Finds a file inode given the path */
inode *findFile(char *path, char *data)
{
    inode *current = getInode(1, data);

    char *tempPath = malloc(sizeof(path));
    strcpy(tempPath, path);

    char *token = strtok(tempPath, "/");
    while (token != NULL)
    {
        dirent *newDir = getDirEntName(token, current, data);

        if (current == NULL)
        {
            return NULL;
        }
        current = getInode(newDir->inode, data);

        token = strtok(NULL, "/");
    }

    return current;
}

/* Prints the permissions and other info for a file */
void printFileInfo(inode *file, char *name)
{
    char modes[10];
    modes[0] = ((file->mode & 0170000) == 040000) ? 'd' : '-';
    modes[1] = (file->mode & 0400) ? 'r' : '-';
    modes[2] = (file->mode & 0200) ? 'w' : '-';
    modes[3] = (file->mode & 0100) ? 'x' : '-';
    modes[4] = (file->mode & 040) ? 'r' : '-';
    modes[5] = (file->mode & 020) ? 'w' : '-';
    modes[6] = (file->mode & 010) ? 'x' : '-';
    modes[7] = (file->mode & 04) ? 'r' : '-';
    modes[8] = (file->mode & 02) ? 'w' : '-';
    modes[9] = (file->mode & 01) ? 'x' : '-';

    printf("%s %9d %s\n", modes, file->size, name);
}

/* Prints the contents of a directory */
int printContents(inode *dir, char *path, char *data, int zonesize)
{
    if ((dir->mode & 0170000) == 040000)
    {
        int containedFiles = dir->size / 64;

        char *zone1 = data + (dir->zone[0] * zonesize);
        dirent *contents = (dirent *)zone1;

        printf("%s/:\n", path);

        for (int i = 0; i < containedFiles; i++)
        {
            dirent *current = getDirEntIndex(i, dir, data);

            if (current->inode != 0)
            {
                printFileInfo(getInode(current->inode, data), current->name);
            }
        }

        return 0;
    }
    else
    {
        printFileInfo(dir, path);
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

/* Gets the location on disk of a specified partition */
unsigned char *enterPartition(unsigned char *data, unsigned char *diskStart, int partIndex)
{
    /* Check partition table signature for validity */
    if (*((data + 510)) != 0x55 || *(data + 511) != 0xAA)
    {
        printf("ERROR: invalid partition table!\n");
        exit(-1);
    }

    /* Get the target partition table entry */
    partition *part = (partition *)(data + 0x1BE + (partIndex * sizeof(partition)));

    /* Make sure it is a valid MINIX partition */
    if (part->type != 0x81)
    {
        printf("ERROR: not a MINIX partition!\n");
        exit(-1);
    }

    /* Get the offset to the partition contents */
    int start = part->lFirst * 512;

    // TODO: make sure this isn't off the end of the image
    return (diskStart + start);
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
    char *path = "";

    int v_flag = 0;
    int usePartition = -1;
    int useSubpart = -1;

    /* Loop through argument flags */
    int c;
    while ((c = getopt(argc, argv, "hvp:s:")) != -1)
    {
        switch (c)
        {
        /* Partition is specified */
        case 'p':
            usePartition = atoi(optarg);
            if (usePartition < 0 || usePartition > 3)
            {
                printf("ERROR: partition must be in the range 0-3.\n");
                exit(-1);
            }
            break;
        /* Subpartition is specified */
        case 's':
            if (usePartition == -1)
            {
                printf("ERROR: cannot set subpartition"
                       " unless main partition is specified.\n");
                return -1;
            }
            useSubpart = atoi(optarg);
            if (useSubpart < 0 || useSubpart > 3)
            {
                printf("ERROR: subpartition must be in the range 0-3.\n");
                exit(-1);
            }
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
    unsigned char *diskStart = malloc(filesize);
    fread(diskStart, 1, filesize, fp);
    fclose(fp);

    unsigned char *data = diskStart;

    /* If a partition was specified */
    if (usePartition != -1)
    {
        /* Switch to specified partition */
        unsigned char *diskStart = data;
        data = enterPartition(data, diskStart, usePartition);

        /* If subpartition was specified */
        if (useSubpart != -1)
        {
            /* Switch to subpartition */
            data = enterPartition(data, diskStart, useSubpart);
        }  
    }

    /* Find superblock */
    superblock *sb = (superblock *)(data + 1024);

    /* Check magic number to ensure that this is a MINIX filesystem */
    if (sb->magic != 0x4D5A)
    {
        printf("Bad magic number. (0x%04X)\n", sb->magic);
        printf("This doesn't look like a MINIX filesystem.\n");
        exit(-1);
    }

    /* Calculate zone size */
    int zonesize = sb->blocksize << sb->log_zone_size;

    /* Get inode of file at path specified */
    inode *file = findFile(path, data);

    /* Make sure file was found */
    if (!file)
    {
        printf("ERROR: file not found!\n");
        return -1;
    }

    /* Print info about file, or directory contents */
    printContents(file, path, data, zonesize);

    /* Free the memory allocated for the image data */
    free(diskStart);
}