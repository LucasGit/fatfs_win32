/*------------------------------------------------------------------------/
/  The Main Development Bench of FatFs Module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2018, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/


#include <string.h>
#include <stdio.h>
#include <locale.h>
#include "ff.h"
#include "diskio.h"
#include "ff_user.h"
#include "memory.h"

int assign_drives (void);   /* Initialization of low level I/O module */


#if FF_MULTI_PARTITION

/* This is an example of volume - partition resolution table */

PARTITION VolToPart[FF_VOLUMES] =
{
    {0, 1}, /* "0:" <== PD# 0, 1st partition */
    {0, 2}, /* "1:" <== PD# 0, 2nd partition */
    {0, 3}, /* "2:" <== PD# 0, 3rd partition */
    {0, 4}, /* "3:" <== PD# 0, 4th partition */
    {1, 0}, /* "4:" <== PD# 1, auto detect */
    {2, 0}, /* "5:" <== PD# 2, auto detect */
    {3, 0}, /* "6:" <== PD# 3, auto detect */
    {4, 0}  /* "7:" <== PD# 4, auto detect */
};
#endif


#define LINE_LEN 300


/*---------------------------------------------------------*/
/* Work Area                                               */
/*---------------------------------------------------------*/

extern long long AccSize;           /* Work register for scan_files() */
extern WORD AccFiles, AccDirs;
extern FILINFO Finfo;

char Line[LINE_LEN];        /* Console input/output buffer */
HANDLE hConOut, hConIn;
WORD CodePage = FF_CODE_PAGE;

extern FATFS FatFs[FF_VOLUMES]; /* Filesystem object for logical drive */
extern BYTE Buff[262144];           /* Working buffer */

#if FF_USE_FASTSEEK
DWORD SeekTbl[16];          /* Link map table for fast seek feature */
#endif



/*-------------------------------------------------------------------*/
/* User Provided RTC Function for FatFs module                       */
/*-------------------------------------------------------------------*/
/* This is a real time clock service to be called from FatFs module. */
/* This function is needed when FF_FS_READONLY == 0 and FF_FS_NORTC == 0 */

DWORD get_fattime (void)
{
    SYSTEMTIME tm;

    /* Get local time */
    GetLocalTime(&tm);

    /* Pack date and time into a DWORD variable */
    return    ((DWORD)(tm.wYear - 1980) << 25)
              | ((DWORD)tm.wMonth << 21)
              | ((DWORD)tm.wDay << 16)
              | (WORD)(tm.wHour << 11)
              | (WORD)(tm.wMinute << 5)
              | (WORD)(tm.wSecond >> 1);
}




/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */


int xatoll (        /* 0:Failed, 1:Successful */
    char **str, /* Pointer to pointer to the string */
    QWORD *res      /* Pointer to a valiable to store the value */
)
{
    QWORD val;
    UINT r;
    char c;


    *res = 0;
    while ((c = **str) == ' ') (*str)++;    /* Skip leading spaces */

    if (c == '0')
    {
        c = *(++(*str));
        switch (c)
        {
            case 'x':       /* hexdecimal */
                r = 16;
                c = *(++(*str));
                break;
            case 'b':       /* binary */
                r = 2;
                c = *(++(*str));
                break;
            default:
                if (c <= ' ') return 1; /* single zero */
                if (c < '0' || c > '9') return 0;   /* invalid char */
                r = 8;      /* octal */
        }
    }
    else
    {
        if (c < '0' || c > '9') return 0;   /* EOL or invalid char */
        r = 10;         /* decimal */
    }

    val = 0;
    while (c > ' ')
    {
        if (c >= 'a') c -= 0x20;
        c -= '0';
        if (c >= 17)
        {
            c -= 7;
            if (c <= 9) return 0;   /* invalid char */
        }
        if (c >= r) return 0;       /* invalid char for current radix */
        val = val * r + c;
        c = *(++(*str));
    }

    *res = val;
    return 1;
}


int xatoi (
    char **str, /* Pointer to pointer to the string */
    DWORD *res      /* Pointer to a valiable to store the value */
)
{
    QWORD d;


    *res = 0;
    if (!xatoll(str, &d)) return 0;
    *res = (DWORD)d;
    return 1;
}


void zputc (
    char c
)
{
    DWORD wc;

    WriteConsole(hConOut, &c, 1, &wc, 0);
}


void get_line (
    char* buf,
    UINT len
)
{
    UINT i = 0;
    DWORD n;


    for (;;)
    {
        ReadConsole(hConIn, &buf[i], 1, &n, 0);
        if (buf[i] == '\b')
        {
            if (i) i--;
            continue;
        }
        if (buf[i] == '\r')
        {
            buf[i] = 0;
            break;
        }
        if ((UINT)buf[i] >= ' ' && i + n < len) i += n;
    }
}







/*----------------------------------------------*/
/* Dump a block of byte array                   */

void put_dump (
    const BYTE* buff,   /* Pointer to the byte array to be dumped */
    DWORD addr,         /* Heading address value */
    int cnt             /* Number of bytes to be dumped */
)
{
    int i;


    zprintf("%08X:", addr);

    for (i = 0; i < cnt; i++)
    {
        zprintf(" %02X", buff[i]);
    }

    zputc(' ');
    for (i = 0; i < cnt; i++)
    {
        zputc((buff[i] >= ' ' && buff[i] <= '~') ? buff[i] : '.');
    }

    zputc('\n');
}



UINT forward (
    const BYTE* buf,
    UINT btf
)
{
    UINT i;


    if (btf)    /* Transfer call? */
    {
        for (i = 0; i < btf; i++) zputc((TCHAR)buf[i]);
        return btf;
    }
    else        /* Sens call */
    {
        return 1;
    }
}








void put_rc (FRESULT rc)
{
    const char *p =
        "OK\0DISK_ERR\0INT_ERR\0NOT_READY\0NO_FILE\0NO_PATH\0INVALID_NAME\0"
        "DENIED\0EXIST\0INVALID_OBJECT\0WRITE_PROTECTED\0INVALID_DRIVE\0"
        "NOT_ENABLED\0NO_FILE_SYSTEM\0MKFS_ABORTED\0TIMEOUT\0LOCKED\0"
        "NOT_ENOUGH_CORE\0TOO_MANY_OPEN_FILES\0INVALID_PARAMETER\0";
    FRESULT i;

    for (i = 0; i != rc && *p; i++)
    {
        while(*p++) ;
    }
    zprintf("rc=%u FR_%s\n", (UINT)rc, p);
}



const char HelpStr[] =
{
    "[Disk contorls]\n"
    " di <pd#> - Initialize disk\n"
    " dd [<pd#> <sect>] - Dump a secrtor\n"
    " ds <pd#> - Show disk status\n"
    " dl <file> - Load FAT image into RAM disk (pd#0)\n"
    " ds <file> - Save RAM disk (pd#0) into FAT image file\n"
    "[Buffer contorls]\n"
    " bd <ofs> - Dump working buffer\n"
    " be <ofs> [<data>] ... - Edit working buffer\n"
    " br <pd#> <sect> <count> - Read disk into working buffer\n"
    " bw <pd#> <sect> <count> - Write working buffer into disk\n"
    " bf <val> - Fill working buffer\n"
    "[Filesystem contorls]\n"
    " fi <ld#> [<opt>] - Force initialized the volume\n"
    " fs [<path>] - Show volume status\n"
    " fl [<path>] - Show a directory\n"
    " fL <path> <pat> - Find a directory\n"
    " fo <mode> <file> - Open a file\n"
    " fc - Close the file\n"
    " fe <ofs> - Move fp in normal seek\n"
    " fE <ofs> - Move fp in fast seek or Create link table\n"
    " ff <len> - Forward file data to the console\n"
    " fh <fsz> <opt> - Allocate a contiguous block to the file\n"
    " fd <len> - Read and dump the file\n"
    " fr <len> - Read the file\n"
    " fw <len> <val> - Write to the file\n"
    " fn <object name> <new name> - Rename an object\n"
    " fu <object name> - Unlink an object\n"
    " fv - Truncate the file at current fp\n"
    " fk <dir name> - Create a directory\n"
    " fa <atrr> <mask> <object name> - Change object attribute\n"
    " ft <year> <month> <day> <hour> <min> <sec> <object name> - Change timestamp of an object\n"
    " fx <src file> <dst file> - Copy a file\n"
    " fz <src file> - Copy a file to fatfs\n"
    " fy <src file> - Copy a file from fatfs\n"
    " fg <path> - Change current directory\n"
    " fj <path> - Change current drive\n"
    " fq - Show current directory path\n"
    " fb <name> - Set volume label\n"
    " fm <ld#> <type> <au> - Create FAT volume\n"
    " fp <pd#> <p1 size> <p2 size> <p3 size> <p4 size> - Divide physical drive\n"
    " p <cp#> - Set code page\n"
    "\n"
};



int set_console_size (
    HANDLE hcon,
    int width,
    int height,
    int bline
)
{
    COORD dim;
    SMALL_RECT rect;


    dim.X = (SHORT)width;
    dim.Y = (SHORT)bline;
    rect.Top = rect.Left = 0;
    rect.Right = (SHORT)(width - 1);
    rect.Bottom = (SHORT)(height - 1);

    if (SetConsoleScreenBufferSize(hcon, dim) && SetConsoleWindowInfo(hcon, TRUE, &rect) ) return 1;

    return 0;
}





/*-----------------------------------------------------------------------*/
/* Main                                                                  */
/*-----------------------------------------------------------------------*/


int main (void)
{
    char *ptr, *ptr2, pool[64];
    char tpool[128];
    DWORD p1, p2, p3;
    QWORD px;
    BYTE *buf;
    UINT s1, s2, cnt;
    WORD w;
    DWORD dw, ofs = 0, sect = 0, drv = 0;
    const char *ft[] = {"", "FAT12", "FAT16", "FAT32", "exFAT"};
    const char *uni[] = {"ANSI/OEM", "UTF-16", "UTF-8", "UTF-32"};
    HANDLE h;
    FRESULT fr;
    DRESULT dr;
    FATFS *fs;              /* Pointer to file system object */
    DIR dir;                /* Directory object */
    FIL file[2];            /* File objects */

    mymem_init();


    hConIn = GetStdHandle(STD_INPUT_HANDLE);
    hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
    set_console_size(hConOut, 100, 35, 500);

#if FF_CODE_PAGE != 0
#if 0
    if (GetConsoleCP() != FF_CODE_PAGE)
    {
        if (!SetConsoleCP(FF_CODE_PAGE) || !SetConsoleOutputCP(FF_CODE_PAGE))
        {
            zprintf(L"Error: Failed to change the console code page.\n");
        }
    }
#endif
#else
    w = GetConsoleCP();
    zprintf("f_setcp(%u)\n", w);
    put_rc(f_setcp(w));
#endif

    zprintf("FatFs module test monitor (%s, CP%u, %s)\n\n", FF_USE_LFN ? "LFN" : "SFN", FF_CODE_PAGE, uni[FF_LFN_UNICODE]);

    sprintf(pool, "FatFs debug console (%s, CP%u, %s)", FF_USE_LFN ? "LFN" : "SFN", FF_CODE_PAGE, uni[FF_LFN_UNICODE]);
    SetConsoleTitle(pool);

    assign_drives();    /* Find physical drives on the PC */

#if FF_MULTI_PARTITION
    zprintf("\nMultiple partition is enabled. Each logical drive is tied to the patition as follows:\n");
    for (cnt = 0; cnt < sizeof VolToPart / sizeof (PARTITION); cnt++)
    {
        const char *pn[] = {"auto detect", L"1st partition", L"2nd partition", L"3rd partition", L"4th partition"};

        zprintf("\"%u:\" <== Disk# %u, %s\n", cnt, VolToPart[cnt].pd, pn[VolToPart[cnt].pt]);
    }
    zprintf("\n");
#else
    zprintf("\nMultiple partition is disabled.\nEach logical drive is tied to the same physical drive number.\n\n");
#endif


    for (;;)
    {
        zprintf(">");
        get_line(Line, LINE_LEN);
        ptr = Line;
        printf("cmd line = %s\r\n", Line);

        switch (*ptr++)     /* Branch by primary command character */
        {

            case 'q' :  /* Exit program */
                return 0;

            case '?':       /* Show usage */
                zprintf(HelpStr);
                break;

            case 'T' :

                /* Quick test space */
                while (*ptr == ' ') ptr++;
                fr = delete_directory(ptr);
                put_rc(fr);

                break;

            case 'd' :  /* Disk I/O command */
                switch (*ptr++)     /* Branch by secondary command character */
                {
                    case 'd' :  /* dd [<pd#> <sect>] - Dump a secrtor */
                        if (!xatoi(&ptr, &p1))
                        {
                            p1 = drv;
                            p2 = sect;
                        }
                        else
                        {
                            if (!xatoi(&ptr, &p2)) break;
                        }
                        dr = disk_read((BYTE)p1, Buff, p2, 1);
                        if (dr)
                        {
                            zprintf("rc=%d\n", (WORD)dr);
                            break;
                        }
                        zprintf("PD#:%u LBA:%lu\n", p1, p2);
                        if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) != RES_OK) break;
                        sect = p2 + 1;
                        drv = p1;
                        for (buf = Buff, ofs = 0; ofs < w; buf += 16, ofs += 16)
                        {
                            put_dump(buf, ofs, 16);
                        }
                        break;

                    case 'i' :  /* di <pd#> - Initialize physical drive */
                        if (!xatoi(&ptr, &p1)) break;
                        dr = disk_initialize((BYTE)p1);
                        zprintf("rc=%d\n", dr);
                        if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w) == RES_OK)
                        {
                            zprintf("Sector size = %u\n", w);
                        }
                        if (disk_ioctl((BYTE)p1, GET_SECTOR_COUNT, &dw) == RES_OK)
                        {
                            zprintf("Number of sectors = %u\n", dw);
                        }
                        break;

                    case 'l' :  /* dl <image file> - Load image of a FAT volume into RAM disk */
                        while (*ptr == ' ') ptr++;
                        if (disk_ioctl(0, 200, ptr) == RES_OK)
                        {
                            zprintf("Ok\n");
                        }
                        break;
                    case 's' : /* ds <image file> - Save image of a FAT volume from RAM disk */
                        while (*ptr == ' ') ptr++;
                        if (disk_ioctl(0, 201, ptr) == RES_OK)
                        {
                            zprintf("Ok\n");
                        }
                        break;

                }
                break;

            case 'b' :  /* Buffer control command */
                switch (*ptr++)     /* Branch by secondary command character */
                {
                    case 'd' :  /* bd <ofs> - Dump Buff[] */
                        if (!xatoi(&ptr, &p1)) break;
                        for (buf = &Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, buf += 16, ofs += 16)
                        {
                            put_dump(buf, ofs, 16);
                        }
                        break;

                    case 'e' :  /* be <ofs> [<data>] ... - Edit Buff[] */
                        if (!xatoi(&ptr, &p1)) break;
                        if (xatoi(&ptr, &p2))
                        {
                            do
                            {
                                Buff[p1++] = (BYTE)p2;
                            }
                            while (xatoi(&ptr, &p2));
                            break;
                        }
                        for (;;)
                        {
                            zprintf("%04X %02X-", (WORD)(p1), (WORD)Buff[p1]);
                            get_line(Line, LINE_LEN);
                            ptr = Line;
                            if (*ptr == '.') break;
                            if (*ptr < ' ')
                            {
                                p1++;
                                continue;
                            }
                            if (xatoi(&ptr, &p2))
                            {
                                Buff[p1++] = (BYTE)p2;
                            }
                            else
                            {
                                zprintf("???\n");
                            }
                        }
                        break;

                    case 'r' :  /* br <pd#> <sector> <count> - Read disk into Buff[] */
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
                        zprintf("rc=%u\n", disk_read((BYTE)p1, Buff, p2, p3));
                        break;

                    case 'w' :  /* bw <pd#> <sect> <count> - Write Buff[] into disk */
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
                        zprintf("rc=%u\n", disk_write((BYTE)p1, Buff, p2, p3));
                        break;

                    case 'f' :  /* bf <n> - Fill Buff[] */
                        if (!xatoi(&ptr, &p1)) break;
                        memset(Buff, p1, sizeof Buff);
                        break;

                    case 's' :  /* bs - Save Buff[] */
                        while (*ptr == ' ') ptr++;
                        h = CreateFile("Buff.bin", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
                        if (h != INVALID_HANDLE_VALUE)
                        {
                            WriteFile(h, Buff, sizeof Buff, &dw, 0);
                            CloseHandle(h);
                            zprintf("Ok.\n");
                        }
                        break;

                }
                break;

            case 'f' :  /* FatFs test command */
                switch (*ptr++)     /* Branch by secondary command character */
                {

                    case 'i' :  /* fi <pd#> <opt> <path> - Force initialized the logical drive */
                        if (!xatoi(&ptr, &p1) || (UINT)p1 > 9) break;
                        if (!xatoi(&ptr, &p2)) p2 = 0;
                        sprintf(ptr, "%d:", p1);
                        put_rc(f_mount(&FatFs[p1], ws2ts(ptr), (BYTE)p2));
                        break;

                    case 's' :  /* fs [<path>] - Show logical drive status */
                        while (*ptr == ' ') ptr++;
                        ptr2 = ptr;
#if FF_FS_READONLY
                        fr = f_opendir(&dir, ts2ws(ptr));
                        if (fr == FR_OK)
                        {
                            fs = dir.obj.fs;
                            f_closedir(&dir);
                        }
#else
                        fr = f_getfree(ws2ts(ptr), (DWORD*)&p1, &fs);
#endif
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }
                        zprintf("FAT type = %s\n", ft[fs->fs_type]);
                        zprintf("Cluster size = %lu bytes\n",
#if FF_MAX_SS != FF_MIN_SS
                                (DWORD)fs->csize * fs->ssize);
#else
                                (DWORD)fs->csize * FF_MAX_SS);
#endif
                        if (fs->fs_type < FS_FAT32) zprintf("Root DIR entries = %u\n", fs->n_rootdir);
                        zprintf("Sectors/FAT = %lu\nNumber of FATs = %u\nNumber of clusters = %lu\nVolume start sector = %lu\nFAT start sector = %lu\nRoot DIR start %s = %lu\nData start sector = %lu\n\n",
                                fs->fsize, fs->n_fats, fs->n_fatent - 2, fs->volbase, fs->fatbase, fs->fs_type >= FS_FAT32 ? "cluster" : "sector", fs->dirbase, fs->database);
#if FF_USE_LABEL
                        fr = f_getlabel(ws2ts(ptr2), tpool, &dw);
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }
                        if (tpool[0])
                        {
                            zprintf("Volume name is %s\n", ts2ws(tpool));
                        }
                        else
                        {
                            zprintf("No volume label\n");
                        }
                        zprintf("Volume S/N is %04X-%04X\n", dw >> 16, dw & 0xFFFF);
#endif
                        zprintf("...");
                        AccSize = AccFiles = AccDirs = 0;
                        fr = scan_files(ptr);
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }
                        p2 = (fs->n_fatent - 2) * fs->csize;
                        p3 = p1 * fs->csize;
#if FF_MAX_SS != FF_MIN_SS
                        p2 *= fs->ssize / 512;
                        p3 *= fs->ssize / 512;
#endif
                        p2 /= 2;
                        p3 /= 2;
                        zprintf("\r%u files, %llu bytes.\n%u folders.\n%lu KiB total disk space.\n",
                                AccFiles, AccSize, AccDirs, p2);
#if !FF_FS_READONLY
                        zprintf("%lu KiB available.\n", p3);
#endif
                        break;

                    case 'l' :  /* fl [<path>] - Directory listing */
                        fr = dir_directory(ptr);
                        put_rc(fr);
                        break;
#if FF_USE_FIND
                    case 'L' :  /* fL <path> <pattern> - Directory search */

                        while (*ptr == ' ') ptr++;
                        ptr2 = ptr;
                        while (*ptr != ' ') ptr++;
                        *ptr++ = 0;
                        fr = find_directory(ptr2, ptr);
                        put_rc(fr);
                        break;
#endif
                    case 'o' :  /* fo <mode> <file> - Open a file */
                        if (!xatoi(&ptr, &p1)) break;
                        while (*ptr == ' ') ptr++;
                        fr = f_open(&file[0], ws2ts(ptr), (BYTE)p1);
                        put_rc(fr);
                        break;

                    case 'c' :  /* fc - Close a file */
                        put_rc(f_close(&file[0]));
                        break;

                    case 'r' :  /* fr <len> - read file */
                        if (!xatoi(&ptr, &p1)) break;
                        p2 = 0;
                        while (p1)
                        {
                            if (p1 >= sizeof Buff)
                            {
                                cnt = sizeof Buff;
                                p1 -= sizeof Buff;
                            }
                            else
                            {
                                cnt = p1;
                                p1 = 0;
                            }
                            fr = f_read(&file[0], Buff, cnt, &s2);
                            if (fr != FR_OK)
                            {
                                put_rc(fr);
                                break;
                            }
                            p2 += s2;
                            if (cnt != s2) break;
                        }
                        zprintf("%lu bytes read.\n", p2);
                        break;

                    case 'd' :  /* fd <len> - read and dump file from current fp */
                        if (!xatoi(&ptr, &p1)) p1 = 128;
                        ofs = (DWORD)file[0].fptr;
                        while (p1)
                        {
                            if (p1 >= 16)
                            {
                                cnt = 16;
                                p1 -= 16;
                            }
                            else
                            {
                                cnt = p1;
                                p1 = 0;
                            }
                            fr = f_read(&file[0], Buff, cnt, &cnt);
                            if (fr != FR_OK)
                            {
                                put_rc(fr);
                                break;
                            }
                            if (!cnt) break;
                            put_dump(Buff, ofs, cnt);
                            ofs += 16;
                        }
                        break;

                    case 'e' :  /* fe <ofs> - Seek file pointer */
                        if (!xatoll(&ptr, &px)) break;
                        fr = f_lseek(&file[0], (FSIZE_t)px);
                        put_rc(fr);
                        if (fr == FR_OK)
                        {
                            zprintf("fptr = %llu(0x%llX)\n", (QWORD)file[0].fptr, (QWORD)file[0].fptr);
                        }
                        break;
#if FF_USE_FASTSEEK
                    case 'E' :  /* fE - Enable fast seek and initialize cluster link map table */
                        file[0].cltbl = SeekTbl;            /* Enable fast seek (set address of buffer) */
                        SeekTbl[0] = sizeof SeekTbl / sizeof * SeekTbl; /* Buffer size */
                        fr = f_lseek(&file[0], CREATE_LINKMAP); /* Create link map table */
                        put_rc(fr);
                        if (fr == FR_OK)
                        {
                            zprintf((SeekTbl[0] > 4) ? "fragmented in %d.\n" : "contiguous.\n", SeekTbl[0] / 2 - 1);
                            zprintf("%u items used.\n", SeekTbl[0]);

                        }
                        if (fr == FR_NOT_ENOUGH_CORE)
                        {
                            zprintf("%u items required to create the link map table.\n", SeekTbl[0]);
                        }
                        break;
#endif  /* FF_USE_FASTSEEK */
#if FF_FS_RPATH >= 1
                    case 'g' :  /* fg <path> - Change current directory */
                        while (*ptr == ' ') ptr++;
                        put_rc(f_chdir(ws2ts(ptr)));
                        break;
                    case 'j' :  /* fj <path> - Change current drive */
                        while (*ptr == ' ') ptr++;
                        put_rc(f_chdrive(ws2ts(ptr)));
                        break;
#if FF_FS_RPATH >= 2
                    case 'q' :  /* fq - Show current dir path */
                        fr = f_getcwd(tpool, sizeof tpool / sizeof tpool[0]);
                        if (fr)
                        {
                            put_rc(fr);
                        }
                        else
                        {
                            zprintf("%s\n", ts2ws(tpool));
                        }
                        break;
#endif
#endif
#if FF_USE_FORWARD
                    case 'f' :  /* ff <len> - Forward data */
                        if (!xatoi(&ptr, &p1)) break;
                        fr = f_forward(&file[0], forward, p1, &s1);
                        put_rc(fr);
                        if (fr == FR_OK) zprintf("\n%u bytes tranferred.\n", s1);
                        break;
#endif
#if !FF_FS_READONLY
                    case 'w' :  /* fw <len> <val> - Write file */
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
                        memset(Buff, p2, sizeof Buff);
                        p2 = 0;
                        while (p1)
                        {
                            if (p1 >= sizeof Buff)
                            {
                                cnt = sizeof Buff;
                                p1 -= sizeof Buff;
                            }
                            else
                            {
                                cnt = p1;
                                p1 = 0;
                            }
                            fr = f_write(&file[0], Buff, cnt, &s2);
                            if (fr != FR_OK)
                            {
                                put_rc(fr);
                                break;
                            }
                            p2 += s2;
                            if (cnt != s2) break;
                        }
                        zprintf("%lu bytes written.\n", p2);
                        break;

                    case 'v' :  /* fv - Truncate file */
                        put_rc(f_truncate(&file[0]));
                        break;

                    case 'n' :  /* fn <name> <new_name> - Change file/dir name */
                        while (*ptr == ' ') ptr++;
                        ptr2 = strchr(ptr, ' ');
                        if (!ptr2) break;
                        *ptr2++ = 0;
                        while (*ptr2 == ' ') ptr2++;
                        put_rc(f_rename(ws2ts(ptr), ws2ts(ptr2)));
                        break;

                    case 'u' :  /* fu <name> - Unlink a file/dir */
                        while (*ptr == ' ') ptr++;
                        put_rc(f_unlink(ws2ts(ptr)));
                        break;

                    case 'k' :  /* fk <name> - Create a directory */
                        while (*ptr == ' ') ptr++;
                        printf("make dir %s\r\n", ws2ts(ptr));
                        put_rc(f_mkdir(ws2ts(ptr)));
                        break;

                    case 'x' : /* fx <src_name> <dst_name> - Copy a file */
                        while (*ptr == ' ') ptr++;
                        ptr2 = strchr(ptr, ' ');
                        if (!ptr2) break;
                        *ptr2++ = 0;
                        while (*ptr2 == ' ') ptr2++;
                        zprintf("Opening \"%s\"", ptr);
                        fr = f_open(&file[0], ws2ts(ptr), FA_OPEN_EXISTING | FA_READ);
                        zprintf("\n");
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }
                        while (*ptr2 == ' ') ptr2++;
                        zprintf("Creating \"%s\"", ptr2);
                        fr = f_open(&file[1], ws2ts(ptr2), FA_CREATE_ALWAYS | FA_WRITE);
                        zprintf("\n");
                        if (fr)
                        {
                            put_rc(fr);
                            f_close(&file[0]);
                            break;
                        }
                        zprintf("Copying...");
                        p1 = 0;
                        for (;;)
                        {
                            fr = f_read(&file[0], Buff, sizeof Buff, &s1);
                            if (fr || s1 == 0) break;   /* error or eof */
                            fr = f_write(&file[1], Buff, s1, &s2);
                            p1 += s2;
                            if (fr || s2 < s1) break;   /* error or disk full */
                        }
                        zprintf("\n");
                        if (fr) put_rc(fr);
                        f_close(&file[0]);
                        f_close(&file[1]);
                        zprintf("%lu bytes copied.\n", p1);
                        break;
                    case 'z':
                    {
                        while (*ptr == ' ') ptr++;
                        zprintf("Opening \"%s\"", ptr);
                        fr = f_open(&file[0], ws2ts(ptr), FA_CREATE_ALWAYS | FA_WRITE);
                        zprintf("\n");
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }

                        h = CreateFile(ws2ts(ptr), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

                        if (h == INVALID_HANDLE_VALUE)
                        {
                            printf("open file %s error\r\n", ptr);
                            f_close(&file[0]);
                            break;
                        }

                        zprintf("Copying...");
                        p1 = 0;
                        for (;;)
                        {
                            DWORD br;

                            if (ReadFile(h, Buff, sizeof Buff, &s1, 0))
                            {
                                //RES_OK;
                                fr = RES_OK;
                            }
                            else
                            {
                                break;
                            }
                            if (fr || s1 == 0) break;   /* error or eof */
                            fr = f_write(&file[0], Buff, s1, &s2);
                            p1 += s2;
                            if (fr || s2 < s1) break;   /* error or disk full */
                        }
                        zprintf("\n");
                        if (fr) put_rc(fr);
                        f_close(&file[0]);
                        CloseHandle(h);
                        zprintf("%lu bytes copied.\n", p1);
                    }
                    break;
                    case 'y':
                    {
                        while (*ptr == ' ') ptr++;
                        zprintf("Opening \"%s\"", ptr);
                        fr = f_open(&file[0], ws2ts(ptr), FA_OPEN_EXISTING | FA_READ);
                        zprintf("\n");
                        if (fr)
                        {
                            put_rc(fr);
                            break;
                        }

                        h = CreateFile(ws2ts(ptr), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

                        if (h == INVALID_HANDLE_VALUE)
                        {
                            printf("open file %s error\r\n", ptr);
                            f_close(&file[0]);
                            break;
                        }

                        zprintf("Copying...");
                        p1 = 0;
                        for (;;)
                        {
                            fr = f_read(&file[0], Buff, sizeof Buff, &s1);
                            if (fr || s1 == 0) break;   /* error or eof */

                            //fr = f_write(&file[1], Buff, s1, &s2);
                            if (WriteFile(h, Buff, s1, &s2, 0))
                            {
                                fr = RES_OK;
                            }
                            else
                            {
                                break;
                            }
                            p1 += s2;
                            if (fr || s2 < s1) break;   /* error or disk full */
                        }
                        zprintf("\n");
                        if (fr) put_rc(fr);
                        f_close(&file[0]);
                        CloseHandle(h);
                        zprintf("%lu bytes copied.\n", p1);
                    }
                    break;
#if FF_USE_EXPAND
                    case 'h':   /* fh <fsz> <opt> - Allocate contiguous block */
                        if (!xatoll(&ptr, &px) || !xatoi(&ptr, &p2)) break;
                        fr = f_expand(&file[0], (FSIZE_t)px, (BYTE)p2);
                        put_rc(fr);
                        break;
#endif
#if FF_USE_CHMOD
                    case 'a' :  /* fa <atrr> <mask> <name> - Change file/dir attribute */
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
                        while (*ptr == L' ') ptr++;
                        put_rc(f_chmod(ws2ts(ptr), (BYTE)p1, (BYTE)p2));
                        break;

                    case 't' :  /* ft <year> <month> <day> <hour> <min> <sec> <name> - Change timestamp of a file/dir */
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
                        Finfo.fdate = (WORD)(((p1 - 1980) << 9) | ((p2 & 15) << 5) | (p3 & 31));
                        if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
                        Finfo.ftime = (WORD)(((p1 & 31) << 11) | ((p2 & 63) << 5) | ((p3 >> 1) & 31));
                        while (FF_USE_LFN && *ptr == L' ') ptr++;
                        put_rc(f_utime(ws2ts(ptr), &Finfo));
                        break;
#endif
#if FF_USE_LABEL
                    case 'b' :  /* fb <name> - Set volume label */
                        while (*ptr == ' ') ptr++;
                        put_rc(f_setlabel(ws2ts(ptr)));
                        break;
#endif
#if FF_USE_MKFS
                    case 'm' :  /* fm <ld#> <partition rule> <cluster size> - Create filesystem */
                        if (!xatoi(&ptr, &p1) || (UINT)p1 > 9 || !xatoi(&ptr, &p2) || !xatoi(&ptr, &p3)) break;
                        zprintf("The volume will be formatted. Are you sure? (Y/n)=");
                        get_line(ptr, 256);
                        if (*ptr != 'Y') break;
                        sprintf(ptr, "%u:", (UINT)p1);
                        put_rc(f_mkfs(ws2ts(ptr), (BYTE)p2, p3, Buff, sizeof Buff));
                        break;
#if FF_MULTI_PARTITION
                    case 'p' :  /* fp <pd#> <size1> <size2> <size3> <size4> - Create partition table */
                    {
                        DWORD pts[4];

                        if (!xatoi(&ptr, &p1)) break;
                        xatoi(&ptr, &pts[0]);
                        xatoi(&ptr, &pts[1]);
                        xatoi(&ptr, &pts[2]);
                        xatoi(&ptr, &pts[3]);
                        zprintf("The physical drive %u will be re-partitioned. Are you sure? (Y/n)=", p1);
                        get_line(ptr, 256);
                        if (*ptr != 'Y') break;
                        put_rc(f_fdisk((BYTE)p1, pts, Buff));
                    }
                    break;
#endif  /* FF_MULTI_PARTITION */
#endif  /* FF_USE_MKFS */
#endif  /* !FF_FS_READONLY */
                }
                break;
#if FF_CODE_PAGE == 0
            case 'p' :  /* p <codepage> - Set code page */
                if (!xatoi(&ptr, &p1)) break;
                w = (WORD)p1;
                fr = f_setcp(w);
                put_rc(fr);
                if (fr == FR_OK)
                {
                    CodePage = w;
                    if (SetConsoleCP(w) && SetConsoleOutputCP(w))
                    {
                        zprintf("Console CP = %u\n", w);
                    }
                    else
                    {
                        zprintf("Failed to change the console CP.\n");
                    }
                }
                break;
#endif

        }
    }

}


