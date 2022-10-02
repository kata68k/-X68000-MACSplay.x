#ifndef	FILEMANAGER_C
#define	FILEMANAGER_C

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <io.h>
#include <time.h>
#include <doslib.h>
#include <iocslib.h>
#include <interrupt.h>

#include "inc/usr_macro.h"

#include "FileManager.h"

asm("	.include	doscall.mac");
		
/* �֐��̃v���g�^�C�v�錾 */
int16_t File_Load(int8_t *, void *, size_t, size_t);
int16_t FileHeader_Load(int8_t *, void *, size_t, size_t);
int16_t File_strSearch(FILE *, char *, int, long);
int16_t File_Save(int8_t *, void *, size_t, size_t);
int16_t GetFileLength(int8_t *, int32_t *);
void *MyMalloc(int32_t);
void *MyMallocJ(int32_t);
void *MyMallocHi(int32_t);
int16_t MyMfree(void *);
int16_t	MyMfreeJ(void *);
int16_t	MyMfreeHi(void *);
int32_t	MaxMemSize(int8_t);

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/* �t�@�C���ǂݍ��� */
/* *fname	�t�@�C���� */
/* *ptr		�i�[��̐擪�A�h���X */
/* size		�f�[�^�̃T�C�Y */
/* n		�f�[�^�̌� */
int16_t File_Load(int8_t *fname, void *ptr, size_t size, size_t n)
{
	FILE *fp;
	int16_t ret = 0;

	/* �t�@�C�����J���� */
	fp = fopen(fname, "rb");
	
	if(fp == NULL)
	{
		/* �t�@�C�����ǂݍ��߂܂��� */
		printf("error:%s�t�@�C����������܂���I\n", fname);
		ret = -1;
	}
	else
	{
		int i, j;
		size_t ld, ld_mod, ld_t;
		
		/* �f�[�^�����w�肵�Ȃ��ꍇ */
		if(n == 0)
		{
			/* �t�@�C���T�C�Y���擾 */
			n = filelength(fileno(fp));
		}
		
		fprintf(stderr, "0%%       50%%       100%%\n");
		fprintf(stderr, "+---------+---------+   <cancel:ESC>\n");
		ld = n / 100;
		ld_mod = n % 100;
		ld_t = 0;
		
		for (i = 0; i <= 100; i++) {
			/* �t�@�C���ǂݍ��� */
			if(i < 100)
			{
				fread (ptr, size, ld, fp);
				ptr += ld;
				ld_t += ld;
			}
			else
			{
				fread (ptr, size, ld_mod, fp);
				ld_t += ld_mod;
			}
			
			for (j = 0; j < i / 5 + 1; j++) {
				fprintf(stderr, "#");
			}
			fprintf(stderr, "\n");
			fprintf(stderr, "%3d%%(%d/%d)\n", i, ld_t, n);
			fprintf(stderr, "\033[2A");
			
			if((BITSNS( 0 ) & Bit_1) != 0u)
			{
				break;
			}
		}
		if(i >= 100)
		{
			fprintf(stderr, "\n\nfinish!\n");
		}
		else
		{
			fprintf(stderr, "\n\ncancel\n");
			ret = -1;
		}
		
		/* �t�@�C������� */
		fclose (fp);
	}

	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/* �t�@�C���ǂݍ��� */
/* *fname	�t�@�C���� */
/* *ptr		�i�[��̐擪�A�h���X */
/* size		�f�[�^�̃T�C�Y */
/* n		�f�[�^�̌� */
int16_t FileHeader_Load(int8_t *fname, void *ptr, size_t size, size_t n)
{
	FILE *fp;
	int16_t ret = 0;

	/* �t�@�C�����J���� */
	fp = fopen(fname, "rb");
	
	if(fp == NULL)
	{
		/* �t�@�C�����ǂݍ��߂܂��� */
		printf("error:%s�t�@�C����������܂���I\n", fname);
		ret = -1;
	}
	else
	{
		char sBuf[256];
		
		memset(sBuf, 0, sizeof(sBuf));
		fread (sBuf, sizeof(char), 8, fp);
		
		if(strcmp(sBuf, "MACSDATA") == 0)	/* MACSDATA���� */
		{
			char toolver[2];
			long ld;
			
			fseek(fp, 8L, SEEK_SET);
			memset(sBuf, 0, sizeof(sBuf));
			fread (&toolver[0], sizeof(char), 1, fp);	/* �o�[�W���� H */
			fseek(fp, 9L, SEEK_SET);
			memset(sBuf, 0, sizeof(sBuf));
			fread (&toolver[1], sizeof(char), 1, fp);	/* �o�[�W���� L */
			ret = (int16_t)((toolver[0]<<8) + toolver[1]);
			
			fseek(fp, 10L, SEEK_SET);
			memset(sBuf, 0, sizeof(sBuf));
			fread (&ld, sizeof(long), 1, fp);	/* �t�@�C���T�C�Y */
			
			if( (toolver[0] == 1) && (toolver[1] > 0x16))
			{
				printf("   ");
				if(File_strSearch( fp, "TITLE:", 6, 14L ) == 0)		/* TITLE: */
				{
					printf("\n");
				}
				printf(" ");
				if(File_strSearch( fp, "COMMENT:", 8, 14L ) == 0)		/* COMMENT: */
				{
					printf("\n");
				}
			}
			
			printf("FILEINFO:");
			printf("DataVer=%x.%x ", toolver[0], toolver[1]);
			printf("FileSize=%ld[MB] ", ld>>20);

			if( (toolver[0] == 1) && (toolver[1] > 0x16))
			{
				printf("PCM=");
				if(File_strSearch( fp, "DUALPCM/", 8, 14L ) == 0)		/* DUALPCM/ */
				{
					printf("\n");
				}
				else if(File_strSearch( fp, "PCM8PP", 6, 14L ) == 0)	/* PCM8PP */
				{
					printf("\n");
				}
				else if(File_strSearch( fp, "ADPCM", 5, 14L ) == 0)	/* ADPCM */
				{
					printf("\n");
				}
				else
				{
					/* �������Ȃ� */
				}
			}
			printf("\n");
		}
		
		/* �t�@�C������� */
		fclose (fp);
	}
	return ret;
}
	
/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/* �t�@�C���ۑ� */
/* *fname	�t�@�C���� */
/* *ptr		�i�[��̐擪�A�h���X */
/* size		�f�[�^�̃T�C�Y */
/* n		�f�[�^�̌� */
int16_t File_strSearch(FILE *fp, char *str, int len, long file_ofst)
{
	int16_t ret = 0;
	
	int i;
	int cnt, sMeslen;
	char sBuf[256];
	
	sMeslen = 0;
	
	/* �T�����L�[���[�h����NULL�܂ŕ\������ */
	for(i=0; i < 256; i++)
	{
		fseek(fp, file_ofst + i, SEEK_SET);
		
		memset(sBuf, 0, sizeof(sBuf));
		fread (sBuf, sizeof(char), len, fp);	/* �t�@�C���ǂݍ��� */
		
		if(strncmp(sBuf, str, len) == 0)	/* �ǂݍ��񂾃f�[�^�ƃL�[���[�h�̈�v�m�F */
		{
			fseek(fp, file_ofst + i + len, SEEK_SET);
			memset(sBuf, 0, sizeof(sBuf));
			fgets(sBuf, 256, fp);
			sMeslen = strlen(sBuf);
			cnt = i;
			ret = 0;
			break;
		}
		memset(sBuf, 0, sizeof(sBuf));
		ret = -1;
	}
	
	if(sMeslen != 0)
	{
		printf("%s%s", str, sBuf);
	}

	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/* �t�@�C���ۑ� */
/* *fname	�t�@�C���� */
/* *ptr		�i�[��̐擪�A�h���X */
/* size		�f�[�^�̃T�C�Y */
/* n		�f�[�^�̌� */
int16_t File_Save(int8_t *fname, void *ptr, size_t size, size_t n)
{
	FILE *fp;
	int16_t ret = 0;

	/* �t�@�C�����J���� */
	fp = fopen(fname, "rb");
	
	if(fp == NULL)	/* �t�@�C�������� */
	{
		/* �t�@�C�����J���� */
		fp = fopen(fname, "wb");
		fwrite(ptr, size, n, fp);
	}
	else
	{
		/* �t�@�C�������݂���ꍇ�͉������Ȃ� */
	}
	/* �t�@�C������� */
	fclose (fp);

	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t	GetFileLength(int8_t *pFname, int32_t *pSize)
{
	FILE *fp;
	int16_t ret = 0;
	int32_t	Tmp;

	fp = fopen( pFname , "rb" ) ;
	if(fp == NULL)		/* Error */
	{
		ret = -1;
	}
	else
	{
		Tmp = fileno( fp );
		if(Tmp == -1)	/* Error */
		{
			*pSize = 0;
			ret = -1;
		}
		else
		{
			Tmp = filelength( Tmp );
			if(Tmp == -1)		/* Error */
			{
				*pSize = 0;
				ret = -1;
			}
			else
			{
				*pSize = Tmp;
//				printf("GetFileLength = (%4d, %4d)\n", *pSize, Tmp );
			}
		}

		fclose( fp ) ;
	}
	
	return ret;
}



/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void *MyMalloc(int32_t Size)
{
	void *pPtr = NULL;

	if(Size >= 0x1000000u)
	{
		printf("error:�������m�ۃT�C�Y���傫�����܂�(0x%x)\n", Size );
	}
	else

	{
		pPtr = _dos_malloc(Size);	/* �������m�� */

//		pPtr = malloc(Size);	/* �������m�� */
		
		if(pPtr == NULL)
		{
			puts("error:���������m�ۂł��܂���ł���");
		}
		else if((uint32_t)pPtr >= 0x81000000)
		{
			if((uint32_t)pPtr >= 0x82000000)
			{
				printf("error:���������m�ۂł��܂���ł���(0x%x)\n", (uint32_t)pPtr);
			}
			else
			{
				printf("error:���������m�ۂł��܂���ł���(0x%x)\n", (uint32_t)pPtr - 0x81000000 );
			}
			pPtr = NULL;
		}
		else
		{
			//printf("Mem Address 0x%p Size = %d[byte]\n", pPtr, Size);
			//printf("�������A�h���X(0x%p)=%d\n", pPtr, Size);
		}
	}
	
	return pPtr;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void *MyMallocJ(int32_t Size)
{
	void *pPtr = NULL;
	
	PRAMREG(d0_reg, d0);		/* d0��ϐ�d0_reg�Ɋ��蓖�Ă� */
	
	asm("\tmove.l 4(sp),d3");	/* �X�^�b�N�Ɋi�[���ꂽ����Size��(d3)�֑�� */
	
	asm("\tmove.l d3,-(sp)");	/* MALLOC3�̈�����d3������ */
	
 	asm("\tDOS _MALLOC3");		/* MALLOC3 */

	asm("\taddq.l #4, sp");		
	
	pPtr = (void *)d0_reg;
	
	if(pPtr == NULL)
	{
		puts("error:���������m�ۂł��܂���ł���");
	}
	else if((uint32_t)pPtr >= 0x81000000)
	{
		if((uint32_t)pPtr >= 0x82000000)
		{
			printf("error:���������m�ۂł��܂���ł���(0x%x)\n", (uint32_t)pPtr);
		}
		else
		{
			printf("error:���������m�ۂł��܂���ł���(0x%x)\n", (uint32_t)pPtr - 0x81000000 );
		}
		pPtr = NULL;
	}
	else
	{
		printf("JMem Address 0x%p Size = %d[byte]\n", pPtr, Size);
	}
	
	return (void*)pPtr;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void *MyMallocHi(int32_t Size)
{
	void *pPtr = NULL;

	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t		retReg;	/* d0 */
	
	stInReg.d0 = 0xF8;					/* IOCS _HIMEM */
	stInReg.d1 = 0x03;					/* HIMEM_GETSIZE */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	if(stOutReg.d0 == 0)
	{
		printf("error:�������T�C�Y%d[MB](%d[byte])/��x�Ɋm�ۂł���ő�̃T�C�Y %d[byte]\n", stOutReg.d0 >> 20u, stOutReg.d0, stOutReg.d1);
		return pPtr;
	}
	
	
	stInReg.d0 = 0xF8;					/* IOCS _HIMEM */
	stInReg.d1 = 0x01;					/* HIMEM_MALLOC */
	stInReg.d2 = Size;					/* �T�C�Y */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	if(stOutReg.d0 == 0)
	{
		pPtr = (void *)stOutReg.a1;	/* �m�ۂ����������̃A�h���X */
		//printf("HiMem Address 0x%p Size = %d[byte]\n", pPtr, Size);
	}
	else
	{
		puts("error:���������m�ۂ��邱�Ƃ��o���܂���ł���");
	}
	return pPtr;
}


/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t	MyMfree(void *pPtr)
{
	int16_t ret = 0;
	uint32_t	result;
	
	if(pPtr == 0)
	{
		puts("���v���Z�X�A�q�v���Z�X�Ŋm�ۂ������������t���ŉ�����܂�");
	}
	
	result = _dos_mfree(pPtr);
	//free(pPtr);
	
	if(result < 0)
	{
		puts("error:����������Ɏ��s");
		ret = -1;
	}
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t	MyMfreeJ(void *pPtr)
{
	int16_t ret = 0;
	
	asm("\tmove.l 4(sp),d3");	/* �X�^�b�N�Ɋi�[���ꂽ����pPtr��(d3)�֑�� */
	
	asm("\tmove.l d3,-(sp)");	/* _MFREE�̈�����d3������ */
	
 	asm("\tDOS _MFREE");		/* _MFREE */

	asm("\taddq.l #4, sp");		
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t	MyMfreeHi(void *pPtr)
{
	int16_t ret = 0;
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	if(pPtr == NULL)
	{
		puts("���v���Z�X�A�q�v���Z�X�Ŋm�ۂ������������t���ŉ�����܂�");
	}
	
	stInReg.d0 = 0xF8;					/* IOCS _HIMEM */
	stInReg.d1 = 0x02;					/* HIMEM_FREE */
	stInReg.d2 = (int32_t)pPtr;			/* �m�ۂ����������̐擪�A�h���X */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	if(stOutReg.d0 < 0)
	{
		puts("error:����������Ɏ��s");
	}
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	MaxMemSize(int8_t SizeType)
{
	int32_t ret = 0;
	int32_t i, dummy;
	int32_t chk[2];
	int8_t *ptMem[2];
	
	ptMem[0] = (int8_t *)0x0FFFFF;
	ptMem[1] = (int8_t *)0x100000;
	
	do{
		for(i=0; i<2; i++)
		{
			if((int32_t)ptMem[i] >= 0xC00000)	/* 12MB�̏�� */
			{
				chk[0] = 0;	/* �������[�v�E�o */
				chk[1] = 2;	/* �������[�v�E�o */
				break;
			}
			else
			{
				chk[i] = _dos_memcpy(ptMem[i], &dummy, 1);	/* �o�X�G���[�`�F�b�N */
			}
		}
		
		/* �����������̋��E */
		if( (chk[0] == 0) &&	/* �ǂݏ����ł��� */
			(chk[1] == 2) )		/* �o�X�G���[ */
		{
			break;	/* ���[�v�E�o */
		}
		
		ptMem[0] += 0x100000;	/* +1MB ���Z */
		ptMem[1] += 0x100000;	/* +1MB ���Z */
	}while(1);
	
//	printf("Memory Size = %d[MB](%d[Byte])(0x%p)\n", ((int)ptMem[1])>>20, ((int)ptMem[1]), ptMem[0]);

	switch(SizeType)
	{
	case 0:	/* Byte */
		ret = ((int)ptMem[1]);
		break;
	case 1:	/* KByte */
		ret = ((int)ptMem[1])>>10;
		break;
	case 2:	/* MByte */
	default:
		ret = ((int)ptMem[1])>>20;
		break;
	}
	
	return ret;
}

#endif	/* FILEMANAGER_C */

