/* vfs.c — MBR + mount point + FAT32 + ext2 (chỉ đọc), theo cavOS (Bài 15). Xem vfs.h. */
#include "vfs.h"
#include "ahci.h"
#include "boot.h"
#include "serial.h"

uint64_t ext2IndirectReads;

/* ------------------------------------------------------------ tiện ích */
static uint64_t slen(const char *s) { uint64_t n = 0; while (s[n]) n++; return n; }
static int meq(const void *a, const void *b, uint64_t n) {
    const uint8_t *x = a, *y = b;
    for (uint64_t i = 0; i < n; i++) if (x[i] != y[i]) return 0;
    return 1;
}
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static char lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int ieq(const char *a, const char *b, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) if (lower(a[i]) != lower(b[i])) return 0;
    return 1;
}
static void hex(uint64_t v) { serial_puthex_short(v); }

/* ------------------------------------------------------------ đĩa: drivers/disk.c */
/* = cavOS diskBytes: ổ SATA ĐẦU TIÊN của controller AHCI đầu tiên. Chia thành lệnh <= 56
 * sector (28 KiB): buffer lệch trang vẫn vừa 8 entry PRDT mỗi trang một entry. */
void getDiskBytes(uint8_t *dst, uint64_t lba, uint32_t count) {
    ahci *a = &ahciInfo;
    if (!a->sata) {
        memset(dst, 0, (uint64_t)count * SECTOR_SIZE);
        return;
    }
    int pos = 0;
    while (!(a->sata & (1u << pos))) pos++;
    while (count) {
        uint32_t n = count > 56 ? 56 : count;
        ahciRead(a, pos, lba, n, dst);
        dst += (uint64_t)n * SECTOR_SIZE;
        lba += n;
        count -= n;
    }
}

static uint8_t sector[SECTOR_SIZE] __attribute__((aligned(4096)));

int validateMbr(const uint8_t *s) { return s[510] == 0x55 && s[511] == 0xAA; }

int openDisk(uint32_t disk, uint8_t partition, mbr_partition *out) {
    (void)disk;
    getDiskBytes(sector, 0, 1);
    if (!validateMbr(sector))
        return 0;
    memcpy(out, sector + 0x1BE + 16 * partition, sizeof(mbr_partition));
    return 1;
}

void diskDumpMbr(void) {
    getDiskBytes(sector, 0, 1);
    serial_puts("[disk] LBA 0 (MBR): signature ");
    hex(sector[510]);
    serial_putc(' ');
    hex(sector[511]);
    serial_puts(validateMbr(sector) ? " -> valid\n" : " -> INVALID\n");
    for (int i = 0; i < 4; i++) {
        mbr_partition *p = (mbr_partition *)(sector + 0x1BE + 16 * i);
        if (!p->type) continue;
        serial_puts("[disk]   partition ");
        serial_putdec(i);
        serial_puts(": type ");
        hex(p->type);
        serial_puts(p->status == 0x80 ? " (bootable)" : "           ");
        serial_puts("  first LBA ");
        serial_putdec(p->lba_first_sector);
        serial_puts("  sectors ");
        serial_putdec(p->sector_count);
        serial_puts(" (");
        serial_putdec(p->sector_count / 2048);
        serial_puts(" MiB)\n");
    }
}

/* ------------------------------------------------------------ FAT32 */
static uint8_t clusterBuf[64 * 1024] __attribute__((aligned(4096)));

static uint64_t clusterLba(FAT32 *f, uint32_t c) {
    return f->offsetClusters + (uint64_t)(c - 2) * f->sectorsPerCluster;
}

/* = cavOS fat32FATtraverse: ô thứ c của bảng FAT là cluster kế tiếp; >= 0x0FFFFFF8 = hết */
static uint32_t fat32FATtraverse(FAT32 *f, uint32_t c) {
    static uint8_t fs[SECTOR_SIZE] __attribute__((aligned(512)));
    uint32_t off = c * 4;
    getDiskBytes(fs, f->offsetFats + off / SECTOR_SIZE, 1);
    uint32_t next = rd32(fs + off % SECTOR_SIZE) & 0x0FFFFFFF;
    return (next >= 0x0FFFFFF8 || next == 0x0FFFFFF7) ? 0 : next;
}

static int fat32Mount(MountPoint *m) {
    FAT32 *f = &m->fat;
    f->offsetBase = m->mbr.lba_first_sector;
    getDiskBytes(sector, f->offsetBase, 1);
    if (rd16(sector + 11) != SECTOR_SIZE) return 0;
    f->sectorsPerCluster = sector[13];
    f->reserved = rd16(sector + 14);
    f->fats = sector[16];
    f->fatSize = rd32(sector + 36);
    f->rootCluster = rd32(sector + 44);
    f->offsetFats = f->offsetBase + f->reserved;
    f->offsetClusters = f->offsetFats + (uint64_t)f->fats * f->fatSize;
    char label[12];
    memcpy(label, sector + 71, 11);
    label[11] = 0;
    serial_puts("[fat32] boot sector at LBA ");
    serial_putdec(f->offsetBase);
    serial_puts(": label \"");
    serial_puts(label);
    serial_puts("\", ");
    serial_putdec(f->sectorsPerCluster);
    serial_puts(" sectors/cluster, ");
    serial_putdec(f->reserved);
    serial_puts(" reserved, ");
    serial_putdec(f->fats);
    serial_puts(" FATs x ");
    serial_putdec(f->fatSize);
    serial_puts(" sectors, root cluster ");
    serial_putdec(f->rootCluster);
    serial_puts("\n[fat32]   -> FAT at LBA ");
    serial_putdec(f->offsetFats);
    serial_puts(", cluster 2 at LBA ");
    serial_putdec(f->offsetClusters);
    serial_putc('\n');
    return 1;
}

typedef struct { char name[64]; uint32_t cluster, size; uint8_t attr; } FatEntry;

/* gọi cb cho mỗi mục của thư mục bắt đầu ở cluster `dir`; tên dài (LFN) ghép từ các mục 0x0F */
static int fat32Walk(FAT32 *f, uint32_t dir, int (*cb)(FatEntry *, void *), void *ctx) {
    char lfn[64];
    int lfnLen = 0;
    uint32_t bytes = f->sectorsPerCluster * SECTOR_SIZE;
    for (uint32_t c = dir; c; c = fat32FATtraverse(f, c)) {
        getDiskBytes(clusterBuf, clusterLba(f, c), f->sectorsPerCluster);
        for (uint32_t o = 0; o < bytes; o += 32) {
            uint8_t *e = clusterBuf + o;
            if (e[0] == 0x00) return 0;                 /* hết thư mục */
            if (e[0] == 0xE5) { lfnLen = 0; continue; } /* mục đã xoá */
            if (e[11] == 0x0F) {                        /* một mảnh tên dài: 13 ký tự UTF-16 */
                int seq = (e[0] & 0x1F) - 1;
                static const uint8_t at[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
                for (int k = 0; k < 13; k++) {
                    int idx = seq * 13 + k;
                    uint16_t ch = rd16(e + at[k]);
                    if (ch == 0 || ch == 0xFFFF) {          /* hết tên */
                        if (e[0] & 0x40) lfnLen = idx;
                        break;
                    }
                    if (idx < 63) lfn[idx] = (char)ch;      /* lab: chỉ giữ ASCII */
                    if (k == 12 && (e[0] & 0x40)) lfnLen = idx + 1;
                }
                if (lfnLen > 63) lfnLen = 63;
                continue;
            }
            if (e[11] & 0x08) { lfnLen = 0; continue; } /* nhãn ổ */
            FatEntry fe;
            if (lfnLen) {
                memcpy(fe.name, lfn, lfnLen);
                fe.name[lfnLen] = 0;
            } else {                                    /* tên 8.3: "KERNEL  BIN" -> KERNEL.BIN */
                int n = 0;
                for (int k = 0; k < 8 && e[k] != ' '; k++) fe.name[n++] = e[k];
                if (e[8] != ' ') { fe.name[n++] = '.'; for (int k = 8; k < 11 && e[k] != ' '; k++) fe.name[n++] = e[k]; }
                fe.name[n] = 0;
            }
            lfnLen = 0;
            fe.attr = e[11];
            fe.cluster = ((uint32_t)rd16(e + 20) << 16) | rd16(e + 26);
            fe.size = rd32(e + 28);
            if (cb(&fe, ctx)) return 1;
        }
    }
    return 0;
}

typedef struct { const char *name; uint64_t len; FatEntry found; int ok; } FatFind;
static int fatFindCb(FatEntry *e, void *ctx) {
    FatFind *f = ctx;
    if (slen(e->name) == f->len && ieq(e->name, f->name, f->len)) { f->found = *e; f->ok = 1; return 1; }
    return 0;
}

/* đi theo đường dẫn "a/b/c" từ thư mục gốc; trả mục cuối */
static int fat32Lookup(FAT32 *f, const char *path, FatEntry *out) {
    FatEntry cur = {.cluster = f->rootCluster, .attr = 0x10};
    while (*path == '/') path++;
    while (*path) {
        uint64_t n = 0;
        while (path[n] && path[n] != '/') n++;
        FatFind ff = {.name = path, .len = n};
        fat32Walk(f, cur.cluster, fatFindCb, &ff);
        if (!ff.ok) return 0;
        cur = ff.found;
        path += n;
        while (*path == '/') path++;
    }
    *out = cur;
    return 1;
}

static int fatListCb(FatEntry *e, void *ctx) {
    (void)ctx;
    if (e->name[0] == '.') return 0;
    serial_puts("    ");
    serial_puts_pad(e->name, 22);
    serial_puts((e->attr & 0x10) ? "<DIR>       " : "file  ");
    if (!(e->attr & 0x10)) { serial_putdec_right(e->size, 8); serial_puts(" B  "); }
    serial_puts("first cluster ");
    serial_putdec(e->cluster);
    serial_putc('\n');
    return 0;
}

static long fat32Read(FAT32 *f, const char *path, uint8_t *buf, uint64_t max) {
    FatEntry e;
    if (!fat32Lookup(f, path, &e) || (e.attr & 0x10)) return -1;
    uint64_t done = 0, bytes = f->sectorsPerCluster * SECTOR_SIZE;
    uint32_t clusters = 0;
    for (uint32_t c = e.cluster; c && done < e.size && done < max; c = fat32FATtraverse(f, c)) {
        getDiskBytes(clusterBuf, clusterLba(f, c), f->sectorsPerCluster);
        uint64_t n = e.size - done;
        if (n > bytes) n = bytes;
        if (done + n > max) n = max - done;
        memcpy(buf + done, clusterBuf, n);
        done += n;
        clusters++;
    }
    serial_puts("[fat32]   ");
    serial_puts(path);
    serial_puts(": ");
    serial_putdec(e.size);
    serial_puts(" B in a chain of ");
    serial_putdec(clusters);
    serial_puts(" clusters starting at ");
    serial_putdec(e.cluster);
    serial_putc('\n');
    return (long)done;
}

/* ------------------------------------------------------------ ext2 */
static uint8_t blockBuf[4096] __attribute__((aligned(4096)));
static uint32_t ind1[1024] __attribute__((aligned(4096)));
static uint32_t ind2[1024] __attribute__((aligned(4096)));
static uint64_t ind1Block = (uint64_t)-1, ind2Block = (uint64_t)-1;

#define BLOCK_TO_LBA(e, b) ((e)->offsetBase + (uint64_t)(b) * (e)->blockSize / SECTOR_SIZE)

static void readBlock(Ext2 *e, uint32_t b, void *dst) { getDiskBytes(dst, BLOCK_TO_LBA(e, b), e->blockSize / SECTOR_SIZE); }

static int ext2Mount(MountPoint *m) {
    Ext2 *e = &m->ext2;
    e->offsetBase = m->mbr.lba_first_sector;
    static uint8_t sb[1024] __attribute__((aligned(4096)));
    getDiskBytes(sb, e->offsetBase + 2, 2);           /* superblock: byte 1024..2047 */
    if (rd16(sb + 56) != 0xEF53) return 0;
    e->totalInodes = rd32(sb + 0);
    e->totalBlocks = rd32(sb + 4);
    e->firstDataBlock = rd32(sb + 20);
    e->blockSize = 1024u << rd32(sb + 24);
    e->blocksPerGroup = rd32(sb + 32);
    e->inodesPerGroup = rd32(sb + 40);
    e->inodeSize = rd32(sb + 76) >= 1 ? rd16(sb + 88) : 128;
    e->incompat = rd32(sb + 96);
    e->blockGroups = DivRoundUp(e->totalBlocks - e->firstDataBlock, e->blocksPerGroup);
    char name[17];
    memcpy(name, sb + 120, 16);
    name[16] = 0;
    serial_puts("[ext2] superblock at LBA ");
    serial_putdec(e->offsetBase + 2);
    serial_puts(": magic ");
    hex(rd16(sb + 56));
    serial_puts(", volume \"");
    serial_puts(name);
    serial_puts("\", block size ");
    serial_putdec(e->blockSize);
    serial_puts(", ");
    serial_putdec(e->totalBlocks);
    serial_puts(" blocks, ");
    serial_putdec(e->totalInodes);
    serial_puts(" inodes, ");
    serial_putdec(e->blockGroups);
    serial_puts(" block groups (");
    serial_putdec(e->blocksPerGroup);
    serial_puts(" blocks, ");
    serial_putdec(e->inodesPerGroup);
    serial_puts(" inodes each), inode size ");
    serial_putdec(e->inodeSize);
    serial_puts(", incompat features ");
    hex(e->incompat);
    serial_puts(e->incompat == 2 ? " (= FILETYPE only: cavOS accepts it)\n" : "\n");

    /* BGDT: block ngay sau block chứa superblock */
    readBlock(e, e->firstDataBlock + 1, blockBuf);
    for (uint32_t g = 0; g < e->blockGroups && g < 64; g++)
        e->inodeTable[g] = rd32(blockBuf + 32 * g + 8);
    serial_puts("[ext2]   BGDT at block ");
    serial_putdec(e->firstDataBlock + 1);
    serial_puts(": inode table of group 0 at block ");
    serial_putdec(e->inodeTable[0]);
    if (e->blockGroups > 1) { serial_puts(", group 1 at block "); serial_putdec(e->inodeTable[1]); }
    serial_putc('\n');
    return 1;
}

typedef struct { uint16_t mode; uint32_t size; uint32_t block[15]; } Ext2Inode;

/* = cavOS ext2InodeFetch: nhóm = (n-1) / inodesPerGroup, chỉ số = (n-1) % inodesPerGroup */
static void ext2InodeFetch(Ext2 *e, uint32_t n, Ext2Inode *out) {
    uint32_t group = (n - 1) / e->inodesPerGroup, index = (n - 1) % e->inodesPerGroup;
    uint64_t byte = (uint64_t)index * e->inodeSize;
    uint64_t lba = BLOCK_TO_LBA(e, e->inodeTable[group]) + byte / SECTOR_SIZE;
    getDiskBytes(sector, lba, 1);
    uint8_t *p = sector + byte % SECTOR_SIZE;
    out->mode = rd16(p + 0);
    out->size = rd32(p + 4);
    for (int i = 0; i < 15; i++) out->block[i] = rd32(p + 40 + 4 * i);
}

/* = cavOS ext2BlockFetch: block thứ `curr` của file. 0..11 trực tiếp; rồi block[12] trỏ tới
 * một block đầy số block (gián tiếp 1 cấp); block[13] gián tiếp 2 cấp. */
static uint32_t ext2BlockFetch(Ext2 *e, Ext2Inode *ino, uint64_t curr) {
    uint64_t per = e->blockSize / 4;
    if (curr < 12) return ino->block[curr];
    curr -= 12;
    if (curr < per) {
        if (!ino->block[12]) return 0;
        if (ind1Block != ino->block[12]) { readBlock(e, ino->block[12], ind1); ind1Block = ino->block[12]; ext2IndirectReads++; }
        return ind1[curr];
    }
    curr -= per;
    if (curr < per * per) {
        if (!ino->block[13]) return 0;
        if (ind1Block != ino->block[13]) { readBlock(e, ino->block[13], ind1); ind1Block = ino->block[13]; ext2IndirectReads++; }
        uint32_t mid = ind1[curr / per];
        if (!mid) return 0;
        if (ind2Block != mid) { readBlock(e, mid, ind2); ind2Block = mid; ext2IndirectReads++; }
        return ind2[curr % per];
    }
    return 0;                                         /* gián tiếp 3 cấp: lab không cần */
}

typedef int (*Ext2DirCb)(uint32_t inode, uint8_t type, const char *name, uint8_t len, void *ctx);

static int ext2WalkDir(Ext2 *e, uint32_t dirInode, Ext2DirCb cb, void *ctx) {
    Ext2Inode ino;
    ext2InodeFetch(e, dirInode, &ino);
    static uint8_t names[4096] __attribute__((aligned(4096)));
    uint64_t blocks = DivRoundUp(ino.size, e->blockSize);
    for (uint64_t i = 0; i < blocks; i++) {
        uint32_t b = ext2BlockFetch(e, &ino, i);
        if (!b) break;
        readBlock(e, b, names);
        for (uint32_t o = 0; o < e->blockSize;) {
            uint8_t *d = names + o;
            uint32_t inode = rd32(d);
            uint16_t recLen = rd16(d + 4);
            if (recLen == 0) break;
            if (inode && cb(inode, d[7], (const char *)d + 8, d[6], ctx)) return 1;
            o += recLen;
        }
    }
    return 0;
}

typedef struct { const char *name; uint64_t len; uint32_t inode; } Ext2Find;
static int ext2FindCb(uint32_t inode, uint8_t type, const char *name, uint8_t len, void *ctx) {
    (void)type;
    Ext2Find *f = ctx;
    if (len == f->len && meq(name, f->name, len)) { f->inode = inode; return 1; }
    return 0;
}

/* = cavOS ext2TraversePath: từ inode 2 (thư mục gốc), mỗi thành phần một lần ext2Traverse */
static uint32_t ext2TraversePath(Ext2 *e, const char *path) {
    uint32_t cur = 2;
    while (*path == '/') path++;
    while (*path) {
        uint64_t n = 0;
        while (path[n] && path[n] != '/') n++;
        Ext2Find f = {.name = path, .len = n};
        ext2WalkDir(e, cur, ext2FindCb, &f);
        if (!f.inode) return 0;
        cur = f.inode;
        path += n;
        while (*path == '/') path++;
    }
    return cur;
}

static int ext2ListCb(uint32_t inode, uint8_t type, const char *name, uint8_t len, void *ctx) {
    Ext2 *e = ctx;
    char nm[64];
    if (len > 63) len = 63;
    memcpy(nm, name, len);
    nm[len] = 0;
    Ext2Inode ino;
    ext2InodeFetch(e, inode, &ino);
    serial_puts("    ");
    serial_puts_pad(nm, 22);
    serial_puts("inode ");
    serial_putdec_right(inode, 4);
    serial_puts("  type ");
    serial_putdec(type);
    serial_puts(type == 2 ? " (dir) " : type == 1 ? " (file)" : type == 7 ? " (link)" : "       ");
    serial_puts("  mode ");
    hex(ino.mode);
    serial_puts("  size ");
    serial_putdec(ino.size);
    serial_putc('\n');
    return 0;
}

static long ext2Read(Ext2 *e, const char *path, uint8_t *buf, uint64_t max) {
    uint32_t n = ext2TraversePath(e, path);
    if (!n) return -1;
    Ext2Inode ino;
    ext2InodeFetch(e, n, &ino);
    uint64_t done = 0, blocks = 0;
    uint64_t ind0 = ext2IndirectReads;
    for (uint64_t i = 0; done < ino.size && done < max; i++) {
        uint32_t b = ext2BlockFetch(e, &ino, i);
        uint64_t len = ino.size - done;
        if (len > e->blockSize) len = e->blockSize;
        if (done + len > max) len = max - done;
        if (b) { readBlock(e, b, blockBuf); memcpy(buf + done, blockBuf, len); }
        else memset(buf + done, 0, len);              /* lỗ (sparse) */
        done += len;
        blocks++;
    }
    serial_puts("[ext2]   ");
    serial_puts(path);
    serial_puts(": inode ");
    serial_putdec(n);
    serial_puts(", ");
    serial_putdec(ino.size);
    serial_puts(" B = ");
    serial_putdec(blocks);
    serial_puts(" blocks; block[0..11] = ");
    for (int i = 0; i < 12 && ino.block[i]; i++) { serial_putdec(ino.block[i]); serial_putc(i < 11 && ino.block[i + 1] ? ',' : ' '); }
    serial_puts("block[12] (single indirect) = ");
    serial_putdec(ino.block[12]);
    serial_puts(", block[13] (double) = ");
    serial_putdec(ino.block[13]);
    serial_puts("; indirect blocks read: ");
    serial_putdec(ext2IndirectReads - ind0);
    serial_putc('\n');
    return (long)done;
}

/* ------------------------------------------------------------ VFS: filesystems/vfs/vfs_mnt.c */
static MountPoint mounts[4];
static int nmounts;

/* = cavOS isFat: byte 66 của boot sector = 0x28/0x29 (chữ ký BPB mở rộng của FAT32) */
static int isFat(mbr_partition *p) { getDiskBytes(sector, p->lba_first_sector, 1); return sector[66] == 0x28 || sector[66] == 0x29; }
/* = cavOS isExt2: chỉ nhìn loại partition trong MBR */
static int isExt2(mbr_partition *p) { return p->type == 0x83; }

MountPoint *fsMount(const char *prefix, uint32_t disk, uint8_t partition) {
    if (nmounts >= 4) return 0;
    MountPoint *m = &mounts[nmounts];
    memset(m, 0, sizeof(*m));
    memcpy(m->prefix, prefix, slen(prefix) + 1);
    m->partition = partition;
    serial_puts("[vfs] fsMount(\"");
    serial_puts(prefix);
    serial_puts("\", CONNECTOR_AHCI, ");
    serial_putdec(disk);
    serial_puts(", ");
    serial_putdec(partition);
    serial_puts(")\n");
    if (!openDisk(disk, partition, &m->mbr) || !m->mbr.type) {
        serial_puts("[vfs]   no such partition\n");
        return 0;
    }
    int ok = 0;
    if (isFat(&m->mbr)) { m->filesystem = FS_FATFS; ok = fat32Mount(m); }
    else if (isExt2(&m->mbr)) { m->filesystem = FS_EXT2; ok = ext2Mount(m); }
    serial_puts(ok ? (m->filesystem == FS_FATFS ? "[vfs]   mounted as FAT32\n" : "[vfs]   mounted as ext2\n")
                   : "[vfs]   unknown filesystem, not mounted\n");
    if (!ok) return 0;
    nmounts++;
    return m;
}

/* = cavOS fsDetermineMountPoint: prefix dài nhất khớp với đầu đường dẫn */
MountPoint *fsDetermineMountPoint(const char *path) {
    MountPoint *best = 0;
    uint64_t bestLen = 0;
    for (int i = 0; i < nmounts; i++) {
        uint64_t len = slen(mounts[i].prefix) - 1;        /* bỏ '/' cuối */
        if (len >= bestLen && meq(path, mounts[i].prefix, len) && (path[len] == '/' || path[len] == 0)) {
            best = &mounts[i];
            bestLen = len;
        }
    }
    return best;
}

long fsReadFile(const char *path, uint8_t *buf, uint64_t max) {
    MountPoint *m = fsDetermineMountPoint(path);
    if (!m) return -1;
    const char *inner = path + slen(m->prefix) - 1;
    return m->filesystem == FS_FATFS ? fat32Read(&m->fat, inner, buf, max) : ext2Read(&m->ext2, inner, buf, max);
}

int fsListDir(const char *path) {
    MountPoint *m = fsDetermineMountPoint(path);
    if (!m) return 0;
    const char *inner = path + slen(m->prefix) - 1;
    serial_puts("[vfs] ls ");
    serial_puts(path);
    serial_puts("  (mount \"");
    serial_puts(m->prefix);
    serial_puts("\", path inside it \"");
    serial_puts(*inner ? inner : "/");
    serial_puts("\")\n");
    if (m->filesystem == FS_FATFS) {
        FatEntry e;
        if (!fat32Lookup(&m->fat, inner, &e)) return 0;
        fat32Walk(&m->fat, e.cluster, fatListCb, 0);
    } else {
        uint32_t n = ext2TraversePath(&m->ext2, inner);
        if (!n) return 0;
        ext2WalkDir(&m->ext2, n, ext2ListCb, &m->ext2);
    }
    return 1;
}
