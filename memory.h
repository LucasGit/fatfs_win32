// create by anve 2018.11.12


#ifndef _MEMORY_H_
#define _MEMORY_H_




#define MEMMAN_FREES 1022   /* ��Լ��8KB  ���ڼ�¼�����ڴ����Ŀ*/
//#define MEMMAN_ADDR			0xc0000000ul  // SDRAM �Ķ�д��ַ
#define MEMMAN_SIZE 8*1024*1024

#define MEMMAN_ADDR &_memBuffer[0]



/* low level api */
struct FREEINFO { 
/* ������Ϣ */	unsigned int addr, size;
};
struct MEMMAN { /* �ڴ���� */	
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

