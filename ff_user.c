


#include "ff_user.h"

long long  AccSize;         /* Work register for scan_files() */
WORD AccFiles, AccDirs;

FILINFO Finfo;

DIR dir;                /* Directory object */

FATFS FatFs[FF_VOLUMES];    /* Filesystem object for logical drive */
BYTE Buff[262144];          /* Working buffer */




FRESULT scan_files (
    char* path      /* Pointer to the path name working buffer */
)
{
    DIR dir;
    FRESULT res;
    int i;


    if ((res = f_opendir(&dir, ws2ts(path))) == FR_OK)
    {
        i = strlen(path);
        while (((res = f_readdir(&dir, &Finfo)) == FR_OK) && Finfo.fname[0])
        {
            if (Finfo.fattrib & AM_DIR)
            {
                AccDirs++;
                *(path + i) = '/';
                strcpy(path + i + 1, ts2ws(Finfo.fname));
                res = scan_files(path);
                *(path + i) = '\0';
                if (res != FR_OK) break;
            }
            else
            {
                AccFiles++;
                AccSize += Finfo.fsize;
            }
        }
        f_closedir(&dir);
    }

    return res;
}




static FRESULT _delete_directory (
    char* path,    /* Working buffer with the directory to delete 保存当前目录名+子目录名 */
    int sz_buff,    /* Buffer size (items) 限制目录最大长度*/
    FILINFO* fno    /* Name read buffer */
)
{
    int i, j;
    FRESULT fr;
    DIR dir;


    fr = f_opendir(&dir, path); /* Open the directory */
    if (fr != FR_OK) return fr;

    for (i = 0; path[i]; i++) ; /* Get current path length */
    path[i++] = ('/');

    for (;;)
    {
        fr = f_readdir(&dir, fno);  /* Get a directory item */
        if (fr != FR_OK || !fno->fname[0]) break;   /* End of directory? */
        j = 0;
        do      /* Make a path name */
        {
            if (i + j >= sz_buff)   /* Buffer over flow? */
            {
                fr = 100;
                break;
            }
            path[i + j] = fno->fname[j];
        }
        while (fno->fname[j++]);
        if (fno->fattrib & AM_DIR)      /* Is it a directory? */
        {
            fr = _delete_directory(path, sz_buff, fno);  /* Delete the directory */
        }
        else
        {
            zprintf("%s\n", path);
            fr = f_unlink(ts2ws(path));    /* Delete the file */
        }
        if (fr != FR_OK) break;
    }

    path[--i] = 0;  /* Restore the path name */
    f_closedir(&dir);

    if (fr == FR_OK)
    {
        zprintf("%s\n", ts2ws(path));
        fr = f_unlink(path);  /* Delete the directory */
    }
    return fr;
}



FRESULT delete_directory (
    char* dir_path    /* the directory to delete  */
)
{
    FRESULT fr;
    int sz_buff = 256; /* Buffer size (items) 限制目录最大长度*/

    char *path = (char*)mymem_malloc(sz_buff);
    FILINFO* fno = (FILINFO*) mymem_malloc(sizeof(FILINFO));
    strcpy(path, dir_path);

    fr = _delete_directory(path, sz_buff, fno);

    mymem_free((int)path);
    mymem_free((int)fno);
    return fr;
}


FRESULT dir_directory(char *dir_path)
{
    FRESULT fr;
    UINT s1, s2, cnt, p1;
    FATFS *fs;              /* Pointer to file system object */
    char *ptr = dir_path;

    while (*ptr == ' ') ptr++;
    fr = f_opendir(&dir, ws2ts(ptr));
    if (fr)
    {
        return (fr);
    }
    AccSize = s1 = s2 = 0;
    for(;;)
    {
        fr = f_readdir(&dir, &Finfo);
        if ((fr != FR_OK) || !Finfo.fname[0]) break;
        if (Finfo.fattrib & AM_DIR)
        {
            s2++;
        }
        else
        {
            s1++;
            AccSize += Finfo.fsize;
        }
        zprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %10llu  ",
                (Finfo.fattrib & AM_DIR) ? 'D' : '-',
                (Finfo.fattrib & AM_RDO) ? 'R' : '-',
                (Finfo.fattrib & AM_HID) ? 'H' : '-',
                (Finfo.fattrib & AM_SYS) ? 'S' : '-',
                (Finfo.fattrib & AM_ARC) ? 'A' : '-',
                (Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
                (Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, (QWORD)Finfo.fsize);
#if FF_USE_LFN && FF_USE_FIND == 2
        zprintf("%-14s", ts2ws(Finfo.altname));
#endif
        zprintf("%s\n", ts2ws(Finfo.fname));
    }
    f_closedir(&dir);
    if (fr == FR_OK)
    {
        zprintf("%4u File(s),%11llu bytes total\n%4u Dir(s)", s1, AccSize, s2);
#if !FF_FS_READONLY
        fr = f_getfree(ws2ts(ptr), (DWORD*)&p1, &fs);
        if (fr == FR_OK)
        {
            zprintf(",%12llu bytes free", (QWORD)p1 * fs->csize * 512);
        }
#endif
        zprintf("\n");
    }
    else
    {
        return (fr);
    }
    return fr;
}

FRESULT find_directory(char *dir_path, char *pattern)
{
    FRESULT fr;
    
    fr = f_findfirst(&dir, &Finfo, ws2ts(dir_path), ws2ts(pattern));
    while (fr == FR_OK && Finfo.fname[0])
    {
        zprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %10llu  ",
                (Finfo.fattrib & AM_DIR) ? 'D' : '-',
                (Finfo.fattrib & AM_RDO) ? 'R' : '-',
                (Finfo.fattrib & AM_HID) ? 'H' : '-',
                (Finfo.fattrib & AM_SYS) ? 'S' : '-',
                (Finfo.fattrib & AM_ARC) ? 'A' : '-',
                (Finfo.fdate >> 9) + 1980, (Finfo.fdate >> 5) & 15, Finfo.fdate & 31,
                (Finfo.ftime >> 11), (Finfo.ftime >> 5) & 63, (QWORD)Finfo.fsize);
#if FF_USE_LFN && FF_USE_FIND == 2
        zprintf("%-12s  ", ts2ws(Finfo.altname));
#endif
        zprintf("%s\n", ts2ws(Finfo.fname));
        fr = f_findnext(&dir, &Finfo);
    }
    if (fr) return (fr);

    f_closedir(&dir);

    return fr;

}




