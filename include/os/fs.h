#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0xDF4C4459
#define NUM_FDESCS 16

#define BLOCK_SIZE 4096  // 4KB per block
#define SECTOR_SIZE 512  // 512B per sector

#define CEIL(a,n)   ((a) / (n) + ((((a) % (n))) ? 1 : 0))
#define INODE_NUM 512
#define DATA_BLOCK_NUM (1<<18)  // 256K data blocks
#define INODE_MAP_SEC_NUM CEIL(INODE_NUM, 8 * SECTOR_SIZE)
#define BLOCK_MAP_SEC_NUM CEIL(DATA_BLOCK_NUM, 8 * SECTOR_SIZE)
#define INODE_SEC_NUM CEIL(INODE_NUM * sizeof(inode_t), SECTOR_SIZE)
#define SUPERBLOCK_SEC_NUM 1
#define DATA_BLOCK_SEC_NUM (DATA_BLOCK_NUM * BLOCK_SIZE / SECTOR_SIZE)

#define BLOCK_MAP_OFFSET SUPERBLOCK_SEC_NUM
#define INODE_MAP_OFFSET (BLOCK_MAP_OFFSET + BLOCK_MAP_SEC_NUM)
#define INODE_OFFSET (INODE_MAP_OFFSET + INODE_MAP_SEC_NUM)
#define DATA_BLOCK_OFFSET (INODE_OFFSET + INODE_SEC_NUM)

#define FS_SIZE (DATA_BLOCK_OFFSET + DATA_BLOCK_SEC_NUM)  // in sectors
#define FS_START_SECTOR ((1<<29) / SECTOR_SIZE)  // starting at 512MB

#define INODE_PER_SEC (SECTOR_SIZE / sizeof(inode_t))
#define DENTRY_PER_SEC (SECTOR_SIZE / sizeof(dentry_t))
#define DENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(dentry_t))

#define DIR 0
#define FILE 1

#define ADDR_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define ADDR_PER_SECTOR (SECTOR_SIZE / sizeof(uint32_t))
#define DIRECT_ADDR_NUM 13
#define DIRECT_SIZE (DIRECT_ADDR_NUM * BLOCK_SIZE)  // direct blocks
#define INDIRECT_SIZE_1 (3*BLOCK_SIZE*ADDR_PER_BLOCK)  // single indirect
#define INDIRECT_SIZE_2 (2*BLOCK_SIZE*ADDR_PER_BLOCK*ADDR_PER_BLOCK)  // double indirect

#define MAX_FILE_SIZE (DIRECT_SIZE + INDIRECT_SIZE_1 + INDIRECT_SIZE_2)
/* data structures of file system */
typedef struct superblock {
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;              // magic number
    uint32_t fs_start_sec;       // starting sector of the file system
    uint32_t fs_size;     // size of the file system in sectors
    uint32_t block_map_offset;
    uint32_t inode_map_offset;
    uint32_t inode_offset;
    uint32_t data_block_offset;
    uint32_t inode_num;
    uint32_t data_block_num;
} superblock_t;

typedef struct dentry {
    // TODO [P6-task1]: Implement the data structure of directory entry
    char filename[16];
    int ino;
} dentry_t;

typedef struct inode { 
    // TODO [P6-task1]: Implement the data structure of inode
    char type;                   // file type: DIR or FILE
    char mode;                   // permission mode
    uint16_t nlink;              // number of links
    uint32_t ino;
    uint32_t size;
    uint32_t direct_addr[DIRECT_ADDR_NUM];  // direct addresses
    uint32_t indirect_addr_1[3];    // single indirect address
    uint32_t indirect_addr_2[2];    // double indirect address
} inode_t;

typedef struct fdesc {
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint8_t valid;
    uint8_t mode;    // open mode
    int ino;
    short ref;
    uint32_t write_ptr;  // write pointer
    uint32_t read_ptr;   // read pointer
} fdesc_t;

/* modes of do_open */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(int force);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_open(char *path, int mode);
extern int do_read(int fd, char *buff, int length);
extern int do_write(int fd, char *buff, int length);
extern int do_close(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif