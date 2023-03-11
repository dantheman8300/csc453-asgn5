#include "minutil.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *getZoneByIndex(int index, inode *file, char *data, int verbose)
{

    if (verbose == 1)
        printf("getZoneByIndex: %d\n", index);

    superblock *sb = (superblock *)(data + 1024);
    int blocksize = sb->blocksize;
    int zonesize = blocksize << sb->log_zone_size;

    /* Target is a direct zone */
    if (index < 7)
    {
        if (verbose == 1)
            printf("\tzone: %d\n", file->zone[index]);

        if (verbose == 1)
            printf("\tzone data: %s\n", data + (file->zone[index] * zonesize));

        return data + (file->zone[index] * zonesize);
    }
    /* Target is an indirect zone */
    else
    {
        int indirectIndex = index - 7;
        int numIndirectLinks = blocksize / sizeof(uint32_t);

        /* Target is in a single indirect zone */
        if (indirectIndex < numIndirectLinks)
        {
            /* Get reference to indirect zone */
            uint32_t *indirectZone =
                (uint32_t *)(data + (file->indirect * zonesize));

            /* Check if the zone is a hole */
            if (indirectZone[indirectIndex] == 0)
                return NULL;

            /* Find and return the target zone */
            return data + (indirectZone[indirectIndex] * zonesize);
        }
        /* Target is in a doubly indirect zone */
        // else
        // {
        //     int doubleIndirectIndex = indirectIndex - numIndirectLinks;

        //     if (doubleIndirectIndex < numIndirectLinks * numIndirectLinks)
        //     {
        //         /* Get reference to doubly indirect zone */
        //         uint32_t *doubleIndirectZone =
        //             (uint32_t *)(data + (file->two_indirect * zonesize));

        //         /* Get reference to indirect zone */
        //         uint32_t *indirectZone =
        //             (uint32_t *)(data +
        //             (doubleIndirectZone[doubleIndirectIndex] * zonesize));

        //         /* Check if the zone is a hole */
        //         if(indirectZone[doubleIndirectIndex % numIndirectLinks] == 0)
        //             return NULL;

        //         /* Find and return the target zone */
        //         return data + (indirectZone[doubleIndirectIndex] * zonesize);
        //     }
        // }

        // TODO: read doubly indirect zones
        printf("second-degree indirect zones exceeded!\n");
        exit(-1);
    }
}

/* Retrieves the contents of a file */
char *getFileContents(inode *file, char *data, int verbose)
{
    /* Initialize buffer for file data with all zeros */
    char *fileData = (char *)calloc(file->size + 1, sizeof(char));

    if (verbose == 1)
        printf("fileData size: %d\n", file->size + 1);

    if (fileData == NULL)
    {
        fprintf(stderr, "ERROR: could not allocate memory for file data!\n");
        exit(-1);
    }

    superblock *sb = (superblock *)(data + 1024);
    int zonesize = sb->blocksize << sb->log_zone_size;

    /* Calculate the number of zones the file contains */
    int totalZones = (file->size / zonesize) + ((file->size % zonesize) != 0);

    /* Loop through all relevant zones */
    int i;
    for (i = 0; i < totalZones; i++)
    {
        /* Retrieve the target zone */
        char *zoneData = getZoneByIndex(i, file, data, verbose);

        /* If it's not a hole, copy it to the buffer */
        if (zoneData != NULL)
        {

            // if (verbose == 1) printf("zoneData: %s\n", zoneData);

            int bytesToCopy = zonesize;

            /* If this is the last zone, only copy the relevant portion */
            if (i == totalZones - 1)
                bytesToCopy = file->size % zonesize;

            if (verbose == 1)
                printf("bytesToCopy: %d\n", bytesToCopy);

            memcpy(fileData + (i * zonesize), zoneData, bytesToCopy);

            if (verbose == 1)
                printf("fileData: %s\n", fileData);
        }
    }
    fileData[file->size] = '\0';

    if (verbose == 1)
        printf("fileData: %s\n", fileData);

    return fileData;
}