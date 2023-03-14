#include "minutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Prints program usage information */
void printUsage()
{
    fprintf(
        stderr,
        "usage: minget [ -v ] [ -p num [ -s num ] ]"
        " imagefile srcpath [ dstpath ]\n"
        "Options:\n"
        "-p part    --- select partition for filesystem (default: none)\n"
        "-s sub     --- select subpartition for filesystem (default: none)\n"
        "-h help    --- print usage information and exit\n"
        "-v verbose --- increase verbosity level\n");
}

int main(int argc, char **argv)
{
    /* If no arguments specified, print usage */
    if(argc < 3)
    {
        printUsage();
        return -1;
    }

    char *filename = NULL;
    char *srcpath = NULL;
    char *dstpath = NULL;

    int verbose = 0;
    int usePartition = -1;
    int useSubpart = -1;

    /* Loop through argument flags */
    int c;
    while((c = getopt(argc, argv, "hvp:s:")) != -1)
    {
        switch(c)
        {
        /* Partition is specified */
        case 'p':
            usePartition = atoi(optarg);
            if(usePartition < 0 || usePartition > 3)
            {
                fprintf(stderr, "ERROR: partition must be in the range 0-3.\n");
                return -1;
            }
            break;
        /* Subpartition is specified */
        case 's':
            if(usePartition == -1)
            {
                fprintf(stderr, "ERROR: cannot set subpartition"
                                " unless main partition is specified.\n");
                return -1;
            }
            useSubpart = atoi(optarg);
            if(useSubpart < 0 || useSubpart > 3)
            {
                fprintf(stderr,
                        "ERROR: subpartition must be in the range 0-3.\n");
                return -1;
            }
            break;
        /* Help flag */
        case 'h':
            printUsage();
            return -1;
        /* Verbose mode enabled */
        case 'v':
            verbose = 1;
            break;
        }
    }

    /* Get image filename */
    if(optind < argc)
    {
        filename = argv[optind];
    }
    else
    {
        if(verbose == 1)
            fprintf(stderr, "ERROR: image name required.\n");
        printUsage();
        return -1;
    }

    optind++;

    /* Get image src path */
    if(optind < argc)
    {
        srcpath = argv[optind];
    }
    else
    {
        if(verbose == 1)
            fprintf(stderr, "ERROR: srcpath required.\n");
        printUsage();
        return -1;
    }

    optind++;

    /* Get optional image dst path */
    if(optind < argc)
    {
        dstpath = argv[optind];
    }
    else
    {
        if(verbose == 1)
            printf("No dst path given, printing to stdout\n");
    }

    if(verbose == 1)
    {
        printf("srcpath: %s\n", srcpath);
        printf("opening file: %s\n", filename);
    }

    /* Open image file */
    FILE *image = fopen(filename, "rb");
    if(image == NULL)
    {
        if(verbose == 1)
            fprintf(stderr, "ERROR: file not found!\n");
        return -1;
    }

    int partitionStart = 0;

    /* If a partition was specified */
    if(usePartition != -1)
    {
        /* Switch to specified partition */
        partitionStart = enterPartition(image, partitionStart, usePartition);

        /* If subpartition was specified */
        if(useSubpart != -1)
        {
            /* Switch to subpartition */
            partitionStart = enterPartition(image, partitionStart, useSubpart);
        }
    }

    /* Get superblock */
    superblock *sb =
        (superblock *)getData(1024, sizeof(superblock), image, partitionStart);

    /* Check magic number to ensure that this is a MINIX filesystem */
    if(sb->magic != 0x4D5A)
    {
        fprintf(stderr, "Bad magic number. (0x%04X)\n", sb->magic);
        fprintf(stderr, "This doesn't look like a MINIX filesystem.\n");
        return -1;
    }

    int zonesize = sb->blocksize << sb->log_zone_size;

    if(verbose == 1)
        printf("zone size: %d\n", zonesize);

    inode *file = findFile(srcpath, image, sb, partitionStart, verbose);

    if(file == NULL)
    {
        fprintf(stderr, "ERROR: file not found\n");
        return -1;
    }
    else if(!isRegularFile(file))
    {
        fprintf(stderr, "ERROR: not a regular file!\n");
        return -1;
    }

    if(verbose == 1)
    {
        printf("\tfile's zone[0]: %d (first datazone: %d)\n", file->zone[0],
               sb->firstdata);
        printf("\tfile size: %d \n", file->size);
    }

    char *fileData = getFileContents(file, image, sb, partitionStart, verbose);
    if(fileData == NULL)
    {
        fprintf(stderr, "ERROR: fileData not found!\n");
        return -1;
    }

    /* No output path specified; print to stdout */
    if(dstpath == NULL)
    {
        int bytesWritten = fwrite(fileData, 1, file->size, stdout);
        if(bytesWritten != file->size)
        {
            fprintf(stderr, "ERROR: could not write all data to stdout!\n");
            return -1;
        }
    }
    /* Output path specified; write results to file */
    else
    {
        FILE *outfile = fopen(dstpath, "wb");
        if(outfile == NULL)
        {
            fprintf(stderr, "ERROR: could not create/open output file!\n");
            return -1;
        }

        int bytesWritten = fwrite(fileData, 1, file->size, outfile);
        if(bytesWritten != file->size)
        {
            fprintf(stderr, "ERROR: could not write all data to file!\n");
            return -1;
        }

        int status = fclose(outfile);
        if(status != 0)
            fprintf(stderr, "WARNING: could not close output file!\n");
    }

    free(sb);
    free(file);
    free(fileData);
    fclose(image);
}
