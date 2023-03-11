#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "minutil.h"

int verbose = 0;

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * (count + 1));

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            // assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        // assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
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
                if (verbose == 1) printf("file: %s, fd: %d\n", current->name, current->inode);

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

inode *findFile(inode *dir, char **srcpathlist, char *data, int zonesize, superblock *sb, inode *root)
{

    if (srcpathlist[0] == NULL) {
        printf("srcpathlist is empty\n");
        return NULL;
    }

    if (verbose == 1) printf("%s\n", srcpathlist[0]);


    if ((dir->mode & 0170000) == 040000)
    {
        int containedFiles = dir->size / 64;

        char *zone1 = data + (dir->zone[0] * zonesize);
        dirent *contents = (dirent *)zone1;
        dirent *current;

        for (int i = 0; i < containedFiles; i++)
        {
            current = (contents + i);

            if (current->inode != 0)
            {
                // if (verbose == 1) printf("file: %s, fd: %d\n", current->name, current->inode);

                if (strcmp(current->name, srcpathlist[0]) == 0) 
                {
                    if (verbose == 1) printf("\tthis is the matching name\n");
                    if (verbose == 1) printf("\tinode: %d\n", current->inode);
                    inode *foundFile = root + current->inode - 1;
                    if (verbose == 1) printf("\tfirst inode size: %d\n", foundFile->size);
                    if (verbose == 1) printf("\tfirst inode mode: %d\n", foundFile->mode);
                    
                    if ((foundFile->mode & 0170000) == 040000) 
                    {
                        return findFile(foundFile, srcpathlist + 1, data, zonesize, sb, root);
                    } 
                    else 
                    {
                        if (srcpathlist[1] == NULL ) 
                        {
                            if (verbose == 1) printf("**found file: %s\n", current->name);
                            return foundFile;    
                        }
                        printf("ERROR: Reached file too early\n");
                        return NULL;
                    }

                }
            }
        }
    }
    else if ((dir->mode & 0170000) == 040000)
    {
        printf("ERROR: file is not a directory!\n");

        return NULL;
    }

    return NULL;
}

char *getFileContentsOld(inode *file, char* data, superblock* sb)
{
    char *fileData;
    int dataLength = 0;
    fileData = (char *)malloc(0);

    int zonesize = sb->blocksize << sb->log_zone_size;

    int zoneIndex = 0;

    int numIndirectLinks = sb->blocksize / sizeof(uint32_t);

    while (zoneIndex < 7) 
    {

        // zone is a hole
        if (file->zone[zoneIndex] == 0) 
        {
            if (zoneIndex == 0) 
            {
                if (verbose == 1) { printf("File is empty\n"); }
                fileData = (char *)realloc(fileData, 1);
                fileData[0] = '\0';
                return fileData;
            }
            zoneIndex ++;
            continue;
        }

        char *currZoneData = (char *)(data + (file->zone[zoneIndex] * zonesize));

        dataLength += strlen(currZoneData);
        fileData = (char *)realloc(fileData, dataLength);

        strcat(fileData, currZoneData);
        if (verbose == 1) printf("zoneIndex: %d, dataZoneIndex: %d,  data size: %lu\n", zoneIndex, file->zone[zoneIndex],  strlen(fileData));
        zoneIndex++;
    }

    uint32_t zoneIndirect = file->indirect;
    if (verbose == 1) printf("indirect: %d\n", zoneIndirect);
    
    if (zoneIndirect == 0)
    {
        if (verbose == 1) printf("indirect is zero\n");
    }
    else 
    {
        uint32_t *indirectZoneData = (uint32_t *)(data + (zoneIndirect * zonesize));
        // int indirectZoneIndex = 0;

        if (verbose == 1) printf("numIndirectLinks: %d\n", numIndirectLinks);

        // while (indirectZoneIndex < numIndirectLinks)
        for (int indirectZoneIndex = 0; indirectZoneIndex < numIndirectLinks; indirectZoneIndex++)
        {
            uint32_t currZoneDataIndex = indirectZoneData[indirectZoneIndex];

            if (currZoneDataIndex == 0) continue;

            if (verbose == 1) printf("indirect zone data index [%d]: %u\n", indirectZoneIndex, currZoneDataIndex);

            char *currZoneData = (char *)(data + (currZoneDataIndex * zonesize));

            // if (verbose == 1) printf("currZoneData: %s\n", currZoneData);


            dataLength += strlen(currZoneData);
            fileData = (char *)realloc(fileData, dataLength);

            strcat(fileData, currZoneData);

            if (verbose == 1) printf("\tzoneIndex: %d, data size: %lu\n", currZoneDataIndex, strlen(fileData));

            // indirectZoneIndex++;
        }

    }

    uint32_t zoneDoubleIndirect = file->two_indirect;
    if (verbose == 1) printf("double indirect: %d\n", zoneDoubleIndirect);
    
    if (zoneDoubleIndirect == 0)
    {
        if (verbose == 1) printf("double indirect is zero\n");
    }
    else 
    {
        uint32_t *doubleIndirectZoneData = (uint32_t *)(data + (zoneDoubleIndirect * zonesize));
        // int doubleIndirectZoneIndex = 0;

        for (int doubleIndirectZoneIndex = 0; doubleIndirectZoneIndex < numIndirectLinks; doubleIndirectZoneIndex++)
        {
            uint32_t indirectZoneIndex = doubleIndirectZoneData[doubleIndirectZoneIndex];

            if (indirectZoneIndex == 0) continue;

            if (verbose == 1) printf("double indirect zone data index [%d]: %u\n", doubleIndirectZoneIndex, indirectZoneIndex);

            uint32_t *indirectZoneData = (uint32_t *)(data + (indirectZoneIndex * zonesize));

            for (int indirectZoneIndex = 0; indirectZoneIndex < numIndirectLinks; indirectZoneIndex++)
            {

                uint32_t currZoneDataIndex = indirectZoneData[indirectZoneIndex];

                if (currZoneDataIndex == 0) continue;

                if (verbose == 1) printf("\tindirect zone data index [%d]: %u\n", indirectZoneIndex, currZoneDataIndex);

                char *currZoneData = (char *)(data + (currZoneDataIndex * zonesize));

                // if (verbose == 1) printf("currZoneData: %s\n", currZoneData);


                dataLength += strlen(currZoneData);
                fileData = (char *)realloc(fileData, dataLength);

                strcat(fileData, currZoneData);

                if (verbose == 1) printf("\tzoneIndex: %d, data size: %lu\n", currZoneDataIndex, strlen(fileData));

                // indirectZoneIndex++;
            }

            // char *currZoneData = (char *)(data + (currZoneDataIndex * zonesize));

            // if (verbose == 1) printf("currZoneData: %s\n", currZoneData);
        }
    }

    return fileData;
}

/* Prints program usage information */
void printUsage()
{
    printf("usage: minget [ -v ] [ -p num [ -s num ] ] imagefile srcpath [ dstpath ]\n"
           "Options:\n"
           "-p part    --- select partition for filesystem (default: none)\n"
           "-s sub     --- select subpartition for filesystem (default: none)\n"
           "-h help    --- print usage information and exit\n"
           "-v verbose --- increase verbosity level\n");
}

int main(int argc, char **argv) {

  /* If no arguments specified, print usage */
    if (argc < 3)
    {
        printUsage();
        return 0;
    }

    char *filename = NULL;
    char *srcpath = NULL;
    char *dstpath = NULL;

    char **srcpathlist;

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
        /* Help flag */ // Should we also return right way after printing usage?
        case 'h':
            printUsage();
            break;
        /* Verbose mode enabled */
        case 'v':
            v_flag = 1;
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
        if (verbose == 1) printf("ERROR: image name required.\n");
        printUsage();
        exit(-1);
    }

    optind++;

    /* Get image src path */
    if (optind < argc)
    {
        srcpath = argv[optind];

        srcpathlist = str_split(srcpath, '/');

    }
    else
    {
        if (verbose == 1) printf("ERROR: srcpath required.\n");
        printUsage();
        exit(-1);
    }

    optind++;

    /* Get optional image dst path */
    if (optind < argc)
    {
        dstpath = argv[optind];
    }
    else
    {
        if (verbose == 1) printf("No dst path given, printing to stdout\n");
    }

    if (verbose == 1) printf("srcpath: %s\n", srcpath);
    if (verbose == 1) printf("srcpathlist[0]: %s\n", srcpathlist[0]);

    if (verbose == 1) printf("opening file: %s\n", filename);


    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        if (verbose == 1) printf("ERROR: file not found!\n");
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

    superblock *sb = (superblock *)(data + 1024);

    /* Check magic number to ensure that this is a MINIX filesystem */
    if (sb->magic != 0x4D5A)
    {
        fprintf(stderr, "Bad magic number. (0x%04X)\n", sb->magic);
        fprintf(stderr, "This doesn't look like a MINIX filesystem.\n");
        exit(-1);
    }

    int zonesize = sb->blocksize << sb->log_zone_size;

    if (verbose == 1) printf("zone size: %d\n", zonesize);

    inode *root = (inode *)(data + ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize));

    if (verbose == 1) printf("first inode size: %d\n", root->size);
    if (verbose == 1) printf("first inode mode: %d\n", root->mode);

    // printContents(root, data, zonesize);
    inode *file = findFile(root, srcpathlist, data, zonesize, sb, root);

    if (file == NULL) 
    {
        printf("ERROR: file not found\n");
        return -1;
    }

    if (verbose == 1) printf("\tfile's zone[0]: %d (first datazone: %d)\n", file->zone[0], sb->firstdata);
    if (verbose == 1) printf("\tfile size: %d \n", file->size);

    char *fileData = getFileContents(file, data, verbose);
    // char *fileData = getFileContentsOld(file, data, sb);
    if (fileData == NULL)
    {
        printf("ERROR: fileData not found\n");
        return -1;
    }

    if (dstpath == NULL)
    {
        printf("%s", fileData);
    }
    else 
    {
        FILE *outfile = fopen(dstpath, "w");
        fwrite(fileData, 1, strlen(fileData), fp);
    }


    free(diskStart);


}