#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "minutil.h"

inode *getInode(int number, char *data, int verbose)
{
    superblock *sb = (superblock *)(data + 1024);
    return (inode *)(data + ((2 + sb->i_blocks + sb->z_blocks)
        * sb->blocksize) + ((number - 1) * sizeof(inode)));
}

// /* Gets a data zone from an inode at a given index (starting from zero) */
// char *getZoneByIndex(int index, inode *file, char *data)
// {
//     superblock *sb = (superblock *)(data + 1024);
//     int blocksize = sb->blocksize;
//     int zonesize = blocksize << sb->log_zone_size;

//     /* Target is a direct zone */
//     if(index < 7)
//     {
//         return data + (file->zone[index] * zonesize);
//     }
//     /* Target is an indirect zone */
//     else
//     {
//         int indirectIndex = index - 7;
//         int numIndirectLinks = blocksize / sizeof(uint32_t);
        
//         /* Target is in a single indirect zone */
//         if (indirectIndex < numIndirectLinks)
//         {
//             /* Get reference to indirect zone */
//             uint32_t *indirectZone =
//                 (uint32_t *)(data + (file->indirect * zonesize));

//             /* Check if the zone is a hole */
//             if(indirectZone[indirectIndex] == 0)
//                 return NULL;
            
//             /* Find and return the target zone */
//             return data + (indirectZone[indirectIndex] * zonesize);
//         }

//         // TODO: read doubly indirect zones
//         printf("first-degree indirect zones exceeded!\n");
//         exit(-1);
//     }
// }

/* Gets the directory entry at a certain index */
dirent *getDirEntByIndex(int index, inode *dir, char *data, int verbose)
{
    superblock *sb = (superblock *)(data + 1024);
    int zonesize = sb->blocksize << sb->log_zone_size;
    int direntsPerZone = zonesize / sizeof(dirent);

    /* Get zone containing target dirent */
    int targetZoneIndex = index / direntsPerZone;
    char *targetZone = getZoneByIndex(targetZoneIndex, dir, data, verbose);

    /* Get index of target dirent in the zone */
    int relativeIndex = index - (targetZoneIndex * direntsPerZone);

    /* Get target dirent */
    return (dirent *)(targetZone + (relativeIndex * sizeof(dirent)));
}

/* Gets the directory entry with a certain name */
dirent *getDirEntByName(char *name, inode *dir, char *data, int verbose)
{
    int containedFiles = dir->size / 64;

    int i;
    for (i = 0; i < containedFiles; i++)
    {
        dirent *current = getDirEntByIndex(i, dir, data, verbose);

        if (current->inode != 0 && strcmp(name, current->name) == 0)
            return current;
    }

    return NULL;
}

/* Checks if a file is a directory */
int isDirectory(inode *file)
{
    return (file->mode & 0170000) == 040000;
}

/* Finds a file inode given the path */
inode *findFile(char *path, char *data, int verbose)
{
    inode *current = getInode(1, data, verbose);

    char *tempPath = malloc(strlen(path) + 1);
    strcpy(tempPath, path);

    char *token = strtok(tempPath, "/");
    while (token != NULL)
    {
        if(!isDirectory(current))
        {
            fprintf(stderr, "ERROR: file not found!\n");
            exit(-1);
        }

        dirent *newDir = getDirEntByName(token, current, data, verbose);        

        if (newDir == NULL)
        {
            return NULL;
        }
        current = getInode(newDir->inode, data, verbose);

        token = strtok(NULL, "/");
    }

    free(tempPath);
    return current;
}

/* Gets the permission string for a file */
void *getPermissionString(int mode, char *modes)
{
    modes[0] = ((mode & 0170000) == 040000) ? 'd' : '-';
    modes[1] = (mode & 0400) ? 'r' : '-';
    modes[2] = (mode & 0200) ? 'w' : '-';
    modes[3] = (mode & 0100) ? 'x' : '-';
    modes[4] = (mode & 040) ? 'r' : '-';
    modes[5] = (mode & 020) ? 'w' : '-';
    modes[6] = (mode & 010) ? 'x' : '-';
    modes[7] = (mode & 04) ? 'r' : '-';
    modes[8] = (mode & 02) ? 'w' : '-';
    modes[9] = (mode & 01) ? 'x' : '-';
    modes[10] = '\0';
}

/* Prints the permissions and other info for a file */
void printFileInfo(inode *file, char *name)
{
    char modes[11];
    getPermissionString(file->mode, modes);

    printf("%s %9d %s\n", modes, file->size, name);
}

/* Prints the contents of a directory */
int printContents(inode *dir, char *path, char *data, int zonesize, int verbose)
{
    if (isDirectory(dir))
    {
        int containedFiles = dir->size / 64;

        char *zone1 = data + (dir->zone[0] * zonesize);
        dirent *contents = (dirent *)zone1;

        printf("%s:\n", path);

        int i;
        for (i = 0; i < containedFiles; i++)
        {
            dirent *current = getDirEntByIndex(i, dir, data, verbose);

            if (current->inode != 0)
            {
                printFileInfo(
                    getInode(current->inode, data, verbose), current->name);
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
    printf(
        "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n"
        "Options:\n"
        "-p part    --- select partition for filesystem (default: none)\n"
        "-s sub     --- select subpartition for filesystem (default: none)\n"
        "-h help    --- print usage information and exit\n"
        "-v verbose --- increase verbosity level\n");
}

/* Gets the location on disk of a specified partition */
unsigned char *enterPartition(
    unsigned char *data, unsigned char *diskStart, int partIndex)
{
    /* Check partition table signature for validity */
    if (*((data + 510)) != 0x55 || *(data + 511) != 0xAA)
    {
        fprintf(stderr, "ERROR: invalid partition table!\n");
        exit(-1);
    }

    /* Get the target partition table entry */
    partition *part =
        (partition *)(data + 0x1BE + (partIndex * sizeof(partition)));

    /* Make sure it is a valid MINIX partition */
    if (part->type != 0x81)
    {
        fprintf(stderr, "ERROR: not a MINIX partition!\n");
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
    char *path = "/";

    int verbose = 0;
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
                fprintf(stderr, "ERROR: partition must be in the range 0-3.\n");
                exit(-1);
            }
            break;
        /* Subpartition is specified */
        case 's':
            if (usePartition == -1)
            {
                fprintf(stderr, "ERROR: cannot set subpartition"
                       " unless main partition is specified.\n");
                return -1;
            }
            useSubpart = atoi(optarg);
            if (useSubpart < 0 || useSubpart > 3)
            {
                fprintf(stderr,
                    "ERROR: subpartition must be in the range 0-3.\n");
                exit(-1);
            }
            break;
        /* Help flag */
        case 'h':
            printUsage();
            return 0;
        /* Verbose mode enabled */
        case 'v':
            verbose = 1;
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
        fprintf(stderr, "ERROR: image name required.\n");
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
        fprintf(stderr, "ERROR: file not found!\n");
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
        fprintf(stderr, "Bad magic number. (0x%04X)\n", sb->magic);
        fprintf(stderr, "This doesn't look like a MINIX filesystem.\n");
        exit(-1);
    }

    /* Calculate zone size */
    int zonesize = sb->blocksize << sb->log_zone_size;

    /* If verbose mode is enabled, print superblock info */
    if(verbose)
    {
        printf("\nSuperblock Contents:\n"
            "Stored Fields:\n"
            "  ninodes \t%10d\n"
            "  i_blocks \t%10d\n"
            "  z_blocks \t%10d\n"
            "  firstdata \t%10d\n"
            "  log_zone_size %10d (zone size: %d)\n"
            "  max_file \t%10u\n"
            "  magic \t%10d\n"
            "  zones \t%10d\n"
            "  blocksize \t%10d\n"
            "  subversion \t%10d\n\n",
            sb->ninodes,
            sb->i_blocks,
            sb->z_blocks,
            sb->firstdata,
            sb->log_zone_size,
            zonesize,
            sb->max_file,
            sb->magic,
            sb->zones,
            sb->blocksize,
            sb->subversion);
    }

    /* Get inode of file at path specified */
    inode *file = findFile(path, data, verbose);

    /* If verbose mode is enabled, print file inode info */
    if(verbose)
    {
        time_t at = file->atime;
        time_t mt = file->mtime;
        time_t ct = file->ctime;

        char modes[11];
        getPermissionString(file->mode, modes);

        printf("File inode:\n"
            "  uint16_t mode \t%#10x (%s)\n"
            "  uint16_t links \t%10d\n"
            "  uint16_t uid \t\t%10d\n"
            "  uint16_t gid \t\t%10d\n"
            "  uint32_t size \t%10d\n"
            "  uint32_t atime \t%10d --- %s"
            "  uint32_t mtime \t%10d --- %s"
            "  uint32_t ctime \t%10d --- %s\n",
            file->mode,
            modes,
            file->links,
            file->uid,
            file->gid,
            file->size,
            file->atime,
            ctime(&at),
            file->mtime,
            ctime(&mt),
            file->ctime,
            ctime(&ct));

        printf("Direct zones:\n"
            "           zone[0]   = %10d\n"
            "           zone[1]   = %10d\n"
            "           zone[2]   = %10d\n"
            "           zone[3]   = %10d\n"
            "           zone[4]   = %10d\n"
            "           zone[5]   = %10d\n"
            "           zone[6]   = %10d\n"
            "  uint32_t indirect    %10d\n"
            "  uint32_t double      %10d\n",
            file->zone[0],
            file->zone[1],
            file->zone[2],
            file->zone[3],
            file->zone[4],
            file->zone[5],
            file->zone[6],
            file->indirect,
            file->two_indirect);
    }

    /* Make sure file was found */
    if (!file)
    {
        fprintf(stderr, "ERROR: file not found!\n");
        return -1;
    }

    /* Print info about file, or directory contents */
    printContents(file, path, data, zonesize, verbose);

    /* Free the memory allocated for the image data */
    free(diskStart);
}
