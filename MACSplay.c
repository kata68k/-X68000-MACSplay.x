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

#define MFP_USART_REG	(0xE8802F)	/* USART�f�[�^���W�X�^ */
#define IOCS_WORK0		(0x800)		/* IOCS���[�N�G���A0 */
#define IOCS_WORK2		(0x802)		/* IOCS���[�N�G���A2 */
#define IOCS_WORK3		(0x803)		/* IOCS���[�N�G���A2 */
#define	GP_VDISP	(0x10)

int32_t	MACS_Play(int8_t *, int32_t, int32_t, int32_t, int32_t);
int32_t	MACS_error(int32_t);
int32_t	MACS_PlayCtrl(int32_t);
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
int16_t TimerD_INIT(void);
int16_t TimerD_EXIT(void);
static void interrupt Timer_D_Func(void);
int16_t wait_vdisp(int16_t);
void HelpMessage(void);
int16_t main(int16_t, int8_t **);

/* �O���[�o���ϐ� */
static int8_t g_bSetCommand = FALSE;
static int32_t g_nCommand = 0;
static int32_t g_nBuffSize = -1;
static int32_t g_nAbort = -1;
static int32_t g_nEffect = 0;
static int32_t g_nPCM8Achk = 0;
static int32_t g_nRepeat = 1;
static int32_t g_nRepeatOnMem = 0;
static int32_t g_nPlayHistory = 0;
static int32_t g_nBreak = 0;
static int32_t g_nCansel = 0;
static int32_t g_nIntLevel = 0;
static int32_t g_nIntCount = 0;
static uint8_t g_bTimer_D;
static uint32_t g_unDownTimer;
static int32_t g_nReturn;
static int8_t	*g_pBuff;
static int32_t	g_nSuperchk;

int8_t		g_sMACS_File_List[256][256]	=	{0};
uint32_t	g_unMACS_File_List_MAX	=	0u;

int8_t		g_sMACS_File_Path[256][256]	=	{0};

int32_t		g_nPlayListMode = 0;
int32_t		g_nFileListNum = 0;		/* ���݂̃��X�g�ԍ� */
int32_t		g_nFileListNumMAX = 0;	/* �t�@�C�����X�g�̍ő吔 */
int32_t		g_nMemAllocNumMAX = 0;	/* ���������X�g�̍ő吔 */
static int8_t	*g_pMACS_Buff[256];		/* MACS�f�[�^�̃|�C���^ */
static int8_t	g_bMACS_MemMode[256];	/* MACS�f�[�^�̃��������[�h */

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int32_t	MACS_error(int32_t nStatus)
{
	int32_t ret = 0;
	
	ret = nStatus;

	switch(ret)
	{
		case  0:	break;
		case -1:	puts("MACSDRV:MACS�f�[�^�ł͂���܂���");	break;
		case -2:	puts("MACSDRV:MACS�̃o�[�W�������Â����܂�");	break;
		case -3:	puts("MACSDRV:�f�[�^���o�b�t�@�ɑS�����͂���Ă��܂���");	break;
		case -4:	puts("MACSDRV:�A�{�[�g����܂���");	break;
		case -5:	puts("MACSDRV:���Ɍ��݃A�j���[�V�����Đ����ł�");	break;
		case -6:	puts("MACSDRV:�����ȃt�@���N�V�������w�肵�܂���");	break;
		case -7:	puts("MACSDRV:���[�U�[���X�g�������͊��荞�݃��X�g����t�ł�");	break;
		case -8:	puts("MACSDRV:���[�U�[���X�g�������͊��荞�݃��X�g�ɓo�^����Ă��܂���");	break;
		default:	printf("MACSDRV:error(%d)\n", ret);	break;
	}
	return ret;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int32_t	MACS_PlayCtrl(int32_t nPlay)
{
	int32_t ret = 0;
	int32_t nPlayCount;
	int8_t	*pBuff = NULL;
#if 0
	void *p;
#endif
	
	g_bSetCommand = FALSE;	/* �R�}���h�Ȃ� */
	
	if( (nPlay < 0) || (nPlay >= g_nFileListNumMAX) )
	{
		nPlay = g_nFileListNumMAX - 1;	/* ���[ */
	}
#if 0
	p = (void *)_iocs_b_intvcs( 0x4C, (int32_t)Timer_D_Func );	/*  */
#endif
	
	for(nPlayCount = nPlay; nPlayCount < g_nFileListNumMAX; nPlayCount++)
	{
		g_nFileListNum = nPlayCount;
		
		pBuff = g_pMACS_Buff[nPlayCount];
		g_pBuff = pBuff;

		if(pBuff != NULL)
		{
			int8_t	bFlag = TRUE;

//				printf("message:�Đ����J�n���܂�(BufferList No.%d)\n", nPlayCount+1);

			g_nCansel = 0;	/* ESC�����Ȃ� */
			g_nCommand = 0;	/* �Đ� */
			g_nReturn = MACS_Play(g_pBuff, g_nCommand, g_nBuffSize, g_nAbort, g_nEffect);	/* �Đ� */

			TimerD_INIT();	/* Timer-D ON */

			do	/* �o�b�N�O�����h���� */
			{
				if(g_nReturn == 0)		/* �Đ��I�� */
				{
					bFlag = FALSE;
				}
				else					/* �G���[�������͒��f */
				{
					if(g_bTimer_D == FALSE)
					{
						if(g_unDownTimer != 0u)
						{
							g_unDownTimer--;
						}
						wait_vdisp(1);	/* ���������M���҂� */
					}

					if(( _iocs_bitsns( 0 ) & 0x02 ) != 0u )		/* ESC */
					{
						g_nCansel = 1;
					}
					else
					{
						g_nCansel = 0;
					}

					if(g_unDownTimer == 0)
					{
						if(g_nCansel != 0)
						{
							puts("message:ESC�L�[������������܂���");
							g_nBreak = 1;		/* �����I�� */
						}
						bFlag = FALSE;
					}
				}
			}
			while(bFlag);

			TimerD_EXIT();	/* Timer-D OFF */

			_dos_kflushio(0xFF);	/* �L�[�o�b�t�@���N���A */
				
			if(g_nBreak != 0u)
			{
				puts("message:ESC�������̋����I�����܂���");
				ret = -2;	/* �����I������ */
				break;
			}
			else
			{
//					printf("message:�Đ����I�����܂���(BufferList No.%d)\n", nPlayCount+1);
				ret = MACS_error(g_nReturn);	/* �G���[�`�F�b�N */
				if(ret < 0)
				{
					continue;
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

#if 0
	_iocs_b_intvcs( 0x4C, (int32_t)p );	/* ���ɖ߂� */
#endif
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int32_t	MACS_Load(int8_t *sFileName, int32_t nFileSize, int8_t bMode)
{
	int32_t ret = 0;
	int8_t	*pBuff = NULL;
	int16_t f_ver;
	
	/* �o�[�W�����`�F�b�N:���o�[�W�����h���C�o�ŐV�o�[�W�����t�@�C�����Đ����Ȃ��悤�ɂ���B */
	ret = MACS_DriverVer_CHK(sFileName, nFileSize, 3);	/* �h���C�o�o�[�W���� */
	
	f_ver = (int16_t)MACS_FileVer_CHK(sFileName, nFileSize);	/* �t�@�C���o�[�W���� */
	
	if(f_ver < 0)
	{
		printf("error:MACS�f�[�^�Ɉُ킪����܂�!(DRV=%x.%x,DATA=%x.%x,FILESIZE=%d)\n", ((ret>>8) & 0xFF), (ret & 0xFF), ((f_ver>>8) & 0xF), (f_ver & 0xFF), nFileSize);
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
				printf("warning:�Â�MACSDRV.X Version %x.%x �����o���܂���\n", ((ret>>8) & 0xFF), (ret & 0xFF));
				puts("error:MACSDRV Version 1.16�̏���ɉ����� v0.15.11�ȍ~���풓���Ă�������!");
				return -1;
			}
		}
		
	}
	
	if(g_nPlayHistory != 0)	/* �t�@�C���ǂݍ��݂��烁�����֕ۑ� */
	{
		int32_t nPlayHisCnt;
		
		nPlayHisCnt = GetHisFileCnt(sFileName);	/* �������m�ۂł�����J�E���g */
		if(nPlayHisCnt > 0)
		{
			printf("message:���̃t�@�C���̍Đ��񐔂�%4ld��ł��B\n", nPlayHisCnt);	/* �q�X�g���t�@�C������ */
		}
		else if(nPlayHisCnt == 0)
		{
			puts("message:���̃t�@�C���͏��߂čĐ�����܂��B");	/* ���Đ� */
		}
		else
		{
			/* �������Ȃ� */
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
		pBuff = NULL;	/* COPY COMMAND */
		break;
	}
	
	if(pBuff == NULL)
	{
		if(bMode >= 3)
		{
			printf("message:COPY COMMAND(%d)\n", bMode);
		}
		else
		{
			if(g_nPlayListMode > 0)	/* �v���C���X�g�Đ� */
			{
				printf("message:���������m�ۂł��܂���ł����B(%d)\n", bMode);
				ret = 1;
			}
			else
			{
				printf("error:MACS_Load buffer null(%d)\n", bMode);
				ret = -1;
			}
		}
	}
	else
	{
		if(pBuff >= (int8_t*)0x1000000)	/* TS-6BE16�����̃A�h���X�l�ȏ�Ŋm�ۂ���Ă��邩�H */
		{
			printf("Hi-Memory(addr=0x%p)\n", pBuff);	/* �������̐擪�A�h���X��\�� */
		}
		else
		{
			printf("Main-Memory(addr=0x%p)\n", pBuff);	/* �������̐擪�A�h���X��\�� */
		}
	}

	if(pBuff == NULL)
	{
		if(bMode >= 3)
		{
			int8_t sSystem[256];
			sprintf(sSystem, "COPY %s MACS", sFileName );	/* COPY */
			system(sSystem);
			ret = 0;
		}
	}
	else
	{
		g_pMACS_Buff[g_nMemAllocNumMAX] = pBuff;	/* �A�h���X����*/
		g_bMACS_MemMode[g_nMemAllocNumMAX] = bMode;	/* ���[�h���L�� */
			
		if( File_Load(sFileName, pBuff, sizeof(uint8_t), nFileSize ) == 0 )	/* �t�@�C���ǂݍ��݂��烁�����֕ۑ� */
		{
			g_nMemAllocNumMAX++;	/* �������m�ۂł�����J�E���g */
			ret = 0;
		}
		else	/* ���[�h�L�����Z�� */
		{
			MACS_MemFree(g_nMemAllocNumMAX);
			g_pMACS_Buff[g_nMemAllocNumMAX] = NULL;	/* ������ */
			ret = 2;
		}
	}

	if(ret == 0)
	{
		if(g_nPlayHistory != 0)	/* �t���p�X���L������ */
		{
			int8_t	*p;
			
			_fullpath(g_sMACS_File_Path[g_nFileListNumMAX], sFileName, 128);	/* �t���p�X���擾 */
			
			while ((p = strchr(g_sMACS_File_Path[g_nFileListNumMAX], '/'))!=NULL) *p = '\\';
		}
		g_nFileListNumMAX++;
	}
	
	return ret;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int32_t	MACS_FileVer_CHK(int8_t *sFileName, int32_t nFileSize)
{
	int32_t ret = 0;
	
	/* �o�[�W�����`�F�b�N */
	ret = (int32_t)FileHeader_Load(sFileName, NULL, sizeof(uint8_t), nFileSize );	/* �w�b�_���ǂݍ��� */
	
	return ret;
}
		
/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
		g_nIntLevel = intlevel(7);	/* ���֐ݒ� */
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
static void interrupt IntVect(void)
{
#if 0
	volatile uint8_t *MFP_USART_D = (uint8_t *)0xE8802Fu;
	uint8_t ubKey;
	
	Set_DI();	/* ���֐ݒ� */
	
	ubKey = *MFP_USART_D;
	
	/* ESC */
	if(ubKey == 0x01u)	/* On */
	{
		if(g_nCansel == 0)
		{
			g_nCansel = 1;
			SetKeyInfo(0, ubKey);	/* �t�@�C���}�l�[�W���[�� */
		}
	}
//	else if(ubKey == 0x81u)	/* Off */
//	{
//		g_nCansel = 0;
//	}
	else
	{
		g_nCansel = 0;
	}

	/* Q */
	if(g_nBreak == 0)
	{
		if(ubKey == 0x11u)	/* On */
		{
			g_nBreak = 1;
			g_nPlayListMode = 0;
			SetKeyInfo(1, ubKey);	/* �t�@�C���}�l�[�W���[�� */
		}
		else if(ubKey == 0x91u)	/* Off */
		{
		}
	}
	
//	printf("Hit! 0x%x :", *MFP_USART_D);
	
	Set_EI();	/* ���։��� */

#endif
	IRTE();	/* 0x0000-0x00FF���荞�݊֐��̍Ō�ŕK�����{ */
//	IRTS();	/* 0x0100-0x01FF���荞�݊֐��̍Ō�ŕK�����{ */
}
/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int16_t	TimerD_INIT()
{
	int16_t ret=0;
	int32_t nUse;

	/* Timer-D */
	nUse = _iocs_timerdst(Timer_D_Func, 7, 20);	/* Timer-D 50us(7) x 20cnt = 1ms */
	if(nUse == 0)	/*  */
	{
		g_unDownTimer = 1000;	/* 1000ms */

//		puts("message:Timer-D start");
		g_bTimer_D = TRUE;
	}
	else
	{
		g_unDownTimer = 55;	/* 55Hz */
		
		g_nSuperchk = _dos_super(0);	/* �X�[�p�[�o�C�U�[���[�h�J�n */

//		puts("error:Timer-D �g���܂���ł����B");
		g_bTimer_D = FALSE;
	}

	return ret;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int16_t	TimerD_EXIT()
{
//	Set_DI();	/* ���荞�݋֎~�ݒ� */
	
	if(g_bTimer_D == TRUE)
	{
		g_bTimer_D = FALSE;
		_iocs_timerdst((void *)0, 0, 1);	/* stop */
//		puts("message:Timer-D end");
	}
	else
	{
		if(g_nSuperchk > 0)
		{
			_iocs_b_super(g_nSuperchk);		/*�X�[�p�[�o�C�U�[���[�h�I��*/
		}
		g_bTimer_D = FALSE;
	}

//	Set_EI();	/* ���荞�݋֎~���� */

	return 0;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
static void interrupt Timer_D_Func(void)
{

	if(g_unDownTimer != 0u)
	{
		g_unDownTimer--;
	}

	IRTE();	/* rte */
}

/*===========================================================================================*/
/* �֐���	�F	*/
/* ����		�F	*/
/* �߂�l	�F	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		�F	*/
/*===========================================================================================*/
int16_t wait_vdisp(int16_t count)	/* ��count�^60�b�҂�	*/
{
	int16_t ret = 0;
	volatile uint8_t *pGPIP = (uint8_t *)0xe88001;
	
	/* GPIP4 VDISP @kamadox ���� */
	/* 0 �͐����u�����L���O���ԁB���Ȃ킿�A�����t�����g�|�[�` + ���������p���X + �����o�b�N�|�[�`�B*/
	/* 1 �͐����f�����ԁB*/
	/* ���[�J�[�R���̎����͐����A�����ԁ^�����\�����ԂƂ����������ɂȂ��Ă���A*/
	/* ���ꂪ���������M���ł��邩�̂悤�Ȍ���������Ă���B*/
	while(count--)
	{
		while(((*pGPIP) & GP_VDISP) == 0u);	/* �����u�����L���O���ԑ҂�	*/
		while(((*pGPIP) & GP_VDISP) != 0u);	/* �����f�����ԑ҂�	*/
	}

	return ret;
}

/*===========================================================================================*/
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
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
/* �֐���	:	*/
/* ����		:	*/
/* �߂�l	:	*/
/*-------------------------------------------------------------------------------------------*/
/* �@�\		:	*/
/*===========================================================================================*/
int16_t main(int16_t argc, int8_t *argv[])
{
	int16_t ret = 0;
	int32_t	nFileSize = 0;
	int32_t	nFilePos = 0;
	
	int32_t	i, j;
	
	puts("MACS data Player�uMACSplay.x�vv1.06p (c)2022-2023 �J�^.");
	
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
				if((bFlag == TRUE) && (bFlag2 == TRUE))
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
					printf("error:�t�@�C����������܂���ł���!%s\n", argv[i]);
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
		puts("error:MACSDRV���풓���Ă�������!");
		ret = -1;
	}
	
	
	if(ret == 0)
	{
		int32_t nOut = 0;
		int8_t	data_b;
		int32_t	nSysStat;
		int32_t	nReTry;
		int32_t nLoop;
		int32_t	nHiMemChk;
		int32_t	nChk;

		nLoop = g_nRepeat;
		
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
		
		nHiMemChk = HIMEM_CHK();	/* �n�C�����������`�F�b�N */
		
		nChk = PCM8A_CHK();			/* PCM8A�풓�`�F�b�N */

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
//		printf("g_unMACS_File_List_MAX = %d\n", g_unMACS_File_List_MAX);
		nReTry = 0xFFFF;

		do
		{

			if(g_nRepeat == 0)	/* �i�v���[�v */
			{
				nLoop = 1;
			}
			else
			{
				nLoop--;
			}

			for(i=0,j=0; i < g_unMACS_File_List_MAX; i++)
			{
				int32_t	nMemSize;
				int8_t	bPlayMode;
#if 0
				void *p;
#endif
				if(g_nBreak != 0u)
				{
					nOut = -2;	/* �����I�� */
					nLoop = 0;	/* ���[�v����O�� */
					break;
				}			//printf("Option = 0x%x\n", g_nEffect);

				if(	g_nRepeatOnMem <= 1 )	/* �I���������ł͂Ȃ� */
				{
					if(g_nPlayListMode != 0)
					{
						printf("\n");	/* ���s�̂� */
		//				printf("-= MACS File List No.%04d %s =-\n", i+1, g_sMACS_File_List[i]);
						printf("-= MACS File List No.%04d =-\n", i+1);
					}

					nFileSize = 0;
					GetFileLength( g_sMACS_File_List[i], &nFileSize );	/* �t�@�C���̃T�C�Y�擾 */
					if(nFileSize == 0)
					{
						puts("error:�t�@�C���T�C�Y���ُ�ł�!");
						continue;
					}
					nMemSize = (int32_t)_dos_malloc(-1) - 0x81000000UL;	/* ���C���������̋󂫃`�F�b�N */

					/* MACS�f�[�^�ǂݍ��݃��[�h���菈�� */
					if( (nHiMemChk == 0) && ((nChk >= 0) || (g_nPCM8Achk > 0)) )	/* �n�C���������� & (PCM8A�풓�`�F�b�N or -AD�Ŗ�����) */
					{
						bPlayMode = 2;	/* �n�C�������Đ� */
					}
					else if( (nHiMemChk != 0) && ((nSysStat == 4) || (nSysStat == 6)) )		/* �n�C������������ & 040/060EXCEL */
					{
						printf("(040/060EXCEL)");
						bPlayMode = 1;	/* �g�����ꂽ�������ōĐ� */
					}
					else if(nFileSize <= nMemSize)	/* ���C�������� */
					{
						bPlayMode = 0;	/* ���C���Đ� */
					}
					else
					{
						nOut = -5;		/* �ُ� */
						bPlayMode = -1;	/* �ǂݍ��݂ł��Ȃ� */
						ret = -1;
						
						if(nChk < 0)	/* >=0:�풓 <0:�풓���Ă��Ȃ� */
						{
							puts("error:PCM8A���풓����Ă��܂���!���n�C���������g���ꍇ�́APCM8A -w18 �ŏ풓��������!");
						}
						
						if(nHiMemChk != 0)
						{
							puts("error:�n�C��������������܂���ł���!");
						}

						if(nFileSize > nMemSize)
						{
							puts("error:���C���������̋󂫂��s���ł�!");
						}
					}
					/* MACS�f�[�^�ǂݍ��ݏ��� */
					if(bPlayMode >= 0)
					{
						nOut = MACS_Load(g_sMACS_File_List[i], nFileSize, bPlayMode);	
					}
					
					/* MACS�f�[�^�ǂݍ��ݏ��� �����I������ */
#if 0
					p = (void *)_iocs_b_intvcs( 0x4C, (int32_t)IntVect );	/* MFP (key seral input ���� */
#endif				
					TimerD_INIT();	/* Timer-D ON */
					do{ 
						if(g_bTimer_D == FALSE)
						{
							if(g_unDownTimer != 0u)
							{
								g_unDownTimer--;
							}
							wait_vdisp(1);	/* ���������M���҂� */
						}

						if(g_unDownTimer == 0)
						{
							puts("message:ESC�L�[������������܂���");
							g_nBreak = 1;		/* �����I�� */
							nLoop = 0;	/* ���[�v����O�� */
							nOut = -2;
							break;
						}
					}while(( _iocs_bitsns( 0 ) & 0x02 ) != 0u );		/* ESC */
					TimerD_EXIT();	/* Timer-D OFF */
#if 0
					_iocs_b_intvcs( 0x4C, (int32_t)p );	/* ���ɖ߂� */
#endif
				}
				else
				{
					/* �������Ȃ� */
				}

				if( nOut >= 0 )	/* ���펞 */
				{
					if(g_nPlayListMode == 0)	/* �P��Đ��� */
					{
						g_nRepeatOnMem = 1;	/* �I���������Ń��[�v���s�s�� */
						if(nOut == 1)	/* ������������Ȃ����� */
						{
							printf("message:�Đ��ł��܂���ł���!skip���܂�!(List No.%d)\n", i+1);
						}
						else
						{
							printf("message:�Đ����܂�!(List No.%d)                   \n", i+1);
							ret = MACS_PlayCtrl(0);	/* �Đ����� */
							MACS_MemFree(-1);		/* ��������� */
						}
					}
					else if((g_nPlayListMode == 1) || (g_nPlayListMode == 2))	/* �v���C���X�g�Đ� */
					{
						if(nOut == 1)	/* ������������Ȃ����� */
						{
							g_nRepeatOnMem = 1;	/* �I���������Ń��[�v���s�s�� */
							if(nReTry != i)
							{
								printf("message:��x�ǂݍ��݂𒆒f���A�ǂݍ��ݍς݃f�[�^���Đ����܂�!(List No.%d)\n", i+1);
								nReTry = i;
								i--;	/* �ǂ߂Ȃ����������ă`�������W */
								ret = MACS_PlayCtrl(0);	/* �Đ����� */
								MACS_MemFree(-1);	/* ��������� */
							}
							else
							{
								printf("message:�ǂݍ��߂܂���ł����Bskip���܂�!(List No.%d)\n", i+1);
								nReTry = i;
							}
						}
						else if(nOut == 2)	/* �ǂݍ��݂𒆒f���� */
						{
							printf("message:skip���܂�!(List No.%d)                     \n", i+1);
							nReTry = i;
						}
						else /* �ǂݍ��߂� */
						{
							if(g_nPlayListMode == 1)
							{
								printf("message:�Đ����܂�!(List No.%d)                   \n", i+1);
								ret = MACS_PlayCtrl(i);	/* �Đ����� */
								if( (g_unMACS_File_List_MAX -1) == i )	/* ���X�g�̍Ō�܂œǂ񂾂Ƃ� */
								{
									if(	g_nRepeatOnMem == 0 )	/* �I���������ł͂Ȃ� */
									{
										g_nPlayListMode = 2;	/* �v���C���X�g�Đ��֏��i */
										g_nRepeatOnMem = 2;		/* �I���������Ń��[�v���s */
									}
									else
									{
										/* �������Ȃ� */
									}
								}
							}
							else
							{

							}
							nReTry = i;
						}
					}
					else
					{
						nOut = -5;	/* �ُ� */
					}
					
				}
				else
				{
					if(nOut > -5)
					{
						/* �������Ȃ� */
					}
					else
					{
						nOut = -5;	/* �ُ� */
					}
				}
				
				if( nOut < 0 )	/* �ُ� */
				{
					break;
				}
				
			}

			if((g_nPlayListMode == 2)	/* �v���C���X�g�Đ� */
			&& (g_nBreak == 0))			/* �����I������Ȃ� */
			{
				//printf("message:�ǂݍ��ݍς݃f�[�^���Đ����܂�! Loop=%d\n", nLoop);
				//puts("message:�v���C���X�g�Đ����܂�!        ");
				ret = MACS_PlayCtrl(0);	/* �Đ����� */
				//puts("message:�v���C���X�g�Đ��I�����܂�!    ");
				nOut = 0;
				if(	g_nRepeatOnMem == 0 )	/* �I���������ł͂Ȃ� */
				{
					g_nRepeatOnMem = 2;	/* �I���������Ń��[�v���s */
				}
				else if( g_nRepeatOnMem == 1 )	/* �I���������ł͂Ȃ� */
				{
					MACS_MemFree(-1);	/* ��������� */
				}
				else
				{
					/* �������Ȃ� */
				}
			}
		}
		while(nLoop);

		MACS_MemFree(-1);	/* ��������� */

		if(nOut < 0)
		{
			switch(nOut)
			{
			case -1:
				puts("error:�Đ��Ɏ��s���܂���!");
				break;
			case -2:
				puts("message:�����I�����܂����B");
				break;
			case -4:
				puts("message:�Đ��𒆒f���܂����B");
				break;
			default:
				printf("error:�ُ�I�����܂���!(%d)\n", nOut);
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