#include "minutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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
int printContents(inode *dir, char *path, FILE *image, superblock *sb,
                  int partitionStart, int zonesize, int verbose)
{
    if(isDirectory(dir))
    {
        int containedFiles = dir->size / 64;

        // char *zone1 = data + (dir->zone[0] * zonesize);
        // dirent *contents = (dirent *)zone1;

        printf("%s:\n", path);

        int i;
        for(i = 0; i < containedFiles; i++)
        {
            dirent *current =
                getDirEntByIndex(i, dir, image, sb, partitionStart, verbose);

            if(current->inode != 0)
            {
                printFileInfo(getInode(current->inode, image, sb,
                                       partitionStart, verbose),
                              current->name);
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
    fprintf(
        stderr,
        "usage: minls [ -v ] [ -p num [ -s num ] ] imagefile [ path ]\n"
        "Options:\n"
        "-p part    --- select partition for filesystem (default: none)\n"
        "-s sub     --- select subpartition for filesystem (default: none)\n"
        "-h help    --- print usage information and exit\n"
        "-v verbose --- increase verbosity level\n");
}

int main(int argc, char **argv)
{
    /* If no arguments specified, print usage */
    if(argc < 2)
    {
        printUsage();
        return -1;
    }

    char *filename = NULL;
    char *path = "/";

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
                exit(-1);
            }
            break;
        /* Subpartition is specified */
        case 's':
            if(usePartition == -1)
            {
                fprintf(stderr, "ERROR: cannot set subpartition unless main "
                                "partition is specified.\n");
                return -1;
            }
            useSubpart = atoi(optarg);
            if(useSubpart < 0 || useSubpart > 3)
            {
                fprintf(stderr,
                        "ERROR: subpartition must be in the range 0-3.\n");
                exit(-1);
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
        fprintf(stderr, "ERROR: image name required.\n");
        printUsage();
        exit(-1);
    }
    /* Get path (if specified) */
    if(optind + 1 < argc)
    {
        path = argv[optind + 1];
    }

    /* Open image file descriptor */
    FILE *image = fopen(filename, "rb");
    if(image == NULL)
    {
        fprintf(stderr, "ERROR: file not found!\n");
        return -1;
    }

    /* Find total length of file, in bytes */
    // fseek(file, 0L, SEEK_END);
    // long filesize = ftell(fp);
    // fseek(fp, 0L, SEEK_SET);

    /* Read contents of file into memory */
    // unsigned char *diskStart = malloc(filesize);
    // fread(diskStart, 1, filesize, fp);
    // fclose(fp);

    // unsigned char *data = diskStart;

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

    /* Find superblock */
    // superblock *sb = (superblock *)(data + 1024);

    /* Check magic number to ensure that this is a MINIX filesystem */
    if(sb->magic != 0x4D5A)
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
               sb->ninodes, sb->i_blocks, sb->z_blocks, sb->firstdata,
               sb->log_zone_size, zonesize, sb->max_file, sb->magic, sb->zones,
               sb->blocksize, sb->subversion);
    }

    /* Get inode of file at path specified */
    inode *file = findFile(path, image, sb, partitionStart, verbose);

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
               file->mode, modes, file->links, file->uid, file->gid, file->size,
               file->atime, ctime(&at), file->mtime, ctime(&mt), file->ctime,
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
               file->zone[0], file->zone[1], file->zone[2], file->zone[3],
               file->zone[4], file->zone[5], file->zone[6], file->indirect,
               file->two_indirect);
    }

    /* Make sure file was found */
    if(!file)
    {
        fprintf(stderr, "ERROR: file not found!\n");
        return -1;
    }

    /* Print info about file, or directory contents */
    printContents(file, path, image, sb, partitionStart, zonesize, verbose);

    free(sb);
    free(file);
    fclose(image);
}
