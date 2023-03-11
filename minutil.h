
#define DIRECT_ZONES 7

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

/* Gets a data zone from an inode at a given index (starting from zero) */
char *getZoneByIndex(int index, inode *file, char *data, int verbose)
{

    if (verbose == 1) printf("getZoneByIndex: %d\n", index);

    superblock *sb = (superblock *)(data + 1024);
    int blocksize = sb->blocksize;
    int zonesize = blocksize << sb->log_zone_size;

    /* Target is a direct zone */
    if(index < 7)
    {
        if (verbose == 1) printf("\tzone: %d\n", file->zone[index]);

        if (verbose == 1) printf("\tzone data: %s\n", data + (file->zone[index] * zonesize));

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
            if(indirectZone[indirectIndex] == 0)
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
        //             (uint32_t *)(data + (doubleIndirectZone[doubleIndirectIndex] * zonesize));

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

    if (verbose == 1) printf("fileData size: %d\n", file->size + 1);

    if(fileData == NULL)
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
    for(i = 0; i < totalZones; i++)
    {
        /* Retrieve the target zone */
        char *zoneData = getZoneByIndex(i, file, data, verbose);

        /* If it's not a hole, copy it to the buffer */
        if(zoneData != NULL)
        {

            // if (verbose == 1) printf("zoneData: %s\n", zoneData);

            int bytesToCopy = zonesize;

            /* If this is the last zone, only copy the relevant portion */
            if(i == totalZones - 1)
                bytesToCopy = file->size % zonesize;

            if (verbose == 1) printf("bytesToCopy: %d\n", bytesToCopy);

            memcpy(fileData + (i * zonesize), zoneData, bytesToCopy);

            if (verbose == 1) printf("fileData: %s\n", fileData);
        }
    }
    fileData[file->size] = '\0';

    if (verbose == 1) printf("fileData: %s\n", fileData);

    return fileData;
}