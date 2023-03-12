#include "minutil.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *getZoneByIndex(int index, inode *file, char *data, int verbose)
{
    if(verbose == 1)
        printf("getZoneByIndex: %d\n", index);

    superblock *sb = (superblock *)(data + 1024);
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

        if(verbose == 1)
            printf("\tzone data: %s\n", data + (file->zone[index] * zonesize));

        return data + (file->zone[index] * zonesize);
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
            uint32_t *indirectZone =
                (uint32_t *)(data + (file->indirect * zonesize));

            /* Check if the zone is a hole */
            if(indirectZone[indirectIndex] == 0)
                return NULL;

            /* Find and return the target zone */
            return data + (indirectZone[indirectIndex] * zonesize);
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
                    (uint32_t *)(data + (file->two_indirect * zonesize));

                /* Get index of indirect zone in doubly indirect zone */
                int indirectZoneIndex = doubleIndirectIndex / numIndirectLinks;

                /* Check if indirect zone is a hole */
                if(doubleIndirectZone[indirectZoneIndex] == 0)
                    return NULL;

                /* Get reference to indirect zone */
                uint32_t *indirectZone =
                    (uint32_t *)(data + (doubleIndirectZone[indirectZoneIndex] *
                                         zonesize));

                /* Get index of target in indirect zone */
                int directZoneIndex = doubleIndirectIndex % numIndirectLinks;

                /* Check if the zone is a hole */
                if(indirectZone[directZoneIndex] == 0)
                    return NULL;

                /* Find and return the target zone */
                return data + (indirectZone[directZoneIndex] * zonesize);
            }
        }

        fprintf(stderr,
                "ERROR: trying to get zone exceeding maximum file size!\n");
        exit(-1);
    }
}

/* Retrieves the contents of a file */
char *getFileContents(inode *file, char *data, int verbose)
{
    /* Initialize buffer for file data with all zeros */
    char *fileData = (char *)calloc(file->size + 1, sizeof(char));

    if(verbose == 1)
        printf("fileData size: %d\n", file->size);

    if(fileData == NULL)
    {
        fprintf(stderr, "ERROR: could not allocate memory for file data!\n");
        exit(-1);
    }

    superblock *sb = (superblock *)(data + 1024);
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
        char *zoneData = getZoneByIndex(i, file, data, verbose);

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
    fileData[file->size] = '\0';

    if(verbose == 1)
        printf("fileData: %s\n", fileData);

    return fileData;
}

/* Gets an inode struct given its index */
inode *getInode(int number, char *data, int verbose)
{
    superblock *sb = (superblock *)(data + 1024);
    return (inode *)(data +
                     ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize) +
                     ((number - 1) * sizeof(inode)));
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
    for(i = 0; i < containedFiles; i++)
    {
        dirent *current = getDirEntByIndex(i, dir, data, verbose);

        if(current->inode != 0 && strcmp(name, current->name) == 0)
            return current;
    }

    return NULL;
}

/* Finds a file inode given the path */
inode *findFile(char *path, char *data, int verbose)
{
    inode *current = getInode(1, data, verbose);

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

        dirent *newDir = getDirEntByName(token, current, data, verbose);

        if(newDir == NULL)
        {
            return NULL;
        }
        current = getInode(newDir->inode, data, verbose);

        token = strtok(NULL, "/");
    }

    free(tempPath);
    return current;
}