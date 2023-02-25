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

#define MFP_USART_REG	(0xE8802F)	/* USARTデータレジスタ */
#define IOCS_WORK0		(0x800)		/* IOCSワークエリア0 */
#define IOCS_WORK2		(0x802)		/* IOCSワークエリア2 */
#define IOCS_WORK3		(0x803)		/* IOCSワークエリア2 */
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

/* グローバル変数 */
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
int32_t		g_nFileListNum = 0;		/* 現在のリスト番号 */
int32_t		g_nFileListNumMAX = 0;	/* ファイルリストの最大数 */
int32_t		g_nMemAllocNumMAX = 0;	/* メモリリストの最大数 */
static int8_t	*g_pMACS_Buff[256];		/* MACSデータのポインタ */
static int8_t	g_bMACS_MemMode[256];	/* MACSデータのメモリモード */

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
/*	MACS_Play() MACS_Cal.docより
*	Input:
*		d1.w > コマンドコード
*		d2.l > バッファサイズ
*		d3.w > アボートフラグ
*		d4.l > 特殊効果フラグ
*		a1.l > 処理アドレス
*	------------------------------------------------------------------------------
*		d1.w = 0	アニメーション再生
*					(a1)から始まるアニメデータを再生します。
*	------------------------------------------------------------------------------
*		d2.lは通常-1($FFFFFFFF)にして下さい。
*				（アニメデータ再生中、d2.lのエリアを越えようとした時,再生を中止します）
*	------------------------------------------------------------------------------
*	Output:  d0.l > Status
*------------------------------------------------------------------------------
*	d1.w = 0	アニメーション再生
*				(a1)から始まるアニメデータを再生します。
*				d2.lは通常-1($FFFFFFFF)にして下さい。
*				（アニメデータ再生中、d2.lのエリアを越えようとした時
*				　再生を中止します）
*				d4.lについては下表を参照
*	
*	d1.w = 1	再生状況検査
*				リターン値が0以外ならばアニメーション再生中です
*	
*	d1.w = 2	中断リクエスト発行
*				アニメーション再生を中断するよう要求します。
*				d3.wの内容により中断後に処理を行なわせることが可能です
*	
*			d3.w = -1	中断後何もしない(通常)
*			d3.w = -2	中断後 d2,d4,a1の内容によりアニメーション開始
*			d3.w = -3	中断後 a1.lのアドレスにサブルーチンコール
*	
*	d1.w = 3	MACSバージョン検査
*				d0.wに常駐しているMACSのバージョン番号を返します。
*				(Version 1.16なら$01_16)
*	
*	d1.w = 4	MACSアプリケーションを登録します
*				a1.lにアプリケーション名へのポインタを設定します。
*				アプリケーション名は
*					'～～.x [アプリケーション名]',0
*				というフォーマットに従って下さい。このコールでアプリを
*				登録することで、MACSの常駐解除を禁止することができます
*	
*	d1.w = 5	MACSアプリケーションの登録を解除します
*				a1.lに(d1.w=4)で登録したアプリケーション名へのポインタ
*				を渡しコールすると登録アプリケーションを解除します。
*				登録可能アプリケーションは最大16個です。(Ver1.00)
*	
*	d1.w = 6	MACSデータのデータバージョンを得ます
*				a1.lにMACSデータの先頭アドレスを入れてコールします。
*				返り値としてd0.lにデータが作成されたMACSのバージョンが
*				入ります。d0.l=-1の時はMACSデータではありません。
*	
*	d1.w = 7	画面を退避し、使用可能にします
*				アニメーションスタートと同じ処理を行ないます。
*				このコールを利用する時は必ずアボートフラグに対応して
*				下さい。
*	
*	d1.w = 8	退避した画面を元に戻します
*				画面再初期化抑制フラグを1にしてアニメーション再生を行
*				った時は必ずあとでこのコールを発行して下さい。
*	
*	d1.w = 9	テキストVRAMにグラフィックを展開します
*				a1.lにMACS形式グラフィックデータのアドレス，a2.lに展開
*				先のテキストVRAMアドレスを指定してコールします。
*	
*	d1.w = 10	垂直同期割り込みを登録します
*				a1.lに割り込みルーチンのアドレス、d2.wにフラグ*256+カ
*				ウンタをセットしてコールします。
*				フラグは現在使用されていません。0にしておいて下さい。
*	
*	d1.w = 11	垂直同期割り込みを解除します
*				a1.lに登録した割り込みルーチンのアドレスをセットして
*				コールします。
*	
*	d1.w = 12	スリープ状況をチェックします
*				リターン値が0以外ならMACSDRVはスリープしています
*	
*	d1.w = 13	スリープ状態を設定します
*				d2.wが0なら再生可能状態，0以外ならスリープします。
*
*	d1.w = 14	自動フレームスキップの有効/無効を設定します。
*				d2.wが0なら無効、1なら有効。2で厳格な基準で有効になります。
*				0 = 描画が遅れてもガン無視放置する
*				1 = 1フレーム弱遅れた程度ならティアリング上等で描画を続ける
*				2 = ほんのちょっとでも遅れたら描画をスキップする
*				初期値は 2 です。互換性維持のため追加された画面モードのみ対応です。
*	
*	d1.w = -1	ワークエリアアドレス獲得
*				d0.lにMACS内部ワークエリアのアドレスを返します。
*------------------------------------------------------------------------------
*特殊効果フラグ (d4.l)
*
*bit 0	ハーフトーン			 		(1の時パレットを50%暗くする)
*bit 1	PCMカット				 		(1の時PCMの再生をしない)
*bit 2	キーセンスアボートOFF 			(1の時キー入力時終了しない)
*bit 3	画面再初期化抑制フラグ 			(1の時アニメ終了時画面を初期化しない)
*bit 4	スリープ状態無視 				(1の時MACSDRVがスリープしている時でも再生する)
*bit 5	グラフィックＶＲＡＭ表示フラグ 	(1の時ｸﾞﾗﾌｨｯｸを表示したまま再生する)
*bit 6	スプライト表示フラグ 			(1の時ｽﾌﾟﾗｲﾄを表示したまま再生する)
*
*bit 7
*  :		Reserved. (必ず0にすること)
*bit31
*
*------------------------------------------------------------------------------
* Status エラーコード一覧 (d0.l)
*
*-1	MACSデータではありません
*-2	MACSのバージョンが古すぎます
*-3	データがバッファに全部入力されていません
*-4	アボートされました
*-5	既に現在アニメーション再生中です
*-6	無効なファンクションを指定しました
*-7	ユーザーリストもしくは割り込みリストが一杯です
*-8	ユーザーリストもしくは割り込みリストに登録されていません
*==============================================================================
*/

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_Play(int8_t *pMacsBuff, int32_t nCommand, int32_t nBuffSize, int32_t nAbort, int32_t nEffect)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_MACS_CALL;	/* IOCS _MACS(macsdrv.x) */
	stInReg.d1 = nCommand;			/* コマンドコード(w) */
	stInReg.d2 = nBuffSize;			/* バッファサイズ(l) */
	stInReg.d3 = nAbort;			/* アボートフラグ(w) */
	stInReg.d4 = nEffect;			/* 特殊効果フラグ(l) */
	stInReg.a1 = (int32_t)pMacsBuff;	/* MACSデータの先頭アドレス */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	return retReg;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_error(int32_t nStatus)
{
	int32_t ret = 0;
	
	ret = nStatus;

	switch(ret)
	{
		case  0:	break;
		case -1:	puts("MACSDRV:MACSデータではありません");	break;
		case -2:	puts("MACSDRV:MACSのバージョンが古すぎます");	break;
		case -3:	puts("MACSDRV:データがバッファに全部入力されていません");	break;
		case -4:	puts("MACSDRV:アボートされました");	break;
		case -5:	puts("MACSDRV:既に現在アニメーション再生中です");	break;
		case -6:	puts("MACSDRV:無効なファンクションを指定しました");	break;
		case -7:	puts("MACSDRV:ユーザーリストもしくは割り込みリストが一杯です");	break;
		case -8:	puts("MACSDRV:ユーザーリストもしくは割り込みリストに登録されていません");	break;
		default:	printf("MACSDRV:error(%d)\n", ret);	break;
	}
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_PlayCtrl(int32_t nPlay)
{
	int32_t ret = 0;
	int32_t nPlayCount;
	int8_t	*pBuff = NULL;
#if 0
	void *p;
#endif
	
	g_bSetCommand = FALSE;	/* コマンドなし */
	
	if( (nPlay < 0) || (nPlay >= g_nFileListNumMAX) )
	{
		nPlay = g_nFileListNumMAX - 1;	/* 末端 */
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

//				printf("message:再生を開始します(BufferList No.%d)\n", nPlayCount+1);

			g_nCansel = 0;	/* ESC反応なし */
			g_nCommand = 0;	/* 再生 */
			g_nReturn = MACS_Play(g_pBuff, g_nCommand, g_nBuffSize, g_nAbort, g_nEffect);	/* 再生 */

			TimerD_INIT();	/* Timer-D ON */

			do	/* バックグランド処理 */
			{
				if(g_nReturn == 0)		/* 再生終了 */
				{
					bFlag = FALSE;
				}
				else					/* エラーもしくは中断 */
				{
					if(g_bTimer_D == FALSE)
					{
						if(g_unDownTimer != 0u)
						{
							g_unDownTimer--;
						}
						wait_vdisp(1);	/* 垂直同期信号待ち */
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
							puts("message:ESCキーが長押しされました");
							g_nBreak = 1;		/* 強制終了 */
						}
						bFlag = FALSE;
					}
				}
			}
			while(bFlag);

			TimerD_EXIT();	/* Timer-D OFF */

			_dos_kflushio(0xFF);	/* キーバッファをクリア */
				
			if(g_nBreak != 0u)
			{
				puts("message:ESC長押しの強制終了しました");
				ret = -2;	/* 強制終了判定 */
				break;
			}
			else
			{
//					printf("message:再生を終了しました(BufferList No.%d)\n", nPlayCount+1);
				ret = MACS_error(g_nReturn);	/* エラーチェック */
				if(ret < 0)
				{
					continue;
				}
				else
				{
					if(g_nPlayHistory != 0)	/* 視聴履歴機能 */
					{
						SetHisFileCnt(g_sMACS_File_Path[nPlayCount]);	/* 再生回数を更新 */
					}
				}
			}
		}
	}

#if 0
	_iocs_b_intvcs( 0x4C, (int32_t)p );	/* 元に戻す */
#endif
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_Load(int8_t *sFileName, int32_t nFileSize, int8_t bMode)
{
	int32_t ret = 0;
	int8_t	*pBuff = NULL;
	int16_t f_ver;
	
	/* バージョンチェック:旧バージョンドライバで新バージョンファイルを再生しないようにする。 */
	ret = MACS_DriverVer_CHK(sFileName, nFileSize, 3);	/* ドライババージョン */
	
	f_ver = (int16_t)MACS_FileVer_CHK(sFileName, nFileSize);	/* ファイルバージョン */
	
	if(f_ver < 0)
	{
		printf("error:MACSデータに異常があります!(DRV=%x.%x,DATA=%x.%x,FILESIZE=%d)\n", ((ret>>8) & 0xFF), (ret & 0xFF), ((f_ver>>8) & 0xF), (f_ver & 0xFF), nFileSize);
		return -1;
	}
	else
	{
		if(ret == 0x123)	/* 改造バージョン */
		{
			/* 下位互換ありなのでチェック不要 */
			ret = MACS_DriverVer_CHK(sFileName, nFileSize, 17);	/* 改造ドライババージョン取得(17) */
			//printf("MACSDRV.X Version %x.%x.%x を検出しました\n", ((ret>>16) & 0xFF), ((ret>>8) & 0xFF), (ret & 0xFF));
		}
		else				/* 普通バージョン */
		{
			if(ret < f_ver)
			{
				printf("warning:古いMACSDRV.X Version %x.%x を検出しました\n", ((ret>>8) & 0xFF), (ret & 0xFF));
				puts("error:MACSDRV Version 1.16の勝手に改造版 v0.15.11以降を常駐してください!");
				return -1;
			}
		}
		
	}
	
	if(g_nPlayHistory != 0)	/* ファイル読み込みからメモリへ保存 */
	{
		int32_t nPlayHisCnt;
		
		nPlayHisCnt = GetHisFileCnt(sFileName);	/* メモリ確保できたらカウント */
		if(nPlayHisCnt > 0)
		{
			printf("message:このファイルの再生回数は%4ld回です。\n", nPlayHisCnt);	/* ヒストリファイルから */
		}
		else if(nPlayHisCnt == 0)
		{
			puts("message:このファイルは初めて再生されます。");	/* 初再生 */
		}
		else
		{
			/* 何もしない */
		}
	}

	/* メモリ確保 */
	switch(bMode)
	{
	case 0:
		pBuff = (int8_t*)MyMalloc(nFileSize);	/* メモリ確保 */
		break;
	case 1:
		pBuff = (int8_t*)MyMallocJ(nFileSize);	/* メモリ確保 */
		break;
	case 2:
		pBuff = (int8_t*)MyMallocHi(nFileSize);	/* メモリ確保 */
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
			if(g_nPlayListMode > 0)	/* プレイリスト再生 */
			{
				printf("message:メモリが確保できませんでした。(%d)\n", bMode);
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
		if(pBuff >= (int8_t*)0x1000000)	/* TS-6BE16相当のアドレス値以上で確保されているか？ */
		{
			printf("Hi-Memory(addr=0x%p)\n", pBuff);	/* メモリの先頭アドレスを表示 */
		}
		else
		{
			printf("Main-Memory(addr=0x%p)\n", pBuff);	/* メモリの先頭アドレスを表示 */
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
		g_pMACS_Buff[g_nMemAllocNumMAX] = pBuff;	/* アドレスを代入*/
		g_bMACS_MemMode[g_nMemAllocNumMAX] = bMode;	/* モードを記憶 */
			
		if( File_Load(sFileName, pBuff, sizeof(uint8_t), nFileSize ) == 0 )	/* ファイル読み込みからメモリへ保存 */
		{
			g_nMemAllocNumMAX++;	/* メモリ確保できたらカウント */
			ret = 0;
		}
		else	/* ロードキャンセル */
		{
			MACS_MemFree(g_nMemAllocNumMAX);
			g_pMACS_Buff[g_nMemAllocNumMAX] = NULL;	/* 初期化 */
			ret = 2;
		}
	}

	if(ret == 0)
	{
		if(g_nPlayHistory != 0)	/* フルパスを記憶する */
		{
			int8_t	*p;
			
			_fullpath(g_sMACS_File_Path[g_nFileListNumMAX], sFileName, 128);	/* フルパスを取得 */
			
			while ((p = strchr(g_sMACS_File_Path[g_nFileListNumMAX], '/'))!=NULL) *p = '\\';
		}
		g_nFileListNumMAX++;
	}
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
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
				MyMfree(pBuff);		/* メモリ解放 */
				break;
			case 1:
				MyMfreeJ(pBuff);	/* メモリ解放 */
				break;
			case 2:
				MyMfreeHi(pBuff);	/* メモリ解放 */
				break;
			default:
				MyMfree(pBuff);		/* メモリ解放 */
				break;
			}
			g_pMACS_Buff[nPlayCount] = NULL;
		}
	}
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_DriverVer_CHK(int8_t *sFileName, int32_t nFileSize, int32_t nDriverVer)
{
	int32_t ret = 0;
	int8_t	*pBuff = NULL;
	
	/* バージョンチェック */
	ret = MACS_Play(pBuff, nDriverVer, g_nBuffSize, g_nAbort, g_nEffect);	/* 実行 */
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_FileVer_CHK(int8_t *sFileName, int32_t nFileSize)
{
	int32_t ret = 0;
	
	/* バージョンチェック */
	ret = (int32_t)FileHeader_Load(sFileName, NULL, sizeof(uint8_t), nFileSize );	/* ヘッダ情報読み込み */
	
	return ret;
}
		
/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	MACS_CHK(void)
{
	int32_t ret = 0;	/* 0:常駐 !0:常駐していない */
	
	int32_t addr_IOCS;
	int32_t addr_MACSDRV;
	int32_t data;
	
	addr_IOCS = ((IOCS_OFST + IOCS_MACS_CALL) * 4);
	
	addr_MACSDRV = _iocs_b_lpeek((int32_t *)addr_IOCS);
	addr_MACSDRV += 2;
	
	data = _iocs_b_lpeek((int32_t *)addr_MACSDRV);
	if(data != 'MACS')ret |= 1;	/* アンマッチ */
	
	addr_MACSDRV += sizeof(int32_t);
	
	data = _iocs_b_lpeek((int32_t *)addr_MACSDRV);
	if(data != 'IOCS')ret |= 1;	/* アンマッチ */
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	SYS_STAT_CHK(void)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_SYS_STAT;	/* IOCS _SYS_STAT($AC) */
	stInReg.d1 = 0;				/* MPUステータスの取得 */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
//	printf("SYS_STAT 0x%x\n", (retReg & 0x0F));
    /* 060turbo.manより
		$0000   MPUステータスの取得
			>d0.l:MPUステータス
					bit0～7         MPUの種類(6=68060)
					bit14           MMUの有無(0=なし,1=あり)
					bit15           FPUの有無(0=なし,1=あり)
					bit16～31       クロックスピード*10
	*/
	
	return (retReg & 0x0F);
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	HIMEM_CHK(void)
{
	int32_t ret = 0;	/* 0:常駐 !0:常駐していない */
	
	int32_t addr_IOCS;
	int32_t addr_HIMEM;
	int32_t data;
	int8_t data_b;
	
	addr_IOCS = ((IOCS_OFST + IOCS_HIMEM) * 4);
	
	addr_HIMEM = _iocs_b_lpeek((int32_t *)addr_IOCS);
	addr_HIMEM -= 6;
	
	data = _iocs_b_lpeek((int32_t *)addr_HIMEM);
	if(data != 'HIME')ret |= 1;	/* アンマッチ */
//	printf("data %x == %x\n", data, 'HIME');
	
	addr_HIMEM += 4;
	
	data_b = _iocs_b_bpeek((int32_t *)addr_HIMEM);
	if(data_b != 'M')ret |= 1;	/* アンマッチ */
//	printf("data %x == %x\n", data_b, 'M');
	
	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int32_t	PCM8A_CHK(void)
{
	struct	_regs	stInReg = {0}, stOutReg = {0};
	int32_t	retReg;	/* d0 */
	
	stInReg.d0 = IOCS_ADPCMMOD;	/* IOCS _ADPCMMOD($67) */
	stInReg.d1 = 'PCMA';		/* PCM8Aの常駐確認 */
	
	retReg = _iocs_trap15(&stInReg, &stOutReg);	/* Trap 15 */
	
	return retReg;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
/* (AD)PCM効果音の停止 */
int32_t ADPCM_Stop(void)
{
	int32_t	ret=0;
	
	if(_iocs_adpcmsns() != 0)	/* 何かしている */
	{
		_iocs_adpcmmod(1);	/* 中止 */
		_iocs_adpcmmod(0);	/* 終了 */
	}
	
	return	ret;
}
/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
void Set_DI(void)
{
	if(g_nIntCount == 0)
	{
#if 0
		/*スーパーバイザーモード終了*/
		if(g_nSuperchk > 0)
		{
			_dos_super(g_nSuperchk);
		}
#endif
		g_nIntLevel = intlevel(7);	/* 割禁設定 */
		g_nIntCount = Minc(g_nIntCount, 1u);
		
#if 0
		/* スーパーバイザーモード開始 */
		g_nSuperchk = _dos_super(0);
#endif
	}
	else
	{
		g_nIntCount = Minc(g_nIntCount, 1u);
	}
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
void Set_EI(void)
{
	if(g_nIntCount == 1)
	{
		g_nIntCount = Mdec(g_nIntCount, 1);
		
		/* スプリアス割り込みの中の人も(以下略)より */
		/* MFPでデバイス毎の割込み禁止設定をする際には必ずSR操作で割込みレベルを上げておく。*/
		asm ("\ttst.b $E9A001\n\tnop\n");
		/*
			*8255(ｼﾞｮｲｽﾃｨｯｸ)の空読み
			nop		*直前までの命令パイプラインの同期
					*早い話、この命令終了までには
					*それ以前のバスサイクルなどが
					*完了していることが保証される。
		*/
#if 0
		/*スーパーバイザーモード終了*/
		if(g_nSuperchk > 0)
		{
			_dos_super(g_nSuperchk);
		}
#endif
		intlevel(g_nIntLevel);	/* 割禁解除 */
#if 0
		/* スーパーバイザーモード開始 */
		g_nSuperchk = _dos_super(0);
#endif
	}
	else
	{
		g_nIntCount = Mdec(g_nIntCount, 1);
	}
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
static void interrupt IntVect(void)
{
#if 0
	volatile uint8_t *MFP_USART_D = (uint8_t *)0xE8802Fu;
	uint8_t ubKey;
	
	Set_DI();	/* 割禁設定 */
	
	ubKey = *MFP_USART_D;
	
	/* ESC */
	if(ubKey == 0x01u)	/* On */
	{
		if(g_nCansel == 0)
		{
			g_nCansel = 1;
			SetKeyInfo(0, ubKey);	/* ファイルマネージャーへ */
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
			SetKeyInfo(1, ubKey);	/* ファイルマネージャーへ */
		}
		else if(ubKey == 0x91u)	/* Off */
		{
		}
	}
	
//	printf("Hit! 0x%x :", *MFP_USART_D);
	
	Set_EI();	/* 割禁解除 */

#endif
	IRTE();	/* 0x0000-0x00FF割り込み関数の最後で必ず実施 */
//	IRTS();	/* 0x0100-0x01FF割り込み関数の最後で必ず実施 */
}
/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
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
		
		g_nSuperchk = _dos_super(0);	/* スーパーバイザーモード開始 */

//		puts("error:Timer-D 使えませんでした。");
		g_bTimer_D = FALSE;
	}

	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int16_t	TimerD_EXIT()
{
//	Set_DI();	/* 割り込み禁止設定 */
	
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
			_iocs_b_super(g_nSuperchk);		/*スーパーバイザーモード終了*/
		}
		g_bTimer_D = FALSE;
	}

//	Set_EI();	/* 割り込み禁止解除 */

	return 0;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
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
/* 関数名	：	*/
/* 引数		：	*/
/* 戻り値	：	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		：	*/
/*===========================================================================================*/
int16_t wait_vdisp(int16_t count)	/* 約count／60秒待つ	*/
{
	int16_t ret = 0;
	volatile uint8_t *pGPIP = (uint8_t *)0xe88001;
	
	/* GPIP4 VDISP @kamadox さん */
	/* 0 は垂直ブランキング期間。すなわち、垂直フロントポーチ + 垂直同期パルス + 垂直バックポーチ。*/
	/* 1 は垂直映像期間。*/
	/* メーカー由来の資料は垂直帰線期間／垂直表示期間という書き方になっており、*/
	/* これが垂直同期信号であるかのような誤解を招いている。*/
	while(count--)
	{
		while(((*pGPIP) & GP_VDISP) == 0u);	/* 垂直ブランキング期間待ち	*/
		while(((*pGPIP) & GP_VDISP) != 0u);	/* 垂直映像期間待ち	*/
	}

	return ret;
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
void HelpMessage(void)
{
	puts("MACSデータの再生を行います。");
	puts("  68030以降に限りメインメモリの空き容量が足りない場合はハイメモリを使って再生を行います。");
	puts("  ※ハイメモリでADPCMを再生する際は、PCM8A.X -w18 での常駐");
	puts("    もしくは、060turbo.sys -ad を設定ください。");
	puts("");
	puts("Usage:MACSplay.x [Option] file");
	puts("Option:-D  パレットを50%暗くする");
	puts("       -P  PCMの再生をしない");
	puts("       -K  キー入力時、終了しない");
	puts("       -C  アニメ終了時、画面を初期化しない");
	puts("       -SL MACSDRVがスリープしている時でも再生する");
	puts("       -GR ｸﾞﾗﾌｨｯｸを表示したまま再生する");
	puts("       -SP ｽﾌﾟﾗｲﾄを表示したまま再生する");
	puts("       -AD PCM8A.Xの常駐をチェックしない(060turbo.sys -adで再生する場合)");
	puts("       -Rx リピート再生するx=1～255回、x=0(無限ループ)");
	puts("       -L  プレイリスト<file>を読み込んで逐次再生する");
	puts("       -LA プレイリスト<file>を読み込んで再生する");
	puts("       -HS 再生したファイルの履歴を<MACSPHIS.LOG>に残す");
}

/*===========================================================================================*/
/* 関数名	:	*/
/* 引数		:	*/
/* 戻り値	:	*/
/*-------------------------------------------------------------------------------------------*/
/* 機能		:	*/
/*===========================================================================================*/
int16_t main(int16_t argc, int8_t *argv[])
{
	int16_t ret = 0;
	int32_t	nFileSize = 0;
	int32_t	nFilePos = 0;
	
	int32_t	i, j;
	
	puts("MACS data Player「MACSplay.x」v1.06p (c)2022-2023 カタ.");
	
	if(argc > 1)	/* オプションチェック */
	{
		for(i = 1; i < argc; i++)
		{
			int8_t	bOption;
			int8_t	bFlag;
			int8_t	bFlag2;
			
			bOption	= ((argv[i][0] == '-') || (argv[i][0] == '/')) ? TRUE : FALSE;

			if(bOption == TRUE)
			{
				/* ===== ２文字 =====*/
				bFlag	= ((argv[i][1] == 's') || (argv[i][1] == 'S')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'l') || (argv[i][2] == 'L')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_4);
					puts("Option:MACSDRVがスリープしている時でも再生します");
					continue;
				}

				bFlag	= ((argv[i][1] == 'g') || (argv[i][1] == 'G')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'r') || (argv[i][2] == 'R')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_5);
					puts("Option:ｸﾞﾗﾌｨｯｸを表示したまま再生します");
					continue;
				}
				
				bFlag	= ((argv[i][1] == 's') || (argv[i][1] == 'S')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'p') || (argv[i][2] == 'P')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_6);
					puts("Option:ｽﾌﾟﾗｲﾄを表示したまま再生します");
					continue;
				}

				bFlag	= ((argv[i][1] == 'a') || (argv[i][1] == 'A')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'd') || (argv[i][2] == 'D')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nPCM8Achk = 1;
					puts("Option:PCM8A.Xの常駐をチェックしません");
					continue;
				}

				bFlag	= ((argv[i][1] == 'r') || (argv[i][1] == 'R')) ? TRUE : FALSE;
				bFlag2	= strlen(&argv[i][2]);
				if((bFlag == TRUE) && (bFlag2 > 0))
				{
					g_nRepeat = atoi(&argv[i][2]);
					if(g_nRepeat < 0)			g_nRepeat = 1;
					else if(g_nRepeat > 255)	g_nRepeat = 1;

					g_nEffect = Mbclr(g_nEffect, Bit_2);	/* -K キー入力終了を有効にする */
					g_nEffect = Mbclr(g_nEffect, Bit_3);	/* -C アニメ終了時画面を初期化しないを有効にする */
					if(g_nRepeat == 0)
					{
						puts("Option:無限ループで再生します");
					}
					else
					{
						printf("Option:%d回リピート再生します\n", g_nRepeat);
					}
					continue;
				}
				
				bFlag	= ((argv[i][1] == 'h') || (argv[i][1] == 'H')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 's') || (argv[i][2] == 'S')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nPlayHistory = 1;
					puts("Option:再生したファイルの履歴を<MACSPHIS.LOG>に残します");
					continue;
				}

				bFlag	= ((argv[i][1] == 'l') || (argv[i][1] == 'L')) ? TRUE : FALSE;
				bFlag2	= ((argv[i][2] == 'a') || (argv[i][2] == 'A')) ? TRUE : FALSE;
				if((bFlag == TRUE) && (bFlag2 == TRUE))
				{
					g_nPlayListMode = 2;
					puts("Option:プレイリスト<file>を読み込んで再生します");
					continue;
				}
				
				/* ===== １文字 =====*/
				
				bFlag	= ((argv[i][1] == 'd') || (argv[i][1] == 'D')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_0);
					puts("Option:パレットを50%暗くします");
					continue;
				}

				bFlag	= ((argv[i][1] == 'p') || (argv[i][1] == 'P')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_1);
					puts("Option:PCMの再生をしません");
					continue;
				}

				bFlag	= ((argv[i][1] == 'k') || (argv[i][1] == 'K')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_2);
					puts("Option:キー入力時、終了しません");
					continue;
				}

				bFlag	= ((argv[i][1] == 'c') || (argv[i][1] == 'C')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nEffect = Mbset(g_nEffect, 0x7F, Bit_3);
					puts("Option:アニメ終了時、画面を初期化しません");
					continue;
				}
				
				
				bFlag	= ((argv[i][1] == 'l') || (argv[i][1] == 'L')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					g_nPlayListMode = 1;
					puts("Option:プレイリスト<file>を読み込んで逐次再生します");
					continue;
				}
				
				bFlag	= ((argv[i][1] == '?') || (argv[i][1] == 'h') || (argv[i][1] == 'H')) ? TRUE : FALSE;
				if(bFlag == TRUE)
				{
					HelpMessage();	/* ヘルプ */
					ret = -1;
					break;
				}
				
				HelpMessage();	/* ヘルプ */
				ret = -1;
				break;
			}
			else
			{
				nFilePos = i;
//				printf("nFilePos = %d\n", nFilePos);
#if 0
				nFileSize = 0;
				if(GetFileLength(argv[i], &nFileSize) < 0)	/* ファイルのサイズ取得 */
				{
					printf("error:ファイルが見つかりませんでした!%s\n", argv[i]);
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
		HelpMessage();	/* ヘルプ */
		ret = -1;
	}
	
	/* ドライバ常駐チェック */
	if(MACS_CHK() != 0)
	{
		puts("error:MACSDRVを常駐してください!");
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
		
		/* 機種判別 */
		data_b = _iocs_b_bpeek((int32_t *)0xCBC);	/* 機種判別 */
		//printf("機種判別(MPU680%d0)\n", data_b);
		
		if(data_b != 0)	/* 68000以外のMPU */
		{
			nSysStat = SYS_STAT_CHK();	/* MPUの種類 */
		}
		else
		{
			nSysStat = (int32_t)data_b;	/* 0が入る */
		}
		
		nHiMemChk = HIMEM_CHK();	/* ハイメモリ実装チェック */
		
		nChk = PCM8A_CHK();			/* PCM8A常駐チェック */

		if(g_nPlayListMode != 0)
		{
//			printf("g_nPlayListMode = %d\n", g_nPlayListMode);
			Load_MACS_List(argv[nFilePos], g_sMACS_File_List, &g_unMACS_File_List_MAX);	/* 動画(MACS)リストの読み込み */
//			for(i = 0; i < g_unMACS_File_List_MAX; i++)
//			{
//				printf("MACS  File %2d = %s\n", i, g_sMACS_File_List[i]);
//			}
		}
		else
		{
//			printf("g_nPlayListMode = %d\n", g_nPlayListMode);
			Get_MACS_File(argv[nFilePos], g_sMACS_File_List, &g_unMACS_File_List_MAX);	/* ファイルをチェックする */
//			memcpy(  g_sMACS_File_List[0], argv[nFilePos], sizeof(char) * strlen(argv[nFilePos]) ); 
		}
//		printf("g_unMACS_File_List_MAX = %d\n", g_unMACS_File_List_MAX);
		nReTry = 0xFFFF;

		do
		{

			if(g_nRepeat == 0)	/* 永久ループ */
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
					nOut = -2;	/* 強制終了 */
					nLoop = 0;	/* ループから外す */
					break;
				}			//printf("Option = 0x%x\n", g_nEffect);

				if(	g_nRepeatOnMem <= 1 )	/* オンメモリではない */
				{
					if(g_nPlayListMode != 0)
					{
						printf("\n");	/* 改行のみ */
		//				printf("-= MACS File List No.%04d %s =-\n", i+1, g_sMACS_File_List[i]);
						printf("-= MACS File List No.%04d =-\n", i+1);
					}

					nFileSize = 0;
					GetFileLength( g_sMACS_File_List[i], &nFileSize );	/* ファイルのサイズ取得 */
					if(nFileSize == 0)
					{
						puts("error:ファイルサイズが異常です!");
						continue;
					}
					nMemSize = (int32_t)_dos_malloc(-1) - 0x81000000UL;	/* メインメモリの空きチェック */

					/* MACSデータ読み込みモード判定処理 */
					if( (nHiMemChk == 0) && ((nChk >= 0) || (g_nPCM8Achk > 0)) )	/* ハイメモリ実装 & (PCM8A常駐チェック or -ADで無効化) */
					{
						bPlayMode = 2;	/* ハイメモリ再生 */
					}
					else if( (nHiMemChk != 0) && ((nSysStat == 4) || (nSysStat == 6)) )		/* ハイメモリ未実装 & 040/060EXCEL */
					{
						printf("(040/060EXCEL)");
						bPlayMode = 1;	/* 拡張されたメモリで再生 */
					}
					else if(nFileSize <= nMemSize)	/* メインメモリ */
					{
						bPlayMode = 0;	/* メイン再生 */
					}
					else
					{
						nOut = -5;		/* 異常 */
						bPlayMode = -1;	/* 読み込みできない */
						ret = -1;
						
						if(nChk < 0)	/* >=0:常駐 <0:常駐していない */
						{
							puts("error:PCM8Aが常駐されていません!※ハイメモリを使う場合は、PCM8A -w18 で常駐ください!");
						}
						
						if(nHiMemChk != 0)
						{
							puts("error:ハイメモリが見つかりませんでした!");
						}

						if(nFileSize > nMemSize)
						{
							puts("error:メインメモリの空きが不足です!");
						}
					}
					/* MACSデータ読み込み処理 */
					if(bPlayMode >= 0)
					{
						nOut = MACS_Load(g_sMACS_File_List[i], nFileSize, bPlayMode);	
					}
					
					/* MACSデータ読み込み処理 強制終了処理 */
#if 0
					p = (void *)_iocs_b_intvcs( 0x4C, (int32_t)IntVect );	/* MFP (key seral input あり */
#endif				
					TimerD_INIT();	/* Timer-D ON */
					do{ 
						if(g_bTimer_D == FALSE)
						{
							if(g_unDownTimer != 0u)
							{
								g_unDownTimer--;
							}
							wait_vdisp(1);	/* 垂直同期信号待ち */
						}

						if(g_unDownTimer == 0)
						{
							puts("message:ESCキーが長押しされました");
							g_nBreak = 1;		/* 強制終了 */
							nLoop = 0;	/* ループから外す */
							nOut = -2;
							break;
						}
					}while(( _iocs_bitsns( 0 ) & 0x02 ) != 0u );		/* ESC */
					TimerD_EXIT();	/* Timer-D OFF */
#if 0
					_iocs_b_intvcs( 0x4C, (int32_t)p );	/* 元に戻す */
#endif
				}
				else
				{
					/* 何もしない */
				}

				if( nOut >= 0 )	/* 正常時 */
				{
					if(g_nPlayListMode == 0)	/* 単一再生時 */
					{
						g_nRepeatOnMem = 1;	/* オンメモリでループ実行不可 */
						if(nOut == 1)	/* メモリが足りなかった */
						{
							printf("message:再生できませんでした!skipします!(List No.%d)\n", i+1);
						}
						else
						{
							printf("message:再生します!(List No.%d)                   \n", i+1);
							ret = MACS_PlayCtrl(0);	/* 再生制御 */
							MACS_MemFree(-1);		/* メモリ解放 */
						}
					}
					else if((g_nPlayListMode == 1) || (g_nPlayListMode == 2))	/* プレイリスト再生 */
					{
						if(nOut == 1)	/* メモリが足りなかった */
						{
							g_nRepeatOnMem = 1;	/* オンメモリでループ実行不可 */
							if(nReTry != i)
							{
								printf("message:一度読み込みを中断し、読み込み済みデータを再生します!(List No.%d)\n", i+1);
								nReTry = i;
								i--;	/* 読めなかった分を再チャレンジ */
								ret = MACS_PlayCtrl(0);	/* 再生制御 */
								MACS_MemFree(-1);	/* メモリ解放 */
							}
							else
							{
								printf("message:読み込めませんでした。skipします!(List No.%d)\n", i+1);
								nReTry = i;
							}
						}
						else if(nOut == 2)	/* 読み込みを中断した */
						{
							printf("message:skipします!(List No.%d)                     \n", i+1);
							nReTry = i;
						}
						else /* 読み込めた */
						{
							if(g_nPlayListMode == 1)
							{
								printf("message:再生します!(List No.%d)                   \n", i+1);
								ret = MACS_PlayCtrl(i);	/* 再生制御 */
								if( (g_unMACS_File_List_MAX -1) == i )	/* リストの最後まで読んだとき */
								{
									if(	g_nRepeatOnMem == 0 )	/* オンメモリではない */
									{
										g_nPlayListMode = 2;	/* プレイリスト再生へ昇格 */
										g_nRepeatOnMem = 2;		/* オンメモリでループ実行 */
									}
									else
									{
										/* 何もしない */
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
						nOut = -5;	/* 異常 */
					}
					
				}
				else
				{
					if(nOut > -5)
					{
						/* 何もしない */
					}
					else
					{
						nOut = -5;	/* 異常 */
					}
				}
				
				if( nOut < 0 )	/* 異常 */
				{
					break;
				}
				
			}

			if((g_nPlayListMode == 2)	/* プレイリスト再生 */
			&& (g_nBreak == 0))			/* 強制終了じゃない */
			{
				//printf("message:読み込み済みデータを再生します! Loop=%d\n", nLoop);
				//puts("message:プレイリスト再生します!        ");
				ret = MACS_PlayCtrl(0);	/* 再生制御 */
				//puts("message:プレイリスト再生終了します!    ");
				nOut = 0;
				if(	g_nRepeatOnMem == 0 )	/* オンメモリではない */
				{
					g_nRepeatOnMem = 2;	/* オンメモリでループ実行 */
				}
				else if( g_nRepeatOnMem == 1 )	/* オンメモリではない */
				{
					MACS_MemFree(-1);	/* メモリ解放 */
				}
				else
				{
					/* 何もしない */
				}
			}
		}
		while(nLoop);

		MACS_MemFree(-1);	/* メモリ解放 */

		if(nOut < 0)
		{
			switch(nOut)
			{
			case -1:
				puts("error:再生に失敗しました!");
				break;
			case -2:
				puts("message:強制終了しました。");
				break;
			case -4:
				puts("message:再生を中断しました。");
				break;
			default:
				printf("error:異常終了しました!(%d)\n", nOut);
				break;
			}
		} 
		printf("\n");
			
	}
	
	ADPCM_Stop();	/* 音を止める */

	_dos_kflushio(0xFF);	/* キーバッファをクリア */
	
	return ret;
}
