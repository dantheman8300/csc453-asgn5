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

    //TODO: handle partitions

    // uint8_t test = *data;
    // printf("%d\n", test);

    // partition * firstPart = (partition *) (data + 0x1BE);

    // printf("type: %x\n", firstPart->type);

    superblock *sb = (superblock *)(data + 1024);

    // printf("inodes: %d\n", sb->ninodes);

    int zonesize = sb->blocksize << sb->log_zone_size;

    // printf("zone size: %d\n", zonesize);

    inode *root = getInode(1, data); //(inode *)(data + ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize));

    // printf("first inode size: %d\n", root->size);
    // printContents(root, "/", data, zonesize);

    inode *file = findFile(path, data);

    if (!file)
    {
        printf("ERROR: file not found!\n");
        return -1;
    }

    printContents(file, path, data, zonesize);

    free(data);
}