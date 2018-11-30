#ifndef FF_USER_H
#define FF_USER_H

#include <stdio.h>
#include "ff.h"
#include "diskio.h"
#include "memory.h"

#define zprintf printf


FRESULT scan_files (
	char* path		/* Pointer to the path name working buffer */
);



FRESULT delete_directory (
    char* dir_path    /* the directory to delete  */    
);

FRESULT dir_directory(char *dir_path);

FRESULT find_directory(char *dir_path, char *pattern);



#define ts2ws(x) (x) 
#define ws2ts(x) (x)





#endif

