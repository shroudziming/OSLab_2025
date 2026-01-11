#include <os/string.h>
#include <os/fs.h>
#include <pgtable.h>
#include <printk.h>
#include <os/kernel.h>

static fdesc_t fdesc_array[NUM_FDESCS];

static superblock_t superblock;
static inode_t current_inode;
static char imap[INODE_MAP_SEC_NUM*SECTOR_SIZE];
static char bmap[BLOCK_MAP_SEC_NUM*SECTOR_SIZE];
static char buffer_f[SECTOR_SIZE];
static char buffer_s[SECTOR_SIZE];
static char level_buffer[3][SECTOR_SIZE];

int if_fs_exist();
int alloc_inode();
int alloc_block(uint32_t *block_addr, int num_blocks);
inode_t set_inode(short type, int mode, int ino);
inode_t* ino2inode(int ino);

int do_mkfs(int force)
{
    // TODO [P6-task1]: Implement do_mkfs
    if(force){
        // format the disk
        int block_num = CEIL(DATA_BLOCK_OFFSET * SECTOR_SIZE, BLOCK_SIZE);
        //clear memory
        bzero((char *)&superblock, sizeof(inode_t));
        bzero((char *)&current_inode, sizeof(inode_t));
        //clear disk
        bzero(buffer_f,BLOCK_SIZE);
        for(int i=0;i<block_num;i++){
            bios_sd_write(kva2pa((uintptr_t)buffer_f),BLOCK_SIZE / SECTOR_SIZE, FS_START_SECTOR + i);
        }
        printk("\n [mkfs] clean disk done.\n");
    }else if(if_fs_exist()){
        printk("\n [mkfs] filesystem already exists. use 'mkfs -f' to force format.\n");
        return 1;  // do_mkfs fails
    }
    printk("\n\t [mkfs] start making filesystem...\n");
    printk("\t [mkfs] setting superblock...\n");
    //setup superblock
    superblock.magic = SUPERBLOCK_MAGIC;
    superblock.fs_start_sec = FS_START_SECTOR;
    superblock.fs_size = FS_SIZE;
    superblock.block_map_offset = BLOCK_MAP_OFFSET;
    superblock.inode_map_offset = INODE_MAP_OFFSET;
    superblock.inode_offset = INODE_OFFSET;
    superblock.data_block_offset = DATA_BLOCK_OFFSET;
    superblock.inode_num = INODE_NUM;
    superblock.data_block_num = DATA_BLOCK_NUM;
    //print out superblock info
    printk("\t magic: 0x%x\n", superblock.magic);
    printk("\t num sector: %d, start sector: %d\n", superblock.fs_size, superblock.fs_start_sec);
    printk("\t block map offset: %d(%d)\n", superblock.block_map_offset, BLOCK_MAP_SEC_NUM);
    printk("\t inode map offset: %d(%d)\n", superblock.inode_map_offset, INODE_MAP_SEC_NUM);
    printk("\t inode offset: %d(%d), inode num: %d\n", superblock.inode_offset, INODE_SEC_NUM, superblock.inode_num);
    printk("\t data offset: %d(%d), data block num: %d\n", superblock.data_block_offset, DATA_BLOCK_SEC_NUM, superblock.data_block_num);
    //write superblock to disk
    bios_sd_write(kva2pa((uintptr_t)&superblock), 1, FS_START_SECTOR);
    //initialize inode map
    bzero(imap,INODE_MAP_SEC_NUM*SECTOR_SIZE);
    bios_sd_write(kva2pa((uintptr_t)imap), INODE_MAP_SEC_NUM, FS_START_SECTOR + INODE_MAP_OFFSET);
    //initialize block map
    bzero(bmap,BLOCK_MAP_SEC_NUM*SECTOR_SIZE);
    bios_sd_write(kva2pa((uintptr_t)bmap), BLOCK_MAP_SEC_NUM, FS_START_SECTOR + BLOCK_MAP_OFFSET);

    //create root directory
    //allocate inode 0
    int root_ino = alloc_inode();
    //create dentry
    bzero(buffer_f, SECTOR_SIZE);
    dentry_t *dentry = (dentry_t *)buffer_f;
    strcpy(dentry[0].filename, ".");
    strcpy(dentry[1].filename, "..");
    dentry[0].ino = root_ino;
    dentry[1].ino = root_ino;
    //allocate data block
    uint32_t data_block_addr;
    alloc_block(&data_block_addr,1);
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, data_block_addr);
    //setup inode
    inode_t *root_inode = ino2inode(root_ino);
    *root_inode = set_inode(DIR,O_RDWR,root_ino);
    root_inode->size = 2 * sizeof(dentry_t);
    root_inode->direct_addr[0] = data_block_addr;
    current_inode = *root_inode;    // set current inode to root
    int offset = root_ino / INODE_PER_SEC;
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET + offset);
    //initialize file descriptor array
    bzero(fdesc_array, sizeof(fdesc_t) * NUM_FDESCS);
    printk("[mkfs] make filesystem done.\n");
    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    if(!if_fs_exist()){
        printk("\n\t [statfs] filesystem does not exist.\n");
        return 1;  // do_statfs fails
    }
    int used_inode = 0;
    int used_block = 0;
    int i,j,mask;
    for(i=0;i<INODE_MAP_SEC_NUM*SECTOR_SIZE;i++){
        mask=1;
        for(j=0;j<8;j++,mask<<=1){
            if(imap[i] & mask){
                used_inode++;
            }
        }
    }
    for(i=0;i<BLOCK_MAP_SEC_NUM*SECTOR_SIZE;i++){
        mask=1;
        for(j=0;j<8;j++,mask<<=1){
            if(bmap[i] & mask){
                used_block++;
            }
        }
    }
    printk("\n\t [statfs] filesystem state:\n");
    printk("\t magic: 0x%x\n", superblock.magic);
    printk("\t total sectors: %d, start at sector: %d\n", superblock.fs_size, superblock.fs_start_sec);
    printk("\t inode map offset: %d, occupieed sectors: %d\n", superblock.inode_map_offset, INODE_MAP_SEC_NUM);
    printk("\t block map offset: %d, occupied sectors: %d\n", superblock.block_map_offset, BLOCK_MAP_SEC_NUM);
    printk("\t inode offset: %d, occupied sectors: %d\n", superblock.inode_offset, INODE_SEC_NUM);
    printk("\t data block offset: %d, occupied sectors: %d\n", superblock.data_block_offset, DATA_BLOCK_SEC_NUM);
    printk("\t inode entry size: %dB, dir entry size: %dB\n", sizeof(inode_t), sizeof(dentry_t));
    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    
    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir

    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core

    return 0;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch

    return 0;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat

    return 0;  // do_cat succeeds
}

int do_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_open

    return 0;  // return the id of file descriptor
}

int do_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_read

    return 0;  // return the length of trully read data
}

int do_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_write

    return 0;  // return the length of trully written data
}

int do_close(int fd)
{
    // TODO [P6-task2]: Implement do_close

    return 0;  // do_close succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}

int if_fs_exist(){
    if(superblock.magic == SUPERBLOCK_MAGIC){
        return 1;
    }
    //read superblock from disk
    bios_sd_read(kva2pa((uintptr_t)&superblock), 1, FS_START_SECTOR);
    if(superblock.magic == SUPERBLOCK_MAGIC){
        bios_sd_read(kva2pa((uintptr_t)imap),INODE_MAP_SEC_NUM, FS_START_SECTOR + INODE_MAP_OFFSET);
        bios_sd_read(kva2pa((uintptr_t)bmap),BLOCK_MAP_SEC_NUM, FS_START_SECTOR + BLOCK_MAP_OFFSET);
        bios_sd_read(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET);
        memcpy((uint8_t *)&current_inode,(const uint8_t *)buffer_f,sizeof(inode_t));
        return 1;
    }
    return 0;
}

//alloc inode, return inode number
int alloc_inode(){
    int i,j;
    int mask=1;
    for(i=0;i<INODE_MAP_SEC_NUM*SECTOR_SIZE;i++){
        for(j=0;j<8;j++,mask<<=1){
            if((imap[i] & mask) == 0){
                imap[i] |= mask;
                bios_sd_write(kva2pa((uintptr_t)imap), INODE_MAP_SEC_NUM, FS_START_SECTOR + INODE_MAP_OFFSET);
                return i*8 + j;
            }
        }
    }
    return -1; //no free inode
}


int alloc_block(uint32_t *addr_array,int num){
    static int i,j=0;
    static int mask=1;
    int count=0;
    for(;i<BLOCK_MAP_SEC_NUM*SECTOR_SIZE ;i++){
        for(; j<8; j++, mask<<=1){
            if((bmap[i] & mask) == 0){
                bmap[i] |= mask;
                int blk_index = i*8+j;
                uint32_t data_blk_addr = blk_index*BLOCK_SIZE/SECTOR_SIZE + FS_START_SECTOR + DATA_BLOCK_OFFSET;
                bzero(buffer_s, BLOCK_SIZE);
                bios_sd_write(kva2pa((uintptr_t)buffer_s), BLOCK_SIZE/SECTOR_SIZE, data_blk_addr);
                addr_array[count] = data_blk_addr;
                count++;

                if(count==num){
                    int sec_num = CEIL(blk_index, SECTOR_SIZE);
                    sec_num = sec_num == 0 ? 1 : sec_num; 
                    bios_sd_write(kva2pa((uintptr_t)bmap), sec_num, FS_START_SECTOR + BLOCK_MAP_OFFSET);
                    return 1;
                }
            }  
        }
        if(j==8){
                j=0;
                mask=1;
        }
        if(i==BLOCK_MAP_SEC_NUM*SECTOR_SIZE-1 && count<num){
            // no enough blocks
            i++;    // to break outer loop
            j = 0; // reset j
            mask = 1; // reset mask
            break;
        }
    }
}

inode_t* ino2inode(int ino){
    int offset = ino / INODE_PER_SEC;
    bios_sd_read(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET + offset);
    return ((inode_t *)buffer_f) + (ino % INODE_PER_SEC);
}

inode_t set_inode(short type,int mode,int ino){
    inode_t inode;
    bzero(&inode, sizeof(inode_t));
    inode.type = type;
    inode.mode = mode;
    inode.ino = ino;
    inode.nlink = 1;
    inode.size = 0;
    return inode;
}