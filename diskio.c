/*-----------------------------------------------------------------------*/
/* Low level disk control module for Win32              (C)ChaN, 2013    */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* Declarations of disk functions */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>


#define MAX_DRIVES	10		/* Max number of physical drives to be used */
#define	SZ_BLOCK	256		/* Block size to be returned by GET_BLOCK_SIZE command */

#define SZ_RAMDISK	128		/* Size of drive 0 (RAM disk) [MiB] */
#define SS_RAMDISK	512		/* Sector size of drive 0 (RAM disk) [byte] */


/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

#define	BUFSIZE 262144UL	/* Size of data transfer buffer */

typedef struct {
	DSTATUS	status;
	WORD sz_sector;
	DWORD n_sectors;
	HANDLE h_drive;
} STAT;


static int Drives;

static volatile STAT Stat[MAX_DRIVES];




static BYTE *Buffer, *RamDisk;	/* Poiter to the data transfer buffer and ramdisk */


/*-----------------------------------------------------------------------*/
/* Timer Functions                                                       */
/*-----------------------------------------------------------------------*/





int get_status (
	BYTE pdrv
)
{
	volatile STAT *stat = &Stat[pdrv];
	DWORD dw;


	if (pdrv == 0) {	/* RAMDISK */
		stat->sz_sector = SS_RAMDISK;
		if (stat->sz_sector < FF_MIN_SS || stat->sz_sector > FF_MAX_SS) return 0;
		stat->n_sectors = (DWORD)((QWORD)SZ_RAMDISK * 0x100000 / SS_RAMDISK);
		stat->status = 0;
		return 1;
	}

	
	return 1;
}




/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize Windows disk accesss layer                                 */
/*-----------------------------------------------------------------------*/

int assign_drives (void)
{
	BYTE pdrv, ndrv;
	char str[50];
	HANDLE h;
	OSVERSIONINFO vinfo = { sizeof (OSVERSIONINFO) };



	Buffer = VirtualAlloc(0, BUFSIZE, MEM_COMMIT, PAGE_READWRITE);
	if (!Buffer) return 0;

	RamDisk = VirtualAlloc(0, SZ_RAMDISK * 0x100000, MEM_COMMIT, PAGE_READWRITE);
	if (!RamDisk) return 0;

	//if (GetVersionEx(&vinfo) == FALSE) return 0;
	ndrv = 1;

	for (pdrv = 0; pdrv < ndrv; pdrv++) {
		if (pdrv) {	/* \\.\PhysicalDrive1 and later are mapped to disk funtion. */
			
		} else {	/* \\.\PhysicalDrive0 is never mapped to disk function, but RAM disk is mapped instead. */
			sprintf(str,  "RAM Disk");
		}
		printf("PD#%u <== %s", pdrv, str);
		if (get_status(pdrv)) {
			printf(" (%uMB, %u bytes * %u sectors)\n", (UINT)((LONGLONG)Stat[pdrv].sz_sector * Stat[pdrv].n_sectors / 1024 / 1024), Stat[pdrv].sz_sector, Stat[pdrv].n_sectors);
		} else {
			printf(" (Not Ready)\n");
		}
	}	

	Drives = pdrv;
	return pdrv;
}





/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive nmuber */
)
{
	DSTATUS sta;



	if (pdrv >= Drives) {
		sta = STA_NOINIT;
	} else {
		get_status(pdrv);
		sta = Stat[pdrv].status;
	}

	return sta;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0) */
)
{
	DSTATUS sta;


	if (pdrv >= Drives) {
		sta = STA_NOINIT;
	} else {
		sta = Stat[pdrv].status;
	}

	return sta;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,			/* Physical drive nmuber (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to read */
)
{
	DWORD nc, rnc;
	LARGE_INTEGER ofs;
	DSTATUS res;


	if (pdrv >= Drives || Stat[pdrv].status & STA_NOINIT ) {
		return RES_NOTRDY;
	}

	nc = (DWORD)count * Stat[pdrv].sz_sector;
	ofs.QuadPart = (LONGLONG)sector * Stat[pdrv].sz_sector;
	if (pdrv) {	/* Physical dirve */
		if (nc > BUFSIZE) {
			res = RES_PARERR;
		} else {
			if (SetFilePointer(Stat[pdrv].h_drive, ofs.LowPart, &ofs.HighPart, FILE_BEGIN) != ofs.LowPart) {
				res = RES_ERROR;
			} else {
				if (!ReadFile(Stat[pdrv].h_drive, Buffer, nc, &rnc, 0) || nc != rnc) {
					res = RES_ERROR;
				} else {
					memcpy(buff, Buffer, nc);
					res = RES_OK;
				}
			}
		}
	} else {	/* RAM disk */
		if (ofs.QuadPart >= (QWORD)SZ_RAMDISK * 1024 * 1024) {
			res = RES_ERROR;
		} else {
			memcpy(buff, RamDisk + ofs.LowPart, nc);
			res = RES_OK;
		}
	}
	return res;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0) */
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to write */
)
{
	DWORD nc = 0, rnc;
	LARGE_INTEGER ofs;
	DRESULT res;


	if (pdrv >= Drives || Stat[pdrv].status & STA_NOINIT ) {
		return RES_NOTRDY;
	}

	res = RES_OK;
	if (Stat[pdrv].status & STA_PROTECT) {
		res = RES_WRPRT;
	} else {
		nc = (DWORD)count * Stat[pdrv].sz_sector;
		if (nc > BUFSIZE) res = RES_PARERR;
	}

	ofs.QuadPart = (LONGLONG)sector * Stat[pdrv].sz_sector;
	if (pdrv) {	/* Physical drives */
		if (res == RES_OK) {
			if (SetFilePointer(Stat[pdrv].h_drive, ofs.LowPart, &ofs.HighPart, FILE_BEGIN) != ofs.LowPart) {
				res = RES_ERROR;
			} else {
				memcpy(Buffer, buff, nc);
				if (!WriteFile(Stat[pdrv].h_drive, Buffer, nc, &rnc, 0) || nc != rnc) {
					res = RES_ERROR;
				}
			}
		}
	} else {	/* RAM disk */
		if (ofs.QuadPart >= (QWORD)SZ_RAMDISK * 1024 * 1024) {
			res = RES_ERROR;
		} else {
			memcpy(RamDisk + ofs.LowPart, buff, nc);
			res = RES_OK;
		}
	}

	return res;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive data */
)
{
	DRESULT res;


	if (pdrv >= Drives || (Stat[pdrv].status & STA_NOINIT)) {
		return RES_NOTRDY;
	}

	res = RES_PARERR;
	switch (ctrl) {
	case CTRL_SYNC:			/* Nothing to do */
		res = RES_OK;
		break;

	case GET_SECTOR_COUNT:	/* Get number of sectors on the drive */
		*(DWORD*)buff = Stat[pdrv].n_sectors;
		res = RES_OK;
		break;

	case GET_SECTOR_SIZE:	/* Get size of sector for generic read/write */
		*(WORD*)buff = Stat[pdrv].sz_sector;
		res = RES_OK;
		break;

	case GET_BLOCK_SIZE:	/* Get internal block size in unit of sector */
		*(DWORD*)buff = SZ_BLOCK;
		res = RES_OK;
		break;

	case 200:				/* Load disk image file to the RAM disk (drive 0) */
		{
			HANDLE h;
			DWORD br;

			if (pdrv == 0) {
				h = CreateFile(buff, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
				if (h != INVALID_HANDLE_VALUE) {
					if (ReadFile(h, RamDisk, SZ_RAMDISK * 1024 * 1024, &br, 0)) {
						res = RES_OK;
					}
					CloseHandle(h);
				}
			}
		}
		break;
    case 201:				/* Save the RAM disk to disk image file (drive 0) */
		{
			HANDLE h;
			DWORD br;

			if (pdrv == 0) {
				h = CreateFile(buff, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
				if (h != INVALID_HANDLE_VALUE) {
                    //WriteFile(h, Buff, sizeof Buff, &dw, 0);
					if (WriteFile(h, RamDisk, SZ_RAMDISK * 1024 * 1024, &br, 0)) {
						res = RES_OK;
					}
					CloseHandle(h);
				}
			}
		}
		break;

	}

	return res;
}



