// create by anve 2018.11.12
// 内存分配管理

#include "memory.h"
#include <stdio.h>




void memman_init(struct MEMMAN *man)
{
    man->frees = 0;    /* 可用信息数目 */
    man->maxfrees = 0; /* 用于观察可用状况：frees的最大值 */
    man->lostsize = 0; /* 释放失败的内存的大小总和 */
    man->losts = 0;    /* 释放失败次数 */
    man->alloc_fail = 0; /* 分配内存失败的次数 */
    man->alloc_fail_size = 0; /* 分配内存失败的内存总和 */
    return;
}

unsigned int memman_total(struct MEMMAN *man)
/* 报告空余内存大小的合计 */
{
    unsigned int i, t = 0;
    for (i = 0; i < man->frees; i++)
    {
        t += man->free[i].size;
    }
    return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* 分配 */
{
    unsigned int i, a;
    size = (size + 0x03) & 0xfffffffc; // 4字节对齐
    for (i = 0; i < man->frees; i++)
    {
        if (man->free[i].size >= size)
        {
            /* 找到了足够大的内存 */
            a = man->free[i].addr;
            man->free[i].addr += size;
            man->free[i].size -= size;
            if (man->free[i].size == 0)
            {
                /* 如果free[i]变成了0，就减掉一条可用信息 */
                man->frees--;
                for (; i < man->frees; i++)
                {
                    man->free[i] = man->free[i + 1]; /* 代入结构体 */
                }
            }
            return a;
        }
    }
    man->alloc_fail++;
    man->alloc_fail_size += size;
    return 0; /* 没有可用空间 */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
    int i, j;
    size = (size + 0x03) & 0xfffffffc;  // 4字节对齐
    /* 为便于归纳内存，将free[]按照addr的顺序排列 */
    /* 所以，先决定应该放在哪里 */
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
        /* 前面有可用内存 */
        if (man->free[i - 1].addr + man->free[i - 1].size == addr)
        {
            /* 可以与前面的可用内存归纳到一起 */
            man->free[i - 1].size += size;
            if (i < man->frees)
            {
                /* 后面也有 */
                if (addr + size == man->free[i].addr)
                {
                    /* 也可以与后面的可用内存归纳到一起 */
                    man->free[i - 1].size += man->free[i].size;
                    /* man->free[i]删除 */
                    /* free[i]变成0后归纳到前面去 */
                    man->frees--;
                    for (; i < man->frees; i++)
                    {
                        man->free[i] = man->free[i + 1]; /* 结构体赋值 */
                    }
                }
            }
            return 0; /* 成功完成 */
        }
    }
    /* 不能与前面的可用空间归纳到一起 */
    if (i < man->frees)
    {
        /* 后面还有 */
        if (addr + size == man->free[i].addr)
        {
            /* 可以与后面的内容归纳到一起 */
            man->free[i].addr = addr;
            man->free[i].size += size;
            return 0; /* 成功完成 */
        }
    }
    /* 既不能与前面归纳到一起，也不能与后面归纳到一起 */
    if (man->frees < MEMMAN_FREES)
    {
        /* free[i]之后的，向后移动，腾出一点可用空间 */
        for (j = man->frees; j > i; j--)
        {
            man->free[j] = man->free[j - 1];
        }
        man->frees++;
        if (man->maxfrees < man->frees)
        {
            man->maxfrees = man->frees; /* 更新最大值 */
        }
        man->free[i].addr = addr;
        man->free[i].size = size;
        return 0; /* 成功完成 */
    }
    /* 不能往后移动 */
    man->losts++;
    man->lostsize += size;
    return -1; /* 失败 */
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
    unsigned int *psize = (unsigned int *) addr;       // 开始的4个字节用于存储分配的空间大小
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


