// create by anve 2018.11.12


#ifndef _MEMORY_H_
#define _MEMORY_H_




#define MEMMAN_FREES 1022   /* 大约是8KB  用于记录空闲内存的条目*/
//#define MEMMAN_ADDR			0xc0000000ul  // SDRAM 的读写地址
#define MEMMAN_SIZE 8*1024*1024

#define MEMMAN_ADDR &_memBuffer[0]



/* low level api */
struct FREEINFO { 
/* 可用信息 */	unsigned int addr, size;
};
struct MEMMAN { /* 内存管理 */	
    int frees, maxfrees, lostsize, losts, alloc_fail, alloc_fail_size;   
    struct FREEINFO free[MEMMAN_FREES];
};

void memman_init(struct MEMMAN *man);
unsigned int memman_total(struct MEMMAN *man);
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size);
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size);
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size);



/* high level api */

void mymem_init(void);
unsigned int mymem_malloc(int size);
int mymem_free(unsigned int addr);
void mymem_print_info(void);






























#endif

