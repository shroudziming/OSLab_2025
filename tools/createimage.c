#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa
#define SWAP_START_LOC   (BOOT_LOADER_SIG_OFFSET - 14)  // 0x1F0
#define SWAP_SECTORS     131072   // swap 区大小

#define NAME_LEN	32

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
	char name[NAME_LEN];
	uint64_t file_off;	//offset in image
	uint64_t file_size;	
	uint64_t entry;		//entrypoint in mem
    uint64_t p_memsz;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    uint32_t p_filesz = 0;
    uint32_t p_memsz = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);
	
	long start_phyaddr = phyaddr;
	uint64_t total_bytes = 0;	//current program size
        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);
	    total_bytes += get_filesz(phdr);	//sum up

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }

            if(phdr.p_type == PT_LOAD){
                p_filesz += phdr.p_filesz;
                p_memsz += phdr.p_memsz;
            }
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
	
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
	//padding in task3 
        /*	else {
		int block_size = SECTOR_SIZE * 15;
		int written_bytes = phyaddr - start_phyaddr;
		int next_aligned = ((written_bytes + block_size - 1) / block_size) * block_size;
		write_padding(img,&phyaddr,start_phyaddr + next_aligned);
	}*/
	//[p1-task4] write task_info
        if(fidx >= 2){
		strcpy(taskinfo[taskidx].name,*files);
		taskinfo[taskidx].file_off = start_phyaddr;
		taskinfo[taskidx].file_size = total_bytes;
		taskinfo[taskidx].entry = ehdr.e_entry;
        taskinfo[taskidx].p_memsz = p_memsz;
	}
	fclose(fp);
        files++;
    }

    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    // long img_end_bytes = ftell(img);

    // // SWAP 起始扇区号（向上取整）
    // uint32_t swap_start_sector = NBYTES2SEC(img_end_bytes);

    // // 把 swap 起始扇区号写进 bootblock
    // fseek(img, SWAP_START_LOC, SEEK_SET);
    // fwrite(&swap_start_sector, sizeof(uint32_t), 1, img);

    // // 扩展 image，预留 SWAP_SECTORS 个扇区
    // fseek(img, swap_start_sector * SECTOR_SIZE, SEEK_SET);
    // long swap_end_bytes =
    //     (long)(swap_start_sector + SWAP_SECTORS) * SECTOR_SIZE;

    // write_padding(img, &phyaddr, swap_end_bytes);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    
    //[p1-task3] write nsectors_kernel and tasknum
    /*	
    short nsectors_kernel = (nbytes_kernel + SECTOR_SIZE - 1) / SECTOR_SIZE;

    //find tail of first sector
    fseek(img,0x1FC,SEEK_SET);
    //store kernel sector num (2 Bytes)
    fwrite(&nsectors_kernel,sizeof(short),1,img);
    //store task num (2 Bytes)
    fwrite(&tasknum,sizeof(short),1,img);

    fflush(img);
    */
    short nsectors_kernel = (nbytes_kernel + SECTOR_SIZE - 1) / SECTOR_SIZE;

    fseek(img,OS_SIZE_LOC,SEEK_SET);
    fwrite(&nsectors_kernel,sizeof(short),1,img);
    fwrite(&tasknum,sizeof(short),1,img);

    //write taskinfo to end of image
    fseek(img,0,SEEK_END);
    uint32_t table_offset_bytes = ftell(img);

    if(tasknum > 0){
	    fwrite(taskinfo,sizeof(task_info_t),tasknum,img);
    }

    //pad end
    long after = ftell(img);
    long pad = (SECTOR_SIZE - (after % SECTOR_SIZE)) % SECTOR_SIZE;
    long i;
    for(i = 0;i<pad;++i){
	    fputc(0,img);
    }

    fflush(img);

    long img_end_bytes = ftell(img);
    // SWAP 起始扇区号（向上取整）
    uint32_t swap_start_sector = NBYTES2SEC(img_end_bytes);

    fseek(img,0x1F0,SEEK_SET);	//0x1F0 - 0x1F3
    fwrite(&swap_start_sector, sizeof(uint32_t), 1, img);

    //write table offset bytes
    fseek(img,0x1F8,SEEK_SET);	//0x1F8 - 0x1FB
    fwrite(&table_offset_bytes,sizeof(uint32_t),1,img);

    fflush(img);
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
