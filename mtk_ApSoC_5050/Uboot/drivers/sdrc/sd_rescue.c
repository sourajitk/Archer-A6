
/*Sean Wang@Mediatek makes SD card resure Happily*/

#include <common.h>
#include <part.h>
#include "msdc.h"
#include "mmc_core.h"

#define MMC_MAX_STOR_DEV 1
#define ACTIVATED 1
#define N_ACTIVATED 0

static block_dev_desc_t mmc_dev_desc[MMC_MAX_STOR_DEV];
static char mmc_dev_activated[MMC_MAX_STOR_DEV] = { N_ACTIVATED };

extern int mmc_block_read (int dev_num, unsigned long blknr, u32 blkcnt,
			   unsigned long *dst);
extern void msdc_set_pio_bits (struct mmc_host *host, int bits);


struct mmc_init_config
{
  char *desc;			/* description */
  int id;			/* host id to test */
  int autocmd;			/* auto command */
  int mode;			/* PIO/DMA mode */
  int uhsmode;			/* UHS mode */
  int burstsz;			/* DMA burst size */
  int piobits;
  unsigned int flags;		/* DMA flags */
  int count;			/* repeat counts */
  int clksrc;			/* clock source */
  unsigned int clock;		/* clock frequency for testing */
  unsigned int buswidth;	/* bus width */
  unsigned long blknr;		/* n'th block number for read/write test */
  unsigned int total_size;	/* total size to test */
  unsigned int blksz;		/* block size */
  int chunk_blks;		/* blocks of chunk */
  char *buf;			/* read/write buffer */
  unsigned char chk_result;	/* check write result? */
  unsigned char tst_single;	/* test single block read/write? */
  unsigned char tst_multiple;	/* test multiple block read/write? */
  unsigned char tst_interleave;	/* test interleave block read/write ? */
};

static int
__mmc_sys_init (void)
{
//mmc init
#if defined (MT7620_FPGA_BOARD) || defined (MT7620_ASIC_BOARD)
  MSDC_CLR_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x1 << 19);
  MSDC_SET_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x1 << 18);
#elif defined (MT7621_FPGA_BOARD) || defined (MT7621_ASIC_BOARD)
  MSDC_CLR_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x1 << 19);
  MSDC_CLR_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x1 << 18);
#elif defined (MT7628_FPGA_BOARD) || defined (MT7628_ASIC_BOARD)
  MSDC_SET_BIT32 (0xb000003c, 0x1e << 16);	// TODO: maybe omitted when RAether already toggle AGPIO_CFG
  MSDC_CLR_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x3 << 10);
#if defined (EMMC_8BIT)
  MSDC_SET_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x3 << 30);
  MSDC_SET_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x3 << 28);
  MSDC_SET_BIT32 (RALINK_SYSCTL_BASE + 0x60, 0x3 << 26);
#endif
#endif

  return 0;
}

static int
__mmc_cfg_init (int id)
{
  struct mmc_init_config _cfg;
  struct mmc_init_config *cfg;
  struct mmc_host *host;
  struct mmc_card *card;
  unsigned int blksz;
  unsigned int clkhz;
  unsigned int status;

  cfg = &_cfg;
  memset (cfg, 0, sizeof (struct mmc_init_config));

  cfg->id = id;
  cfg->desc = "RESCUE SD";
  cfg->mode = MSDC_MODE_PIO;
  cfg->uhsmode = MMC_SWITCH_MODE_SDR25;
  cfg->burstsz = MSDC_BRUST_64B;
  cfg->flags = 0;
  cfg->count = 1;
  cfg->clksrc = -1;
  cfg->blksz = MMC_BLOCK_SIZE;
  cfg->blknr = 0;
  cfg->total_size = 0;
  cfg->chunk_blks = 0;
  cfg->buf = (char *) 0;
  cfg->chk_result = 1;
  cfg->tst_single = 0;
  cfg->tst_interleave = 0;
  cfg->tst_multiple = 0;
#if defined (MT7620_FPGA_BOARD) || defined (MT7620_ASIC_BOARD) || defined (MT7628_FPGA_BOARD) || defined (MT7628_ASIC_BOARD0) 
  cfg->clock = 48000000;
#else // defined (MT7621_FPGA_BOARD) || defined (MT7621_ASIC_BOARD)
  cfg->clock = 50000000;
#endif
  cfg->buswidth = HOST_BUS_WIDTH_4;
  cfg->piobits = 32;

/*
belowing is good configuration to make SD work
[TST] ==============================================
[TST] BEGIN: 1/1, No Stop(0)
[TST] ----------------------------------------------
[TST] ID: 0
[TST] Mode    : 1
[TST] Clock   : 50000 kHz
[TST] BusWidth: 1 bits
[TST] BurstSz : 64 bytes
[TST] BlkAddr : 40000h
[TST] BlkSize : 512bytes
[TST] TstBlks : 256
[TST] AutoCMD : 12(0), 23(0)
[TST] ----------------------------------------------
*/

  printf ("[SDRESCUE] ==============================================\n");
  printf ("[SDRESCUE] ID   : %d \n", id);
  printf ("[SDRESCUE] Mode    : %d\n", cfg->mode);
  printf ("[SDRESCUE] Clock   : %d kHz\n", cfg->clock / 1000);
  printf ("[SDRESCUE] BusWidth: %d bits\n", cfg->buswidth);
  printf ("[SDRESCUE] BurstSz : %d bytes\n", 0x1 << cfg->burstsz);
  printf ("[SDRESCUE] AutoCMD : 12(%d), 23(%d)\n",
	  (cfg->autocmd & MSDC_AUTOCMD12) ? 1 : 0,
	  (cfg->autocmd & MSDC_AUTOCMD23) ? 1 : 0);
  printf ("[SDRESCUE] ----------------------------------------------\n");

  host = mmc_get_host (id);
  card = mmc_get_card (id);

  if (mmc_init_host (id, host, cfg->clksrc, cfg->mode) != 0)
    {
      printf ("mmc_init_host error\n");
      return -1;
    }

  if (mmc_init_card (host, card) != 0)
    {
      printf ("mmc_init_card error\n");
      printf ("Ensure you have SD scare plugged\n");
      return -1;
    }

  msdc_set_dma (host, (u8) cfg->burstsz, (u32) cfg->flags);
  msdc_set_autocmd (host, cfg->autocmd, 1);

  /* change clock */
  if (cfg->clock)
    {
      clkhz = card->maxhz < cfg->clock ? card->maxhz : cfg->clock;
      mmc_set_clock (host, mmc_card_ddr (card), clkhz);
    }
  if (mmc_card_sd (card) && cfg->buswidth == HOST_BUS_WIDTH_8)
    {
      printf ("[SDRESCUE] SD card doesn't support 8-bit bus width (SKIP)\n");
    }
  if (mmc_set_bus_width (host, card, cfg->buswidth) != 0)
    {
      printf ("[SDRESCUE] mmc_set_bus_width error\n");
    }

  /* cmd16 is illegal while card is in ddr mode */
  if (!(mmc_card_mmc (card) && mmc_card_ddr (card)))
    {
      if (mmc_set_blk_length (host, cfg->blksz) != 0)
	{
	  printf ("[SDRESCUE] mmc_set_blk_length error\n");
	}
    }
  if (cfg->piobits)
    {
      printf ("[TST] PIO bits: %d\n", cfg->piobits);
      msdc_set_pio_bits (host, cfg->piobits);
    }

    mmc_send_status(host, card, &status);

    return 0;
}

//low level init of mmc device
int
__mmc_init (int id)
{
  int ret;

  //unsigned long buf[1024];
  ret = __mmc_sys_init ();
  if (ret)
      return -1;
  ret = __mmc_cfg_init (id);
  if (ret) 
      return -1;
  return 0;
}


block_dev_desc_t *
mmc_rc_init (int id)		//sd rescue function init 
{
  int ret;
  char buffer[512];

  if (id < MMC_MAX_STOR_DEV)
    {
      if (mmc_dev_activated[id] == N_ACTIVATED)
	{
      //do low-level thing   
	  ret = __mmc_init (id);
      if(ret)
          return 0;
      //fill up something needed by block_dev_des_t
	  memset (&mmc_dev_desc[id], 0, sizeof (block_dev_desc_t));
	  mmc_dev_desc[id].target = 0xff;
	  mmc_dev_desc[id].if_type = IF_TYPE_MMC;
	  mmc_dev_desc[id].dev = id;
	  mmc_dev_desc[id].part_type = PART_TYPE_UNKNOWN;
	  mmc_dev_desc[id].block_read = mmc_block_read;	//sad_stor_read;
	  mmc_dev_desc[id].blksz = 512;	//sad_stor_read;
	  mmc_dev_activated[id] = ACTIVATED;

      //choose dos fs
      init_part(mmc_dev_desc);
    
      /*
      printf("mmc self testing");
      mmc_block_read(0, 0, 1 , buffer);
      hexdump(512, buffer);
      printf("mmc self testing 2\n");
      mmc_block_read(0, 1, 1 , buffer);
      hexdump(512, buffer);
      printf("mmc self testing end\n");
      */
	}
      return &mmc_dev_desc[id];
    }
  return 0;
}

block_dev_desc_t *
mmc_get_dev (int id)
{
  return mmc_rc_init (id);
}
