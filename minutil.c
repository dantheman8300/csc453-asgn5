#include "minutil.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reads bytes from the filesystem image at a specified location */
unsigned char *getData(uint32_t start, uint32_t size, FILE *file,
                       int partitionStart)
{
    /* Allocate buffer for read data */
    unsigned char *data = (unsigned char *)malloc(size);

    /* Seek to specified start location */
    int status = fseek(file, start + partitionStart, SEEK_SET);
    if(status != 0)
    {
        fprintf(stderr, "ERROR: tried to access invalid image location!\n");
        exit(-1);
    }

    /* Read data from image */
    int read = fread(data, 1, size, file);
    if(read != size)
    {
        fprintf(stderr, "ERROR: could not read all data from image!\n");
        exit(-1);
    }

    return data;
}

/* Gets a data zone from an inode at a given index (starting from zero) */
char *getZoneByIndex(int index, inode *file, FILE *image, superblock *sb,
                     int partitionStart, int verbose)
{
    if(verbose == 1)
        printf("getZoneByIndex: %d\n", index);

    int blocksize = sb->blocksize;
    int zonesize = blocksize << sb->log_zone_size;

    /* Target is a direct zone */
    if(index < DIRECT_ZONES)
    {
        if(verbose == 1)
            printf("\tzone: %d\n", file->zone[index]);

        /* Check if the zone is a hole */
        if(file->zone[index] == 0)
            return NULL;

        char *data = getData(file->zone[index] * zonesize, zonesize, image,
                             partitionStart);

        if(verbose == 1)
            printf("\tzone data: %s\n", data);

        return data;
        // return data + (file->zone[index] * zonesize);
    }
    /* Target is an indirect zone */
    else
    {
        int indirectIndex = index - DIRECT_ZONES;
        int numIndirectLinks = blocksize / sizeof(uint32_t);

        if(verbose == 1)
            printf("indirectIndex: %d\n", indirectIndex);

        /* Target is in a single indirect zone */
        if(indirectIndex < numIndirectLinks)
        {
            /* Check if the indirect zone is a hole */
            if(file->indirect == 0)
                return NULL;

            /* Get reference to indirect zone */
            uint32_t *indirectZone = (uint32_t *)(getData(
                file->indirect * zonesize, zonesize, image, partitionStart));

            /* Check if the zone is a hole */
            if(indirectZone[indirectIndex] == 0)
                return NULL;

            /* Find and return the target zone */
            return getData(indirectZone[indirectIndex] * zonesize, zonesize,
                           image, partitionStart);
        }
        /* Target is in a doubly indirect zone */
        else
        {
            int doubleIndirectIndex = indirectIndex - numIndirectLinks;

            if(doubleIndirectIndex < numIndirectLinks * numIndirectLinks)
            {
                /* Check if doubly indirect zone is a hole */
                if(file->two_indirect == 0)
                    return NULL;

                /* Get reference to doubly indirect zone */
                uint32_t *doubleIndirectZone =
                    (uint32_t *)(getData(file->two_indirect * zonesize,
                                         zonesize, image, partitionStart));

                /* Get index of indirect zone in doubly indirect zone */
                int indirectZoneIndex = doubleIndirectIndex / numIndirectLinks;

                /* Check if indirect zone is a hole */
                if(doubleIndirectZone[indirectZoneIndex] == 0)
                    return NULL;

                /* Get reference to indirect zone */
                uint32_t *indirectZone = (uint32_t *)(getData(
                    doubleIndirectZone[indirectZoneIndex] * zonesize, zonesize,
                    image, partitionStart));

                /* Get index of target in indirect zone */
                int directZoneIndex = doubleIndirectIndex % numIndirectLinks;

                /* Check if the zone is a hole */
                if(indirectZone[directZoneIndex] == 0)
                    return NULL;

                /* Find and return the target zone */
                return getData(indirectZone[directZoneIndex] * zonesize,
                               zonesize, image, partitionStart);
            }
        }

        fprintf(stderr,
                "ERROR: trying to get zone exceeding maximum file size!\n");
        exit(-1);
    }
}

/* Retrieves the contents of a file */
char *getFileContents(inode *file, FILE *image, superblock *sb,
                      int partitionStart, int verbose)
{
    /* Initialize buffer for file data with all zeros */
    char *fileData = (char *)calloc(file->size, sizeof(char));

    if(verbose == 1)
        printf("fileData size: %d\n", file->size);

    if(fileData == NULL)
    {
        fprintf(stderr, "ERROR: could not allocate memory for file data!\n");
        exit(-1);
    }

    int zonesize = sb->blocksize << sb->log_zone_size;

    /* Calculate the number of zones the file contains */
    int totalZones = (file->size / zonesize) + ((file->size % zonesize) != 0);

    if(verbose == 1)
        printf("totalZones: %d\n", totalZones);

    /* Loop through all relevant zones */
    int i;
    for(i = 0; i < totalZones; i++)
    {
        /* Retrieve the target zone */
        char *zoneData =
            getZoneByIndex(i, file, image, sb, partitionStart, verbose);

        /* If it's not a hole, copy it to the buffer */
        if(zoneData != NULL)
        {
            int bytesToCopy = zonesize;

            /* If this is the last zone, only copy the relevant portion */
            if(i == totalZones - 1 && file->size % zonesize != 0)
                bytesToCopy = file->size % zonesize;

            if(verbose == 1)
                printf("bytesToCopy: %d\n", bytesToCopy);

            memcpy(fileData + (i * zonesize), zoneData, bytesToCopy);

            if(verbose == 1)
                printf("fileData: %s\n", fileData);
        }
        else
        {
            if(verbose == 1)
                printf("zone is a hole!\n");
        }
    }

    if(verbose == 1)
        printf("fileData: %s\n", fileData);

    return fileData;
}

/* Gets an inode struct given its index */
inode *getInode(int number, FILE *image, superblock *sb, int partitionStart,
                int verbose)
{
    int start = ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize) +
                ((number - 1) * sizeof(inode));

    return (inode *)getData(start, sizeof(inode), image, partitionStart);
}

/* Checks if a file is a directory */
int isDirectory(inode *file)
{
    return (file->mode & 0170000) == 040000;
}

/* Checks if a file is a regular file */
int isRegularFile(inode *file)
{
    return (file->mode & 0170000) == 0100000;
}

/* Gets the directory entry at a certain index */
dirent *getDirEntByIndex(int index, inode *dir, FILE *image, superblock *sb,
                         int partitionStart, int verbose)
{
    // superblock *sb = (superblock *)(data + 1024);
    int zonesize = sb->blocksize << sb->log_zone_size;
    int direntsPerZone = zonesize / sizeof(dirent);

    /* Get zone containing target dirent */
    int targetZoneIndex = index / direntsPerZone;
    char *targetZone = getZoneByIndex(targetZoneIndex, dir, image, sb,
                                      partitionStart, verbose);

    /* Get index of target dirent in the zone */
    int relativeIndex = index - (targetZoneIndex * direntsPerZone);

    /* Get target dirent */
    return (dirent *)(targetZone + (relativeIndex * sizeof(dirent)));
}

/* Gets the directory entry with a certain name */
dirent *getDirEntByName(char *name, inode *dir, FILE *image, superblock *sb,
                        int partitionStart, int verbose)
{
    int containedFiles = dir->size / 64;

    int i;
    for(i = 0; i < containedFiles; i++)
    {
        dirent *current =
            getDirEntByIndex(i, dir, image, sb, partitionStart, verbose);

        if(current->inode != 0 && strcmp(name, current->name) == 0)
            return current;
    }

    return NULL;
}

/* Finds a file inode given the path */
inode *findFile(char *path, FILE *image, superblock *sb, int partitionStart,
                int verbose)
{
    inode *current = getInode(1, image, sb, partitionStart, verbose);

    char *tempPath = malloc(strlen(path) + 1);
    strcpy(tempPath, path);

    char *token = strtok(tempPath, "/");
    while(token != NULL)
    {
        if(!isDirectory(current))
        {
            fprintf(stderr, "ERROR: file not found!\n");
            exit(-1);
        }

        dirent *newDir =
            getDirEntByName(token, current, image, sb, partitionStart, verbose);

        if(newDir == NULL)
        {
            return NULL;
        }
        current = getInode(newDir->inode, image, sb, partitionStart, verbose);

        token = strtok(NULL, "/");
    }

    free(tempPath);
    return current;
}

/* Gets the location on disk of a specified partition */
uint32_t enterPartition(FILE *file, uint32_t currentStart, int partIndex)
{
    /* Get the start of the partition table */
    unsigned char *data = getData(0, 512, file, currentStart);

    /* Check partition table signature for validity */
    if(*((data + 510)) != 0x55 || *(data + 511) != 0xAA)
    {
        fprintf(stderr, "ERROR: invalid partition table!\n");
        exit(-1);
    }

    /* Get the target partition table entry */
    partition *part =
        (partition *)(data + 0x1BE + (partIndex * sizeof(partition)));

    /* Make sure it is a valid MINIX partition */
    if(part->type != 0x81)
    {
        fprintf(stderr, "ERROR: not a MINIX partition!\n");
        exit(-1);
    }

    /* Get the offset to the partition contents */
    int start = part->lFirst * 512;

    return start;
}
