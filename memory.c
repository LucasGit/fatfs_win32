// create by anve 2018.11.12
// �ڴ�������

#include "memory.h"
#include <stdio.h>




void memman_init(struct MEMMAN *man)
{
    man->frees = 0;    /* ������Ϣ��Ŀ */
    man->maxfrees = 0; /* ���ڹ۲����״����frees�����ֵ */
    man->lostsize = 0; /* �ͷ�ʧ�ܵ��ڴ�Ĵ�С�ܺ� */
    man->losts = 0;    /* �ͷ�ʧ�ܴ��� */
    man->alloc_fail = 0; /* �����ڴ�ʧ�ܵĴ��� */
    man->alloc_fail_size = 0; /* �����ڴ�ʧ�ܵ��ڴ��ܺ� */
    return;
}

unsigned int memman_total(struct MEMMAN *man)
/* ��������ڴ��С�ĺϼ� */
{
    unsigned int i, t = 0;
    for (i = 0; i < man->frees; i++)
    {
        t += man->free[i].size;
    }
    return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* ���� */
{
    unsigned int i, a;
    size = (size + 0x03) & 0xfffffffc; // 4�ֽڶ���
    for (i = 0; i < man->frees; i++)
    {
        if (man->free[i].size >= size)
        {
            /* �ҵ����㹻����ڴ� */
            a = man->free[i].addr;
            man->free[i].addr += size;
            man->free[i].size -= size;
            if (man->free[i].size == 0)
            {
                /* ���free[i]�����0���ͼ���һ��������Ϣ */
                man->frees--;
                for (; i < man->frees; i++)
                {
                    man->free[i] = man->free[i + 1]; /* ����ṹ�� */
                }
            }
            return a;
        }
    }
    man->alloc_fail++;
    man->alloc_fail_size += size;
    return 0; /* û�п��ÿռ� */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* �ͷ� */
{
    int i, j;
    size = (size + 0x03) & 0xfffffffc;  // 4�ֽڶ���
    /* Ϊ���ڹ����ڴ棬��free[]����addr��˳������ */
    /* ���ԣ��Ⱦ���Ӧ�÷������� */
    for (i = 0; i < man->frees; i++)
    {
        if (man->free[i].addr > addr)
        {
            break;
        }
    }
    /* free[i - 1].addr < addr < free[i].addr */
    if (i > 0)
    {
        /* ǰ���п����ڴ� */
        if (man->free[i - 1].addr + man->free[i - 1].size == addr)
        {
            /* ������ǰ��Ŀ����ڴ���ɵ�һ�� */
            man->free[i - 1].size += size;
            if (i < man->frees)
            {
                /* ����Ҳ�� */
                if (addr + size == man->free[i].addr)
                {
                    /* Ҳ���������Ŀ����ڴ���ɵ�һ�� */
                    man->free[i - 1].size += man->free[i].size;
                    /* man->free[i]ɾ�� */
                    /* free[i]���0����ɵ�ǰ��ȥ */
                    man->frees--;
                    for (; i < man->frees; i++)
                    {
                        man->free[i] = man->free[i + 1]; /* �ṹ�帳ֵ */
                    }
                }
            }
            return 0; /* �ɹ���� */
        }
    }
    /* ������ǰ��Ŀ��ÿռ���ɵ�һ�� */
    if (i < man->frees)
    {
        /* ���滹�� */
        if (addr + size == man->free[i].addr)
        {
            /* �������������ݹ��ɵ�һ�� */
            man->free[i].addr = addr;
            man->free[i].size += size;
            return 0; /* �ɹ���� */
        }
    }
    /* �Ȳ�����ǰ����ɵ�һ��Ҳ�����������ɵ�һ�� */
    if (man->frees < MEMMAN_FREES)
    {
        /* free[i]֮��ģ�����ƶ����ڳ�һ����ÿռ� */
        for (j = man->frees; j > i; j--)
        {
            man->free[j] = man->free[j - 1];
        }
        man->frees++;
        if (man->maxfrees < man->frees)
        {
            man->maxfrees = man->frees; /* �������ֵ */
        }
        man->free[i].addr = addr;
        man->free[i].size = size;
        return 0; /* �ɹ���� */
    }
    /* ���������ƶ� */
    man->losts++;
    man->lostsize += size;
    return -1; /* ʧ�� */
}

unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size)
{
    unsigned int a;
    size = (size + 0xfff) & 0xfffff000;
    a = memman_alloc(man, size);
    return a;
}

int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
    int i;
    size = (size + 0xfff) & 0xfffff000;
    i = memman_free(man, addr, size);
    return i;
}



struct MEMMAN *memman;

char _memBuffer[MEMMAN_SIZE];


void mymem_init(void)
{
    unsigned int memStart, memtotal;
    
    memman = (struct MEMMAN *) MEMMAN_ADDR;

    memman_init(memman);

    memStart = MEMMAN_ADDR + sizeof(struct MEMMAN) + 0x03;

    memStart = memStart & 0xfffffffc;
    
    memman_free(memman, memStart, MEMMAN_ADDR + ((unsigned int)MEMMAN_SIZE) - memStart);
}

unsigned int mymem_malloc(int size)
{
    unsigned int addr = memman_alloc(memman, size+4);
    if(addr == 0)
    {
        return 0;
    }
    unsigned int *psize = (unsigned int *) addr;       // ��ʼ��4���ֽ����ڴ洢����Ŀռ��С
    *psize = size+4;
    return addr+4;    
}

int mymem_free(unsigned int addr)
{
    unsigned int *psize = (unsigned int *) (addr-4);
    unsigned int size = *psize;
    return memman_free(memman,addr-4,size);
}

void mymem_print_info(void)
{
    int i;
    struct MEMMAN *man = memman;

    printf("\r\nTotal free %dKB\r\n", memman_total(man) / 1024);


    for (i = 0; i < man->frees; i++)
    {
        printf("Addr = 0x%x,  Size = %d\r\n", man->free[i].addr, man->free[i].size);
    }

    if(man->losts)
    {
        printf("free losts = %d, lost size = %d\r\n", man->losts, man->lostsize);
    }
    if(man->alloc_fail)
    {
        printf("alloc fail = %d, fail size = %d\r\n", man->alloc_fail, man->alloc_fail_size);
    }
}


