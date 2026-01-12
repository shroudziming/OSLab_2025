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
static char buffer_f[BLOCK_SIZE];
static char buffer_s[BLOCK_SIZE];
static char level_buffer[3][BLOCK_SIZE];

int if_fs_exist();
int alloc_inode();
int alloc_block(uint32_t *block_addr, int num_blocks);
inode_t set_inode(short type, int mode, int ino);
inode_t* ino2inode(int ino);
int parse_path(inode_t current_node,char* path,inode_t* target);
int get_inode_by_name(inode_t node,char* name,inode_t* target);

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
    printk("\t [mkfs] make filesystem done.\n");
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
    if(!if_fs_exist()){
        printk("\n\t [cd] filesystem does not exist.\n");
        return 1;  // do_cd fails
    }
    inode_t temp_inode;
    if(parse_path(current_inode, path, &temp_inode) == 0){
        printk("\n\t [cd] directory %s does not exist.\n", path);
        return 1;  // do_cd fails
    }
    if(temp_inode.type != DIR){
        printk("\n\t [cd] %s is not a directory.\n", path);
        return 1;  // do_cd fails
    }
    current_inode = temp_inode;
    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    if(!if_fs_exist()){
        printk("\n\t [mkdir] filesystem does not exist.\n");
        return 1;  // do_mkdir fails
    }
    //check if dir already exists
    inode_t temp_inode;
    inode_t parent_temp_inode;
    char parent_path[20];
    parent_path[0] = '.';
    for(int j = 1;j < 20;j++){
        parent_path[j] = '\0';
    }
    for(int i = strlen(path);i>=0;i--){
        if(path[i] == '/'){
            memcpy((uint8_t *)parent_path,(const uint8_t *)path,i);
            parent_path[i] = '\0';
            int path_len = strlen(parent_path);
            for(int j = 0;j<20;j++){
                if(j < path_len - i){
                    path[j] = path[j + i + 1];
                }else{
                    path[j] = '\0';
                }
            }
            break;
        }
    }
    parse_path(current_inode, parent_path, &parent_temp_inode);
    if(get_inode_by_name(parent_temp_inode,path,NULL)){
        printk("\n\t [mkdir] directory %s already exists.\n", path);
        return 1;  // directory already exists
    }

    //create dentry
    int ino = alloc_inode();
    bzero(buffer_f, SECTOR_SIZE);
    dentry_t *dentry = (dentry_t *)buffer_f;
    strcpy(dentry[0].filename, ".");
    strcpy(dentry[1].filename, "..");
    dentry[0].ino = ino;
    dentry[1].ino = parent_temp_inode.ino;
    //allocate data block
    uint32_t data_block_addr;
    alloc_block(&data_block_addr,1);
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, data_block_addr);
    //setup inode
    inode_t *new_node = ino2inode(ino);
    *new_node = set_inode(DIR,O_RDWR,ino);
    new_node->direct_addr[0] = data_block_addr;
    new_node->size = 2 * sizeof(dentry_t);
    int offset = ino / INODE_PER_SEC;
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET + offset);
    int start_sector = parent_temp_inode.direct_addr[0] + parent_temp_inode.size / SECTOR_SIZE;
    bios_sd_read(kva2pa((uintptr_t)buffer_f),1, start_sector); 
    int i;
    for(i = 0;i < DENTRY_PER_SEC;i++){
        if(dentry[i].filename[0] == 0){
            break;
        }
    }
    dentry[i].ino = ino;
    strcpy(dentry[i].filename, path);
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, start_sector);

    //update parent inode size
    inode_t *parent_inode = ino2inode(parent_temp_inode.ino);
    parent_inode->size += sizeof(dentry_t);
    offset = parent_temp_inode.ino / INODE_PER_SEC;
    bios_sd_write(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET + offset);

    printk("\n\t [mkdir] directory %s created.\n", path);
    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    if(!if_fs_exist()){
        printk("\n\t [rmdir] filesystem does not exist.\n");
        return 1;  // do_rmdir fails
    }
    inode_t dir;
    if(get_inode_by_name(current_inode,path,&dir) == 0){
        printk("\n\t [rmdir] directory %s does not exist.\n", path);
        return 1;  // do_rmdir fails, dir not exist
    }
    if(dir.type != DIR){
        printk("\n\t [rmdir] %s is not a directory.\n", path);
        return 1;  // do_rmdir fails, not a dir
    }
    //check if dir is empty
    if(dir.size > 2 * sizeof(dentry_t)){
        printk("\n\t [rmdir] directory %s is not empty.\n", path);
        return 1;  // do_rmdir fails, dir not empty
    }

    //remove dentry from parent dir
    dir.nlink--;
    if(dir.nlink == 0){
        //inode table
        int offset = dir.ino / INODE_PER_SEC;
        inode_t *dir_inode = ino2inode(dir.ino);
        bzero(dir_inode, sizeof(inode_t));
        bios_sd_write(kva2pa((uintptr_t)buffer_f),1,FS_START_SECTOR + INODE_OFFSET + offset);
        //imap
        imap[dir.ino / 8] &= ~(1 << (dir.ino % 8));
        bios_sd_write(kva2pa((uintptr_t)imap),INODE_MAP_SEC_NUM, FS_START_SECTOR + INODE_MAP_OFFSET);
        //data block
        bzero(buffer_f,BLOCK_SIZE);
        uint32_t data_block_addr = dir.direct_addr[0];
        bios_sd_write(kva2pa((uintptr_t)buffer_f), BLOCK_SIZE / SECTOR_SIZE, data_block_addr);
        //bmap
        int blk_index = (data_block_addr - FS_START_SECTOR - DATA_BLOCK_OFFSET) * SECTOR_SIZE / BLOCK_SIZE;
        bmap[blk_index / 8] &= ~(1 << (blk_index % 8));
        bios_sd_write(kva2pa((uintptr_t)bmap), BLOCK_MAP_SEC_NUM, FS_START_SECTOR + BLOCK_MAP_OFFSET);

        //delete dentry from parent directory(current_inode)
        bios_sd_read(kva2pa((uintptr_t)buffer_f), BLOCK_SIZE / SECTOR_SIZE, current_inode.direct_addr[0]);
        dentry_t* dentry = (dentry_t*)buffer_f;
        int i;
        for(i = 0; i < DENTRY_PER_BLOCK; i++){
            if(dir.ino == dentry[i].ino){
                break;
            }
        }
        bzero(&dentry[i],sizeof(dentry_t));
        bios_sd_write(kva2pa((uintptr_t)buffer_f), BLOCK_SIZE / SECTOR_SIZE, current_inode.direct_addr[0]);
        //update current_inode size
        inode_t *curr_inode = ino2inode(current_inode.ino);
        curr_inode->size -= sizeof(dentry_t);
        offset = current_inode.ino / INODE_PER_SEC;
        bios_sd_write(kva2pa((uintptr_t)buffer_f),1, FS_START_SECTOR + INODE_OFFSET + offset);  //in ino2inode we read curr_inode to buffer_f
    }else{
        //nlink > 0, just update inode
        int offset = dir.ino / INODE_PER_SEC;
        inode_t *dir_inode = ino2inode(dir.ino);
        dir_inode->nlink = dir.nlink;
        bios_sd_write(kva2pa((uintptr_t)buffer_f),1,FS_START_SECTOR + INODE_OFFSET + offset);
    }
    printk("\n\t [rmdir] directory %s removed.\n", path);
    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    if(!if_fs_exist()){
        printk("[ls] filesystem has not been set up!\n");
        return 1;
    }
    inode_t node;
    if(path[0]==0)
        node = current_inode;
    else if(parse_path(current_inode, path, &node)==0)
        return 1;   // do_ls fails, path not exist
    bios_sd_read(kva2pa((uintptr_t)buffer_s), BLOCK_SIZE/SECTOR_SIZE, node.direct_addr[0]);
    dentry_t* dentry = (dentry_t*)buffer_s;

    for(int i=2; i<DENTRY_PER_SEC; i++){
        if(dentry[i].filename[0]==0)
            continue;
        if(option){ //ls -al
            
            printk("\nino: %d ", dentry[i].ino);
            inode_t tmp = *ino2inode(dentry[i].ino);
            printk("%c%c%c nlink:%d size:%d name :%s\n", 
                    tmp.type == DIR ? 'd' : '-',
                    (tmp.mode & O_RDONLY) ? 'r' : '-',
                    (tmp.mode & O_WRONLY) ? 'w' : '-',
                    tmp.nlink, tmp.size,dentry[i].filename
                    );
        }
        else
            printk("\t%s", dentry[i].filename);
    }
    if(option==0)
        printk("\n");
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

//current_node as start point
int parse_path(inode_t current_node,char* path,inode_t* target){
    if(path == NULL || path[0] == 0){
        if(target != NULL){
            *target = current_node;
        }
        return 1;
    }
    int len;
    for(len = 0; len < strlen(path); len++){
        if(path[len] == '/'){
            break;
        }
    }
    char name[len + 1];
    memcpy((uint8_t *)name,(const uint8_t *) path, len);
    name[len] = '\0';
    inode_t temp_inode;
    if(get_inode_by_name(current_inode,name,&temp_inode) == 0){
        //not found
        return 0;
    }
    return parse_path(temp_inode,path+len+1,target);
}

//search inode by name in "node" directory
int get_inode_by_name(inode_t node,char* name,inode_t* target){
    if(node.type != DIR){
        return 0;
    }
    bios_sd_read(kva2pa((uintptr_t)buffer_f),BLOCK_SIZE / SECTOR_SIZE, node.direct_addr[0]);
    dentry_t* dentry = (dentry_t*)buffer_f;
    for(int i = 0; i < DENTRY_PER_BLOCK; i++){
        if(strcmp(dentry[i].filename,name) == 0){
            if(target != NULL){
                *target = *ino2inode(dentry[i].ino);
            }
            return 1;
        }else if(dentry[i].filename[0]==0){
            continue;
        }
    }
    return 0;
}