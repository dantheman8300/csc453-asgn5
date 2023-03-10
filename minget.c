#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


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

inode *findFile(inode *dir, char **srcpathlist, char *data, int zonesize, superblock *sb, inode *root)
{

    if (srcpathlist[0] == NULL) {
        printf("srcpathlist is empty\n");
        return NULL;
    }

    printf("%s\n", srcpathlist[0]);


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
                // printf("file: %s, fd: %d\n", current->name, current->inode);

                if (strcmp(current->name, srcpathlist[0]) == 0) 
                {
                    printf("\tthis is the matching name\n");
                    printf("\tinode: %d\n", current->inode);
                    inode *foundFile = root + current->inode - 1;
                    printf("\tfirst inode size: %d\n", foundFile->size);
                    printf("\tfirst inode mode: %d\n", foundFile->mode);
                    
                    if ((foundFile->mode & 0170000) == 040000) 
                    {
                        return findFile(foundFile, srcpathlist + 1, data, zonesize, sb, root);
                    } 
                    else 
                    {
                        if (srcpathlist[1] == NULL ) 
                        {
                            printf("**found file: %s\n", current->name);
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

char *getFileContents(inode *file, char* data, superblock* sb)
{
    char *fileData;
    int dataLength = 0;
    fileData = (char *)malloc(0);

    int zoneIndex = 0;

    while (zoneIndex < 7) 
    {

        // zone is a hole
        if (file->zone[zoneIndex] == 0) 
        {
            zoneIndex ++;
            continue;
        }

        char *currZoneData = (char *)(data + (file->zone[zoneIndex] * sb->blocksize));

        dataLength += strlen(currZoneData);
        fileData = (char *)realloc(fileData, dataLength);

        strcat(fileData, (char *)(data + (file->zone[zoneIndex] * sb->blocksize)));
        printf("zoneIndex: %d, data size: %d\n", zoneIndex, strlen(fileData));
        zoneIndex++;
    }

    // if (zoneIndex == 7) 
    // {
    //     int zoneIndirect = file->indirect;
    //     printf("indirect: %d\n", zoneIndirect);

    //     char *currZoneData = (char *)(data + (zoneIndirect * sb->blocksize));
    //     printf("%s", currZoneData);
    // }

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

    optind++;

    /* Get image src path */
    if (optind < argc)
    {
        srcpath = argv[optind];

        srcpathlist = str_split(srcpath, '/');

    }
    else
    {
        printf("ERROR: srcpath required.\n");
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
        printf("No dst path given, printing to stdout\n");
    }

    printf("srcpath: %s\n", srcpath);
    printf("srcpathlist[0]: %s\n", srcpathlist[0]);

    printf("opening file: %s\n", filename);


    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
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

    superblock *sb = (superblock *)(data + 1024);

    int zonesize = sb->blocksize << sb->log_zone_size;

    printf("zone size: %d\n", zonesize);

    inode *root = (inode *)(data + ((2 + sb->i_blocks + sb->z_blocks) * sb->blocksize));

    printf("first inode size: %d\n", root->size);
    printf("first inode mode: %d\n", root->mode);

    // printContents(root, data, zonesize);
    inode *file = findFile(root, srcpathlist, data, zonesize, sb, root);

    printf("\tfile's zone[0]: %d (first datazone: %d)\n", file->zone[0], sb->firstdata);
    printf("\tfile size: %d \n", file->size);

    char *fileData = getFileContents(file, data, sb);
    // printf("fileData: %s\n", fileData);

    if (dstpath == NULL)
    {
        printf("%s", fileData);
    }
    else 
    {
        FILE *outfile = fopen(dstpath, "w");
        fwrite(fileData, 1, strlen(fileData), fp);
    }


    free(data);


}