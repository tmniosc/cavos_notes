/* vfs.h — đĩa (MBR), mount point, FAT32 và ext2 chỉ-đọc, theo cavOS (Bài 15):
 *   drivers/disk.c        openDisk, validateMbr, getDiskBytes
 *   filesystems/vfs/      fsMount (isFat / isExt2), fsDetermineMountPoint
 *   filesystems/fat32/    fat32Mount, fat32FATtraverse, duyệt thư mục
 *   filesystems/ext2/     ext2Mount, ext2InodeFetch, ext2BlockFetch, ext2Traverse
 */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t  status;           /* 0x80 = bootable */
    uint8_t  chsFirst[3];
    uint8_t  type;             /* 0x0C FAT32 LBA, 0x83 Linux */
    uint8_t  chsLast[3];
    uint32_t lba_first_sector;
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition;

int  openDisk(uint32_t disk, uint8_t partition, mbr_partition *out);
void getDiskBytes(uint8_t *dst, uint64_t lba, uint32_t count);
void diskDumpMbr(void);

typedef enum { FS_NONE, FS_FATFS, FS_EXT2 } FS_TYPE;

/* FAT32 (cavOS FAT32) */
typedef struct {
    uint64_t offsetBase, offsetFats, offsetClusters;   /* LBA */
    uint32_t sectorsPerCluster, rootCluster, fatSize, reserved;
    uint8_t  fats;
} FAT32;

/* ext2 (cavOS Ext2) */
typedef struct {
    uint64_t offsetBase;                    /* LBA đầu partition */
    uint32_t blockSize, inodeSize, inodesPerGroup, blocksPerGroup, blockGroups;
    uint32_t totalInodes, totalBlocks, firstDataBlock, incompat;
    uint32_t inodeTable[64];                /* từ BGDT: block của bảng inode mỗi nhóm */
} Ext2;

typedef struct {
    char          prefix[16];
    uint8_t       partition;
    mbr_partition mbr;
    FS_TYPE       filesystem;
    FAT32         fat;
    Ext2          ext2;
} MountPoint;

MountPoint *fsMount(const char *prefix, uint32_t disk, uint8_t partition);
MountPoint *fsDetermineMountPoint(const char *path);
/* đọc cả file vào buf (tối đa max byte); trả số byte, -1 nếu không thấy */
long fsReadFile(const char *path, uint8_t *buf, uint64_t max);
/* in nội dung một thư mục */
int  fsListDir(const char *path);
extern uint64_t ext2IndirectReads;
