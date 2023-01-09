#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include <interrupt.h>
#include <unistd.h>

#include "usr_style.h"
#include "usr_define.h"
#include "usr_macro.h"
#include "FileManager.h"

#define IOCS_OFST		(0x100)
#define IOCS_MACS_CALL	(0xD0)
#define IOCS_HIMEM		(0xF8)
#define IOCS_SYS_STAT	(0xAC)
#define IOCS_ADPCMMOD	(0x67)

int32_t	MACS_Play(int8_t *, int32_t, int32_t, int32_t, int32_t);
int32_t	MACS_PlayCtrl(void);
int32_t	MACS_Load(int8_t *, int32_t, int8_t);
int32_t	MACS_MemFree(int32_t);
int32_t	MACS_DriverVer_CHK(int8_t *, int32_t, int32_t);
int32_t	MACS_FileVer_CHK(int8_t *, int32_t);
int32_t	MACS_CHK(void);
int32_t	SYS_STAT_CHK(void);
int32_t	HIMEM_CHK(void);
int32_t	PCM8A_CHK(void);
int32_t ADPCM_Stop(void);
void Set_DI(void);
void Set_EI(void);
static void interrupt IntVect(void);
void HelpMessage(void);
int16_t main(int16_t, int8_t **);

/* �O���[�o���ϐ� */
int32_t g_nCommand = 0;
int32_t g_nBuffSize = -1;
int32_t g_nAbort = 1;
int32_t g_nEffect = 0;
int32_t g_nPCM8Achk = 0;
int32_t g_nRepeat = 1;
int32_t g_nPlayHistory = 0;
int32_t g_nBreak = 0;
int32_t g_nCansel = 0;
int32_t	g_nIntLevel = 0;
int32_t	g_nIntCount = 0;

int8_t		g_sMACS_File_List[256][256]	=	{0};
uint32_t	g_unMACS_File_List_MAX	=	0u;

int8_t		g_sMACS_File_Path[256][256]	=	{0};

int32_t		g_nPlayListMode = 0;
int32_t		g_nFileListNum = 0;		/* ���݂̃��X�g�ԍ� */
int32_t		g_nFileListNumMAX = 0;	/* ���X�g�̍ő吔 */
int32_t		g_nMemAllocNumMAX = 0;	/* ���X�g�̍ő吔 */
static int8_t	*g_pMACS_Buff[256];		/* MACS�f�[�^�̃|�C���^ */
static int8_t	g_bMACS_MemMode[256];	/* MACS�f�[�^�̃��������[�h */

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/*	MACS_Play() MACS_Cal.doc���
*	Input:
*		d1.w > �R�}���h�R�[�h
*		d2.l > �o�b�t�@�T�C�Y
*		d3.w > �A�{�[�g�t���O
*		d4.l > ������ʃt���O
*		a1.l > �����A�h���X
*	------------------------------------------------------------------------------
*		d1.w = 0	�A�j���[�V�����Đ�
*					(a1)����n�܂�A�j���f�[�^���Đ����܂��B
*	------------------------------------------------------------------------------
*		d2.l�͒ʏ�-1($FFFFFFFF)�ɂ��ĉ������B
*				�i�A�j���f�[�^�Đ����Ad2.l�̃G���A���z���悤�Ƃ�����,�Đ��𒆎~���܂��j
*	------------------------------------------------------------------------------
*	Output:  d0.l > Status
*------------------------------------------------------------------------------
*	d1.w = 0	�A�j���[�V�����Đ�
*				(a1)����n�܂�A�j���f�[�^���Đ����܂��B
*				d2.l�͒ʏ�-1($FFFFFFFF)�ɂ��ĉ������B
*				�i�A�j���f�[�^�Đ����Ad2.l�̃G���A���z���悤�Ƃ�����
*				�@�Đ��𒆎~���܂��j
*				d4.l�ɂ��Ă͉��\���Q��
*	
*	d1.w = 1	�Đ��󋵌���
*				���^�[���l��0�ȊO�Ȃ�΃A�j���[�V�����Đ����ł�
*	
*	d1.w = 2	���f���N�G�X�g���s
*				�A�j���[�V�����Đ��𒆒f����悤�v�����܂��B
*				d3.w�̓��e�ɂ�蒆�f��ɏ������s�Ȃ킹�邱�Ƃ��\�ł�
*	
*			d3.w = -1	���f�㉽�����Ȃ�(�ʏ�)
*			d3.w = -2	���f�� d2,d4,a1�̓��e�ɂ��A�j���[�V�����J�n
*			d3.w = -3	���f�� a1.l�̃A�h���X�ɃT�u���[�`���R�[��
*	
*	d1.w = 3	MACS�o�[�W��������
*				d0.w�ɏ풓���Ă���MACS�̃o�[�W�����ԍ���Ԃ��܂��B
*				(Version 1.16�Ȃ�$01_16)
*	
*	d1.w = 4	MACS�A�v���P�[�V������o�^���܂�
*				a1.l�ɃA�v���P�[�V�������ւ̃|�C���^��ݒ肵�܂��B
*				�A�v���P�[�V��������
*					'�`�`.x [�A�v���P�[�V������]',0
*				�Ƃ����t�H�[�}�b�g�ɏ]���ĉ������B���̃R�[���ŃA�v����
*				�o�^���邱�ƂŁAMACS�̏풓�������֎~���邱�Ƃ��ł��܂�
*	
*	d1.w = 5	MACS�A�v���P�[�V�����̓o�^���������܂�
*				a1.l��(d1.w=4)�œo�^�����A�v���P�[�V�������ւ̃|�C���^
*				��n���R�[������Ɠo�^�A�v���P�[�V�������������܂��B
*				�o�^�\�A�v���P�[�V�����͍ő�16�ł��B(Ver1.00)
*	
*	d1.w = 6	MACS�f�[�^�̃f�[�^�o�[�W�����𓾂܂�
*				a1.l��MACS�f�[�^�̐擪�A�h���X�����ăR�[�����܂��B
*				�Ԃ�l�Ƃ���d0.l�Ƀf�[�^���쐬���ꂽMACS�̃o�[�W������
*				����܂��Bd0.l=-1�̎���MACS�f�[�^�ł͂���܂���B
*	
*	d1.w = 7	��ʂ�ޔ����A�g�p�\�ɂ��܂�
*				�A�j���[�V�����X�^�[�g�Ɠ����������s�Ȃ��܂��B
*				���̃R�[���𗘗p���鎞�͕K���A�{�[�g�t���O�ɑΉ�����
*				�������B
*	
*	d1.w = 8	�ޔ�������ʂ����ɖ߂��܂�
*				��ʍď������}���t���O��1�ɂ��ăA�j���[�V�����Đ����s
*				�������͕K�����Ƃł��̃R�[���𔭍s���ĉ������B
*	
*	d1.w = 9	�e�L�X�gVRAM�ɃO���t�B�b�N��W�J���܂�
*				a1.l��MACS�`���O���t�B�b�N�f�[�^�̃A�h���X�Ca2.l�ɓW�J
*				��̃e�L�X�gVRAM�A�h���X���w�肵�ăR�[�����܂��B
*	
*	d1.w = 10	�����������荞�݂�o�^���܂�
*				a1.l�Ɋ��荞�݃��[�`���̃A�h���X�Ad2.w�Ƀt���O*256+�J
*				�E���^���Z�b�g���ăR�[�����܂��B
*				�t���O�͌��ݎg�p����Ă��܂���B0�ɂ��Ă����ĉ������B
*	
*	d1.w = 11	�����������荞�݂��������܂�
*				a1.l�ɓo�^�������荞�݃��[�`���̃A�h���X���Z�b�g����
*				�R�[�����܂��B
*	
*	d1.w = 12	�X���[�v�󋵂��`�F�b�N���܂�
*				���^�[���l��0�ȊO�Ȃ�MACSDRV�̓X���[�v���Ă��܂�
*	
*	d1.w = 13	�X���[�v��Ԃ�ݒ肵�܂�
*				d2.w��0�Ȃ�Đ��\��ԁC0�ȊO�Ȃ�X���[�v���܂��B
*
*	d1.w = 14	�����t���[���X�L�b�v�̗L��/������ݒ肵�܂��B
*				d2.w��0�Ȃ疳���A1�Ȃ�L���B2�Ō��i�Ȋ�ŗL���ɂȂ�܂��B
*				0 = �`�悪�x��Ă��K���������u����
*				1 = 1�t���[����x�ꂽ���x�Ȃ�e�B�A�����O�㓙�ŕ`��𑱂���
*				2 = �ق�̂�����Ƃł��x�ꂽ��`����X�L�b�v����
*				�����l�� 2 �ł��B�݊����ێ��̂��ߒǉ����ꂽ��ʃ��[�h�̂ݑΉ��ł��B
*	
*	d1.w = -1	���[�N�G���A�A�h���X�l��
*				d0.l��MACS�������[�N�G���A�̃A�h���X��Ԃ��܂��B
*------------------------------------------------------------------------------
*������ʃt���O (d4.l)
*
*bit 0	�n�[�t�g�[��			 		(1�̎��p���b�g��50%�Â�����)
*bit 1	PCM�J�b�g				 		(1�̎�PCM�̍Đ������Ȃ�)
*bit 2	�L�[�Z���X�A�{�[�gOFF 			(1�̎��L�[���͎��I�����Ȃ�)
*bit 3	��ʍď������}���t���O 			(1�̎��A�j���I������ʂ����������Ȃ�)
*bit 4	�X���[�v��Ԗ��� 				(1�̎�MACSDRV���X���[�v���Ă��鎞�ł��Đ�����)
*bit 5	�O���t�B�b�N�u�q�`�l�\���t���O 	(1�̎����̨����\�������܂܍Đ�����)
*bit 6	�X�v���C�g�\���t���O 			(1�̎����ײĂ�\�������܂܍Đ�����)
*
*bit 7
*  :		Reserved. (�K��0�ɂ��邱��)
*bit31
*
*------------------------------------------------------------------------------
* Status �G���[�R�[�h�ꗗ (d0.l)
*
*-1	MACS�f�[�^�ł͂���܂���
*-2	MACS�̃o�[�W�������Â����܂�
*-3	�f�[�^���o�b�t�@�ɑS�����͂���Ă��܂���
*-4	�A�{�[�g����܂���
*-5	���Ɍ��݃A�j���[�V�����Đ����ł�
*-6	�����ȃt�@���N�V�������w�肵�܂���
*-7	���[�U�[���X�g�������͊��荞�݃��X�g����t�ł�
*-8	���[�U�[���X�g�������͊��荞�݃��X�g�ɓo�^����Ă��܂���
*==============================================================================
*/

int32_t	MACS_Play(int8_t *pMacsBuff, int32_t nCommand, int32_t nBuffSize, int32_t nAbort, int32_t nEffect)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_MACS_CALL;	/* IOCS _MACS(macsdrv.x) */
	stInReg.d1 = nCommand;			/* �R�}���h�R�[�h(w) */
	stInReg.d2 = nBuffSize;			/* �o�b�t�@�T�C�Y(l) */
	stInReg.d3 = nAbort;			/* �A�{�[�g�t���O(w) */
	stInReg.d4 = nEffect;			/* ������ʃt���O(l) */
	stInReg.a1 = (int32_t)pMacsBuff;	/* MACS�f�[�^�̐擪�A�h���X */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	return retReg;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	MACS_PlayCtrl(void)
{
	int32_t ret = 0;
	int32_t nLoop;
	int32_t nPlayCount;
	int8_t	*pBuff = NULL;
	
	nLoop = g_nRepeat;

	do
	{
		if(g_nRepeat == 0)
		{
			nLoop = 1;
		}
		else
		{
			nLoop--;
		}
	
		for(nPlayCount = 0; nPlayCount < g_nFileListNumMAX; nPlayCount++)
		{
			void *p;
			
			g_nFileListNum = nPlayCount;
			
			pBuff = g_pMACS_Buff[nPlayCount];

			if(pBuff != NULL)
			{
				_dos_kflushio(0xFF);	/* �L�[�o�b�t�@���N���A */
				
				g_nCommand = 0;	/* �Đ� */
				ret = MACS_Play(pBuff, g_nCommand, g_nBuffSize, g_nAbort, g_nEffect);	/* �Đ� */
				
				p = (void *)_iocs_b_intvcs( 0x4C, (int32_t)IntVect );	/* MFP (key seral input ���� */
				sleep(1);
				_iocs_b_intvcs( 0x4C, (int32_t)p );	/* ���ɖ߂� */
				
				if(g_nBreak != 0u)
				{
					printf("message:Q�L�[�ɂ�鋭���I�����܂��B\n");
					nLoop = 0;
					break;		/* �����I�� */
				}
				
				if(ret < 0)
				{
					printf("message:ESC�ɂ�钆�f�������ُ͈�����m���܂����B\n");
					continue;	/* �X�L�b�v */
				}
				else
				{
					if(g_nPlayHistory != 0)	/* ���������@�\ */
					{
						SetHisFileCnt(g_sMACS_File_Path[nPlayCount]);	/* �Đ��񐔂��X�V */
					}
				}
			}
			
		}
	}
	while(nLoop);
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	MACS_Load(int8_t *sFileName, int32_t nFileSize, int8_t bMode)
{
	int32_t ret = 0;
	int8_t	*pBuff = NULL;
	int16_t f_ver;
	
	/* �o�[�W�����`�F�b�N�F���o�[�W�����h���C�o�ŐV�o�[�W�����t�@�C�����Đ����Ȃ��悤�ɂ���B */
	ret = MACS_DriverVer_CHK(sFileName, nFileSize, 3);	/* �h���C�o�o�[�W���� */
	
	f_ver = (int16_t)MACS_FileVer_CHK(sFileName, nFileSize);	/* �t�@�C���o�[�W���� */
	
	if(f_ver < 0)
	{
		printf("error�FMACS�f�[�^�Ɉُ킪����܂��I(DRV=%x.%x,DATA=%x.%x,FILESIZE=%d)\n", ((ret>>8) & 0xFF), (ret & 0xFF), ((f_ver>>8) & 0xF), (f_ver & 0xFF), nFileSize);
		return -1;
	}
	else
	{
		if(ret == 0x123)	/* �����o�[�W���� */
		{
			/* ���ʌ݊�����Ȃ̂Ń`�F�b�N�s�v */
			ret = MACS_DriverVer_CHK(sFileName, nFileSize, 17);	/* �����h���C�o�o�[�W�����擾(17) */
			//printf("MACSDRV.X Version %x.%x.%x �����o���܂���\n", ((ret>>16) & 0xFF), ((ret>>8) & 0xFF), (ret & 0xFF));
		}
		else				/* ���ʃo�[�W���� */
		{
			if(ret < f_ver)
			{
				printf("warning�F�Â�MACSDRV.X Version %x.%x �����o���܂���\n", ((ret>>8) & 0xFF), (ret & 0xFF));
				puts("error�FMACSDRV Version 1.16�̏���ɉ����� v0.15.11�ȍ~���풓���Ă��������I");
				return -1;
			}
		}
		
	}
	
	/* �������m�� */
	switch(bMode)
	{
	case 0:
		pBuff = (int8_t*)MyMalloc(nFileSize);	/* �������m�� */
		break;
	case 1:
		pBuff = (int8_t*)MyMallocJ(nFileSize);	/* �������m�� */
		break;
	case 2:
		pBuff = (int8_t*)MyMallocHi(nFileSize);	/* �������m�� */
		break;
	default:
		pBuff = (int8_t*)MyMalloc(nFileSize);	/* �������m�� */
		break;
	}
	
	if(pBuff == NULL)
	{
		printf("error�FMACS_Load buffer null(%d)\n", bMode);
		return -1;
	}
	else
	{
		if(pBuff >= (int8_t*)0x1000000)	/* TS-6BE16�����̃A�h���X�l�ȏ�Ŋm�ۂ���Ă��邩�H */
		{
			printf("Hi-Memory");
		}
		else
		{
			printf("Main-Memory");
		}
		printf("(addr=0x%p)\n", pBuff);	/* �������̐擪�A�h���X��\�� */
	}
		
	g_pMACS_Buff[g_nMemAllocNumMAX] = pBuff;	/* �A�h���X����*/
	g_bMACS_MemMode[g_nMemAllocNumMAX] = bMode;	/* ���[�h���L�� */
	
	if(g_nPlayHistory != 0)	/* �t���p�X���L������ */
	{
		int32_t nPlayHisCnt;
		
		nPlayHisCnt = GetHisFileCnt(sFileName);	/* �q�X�g���t�@�C������ */
		if(nPlayHisCnt > 0)
		{
			printf("message:���̃t�@�C���̍Đ��񐔂�%4ld��ł��B\n", nPlayHisCnt);	/* �v���C�񐔂�\�� */
		}
		else if(nPlayHisCnt == 0)
		{
			printf("message:���̃t�@�C���͏��߂čĐ�����܂��B\n");	/* ���Đ� */
		}
		else
		{
			/* �������Ȃ� */
		}
	}
		
	if( File_Load(sFileName, pBuff, sizeof(uint8_t), nFileSize ) == 0 )	/* �t�@�C���ǂݍ��݂��烁�����֕ۑ� */
	{
		g_nMemAllocNumMAX++;	/* �������m�ۂł�����J�E���g */
		
		if(g_nPlayHistory != 0)	/* �t���p�X���L������ */
		{
			int8_t	*p;
			
			_fullpath(g_sMACS_File_Path[g_nFileListNumMAX], sFileName, 128);	/* �t���p�X���擾 */
			
			while ((p = strchr(g_sMACS_File_Path[g_nFileListNumMAX], '/'))!=NULL) *p = '\\';
		}
		g_nFileListNumMAX++;
		

		ret = 0;
	}
	else	/* ���[�h�L�����Z�� */
	{
		MACS_MemFree(g_nMemAllocNumMAX);
		g_pMACS_Buff[g_nMemAllocNumMAX] = NULL;	/* ������ */
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
int32_t	MACS_MemFree(int32_t nNum)
{
	int32_t ret = 0;
	int32_t nPlayCount;
	int32_t nPlayCount_s;
	int32_t nPlayCount_e;
	int8_t	*pBuff = NULL;
	int8_t bMode;
	
	nPlayCount = 0;
	
	if(nNum < 0)
	{
		nPlayCount_s = 0;
		nPlayCount_e = g_nMemAllocNumMAX;
	}
	else
	{
		nPlayCount_s = nNum;
		nPlayCount_e = nNum+1;
	}
	
	for(nPlayCount = nPlayCount_s; nPlayCount < nPlayCount_e; nPlayCount++)
	{
		pBuff = g_pMACS_Buff[nPlayCount];
		bMode = g_bMACS_MemMode[nPlayCount];
		
		if(pBuff != NULL)
		{
			switch(bMode)
			{
			case 0:
				MyMfree(pBuff);		/* ��������� */
				break;
			case 1:
				MyMfreeJ(pBuff);	/* ��������� */
				break;
			case 2:
				MyMfreeHi(pBuff);	/* ��������� */
				break;
			default:
				MyMfree(pBuff);		/* ��������� */
				break;
			}
			g_pMACS_Buff[nPlayCount] = NULL;
		}
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
int32_t	MACS_DriverVer_CHK(int8_t *sFileName, int32_t nFileSize, int32_t nDriverVer)
{
	int32_t ret = 0;
	int8_t	*pBuff = NULL;
	
	/* �o�[�W�����`�F�b�N */
	ret = MACS_Play(pBuff, nDriverVer, g_nBuffSize, g_nAbort, g_nEffect);	/* ���s */
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	MACS_FileVer_CHK(int8_t *sFileName, int32_t nFileSize)
{
	int32_t ret = 0;
	
	/* �o�[�W�����`�F�b�N */
	ret = (int32_t)FileHeader_Load(sFileName, NULL, sizeof(uint8_t), nFileSize );	/* �w�b�_���ǂݍ��� */
	
	return ret;
}
		
/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	MACS_CHK(void)
{
	int32_t ret = 0;	/* 0:�풓 !0:�풓���Ă��Ȃ� */
	
	int32_t addr_IOCS;
	int32_t addr_MACSDRV;
	int32_t data;
	
	addr_IOCS = ((IOCS_OFST + IOCS_MACS_CALL) * 4);
	
	addr_MACSDRV = _iocs_b_lpeek((int32_t *)addr_IOCS);
	addr_MACSDRV += 2;
	
	data = _iocs_b_lpeek((int32_t *)addr_MACSDRV);
	if(data != 'MACS')ret |= 1;	/* �A���}�b�` */
	
	addr_MACSDRV += sizeof(int32_t);
	
	data = _iocs_b_lpeek((int32_t *)addr_MACSDRV);
	if(data != 'IOCS')ret |= 1;	/* �A���}�b�` */
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	SYS_STAT_CHK(void)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_SYS_STAT;	/* IOCS _SYS_STAT($AC) */
	stInReg.d1 = 0;				/* MPU�X�e�[�^�X�̎擾 */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
//	printf("SYS_STAT 0x%x\n", (retReg & 0x0F));
    /* 060turbo.man���
		$0000   MPU�X�e�[�^�X�̎擾
			>d0.l:MPU�X�e�[�^�X
					bit0�`7         MPU�̎��(6=68060)
					bit14           MMU�̗L��(0=�Ȃ�,1=����)
					bit15           FPU�̗L��(0=�Ȃ�,1=����)
					bit16�`31       �N���b�N�X�s�[�h*10
	*/
	
	return (retReg & 0x0F);
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	HIMEM_CHK(void)
{
	int32_t ret = 0;	/* 0:�풓 !0:�풓���Ă��Ȃ� */
	
	int32_t addr_IOCS;
	int32_t addr_HIMEM;
	int32_t data;
	int8_t data_b;
	
	addr_IOCS = ((IOCS_OFST + IOCS_HIMEM) * 4);
	
	addr_HIMEM = _iocs_b_lpeek((int32_t *)addr_IOCS);
	addr_HIMEM -= 6;
	
	data = _iocs_b_lpeek((int32_t *)addr_HIMEM);
	if(data != 'HIME')ret |= 1;	/* �A���}�b�` */
//	printf("data %x == %x\n", data, 'HIME');
	
	addr_HIMEM += 4;
	
	data_b = _iocs_b_bpeek((int32_t *)addr_HIMEM);
	if(data_b != 'M')ret |= 1;	/* �A���}�b�` */
//	printf("data %x == %x\n", data_b, 'M');
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int32_t	PCM8A_CHK(void)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_ADPCMMOD;	/* IOCS _ADPCMMOD($67) */
	stInReg.d1 = 'PCMA';		/* PCM8A�̏풓�m�F */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	
	return retReg;
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
/* (AD)PCM���ʉ��̒�~ */
int32_t ADPCM_Stop(void)
{
	int32_t	ret=0;
	
	if(_iocs_adpcmsns() != 0)	/* �������Ă��� */
	{
		_iocs_adpcmmod(1);	/* ���~ */
		_iocs_adpcmmod(0);	/* �I�� */
	}
	
	return	ret;
}
/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void Set_DI(void)
{
	if(g_nIntCount == 0)
	{
#if 0
		/*�X�[�p�[�o�C�U�[���[�h�I��*/
		if(g_nSuperchk > 0)
		{
			_dos_super(g_nSuperchk);
		}
#endif
		g_nIntLevel = intlevel(6);	/* ���֐ݒ� */
		g_nIntCount = Minc(g_nIntCount, 1u);
		
#if 0
		/* �X�[�p�[�o�C�U�[���[�h�J�n */
		g_nSuperchk = _dos_super(0);
#endif
	}
	else
	{
		g_nIntCount = Minc(g_nIntCount, 1u);
	}
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void Set_EI(void)
{
	if(g_nIntCount == 1)
	{
		g_nIntCount = Mdec(g_nIntCount, 1);
		
		/* �X�v���A�X���荞�݂̒��̐l��(�ȉ���)��� */
		/* MFP�Ńf�o�C�X���̊����݋֎~�ݒ������ۂɂ͕K��SR����Ŋ����݃��x�����グ�Ă����B*/
		asm ("\ttst.b $E9A001\n\tnop\n");
		/*
			*8255(�ޮ��è��)�̋�ǂ�
			nop		*���O�܂ł̖��߃p�C�v���C���̓���
					*�����b�A���̖��ߏI���܂łɂ�
					*����ȑO�̃o�X�T�C�N���Ȃǂ�
					*�������Ă��邱�Ƃ��ۏ؂����B
		*/
#if 0
		/*�X�[�p�[�o�C�U�[���[�h�I��*/
		if(g_nSuperchk > 0)
		{
			_dos_super(g_nSuperchk);
		}
#endif
		intlevel(g_nIntLevel);	/* ���։��� */
#if 0
		/* �X�[�p�[�o�C�U�[���[�h�J�n */
		g_nSuperchk = _dos_super(0);
#endif
	}
	else
	{
		g_nIntCount = Mdec(g_nIntCount, 1);
	}
}

static void interrupt IntVect(void)
{
	volatile uint8_t *MFP_USART_D = (uint8_t *)0xE8802Fu;
	uint8_t ubKey;
	
	Set_DI();	/* ���֐ݒ� */
	
	ubKey = *MFP_USART_D;
	
	if(ubKey == 0x11u)	/* Q */
	{
		g_nBreak = 1;
		g_nPlayListMode = 0;
	}
	
	if(g_nCansel == 0x01u) /* ESC */
	{
		g_nCansel = 1;
	}
	
	SetKeyInfo(ubKey);	/* �t�@�C���}�l�[�W���[�� */
	
//	printf("Hit! 0x%x :", *MFP_USART_D);
	
	Set_EI();	/* ���։��� */

	IRTE();	/* 0x0000-0x00FF���荞�݊֐��̍Ō�ŕK�����{ */
//	IRTS();	/* 0x0100-0x01FF���荞�݊֐��̍Ō�ŕK�����{ */
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
void HelpMessage(void)
{
	puts("MACS�f�[�^�̍Đ����s���܂��B");
	puts("  68030�ȍ~�Ɍ��胁�C���������̋󂫗e�ʂ�����Ȃ��ꍇ�̓n�C���������g���čĐ����s���܂��B");
	puts("  ���n�C��������ADPCM���Đ�����ۂ́APCM8A.X -w18 �ł̏풓");
	puts("    �������́A060turbo.sys -ad ��ݒ肭�������B");
	puts("");
	puts("Usage:MACSplay.x [Option] file");
	puts("Option:-D  �p���b�g��50%�Â�����");
	puts("       -P  PCM�̍Đ������Ȃ�");
	puts("       -K  �L�[���͎��A�I�����Ȃ�");
	puts("       -C  �A�j���I�����A��ʂ����������Ȃ�");
	puts("       -SL MACSDRV���X���[�v���Ă��鎞�ł��Đ�����");
	puts("       -GR ���̨����\�������܂܍Đ�����");
	puts("       -SP ���ײĂ�\�������܂܍Đ�����");
	puts("       -AD PCM8A.X�̏풓���`�F�b�N���Ȃ�(060turbo.sys -ad�ōĐ�����ꍇ)");
	puts("       -Rx ���s�[�g�Đ�����x=1�`255��Ax=0(�������[�v)");
	puts("       -L  �v���C���X�g<file>��ǂݍ���Œ����Đ�����");
	puts("       -LA �v���C���X�g<file>��ǂݍ���ōĐ�����");
	puts("       -HS �Đ������t�@�C���̗�����<MACSPHIS.LOG>�Ɏc��");
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t main(int16_t argc, int8_t *argv[])
{
	int16_t ret = 0;
	int32_t	nFileSize = 0;
	int32_t	nFilePos = 0;
	
	int32_t	i, j;
	
	puts("MACS data Player�uMACSplay.x�vv1.06d (c)2022-2023 �J�^.");
	
	if(argc > 1)	/* �I�v�V�����`�F�b�N */
	{
		for(i = 1; i < argc; i++)
		{
			int8_t	bOption;
			int8_t	bFlag;
			int8_t	bFlag2;
			
			bOption	= ((argv[i][0] == '-') || (argv[i][0] == '/')) ? TRUE : FALSE;

			if(bOption == TRUE)
			{
				/* ===== �Q���� =====*/
				bFlag	= ((argv[i][1] == 's') || (argv[i][1] == 'S')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'l') || (argv[i][2] == 'L')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_4);
					puts("Option:MACSDRV���X���[�v���Ă��鎞�ł��Đ����܂�");
					continue;
				}

				bFlag	= ((argv[i][1] == 'g') || (argv[i][1] == 'G')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'r') || (argv[i][2] == 'R')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_5);
					puts("Option:���̨����\�������܂܍Đ����܂�");
					continue;
				}
				
				bFlag	= ((argv[i][1] == 's') || (argv[i][1] == 'S')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'p') || (argv[i][2] == 'P')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_6);
					puts("Option:���ײĂ�\�������܂܍Đ����܂�");
					continue;
				}

				bFlag	= ((argv[i][1] == 'a') || (argv[i][1] == 'A')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'd') || (argv[i][2] == 'D')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nPCM8Achk = 1;
					puts("Option:PCM8A.X�̏풓���`�F�b�N���܂���");
					continue;
				}

				bFlag	= ((argv[i][1] == 'r') || (argv[i][1] == 'R')) ? TRUE : FALSE;
				bFlag2	= strlen(&argv[i][2]);
				if((bFlag == TRUE) && (bFlag2 > 0))
				{
					g_nRepeat = atoi(&argv[i][2]);
					if(g_nRepeat < 0)			g_nRepeat = 1;
					else if(g_nRepeat > 255)	g_nRepeat = 1;

					g_nEffect = Mbclr(g_nEffect, Bit_2);	/* -K �L�[���͏I����L���ɂ��� */
					g_nEffect = Mbclr(g_nEffect, Bit_3);	/* -C �A�j���I������ʂ����������Ȃ���L���ɂ��� */
					if(g_nRepeat == 0)
					{
						puts("Option:�������[�v�ōĐ����܂�");
					}
					else
					{
						printf("Option:%d�񃊃s�[�g�Đ����܂�\n", g_nRepeat);
					}
					continue;
				}
				
				bFlag	= ((argv[i][1] == 'h') || (argv[i][1] == 'H')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 's') || (argv[i][2] == 'S')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nPlayHistory = 1;
					puts("Option:�Đ������t�@�C���̗�����<MACSPHIS.LOG>�Ɏc���܂�");
					continue;
				}

				bFlag	= ((argv[i][1] == 'l') || (argv[i][1] == 'L')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'a') || (argv[i][2] == 'A')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nPlayListMode = 2;
					puts("Option:�v���C���X�g<file>��ǂݍ���ōĐ����܂�");
					continue;
				}
				
				/* ===== �P���� =====*/
				
				bFlag	= ((argv[i][1] == 'd') || (argv[i][1] == 'D')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_0);
					puts("Option:�p���b�g��50%�Â����܂�");
					continue;
				}

				bFlag	= ((argv[i][1] == 'p') || (argv[i][1] == 'P')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_1);
					puts("Option:PCM�̍Đ������܂���");
					continue;
				}

				bFlag	= ((argv[i][1] == 'k') || (argv[i][1] == 'K')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_2);
					puts("Option:�L�[���͎��A�I�����܂���");
					continue;
				}

				bFlag	= ((argv[i][1] == 'c') || (argv[i][1] == 'C')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_3);
					puts("Option:�A�j���I�����A��ʂ����������܂���");
					continue;
				}
				
				
				bFlag	= ((argv[i][1] == 'l') || (argv[i][1] == 'L')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nPlayListMode = 1;
					puts("Option:�v���C���X�g<file>��ǂݍ���Œ����Đ����܂�");
					continue;
				}
				
				bFlag	= ((argv[i][1] == '?') || (argv[i][1] == 'h') || (argv[i][1] == 'H')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					HelpMessage();	/* �w���v */
					ret = -1;
					break;
				}
				
				HelpMessage();	/* �w���v */
				ret = -1;
				break;
			}
			else
			{
				nFilePos = i;
//				printf("nFilePos = %d\n", nFilePos);
#if 0				
				nFileSize = 0;
				if(GetFileLength(argv[i], &nFileSize) < 0)	/* �t�@�C���̃T�C�Y�擾 */
				{
					printf("error�F�t�@�C����������܂���ł����I%s\n", argv[i]);
					ret = -1;
				}
				else
				{
					nFilePos = i;
//					printf("File Size = %d[MB](%d[KB], %d[byte])\n", nFileSize>>20, nFileSize>>10, nFileSize);
				}
#endif
			}
		}
	}
	else
	{
		HelpMessage();	/* �w���v */
		ret = -1;
	}
	
	/* �h���C�o�풓�`�F�b�N */
	if(MACS_CHK() != 0)
	{
		puts("error�FMACSDRV���풓���Ă��������I");
		ret = -1;
	}
	
	
	if(ret == 0)
	{
		int32_t nOut = 0;
		int8_t	data_b;
		int32_t	nSysStat;
		
		/* �@�픻�� */
		data_b = _iocs_b_bpeek((int32_t *)0xCBC);	/* �@�픻�� */
		//printf("�@�픻��(MPU680%d0)\n", data_b);
		
		if(data_b != 0)	/* 68000�ȊO��MPU */
		{
			nSysStat = SYS_STAT_CHK();	/* MPU�̎�� */
		}
		else
		{
			nSysStat = (int32_t)data_b;	/* 0������ */
		}
		
		if(g_nPlayListMode != 0)
		{
//			printf("g_nPlayListMode = %d\n", g_nPlayListMode);
			Load_MACS_List(argv[nFilePos], g_sMACS_File_List, &g_unMACS_File_List_MAX);	/* ����(MACS)���X�g�̓ǂݍ��� */
//			for(i = 0; i < g_unMACS_File_List_MAX; i++)
//			{
//				printf("MACS  File %2d = %s\n", i, g_sMACS_File_List[i]);
//			}
		}
		else
		{
//			printf("g_nPlayListMode = %d\n", g_nPlayListMode);
			Get_MACS_File(argv[nFilePos], g_sMACS_File_List, &g_unMACS_File_List_MAX);	/* �t�@�C�����`�F�b�N���� */
//			memcpy(  g_sMACS_File_List[0], argv[nFilePos], sizeof(char) * strlen(argv[nFilePos]) ); 
		}
		
		for(i=0,j=0; i < g_unMACS_File_List_MAX; i++)
		{
			int32_t	nMemSize;
			int32_t	nHiMemChk;
			int32_t	nChk;
			void *p;

			//printf("Option = 0x%x\n", g_nEffect);
			nFileSize = 0;
			GetFileLength( g_sMACS_File_List[i], &nFileSize );	/* �t�@�C���̃T�C�Y�擾 */
			if(nFileSize == 0)
			{
				puts("error�F�t�@�C���T�C�Y���ُ�ł��I");
				continue;
			}
				
			if(g_nPlayListMode != 0)
			{
				printf("\n");	/* ���s�̂� */
				printf("-= MACS File List No.%04d =-\n", i+1);
			}

			nMemSize = (int32_t)_dos_malloc(-1) - 0x81000000UL;	/* ���C���������̋󂫃`�F�b�N */
			
			nHiMemChk = HIMEM_CHK();	/* �n�C�����������`�F�b�N */
			
			nChk = PCM8A_CHK();			/* PCM8A�풓�`�F�b�N */
				
			p = (void *)_iocs_b_intvcs( 0x4C, (int32_t)IntVect );	/* MFP (key seral input ���� */
				
			if( (nHiMemChk == 0) && ((nChk >= 0) || (g_nPCM8Achk > 0)) )	/* �n�C���������� & (PCM8A�풓�`�F�b�N or -AD�Ŗ�����) */
			{
				nOut = MACS_Load(g_sMACS_File_List[i], nFileSize, 2);	/* �n�C�������Đ� */
			}
			else if( (nHiMemChk != 0) && ((nSysStat == 4) || (nSysStat == 6)) )		/* �n�C������������ & 040/060EXCEL */
			{
				printf("(040/060EXCEL)");
				nOut = MACS_Load(g_sMACS_File_List[i], nFileSize, 1);	/* �g�����ꂽ�������ōĐ� */
			}
			else if(nFileSize <= nMemSize)	/* ���C�������� */
			{
				nOut = MACS_Load(g_sMACS_File_List[i], nFileSize, 0);	/* ���C���Đ� */
			}
			else
			{
				nOut = -5;	/* �ُ� */
				
				if(nChk < 0)	/* >=0:�풓 <0:�풓���Ă��Ȃ� */
				{
					puts("error�FPCM8A���풓����Ă��܂���I���n�C���������g���ꍇ�́APCM8A -w18 �ŏ풓���������I");
				}
				
				if(nHiMemChk != 0)
				{
					puts("error�F�n�C��������������܂���ł����I");
				}
				
				puts("error�F���C���������̋󂫂��s���ł��I");
				
				ret = -1;
			}
			
			_iocs_b_intvcs( 0x4C, (int32_t)p );	/* ���ɖ߂� */
			
			if( (nOut == 0) && (g_nPlayListMode == 0) )	/* �P��Đ��� */
			{
				nOut = MACS_PlayCtrl();	/* �Đ����� */
				MACS_MemFree(-1);			/* ��������� */
			}
			else if(nOut == 1)	/* �v���C���X�g�Đ� */
			{
				if(g_nPlayListMode <= 1)	/* �v���C���X�g�Đ� */
				{
					puts("error�F�Đ��ł��܂���ł����Iskip���܂��I");
				}
				else if(g_nPlayListMode == 2)	/* �v���C���X�g�Đ� */
				{
					nOut = MACS_PlayCtrl();	/* �Đ����� */
					MACS_MemFree(-1);	/* ��������� */
					if(nOut < 0)
					{
						j = 0;
					}
					else
					{
						if(j < 3)
						{
							if(j == 0)
							{
								puts("message:��x�ǂݍ��݂𒆒f���A�Đ����܂��I");
								i--;	/* �ǂ߂Ȃ����������ă`�������W */
							}
							else
							{
								printf("warning�F���g���C���܂��I(%d)", j);
							}
							j++;
						}
						else
						{
							nOut = -5;
							ret = -1;
						}
					}
				}
				else
				{
					nOut = -5;	/* �ُ� */
				}
			}
			
			if( g_nBreak != 0u )	/* Q */
			{
				ret = -2;
				break;
			}
		}
		
		/* �����́A�b����� */
		if(g_nPlayListMode != 0)	/* �v���C���X�g�Đ� */
		{
			nOut = MACS_PlayCtrl();	/* �Đ����� */
			MACS_MemFree(-1);	/* ��������� */
		}
		
		if(nOut < 0)
		{
			switch(nOut)
			{
			case -1:
				puts("error�F�Đ��Ɏ��s���܂����I");
				break;
			case -2:
				puts("message:�����I�����܂����B");
				break;
			case -4:
				puts("message:�Đ��𒆒f���܂����B");
				break;
			default:
				printf("error�F�ُ�I�����܂����I(%d)\n", nOut);
				break;
			}
		} 
		printf("\n");
			
	}
	
	ADPCM_Stop();	/* �����~�߂� */

	_dos_kflushio(0xFF);	/* �L�[�o�b�t�@���N���A */
	
	return ret;
}
