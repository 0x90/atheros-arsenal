/* -*- linux-c -*- */
/*-
 * Copyright (c) 2011 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2011 Joerg Albert    <jal2 *at* gmx.de>
 *
 * This program is free software you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Needed for strtoull, u_int etc */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <endian.h>
#include <byteswap.h>
#include "eeprom.h"

#undef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define dbg(...) \
do { \
	if (verbose) { \
		printf("#DBG %s: ", __func__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} \
 } while (0)

#define err(...) \
do { \
	printf("#ERR %s: ", __func__); \
	printf(__VA_ARGS__); \
	printf("\n"); \
 } while (0)

#define AR5K_PCI_MEM_SIZE 0x10000

#define AR5K_NUM_GPIO	6

#define AR5K_GPIOCR		0x4014	/* Register Address */
#define AR5K_GPIOCR_OUT(n)	(3 << ((n) * 2))	/* Mode 3 for pin n */
#define AR5K_GPIOCR_INT_SEL(n)	((n) << 12)	/* Interrupt for GPIO pin n */

/*
 * GPIO (General Purpose Input/Output) data output register
 */
#define AR5K_GPIODO	0x4018

/*
 * GPIO (General Purpose Input/Output) data input register
 */
#define AR5K_GPIODI	0x401c

struct ath5k_srev_name {
	const char *sr_name;
	u_int8_t sr_val;
};

#define AR5K_SREV_UNKNOWN	0xff

#define AR5K_SREV_AR5210	0x00 /* Crete */
#define AR5K_SREV_AR5311	0x10 /* Maui 1 */
#define AR5K_SREV_AR5311A	0x20 /* Maui 2 */
#define AR5K_SREV_AR5311B	0x30 /* Spirit */
#define AR5K_SREV_AR5211	0x40 /* Oahu */
#define AR5K_SREV_AR5212	0x50 /* Venice */
#define AR5K_SREV_AR5213	0x55 /* ??? */
#define AR5K_SREV_AR5213A	0x59 /* Hainan */
#define AR5K_SREV_AR2413	0x78 /* Griffin lite */
#define AR5K_SREV_AR2414	0x70 /* Griffin */
#define AR5K_SREV_AR5424	0x90 /* Condor */
#define AR5K_SREV_AR5413	0xa4 /* Eagle lite */
#define AR5K_SREV_AR5414	0xa0 /* Eagle */
#define AR5K_SREV_AR2415	0xb0 /* Cobra */
#define AR5K_SREV_AR5416	0xc0 /* PCI-E */
#define AR5K_SREV_AR5418	0xca /* PCI-E */
#define AR5K_SREV_AR2425	0xe0 /* Swan */
#define AR5K_SREV_AR2417	0xf0 /* Nala */

#define AR5K_SREV_RAD_5110	0x00
#define AR5K_SREV_RAD_5111	0x10
#define AR5K_SREV_RAD_5111A	0x15
#define AR5K_SREV_RAD_2111	0x20
#define AR5K_SREV_RAD_5112	0x30
#define AR5K_SREV_RAD_5112A	0x35
#define	AR5K_SREV_RAD_5112B	0x36
#define AR5K_SREV_RAD_2112	0x40
#define AR5K_SREV_RAD_2112A	0x45
#define	AR5K_SREV_RAD_2112B	0x46
#define AR5K_SREV_RAD_2413	0x50
#define AR5K_SREV_RAD_5413	0x60
#define AR5K_SREV_RAD_2316	0x70
#define AR5K_SREV_RAD_2317	0x80
#define AR5K_SREV_RAD_5424	0xa0 /* Mostly same as 5413 */
#define AR5K_SREV_RAD_2425	0xa2
#define AR5K_SREV_RAD_5133	0xc0

#define AR5K_SREV_PHY_5211	0x30
#define AR5K_SREV_PHY_5212	0x41
#define AR5K_SREV_PHY_2112B	0x43
#define AR5K_SREV_PHY_2413	0x45
#define AR5K_SREV_PHY_5413	0x61
#define AR5K_SREV_PHY_2425	0x70

static const struct ath5k_srev_name ath5k_mac_names[] = {
	{ "5210",	AR5K_SREV_AR5210 },
	{ "5311",	AR5K_SREV_AR5311 },
	{ "5311A",	AR5K_SREV_AR5311A },
	{ "5311B",	AR5K_SREV_AR5311B },
	{ "5211",	AR5K_SREV_AR5211 },
	{ "5212",	AR5K_SREV_AR5212 },
	{ "5213",	AR5K_SREV_AR5213 },
	{ "5213A",	AR5K_SREV_AR5213A },
	{ "2413",	AR5K_SREV_AR2413 },
	{ "2414",	AR5K_SREV_AR2414 },
	{ "5424",	AR5K_SREV_AR5424 },
	{ "5413",	AR5K_SREV_AR5413 },
	{ "5414",	AR5K_SREV_AR5414 },
	{ "2415",	AR5K_SREV_AR2415 },
	{ "5416",	AR5K_SREV_AR5416 },
	{ "5418",	AR5K_SREV_AR5418 },
	{ "2425",	AR5K_SREV_AR2425 },
	{ "2417",	AR5K_SREV_AR2417 },
	{ "xxxxx",	AR5K_SREV_UNKNOWN },
};

static const struct ath5k_srev_name ath5k_phy_names[] = {
	{ "5110",	AR5K_SREV_RAD_5110 },
	{ "5111",	AR5K_SREV_RAD_5111 },
	{ "5111A",	AR5K_SREV_RAD_5111A },
	{ "2111",	AR5K_SREV_RAD_2111 },
	{ "5112",	AR5K_SREV_RAD_5112 },
	{ "5112A",	AR5K_SREV_RAD_5112A },
	{ "5112B",	AR5K_SREV_RAD_5112B },
	{ "2112",	AR5K_SREV_RAD_2112 },
	{ "2112A",	AR5K_SREV_RAD_2112A },
	{ "2112B",	AR5K_SREV_RAD_2112B },
	{ "2413",	AR5K_SREV_RAD_2413 },
	{ "5413",	AR5K_SREV_RAD_5413 },
	{ "2316",	AR5K_SREV_RAD_2316 },
	{ "2317",	AR5K_SREV_RAD_2317 },
	{ "5424",	AR5K_SREV_RAD_5424 },
	{ "5133",	AR5K_SREV_RAD_5133 },
	{ "xxxxx",	AR5K_SREV_UNKNOWN },
};

/*
 * Silicon revision register
 */
#define AR5K_SREV		0x4020	/* Register Address */
#define AR5K_SREV_VER		0x000000f0	/* Mask for version */
#define AR5K_SREV_REV		0x000000ff	/* Mask for revision */

/*
 * PHY chip revision register
 */
#define AR5K_PHY_CHIP_ID		0x9818

/*
 * PHY register
 */
#define AR5K_PHY_BASE			0x9800
#define AR5K_PHY(_n)			(AR5K_PHY_BASE + ((_n) << 2))
#define AR5K_PHY_SHIFT_2GHZ		0x00004007
#define AR5K_PHY_SHIFT_5GHZ		0x00000007

#define AR5K_RESET_CTL		0x4000	/* Register Address */
#define AR5K_RESET_CTL_PCU	0x00000001	/* Protocol Control Unit reset */
#define AR5K_RESET_CTL_DMA	0x00000002	/* DMA (Rx/Tx) reset -5210 only */
#define AR5K_RESET_CTL_BASEBAND	0x00000002	/* Baseband reset (5211/5212) */
#define AR5K_RESET_CTL_MAC	0x00000004	/* MAC reset (PCU+Baseband?) -5210 only */
#define AR5K_RESET_CTL_PHY	0x00000008	/* PHY reset -5210 only */
#define AR5K_RESET_CTL_PCI	0x00000010	/* PCI Core reset (interrupts etc) */
#define AR5K_RESET_CTL_CHIP	(AR5K_RESET_CTL_PCU | AR5K_RESET_CTL_DMA |	\
				AR5K_RESET_CTL_MAC | AR5K_RESET_CTL_PHY)

/*
 * Sleep control register
 */
#define AR5K_SLEEP_CTL			0x4004	/* Register Address */
#define AR5K_SLEEP_CTL_SLDUR		0x0000ffff	/* Sleep duration mask */
#define AR5K_SLEEP_CTL_SLDUR_S		0
#define AR5K_SLEEP_CTL_SLE		0x00030000	/* Sleep enable mask */
#define AR5K_SLEEP_CTL_SLE_S		16
#define AR5K_SLEEP_CTL_SLE_WAKE		0x00000000	/* Force chip awake */
#define AR5K_SLEEP_CTL_SLE_SLP		0x00010000	/* Force chip sleep */
#define AR5K_SLEEP_CTL_SLE_ALLOW	0x00020000
#define AR5K_SLEEP_CTL_SLE_UNITS	0x00000008	/* not on 5210 */

#define AR5K_PCICFG			0x4010	/* Register Address */
#define AR5K_PCICFG_EEAE		0x00000001	/* EEPROM access enable [5210] */
#define AR5K_PCICFG_CLKRUNEN		0x00000004	/* CLKRUN enable [5211+] */
#define AR5K_PCICFG_EESIZE		0x00000018	/* Mask for EEPROM size [5211+] */
#define AR5K_PCICFG_EESIZE_S		3
#define AR5K_PCICFG_EESIZE_4K		0	/* 4K */
#define AR5K_PCICFG_EESIZE_8K		1	/* 8K */
#define AR5K_PCICFG_EESIZE_16K		2	/* 16K */
#define AR5K_PCICFG_EESIZE_FAIL		3	/* Failed to get size (?) [5211+] */

#define AR5K_PCICFG_SPWR_DN		0x00010000	/* Mask for power status (5210) */

#define AR5K_EEPROM_BASE	0x6000

/*
 * EEPROM data register
 */
#define AR5K_EEPROM_DATA_5211	0x6004
#define AR5K_EEPROM_DATA_5210	0x6800
#define AR5K_EEPROM_DATA	(eeprom_access == AR5K_EEPROM_ACCESS_5210 ? \
				AR5K_EEPROM_DATA_5210 : AR5K_EEPROM_DATA_5211)

/*
 * EEPROM command register
 */
#define AR5K_EEPROM_CMD		0x6008			/* Register Address */
#define AR5K_EEPROM_CMD_READ	0x00000001	/* EEPROM read */
#define AR5K_EEPROM_CMD_WRITE	0x00000002	/* EEPROM write */
#define AR5K_EEPROM_CMD_RESET	0x00000004	/* EEPROM reset */

/*
 * EEPROM status register
 */
#define AR5K_EEPROM_STAT_5210	0x6c00			/* Register Address [5210] */
#define AR5K_EEPROM_STAT_5211	0x600c			/* Register Address [5211+] */
#define AR5K_EEPROM_STATUS	(eeprom_access == AR5K_EEPROM_ACCESS_5210 ? \
				AR5K_EEPROM_STAT_5210 : AR5K_EEPROM_STAT_5211)
#define AR5K_EEPROM_STAT_RDERR	0x00000001	/* EEPROM read failed */
#define AR5K_EEPROM_STAT_RDDONE	0x00000002	/* EEPROM read successful */
#define AR5K_EEPROM_STAT_WRERR	0x00000004	/* EEPROM write failed */
#define AR5K_EEPROM_STAT_WRDONE	0x00000008	/* EEPROM write successful */

/*
 * EEPROM config register (?)
 */
#define AR5K_EEPROM_CFG	0x6010

/*
 * Read data by masking
 */
#define AR5K_REG_MS(_val, _flags)	\
	(((_val) & (_flags)) >> _flags##_S)

/*
 * Access device registers
 */
#if __BYTE_ORDER == __BIG_ENDIAN
#define AR5K_REG_READ(_reg)		\
	__bswap_32(*((volatile u_int32_t *)(mem + (_reg))))
#define AR5K_REG_WRITE(_reg, _val)	\
	(*((volatile u_int32_t *)(mem + (_reg))) = __bswap_32(_val))
#else
#define AR5K_REG_READ(_reg)		\
	(*((volatile u_int32_t *)(mem + (_reg))))
#define AR5K_REG_WRITE(_reg, _val)	\
	(*((volatile u_int32_t *)(mem + (_reg))) = (_val))
#endif

#define AR5K_REG_ENABLE_BITS(_reg, _flags)	\
	AR5K_REG_WRITE(_reg, AR5K_REG_READ(_reg) | (_flags))

#define AR5K_REG_DISABLE_BITS(_reg, _flags)	\
	AR5K_REG_WRITE(_reg, AR5K_REG_READ(_reg) & ~(_flags))

#define AR5K_TUNE_REGISTER_TIMEOUT		20000

#define AR5K_EEPROM_READ(_o, _v) do {					\
	if ((ret = ath5k_hw_eeprom_read((_o), &(_v))) != 0)	\
		return (ret);						\
} while (0)

/* Names for EEPROM fields */
struct eeprom_entry {
	const char *name;
	int addr;
};

static const struct eeprom_entry eeprom_addr[] = {
	{"pci_dev_id", 0},
	{"pci_vendor_id", 1},
	{"pci_class", 2},
	{"pci_rev_id", 3},
	{"pci_subsys_dev_id", 7},
	{"pci_subsys_vendor_id", 8},
	{"regdomain", AR5K_EEPROM_REG_DOMAIN},
};

/* Command line settings */
static int force_write = 0;
static int verbose = 0;

/* Global device characteristics */
static enum {
	AR5K_EEPROM_ACCESS_5210,
	AR5K_EEPROM_ACCESS_5211,
	AR5K_EEPROM_ACCESS_5416
} eeprom_access;
static unsigned int eeprom_size;
static int mac_revision;
static void *mem;

/* forward decl. */
static void usage(const char *n);

static u_int32_t ath5k_hw_bitswap(u_int32_t val, u_int bits)
{
	u_int32_t retval = 0, bit, i;

	for (i = 0; i < bits; i++) {
		bit = (val >> i) & 1;
		retval = (retval << 1) | bit;
	}

	return (retval);
}

/*
 * Get the PHY Chip revision
 */
static u_int16_t ath5k_hw_radio_revision(u_int8_t chip)
{
	int i;
	u_int32_t srev;
	u_int16_t ret;

	/*
	 * Set the radio chip access register
	 */
	switch (chip) {
	case 0:
		AR5K_REG_WRITE(AR5K_PHY(0), AR5K_PHY_SHIFT_2GHZ);
		break;
	case 1:
		AR5K_REG_WRITE(AR5K_PHY(0), AR5K_PHY_SHIFT_5GHZ);
		break;
	default:
		return (0);
	}

	usleep(2000);

	/* ...wait until PHY is ready and read the selected radio revision */
	AR5K_REG_WRITE(AR5K_PHY(0x34), 0x00001c16);

	for (i = 0; i < 8; i++)
		AR5K_REG_WRITE(AR5K_PHY(0x20), 0x00010000);

	if (mac_revision == AR5K_SREV_AR5210) {
		srev = AR5K_REG_READ(AR5K_PHY(256) >> 28) & 0xf;

		ret = (u_int16_t)ath5k_hw_bitswap(srev, 4) + 1;
	} else {
		srev = (AR5K_REG_READ(AR5K_PHY(0x100)) >> 24) & 0xff;

		ret = (u_int16_t)ath5k_hw_bitswap(((srev & 0xf0) >> 4) |
						  ((srev & 0x0f) << 4), 8);
	}

	/* Reset to the 5GHz mode */
	AR5K_REG_WRITE(AR5K_PHY(0), AR5K_PHY_SHIFT_5GHZ);

	return (ret);
}

/*
 * Read from EEPROM
 */
static int ath5k_hw_eeprom_read(u_int32_t offset, u_int16_t *data)
{
	u_int32_t status, timeout;

	/*
	 * Initialize EEPROM access
	 */
	if (eeprom_access == AR5K_EEPROM_ACCESS_5210) {
		AR5K_REG_ENABLE_BITS(AR5K_PCICFG, AR5K_PCICFG_EEAE);
		(void)AR5K_REG_READ(AR5K_EEPROM_BASE + (4 * offset));
	} else {
		AR5K_REG_WRITE(AR5K_EEPROM_BASE, offset);
		AR5K_REG_ENABLE_BITS(AR5K_EEPROM_CMD, AR5K_EEPROM_CMD_READ);
	}

	for (timeout = AR5K_TUNE_REGISTER_TIMEOUT; timeout > 0; timeout--) {
		status = AR5K_REG_READ(AR5K_EEPROM_STATUS);
		if (status & AR5K_EEPROM_STAT_RDDONE) {
			if (status & AR5K_EEPROM_STAT_RDERR)
				return 1;
			*data = (u_int16_t)
			    (AR5K_REG_READ(AR5K_EEPROM_DATA) & 0xffff);
			return (0);
		}
		usleep(15);
	}

	return 1;
}

/*
 * Write to EEPROM
 */
static int ath5k_hw_eeprom_write(u_int32_t offset, u_int16_t data)
{
	u_int32_t status, timeout;
	u_int16_t read_data;

	/*
	 * Initialize EEPROM access
	 */

	if (eeprom_access == AR5K_EEPROM_ACCESS_5210) {

		AR5K_REG_ENABLE_BITS(AR5K_PCICFG, AR5K_PCICFG_EEAE);

		/* data to write */
		(void)AR5K_REG_WRITE(AR5K_EEPROM_BASE + (4 * offset), data);

	} else {
		/* not 5210 */
		/* reset EEPROM access */
		AR5K_REG_WRITE(AR5K_EEPROM_CMD, AR5K_EEPROM_CMD_RESET);
		usleep(5);

		AR5K_REG_WRITE(AR5K_EEPROM_DATA, data);

		/* set offset in EEPROM to write to */
		AR5K_REG_WRITE(AR5K_EEPROM_BASE, offset);
		usleep(5);

		/* issue write command */
		AR5K_REG_WRITE(AR5K_EEPROM_CMD, AR5K_EEPROM_CMD_WRITE);
	}

	for (timeout = AR5K_TUNE_REGISTER_TIMEOUT; timeout > 0; timeout--) {
		status = AR5K_REG_READ(AR5K_EEPROM_STATUS);
		if (status & AR5K_EEPROM_STAT_WRDONE) {
			if (status & AR5K_EEPROM_STAT_WRERR) {
				err("EEPROM write access to 0x%04x failed",
				    offset);
				return 1;
			}
			ath5k_hw_eeprom_read( offset, &read_data);
			if (read_data != data) {
				err("data doesn't match, write failed at 0x%04x\n",
				    offset);
				return 1;
			}
			return 0;
		}
		usleep(15);
	}

	return 1;
}

/*
 * Translate binary channel representation in EEPROM to frequency
 */
static u_int16_t ath5k_eeprom_bin2freq(struct ath5k_eeprom_info *ee,
				       u_int16_t bin, unsigned int mode)
{
	u_int16_t val;

	if (bin == AR5K_EEPROM_CHANNEL_DIS)
		return bin;

	if (mode == AR5K_EEPROM_MODE_11A) {
		if (ee->ee_version > AR5K_EEPROM_VERSION_3_2)
			val = (5 * bin) + 4800;
		else
			val = bin > 62 ? (10 * 62) + (5 * (bin - 62)) + 5100 :
			    (bin * 10) + 5100;
	} else {
		if (ee->ee_version > AR5K_EEPROM_VERSION_3_2)
			val = bin + 2300;
		else
			val = bin + 2400;
	}

	return val;
}

/*
 * Read antenna info from EEPROM
 */
static int ath5k_eeprom_read_ants(struct ath5k_eeprom_info *ee,
				  u_int32_t *offset, unsigned int mode)
{
	u_int32_t o = *offset;
	u_int16_t val;
	int ret, i = 0;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_switch_settling[mode]	= (val >> 8) & 0x7f;
	ee->ee_atn_tx_rx[mode]		= (val >> 2) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 4) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 12) & 0xf;
	ee->ee_ant_control[mode][i++]	= (val >> 6) & 0x3f;
	ee->ee_ant_control[mode][i++]	= val & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	= (val >> 10) & 0x3f;
	ee->ee_ant_control[mode][i++]	= (val >> 4) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 2) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 14) & 0x3;
	ee->ee_ant_control[mode][i++]	= (val >> 8) & 0x3f;
	ee->ee_ant_control[mode][i++]	= (val >> 2) & 0x3f;
	ee->ee_ant_control[mode][i]	= (val << 4) & 0x3f;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_ant_control[mode][i++]	|= (val >> 12) & 0xf;
	ee->ee_ant_control[mode][i++]	= (val >> 6) & 0x3f;
	ee->ee_ant_control[mode][i++]	= val & 0x3f;

	/* Get antenna modes */
	ee->ee_antenna[mode][0] =
	    (ee->ee_ant_control[mode][0] << 4) | 0x1;
	ee->ee_antenna[mode][AR5K_ANT_FIXED_A] =
	     ee->ee_ant_control[mode][1]	|
	    (ee->ee_ant_control[mode][2] << 6)	|
	    (ee->ee_ant_control[mode][3] << 12) |
	    (ee->ee_ant_control[mode][4] << 18) |
	    (ee->ee_ant_control[mode][5] << 24);
	ee->ee_antenna[mode][AR5K_ANT_FIXED_B] =
	     ee->ee_ant_control[mode][6]	|
	    (ee->ee_ant_control[mode][7] << 6)	|
	    (ee->ee_ant_control[mode][8] << 12) |
	    (ee->ee_ant_control[mode][9] << 18) |
	    (ee->ee_ant_control[mode][10] << 24);

	/* return new offset */
	*offset = o;

	return 0;
}

/*
 * Read supported modes from EEPROM
 */
static int ath5k_eeprom_read_modes(struct ath5k_eeprom_info *ee,
				   u_int32_t *offset, unsigned int mode)
{
	u_int32_t o = *offset;
	u_int16_t val;
	int ret;

	switch (mode){
	case AR5K_EEPROM_MODE_11A:
		AR5K_EEPROM_READ(o++, val);
		ee->ee_adc_desired_size[mode]	= (int8_t)((val >> 8) & 0xff);
		ee->ee_ob[mode][3]		= (val >> 5) & 0x7;
		ee->ee_db[mode][3]		= (val >> 2) & 0x7;
		ee->ee_ob[mode][2]		= (val << 1) & 0x7;

		AR5K_EEPROM_READ(o++, val);
		ee->ee_ob[mode][2]		|= (val >> 15) & 0x1;
		ee->ee_db[mode][2]		= (val >> 12) & 0x7;
		ee->ee_ob[mode][1]		= (val >> 9) & 0x7;
		ee->ee_db[mode][1]		= (val >> 6) & 0x7;
		ee->ee_ob[mode][0]		= (val >> 3) & 0x7;
		ee->ee_db[mode][0]		= val & 0x7;
		break;
	case AR5K_EEPROM_MODE_11B:
		AR5K_EEPROM_READ(o++, val);
		ee->ee_adc_desired_size[mode]	= (int8_t)((val >> 8) & 0xff);
		ee->ee_ob[mode][1]		= (val >> 4) & 0x7;
		ee->ee_db[mode][1]		= val & 0x7;
		break;
	case AR5K_EEPROM_MODE_11G:
		AR5K_EEPROM_READ(o++, val);
		ee->ee_adc_desired_size[mode]	= (signed short int)((val >> 8) & 0xff);
		ee->ee_ob[mode][1]		= (val >> 4) & 0x7;
		ee->ee_db[mode][1]		= val & 0x7;
		break;
	}

	AR5K_EEPROM_READ(o++, val);
	ee->ee_tx_end2xlna_enable[mode]	= (val >> 8) & 0xff;
	ee->ee_thr_62[mode]		= val & 0xff;

	if (ee->ee_version <= AR5K_EEPROM_VERSION_3_2)
		ee->ee_thr_62[mode] = mode == AR5K_EEPROM_MODE_11A ? 15 : 28;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_tx_end2xpa_disable[mode]	= (val >> 8) & 0xff;
	ee->ee_tx_frm2xpa_enable[mode]	= val & 0xff;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_pga_desired_size[mode]	= (val >> 8) & 0xff;

	if ((val & 0xff) & 0x80)
		ee->ee_noise_floor_thr[mode] = -((((val & 0xff) ^ 0xff)) + 1);
	else
		ee->ee_noise_floor_thr[mode] = val & 0xff;

	if (ee->ee_version <= AR5K_EEPROM_VERSION_3_2)
		ee->ee_noise_floor_thr[mode] =
		    mode == AR5K_EEPROM_MODE_11A ? -54 : -1;

	AR5K_EEPROM_READ(o++, val);
	ee->ee_xlna_gain[mode]		= (val >> 5) & 0xff;
	ee->ee_x_gain[mode]		= (val >> 1) & 0xf;
	ee->ee_xpd[mode]		= val & 0x1;

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0)
		ee->ee_fixed_bias[mode] = (val >> 13) & 0x1;

	if (ee->ee_version >= AR5K_EEPROM_VERSION_3_3) {
		AR5K_EEPROM_READ(o++, val);
		ee->ee_false_detect[mode] = (val >> 6) & 0x7f;

		if (mode == AR5K_EEPROM_MODE_11A)
			ee->ee_xr_power[mode] = val & 0x3f;
		else {
			ee->ee_ob[mode][0] = val & 0x7;
			ee->ee_db[mode][0] = (val >> 3) & 0x7;
		}
	}

	if (ee->ee_version < AR5K_EEPROM_VERSION_3_4) {
		ee->ee_i_gain[mode] = AR5K_EEPROM_I_GAIN;
		ee->ee_cck_ofdm_power_delta = AR5K_EEPROM_CCK_OFDM_DELTA;
	} else {
		ee->ee_i_gain[mode] = (val >> 13) & 0x7;

		AR5K_EEPROM_READ(o++, val);
		ee->ee_i_gain[mode] |= (val << 3) & 0x38;

		if (mode == AR5K_EEPROM_MODE_11G) {
			ee->ee_cck_ofdm_power_delta = (val >> 3) & 0xff;

			if (ee->ee_version >= AR5K_EEPROM_VERSION_4_6)
				ee->ee_scaled_cck_delta = (val >> 11) & 0x1f;
		}
	}

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 &&
	    mode == AR5K_EEPROM_MODE_11A) {
		ee->ee_i_cal[mode] = (val >> 8) & 0x3f;
		ee->ee_q_cal[mode] = (val >> 3) & 0x1f;
	}

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0) {
		switch (mode) {
		case AR5K_EEPROM_MODE_11B:
			AR5K_EEPROM_READ(o++, val);

			ee->ee_cal_piers_b = 0;

			ee->ee_pwr_cal_b[0].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff, mode);
			if (ee->ee_pwr_cal_b[0].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_b++;

			ee->ee_pwr_cal_b[1].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff, mode);
			if (ee->ee_pwr_cal_b[1].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_b++;

			AR5K_EEPROM_READ(o++, val);
			ee->ee_pwr_cal_b[2].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff, mode);
			if (ee->ee_pwr_cal_b[2].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_b++;
			break;
		case AR5K_EEPROM_MODE_11G:
			AR5K_EEPROM_READ(o++, val);

			ee->ee_cal_piers_g = 0;

			ee->ee_pwr_cal_g[0].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff, mode);
			if (ee->ee_pwr_cal_g[0].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_g++;

			ee->ee_pwr_cal_g[1].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff, mode);
			if (ee->ee_pwr_cal_g[1].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_g++;

			AR5K_EEPROM_READ(o++, val);
			ee->ee_turbo_max_power[mode] = val & 0x7f;
			ee->ee_xr_power[mode] = (val >> 7) & 0x3f;

			AR5K_EEPROM_READ(o++, val);
			ee->ee_pwr_cal_g[2].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff, mode);
			if (ee->ee_pwr_cal_g[2].freq != AR5K_EEPROM_CHANNEL_DIS)
				ee->ee_cal_piers_g++;
			break;
		}
	}

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_1) {
		switch (mode) {
		case AR5K_EEPROM_MODE_11A:
			AR5K_EEPROM_READ(o++, val);
			ee->ee_margin_tx_rx[mode] = val & 0x3f;
			break;
		case AR5K_EEPROM_MODE_11B:
		case AR5K_EEPROM_MODE_11G:
			ee->ee_margin_tx_rx[mode] = (val >> 8) & 0x3f;
			break;
		}
	}

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 &&
	    mode == AR5K_EEPROM_MODE_11G) {
		AR5K_EEPROM_READ(o++, val);
		ee->ee_i_cal[mode] = (val >> 8) & 0x3f;
		ee->ee_q_cal[mode] = (val >> 3) & 0x1f;

		if (ee->ee_version >= AR5K_EEPROM_VERSION_4_2) {
			AR5K_EEPROM_READ(o++, val);
			ee->ee_cck_ofdm_gain_delta = val & 0xff;
		}
	}

	/*
	 * Read turbo mode information on newer EEPROM versions
	 */
	if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 &&
	    mode == AR5K_EEPROM_MODE_11A) {
		ee->ee_switch_settling_turbo[mode] = (val >> 6) & 0x7f;

		ee->ee_atn_tx_rx_turbo[mode] = (val >> 13) & 0x7;
		AR5K_EEPROM_READ(o++, val);
		ee->ee_atn_tx_rx_turbo[mode] |= (val & 0x7) << 3;
		ee->ee_margin_tx_rx_turbo[mode] = (val >> 3) & 0x3f;

		ee->ee_adc_desired_size_turbo[mode] = (val >> 9) & 0x7f;
		AR5K_EEPROM_READ(o++, val);
		ee->ee_adc_desired_size_turbo[mode] |= (val & 0x1) << 7;
		ee->ee_pga_desired_size_turbo[mode] = (val >> 1) & 0xff;

		if (AR5K_EEPROM_EEMAP(ee->ee_misc0) >=2)
			ee->ee_pd_gain_overlap = (val >> 9) & 0xf;
	}

	if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 &&
	    mode == AR5K_EEPROM_MODE_11G) {
		ee->ee_switch_settling_turbo[mode] = (val >> 8) & 0x7f;

		ee->ee_atn_tx_rx_turbo[mode] = (val >> 15) & 0x7;
		AR5K_EEPROM_READ(o++, val);
		ee->ee_atn_tx_rx_turbo[mode] |= (val & 0x1f) << 1;
		ee->ee_margin_tx_rx_turbo[mode] = (val >> 5) & 0x3f;

		ee->ee_adc_desired_size_turbo[mode] = (val >> 11) & 0x7f;
		AR5K_EEPROM_READ(o++, val);
		ee->ee_adc_desired_size_turbo[mode] |= (val & 0x7) << 5;
		ee->ee_pga_desired_size_turbo[mode] = (val >> 3) & 0xff;
	}

	/* return new offset */
	*offset = o;

	return 0;
}

/*
 * Read per channel calibration info from EEPROM
 *
 * This info is used to calibrate the baseband power table. Imagine
 * that for each channel there is a power curve that's hw specific
 * (depends on amplifier) and we try to "correct" this curve using offests
 * we pass on to phy chip (baseband -> before amplifier) so that it can
 * use accurate power values when setting tx power (takes amplifier's
 * performance on each channel into account).
 *
 * EEPROM provides us with the offsets for some pre-calibrated channels
 * and we have to scale (to create the full table for these channels) and
 * interpolate (in order to create the table for any channel).
 */

static int ath5k_eeprom_read_rf5111_pcal_info(struct ath5k_eeprom_info *ee,
							unsigned int mode)
{
	u_int32_t offset;
	unsigned int i, c;
	int ret;
	u_int16_t val;
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf5111 *chan_pcal_info;
	u_int16_t cal_piers;
	/* Fixed percentage intercepts */
	static const u_int8_t intercepts_3[] =
		{ 0, 5, 10, 20, 30, 50, 70, 85, 90, 95, 100 };
	static const u_int8_t intercepts_3_2[] =
		{ 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
	const u_int8_t *intercepts =
		ee->ee_version < AR5K_EEPROM_VERSION_3_2 ?
			intercepts_3 : intercepts_3_2;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		/*
		 * Read 5GHz EEPROM channels
		 */
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version);
		gen_chan_info = ee->ee_pwr_cal_a;
		ee->ee_cal_piers_a = 0;
		/* Different frequency mask for < 3.2 */
		if (ee->ee_version <= AR5K_EEPROM_VERSION_3_2) {
			AR5K_EEPROM_READ(offset++, val);
			gen_chan_info[0].freq =
				ath5k_eeprom_bin2freq(ee, val >> 9 & 0x7f,
						AR5K_EEPROM_MODE_11A);
			gen_chan_info[1].freq =
				ath5k_eeprom_bin2freq(ee, val >> 2 & 0x7f,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[2].freq = val << 5 & 0x7f;
			AR5K_EEPROM_READ(offset++, val);
			gen_chan_info[2].freq |= val >> 11 & 0x1f;
			gen_chan_info[2].freq =
			ath5k_eeprom_bin2freq(ee, gen_chan_info[2].freq,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[3].freq =
				ath5k_eeprom_bin2freq(ee, val >> 4 & 0x7f,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[4].freq = val << 3 & 0x7f;
			AR5K_EEPROM_READ(offset++, val);
			gen_chan_info[4].freq |= val >> 13 & 0x7;
			gen_chan_info[4].freq =
				ath5k_eeprom_bin2freq(ee, gen_chan_info[4].freq,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[5].freq =
				ath5k_eeprom_bin2freq(ee, val >> 6 & 0x7f,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[6].freq = val << 1 & 0x7f;
			AR5K_EEPROM_READ(offset++, val);
			gen_chan_info[6].freq |= val >> 15 & 0x1;
			gen_chan_info[6].freq =
				ath5k_eeprom_bin2freq(ee, gen_chan_info[6].freq,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[7].freq =
				ath5k_eeprom_bin2freq(ee, val >> 8 & 0x7f,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[8].freq =
				ath5k_eeprom_bin2freq(ee, val >> 1 & 0x7f,
						AR5K_EEPROM_MODE_11A);

			gen_chan_info[9].freq = val << 6 & 0x7f;
			AR5K_EEPROM_READ(offset++, val);
			gen_chan_info[9].freq |= val >> 10 & 0x3f;
			gen_chan_info[9].freq =
				ath5k_eeprom_bin2freq(ee, gen_chan_info[9].freq,
						AR5K_EEPROM_MODE_11A);

			ee->ee_cal_piers_a = 10;
		} else {
			for (i = 0; i < AR5K_EEPROM_N_5GHZ_CHAN; i++) {
				AR5K_EEPROM_READ(offset++, val);

				if ((val & 0xff) == 0)
					break;

				ee->ee_pwr_cal_a[i].freq =
					ath5k_eeprom_bin2freq(ee, val & 0xff,
							AR5K_EEPROM_MODE_11A);
				ee->ee_cal_piers_a++;

				if (((val >> 8) & 0xff) == 0)
					break;

				ee->ee_pwr_cal_a[++i].freq =
					ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff,
								AR5K_EEPROM_MODE_11A);
				ee->ee_cal_piers_a++;

			}
		}
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP2_OFFSET;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP3_OFFSET;
		gen_chan_info = ee->ee_pwr_cal_b;
		/* Fixed cal piers */
		gen_chan_info[0].freq = 2412;
		gen_chan_info[1].freq = 2447;
		gen_chan_info[2].freq = 2484;
		ee->ee_cal_piers_b = 3;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP4_OFFSET;
		gen_chan_info = ee->ee_pwr_cal_g;
		/* Fixed cal piers */
		gen_chan_info[0].freq = 2312;
		gen_chan_info[1].freq = 2412;
		gen_chan_info[2].freq = 2484;
		ee->ee_cal_piers_b = 3;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < cal_piers; i++) {

		gen_chan_info[i].rf5111_info =
				malloc(sizeof(struct ath5k_chan_pcal_info_rf5111));

		chan_pcal_info = gen_chan_info[i].rf5111_info;

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pcdac_max     = (u_int16_t)((val >> 10) & 0x3f);
		chan_pcal_info->pcdac_min    = (u_int16_t)((val >> 4) & 0x3f);
		chan_pcal_info->pwr[0] = (u_int16_t)((val << 2) & 0x3f);

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr[0] |= (u_int16_t)((val >> 14) & 0x3);
		chan_pcal_info->pwr[1] = (u_int16_t)((val >> 8) & 0x3f);
		chan_pcal_info->pwr[2] = (u_int16_t)((val >> 2) & 0x3f);
		chan_pcal_info->pwr[3] = (u_int16_t)((val << 4) & 0x3f);

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr[3] |= (u_int16_t)((val >> 12) & 0xf);
		chan_pcal_info->pwr[4] = (u_int16_t)((val >> 6) & 0x3f);
		chan_pcal_info->pwr[5] = (u_int16_t)(val  & 0x3f);

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr[6] = (u_int16_t)((val >> 10) & 0x3f);
		chan_pcal_info->pwr[7] = (u_int16_t)((val >> 4) & 0x3f);
		chan_pcal_info->pwr[8] = (u_int16_t)((val << 2) & 0x3f);

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr[8] |= (u_int16_t)((val >> 14) & 0x3);
		chan_pcal_info->pwr[9] = (u_int16_t)((val >> 8) & 0x3f);
		chan_pcal_info->pwr[10] = (u_int16_t)((val >> 2) & 0x3f);

		/* Recreate pcdac offsets table for this channel
		 * using intercepts table and PCDAC min/max */
		for (c = 0; c < AR5K_EEPROM_N_PWR_POINTS_5111; c++ )
			chan_pcal_info->pcdac[c] =
				(intercepts[c] * chan_pcal_info->pcdac_max +
				(100 - intercepts[c]) * chan_pcal_info->pcdac_min) / 100;
	}

	return 0;

}

static int ath5k_eeprom_read_rf5112_pcal_info(struct ath5k_eeprom_info *ee,
							unsigned int mode)
{
	u_int32_t offset;
	unsigned int i, c;
	int ret;
	u_int16_t val;
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf5112 *chan_pcal_info;
	u_int16_t cal_piers;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		/*
		 * Read 5GHz EEPROM channels
		 */
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version);
		ee->ee_cal_piers_a = 0;
		for (i = 0; i < AR5K_EEPROM_N_5GHZ_CHAN; i++) {
			AR5K_EEPROM_READ(offset++, val);

			if ((val & 0xff) == 0)
				break;

			ee->ee_pwr_cal_a[i].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff,
						AR5K_EEPROM_MODE_11A);
			ee->ee_cal_piers_a++;

			if (((val >> 8) & 0xff) == 0)
				break;

			ee->ee_pwr_cal_a[++i].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff,
							AR5K_EEPROM_MODE_11A);
			ee->ee_cal_piers_a++;

		}
		offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP2_OFFSET;
		gen_chan_info = ee->ee_pwr_cal_a;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:

		if (AR5K_EEPROM_HDR_11A(ee->ee_header))
			offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
							AR5K_EEPROM_GROUP3_OFFSET;
		else
			offset = AR5K_EEPROM_GROUPS_START(ee->ee_version);

		gen_chan_info = ee->ee_pwr_cal_b;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:

		if (AR5K_EEPROM_HDR_11A(ee->ee_header))	{
			offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP4_OFFSET;
		} else if (AR5K_EEPROM_HDR_11B(ee->ee_header)) {
			offset = AR5K_EEPROM_GROUPS_START(ee->ee_version) +
						AR5K_EEPROM_GROUP2_OFFSET;
		} else
			offset = AR5K_EEPROM_GROUPS_START(ee->ee_version);

		gen_chan_info = ee->ee_pwr_cal_g;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < cal_piers; i++) {

		gen_chan_info[i].rf5112_info =
				malloc(sizeof(struct ath5k_chan_pcal_info_rf5112));

		chan_pcal_info = gen_chan_info[i].rf5112_info;

		/* Power values in dBm * 4
		 * for the lower xpd gain curve
		 * (0 dBm -> higher output power) */
		for (c = 0; c < AR5K_EEPROM_N_XPD0_POINTS; c++) {
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr_x0[c] = (val & 0xff);
			chan_pcal_info->pwr_x0[++c] = ((val >> 8) & 0xff);
		}

		/* PCDAC steps
		 * corresponding to the above power
		 * measurements */
		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pcdac_x0[1] = (val & 0x1f);
		chan_pcal_info->pcdac_x0[2] = ((val >> 5) & 0x1f);
		chan_pcal_info->pcdac_x0[3] = ((val >> 10) & 0x1f);

		/* Power values in dBm * 4
		 * for the higher xpd gain curve
		 * (18 dBm -> lower output power) */
		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr_x3[0] = (val & 0xff);
		chan_pcal_info->pwr_x3[1] = ((val >> 8) & 0xff);

		AR5K_EEPROM_READ(offset++, val);
		chan_pcal_info->pwr_x3[2] = (val & 0xff);

		/* PCDAC steps
		 * corresponding to the above power
		 * measurements (static) */
		chan_pcal_info->pcdac_x3[0] = 20;
		chan_pcal_info->pcdac_x3[1] = 35;
		chan_pcal_info->pcdac_x3[2] = 63;

		if (ee->ee_version >= AR5K_EEPROM_VERSION_4_3) {
			chan_pcal_info->pcdac_x0[0] = ((val >> 8) & 0xff);

			/* Last xpd0 power level is also channel maximum */
			gen_chan_info[i].max_pwr = chan_pcal_info->pwr_x0[3];
		} else {
			chan_pcal_info->pcdac_x0[0] = 1;
			gen_chan_info[i].max_pwr = ((val >> 8) & 0xff);
		}

		/* Recreate pcdac_x0 table for this channel using pcdac steps */
		chan_pcal_info->pcdac_x0[1] += chan_pcal_info->pcdac_x0[0];
		chan_pcal_info->pcdac_x0[2] += chan_pcal_info->pcdac_x0[1];
		chan_pcal_info->pcdac_x0[3] += chan_pcal_info->pcdac_x0[2];
	}

	return 0;
}

static int ath5k_eeprom_read_rf2413_pcal_info(struct ath5k_eeprom_info *ee,
							unsigned int mode)
{
	u_int32_t offset, start_offset;
	unsigned int i, c;
	int ret;
	u_int16_t val;
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf2413 *chan_pcal_info;
	u_int16_t cal_piers;
	u_int8_t pd_gains = 0;

	if (ee->ee_x_gain[mode] & 0x1) pd_gains++;
	if ((ee->ee_x_gain[mode] >> 1) & 0x1) pd_gains++;
	if ((ee->ee_x_gain[mode] >> 2) & 0x1) pd_gains++;
	if ((ee->ee_x_gain[mode] >> 3) & 0x1) pd_gains++;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		start_offset = AR5K_EEPROM_CAL_DATA_START(ee->ee_misc4);
		offset = start_offset;
		ee->ee_cal_piers_a = 0;

		if (!AR5K_EEPROM_HDR_11A(ee->ee_header))
			return 0;

		for (i = 0; i < AR5K_EEPROM_N_5GHZ_CHAN; i++) {
			AR5K_EEPROM_READ(offset++, val);

			if ((val & 0xff) == 0)
				break;

			ee->ee_pwr_cal_a[i].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff,
						AR5K_EEPROM_MODE_11A);
			ee->ee_cal_piers_a++;

			if (((val >> 8) & 0xff) == 0)
				break;

			ee->ee_pwr_cal_a[++i].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff,
							AR5K_EEPROM_MODE_11A);
			ee->ee_cal_piers_a++;

		}
		offset = start_offset + (AR5K_EEPROM_N_5GHZ_CHAN / 2);
		gen_chan_info = ee->ee_pwr_cal_a;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		start_offset = AR5K_EEPROM_CAL_DATA_START(ee->ee_misc4);

		if (AR5K_EEPROM_HDR_11A(ee->ee_header))
			start_offset +=	(ee->ee_cal_piers_a * (3 * ee->ee_pwr_cal_a[0].rf2413_info->pd_gains) +
						(ee->ee_pwr_cal_a[0].rf2413_info->pd_gains == 1 ? 1 : 0)) + 5;

		offset = start_offset;
		ee->ee_cal_piers_b = 0;

		if (!AR5K_EEPROM_HDR_11B(ee->ee_header))
			return 0;

		for (i = 0; i < AR5K_EEPROM_N_2GHZ_CHAN_2413; i++) {
			AR5K_EEPROM_READ(offset++, val);

			if ((val & 0xff) == 0)
				break;

			ee->ee_pwr_cal_b[i].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff,
						AR5K_EEPROM_MODE_11B);
			ee->ee_cal_piers_b++;

			if (((val >> 8) & 0xff) == 0)
				break;

			ee->ee_pwr_cal_b[++i].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff,
							AR5K_EEPROM_MODE_11B);
			ee->ee_cal_piers_b++;

		}
		offset = start_offset + (AR5K_EEPROM_N_2GHZ_CHAN_2413 / 2);
		gen_chan_info = ee->ee_pwr_cal_b;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		start_offset = AR5K_EEPROM_CAL_DATA_START(ee->ee_misc4);

		if (AR5K_EEPROM_HDR_11A(ee->ee_header))
			start_offset +=	(ee->ee_cal_piers_a * (3 * ee->ee_pwr_cal_a[0].rf2413_info->pd_gains) +
							(ee->ee_pwr_cal_a[0].rf2413_info->pd_gains == 1 ? 1 : 0)) + 5;

		if (AR5K_EEPROM_HDR_11B(ee->ee_header))
			start_offset +=	(ee->ee_cal_piers_b * (3 * ee->ee_pwr_cal_b[0].rf2413_info->pd_gains) +
							(ee->ee_pwr_cal_b[0].rf2413_info->pd_gains == 1 ? 1 : 0)) + 2;

		offset = start_offset;
		ee->ee_cal_piers_g = 0;

		if (!AR5K_EEPROM_HDR_11G(ee->ee_header))
			return 0;

		for (i = 0; i < AR5K_EEPROM_N_2GHZ_CHAN_2413; i++) {
			AR5K_EEPROM_READ(offset++, val);

			if ((val & 0xff) == 0)
				break;

			ee->ee_pwr_cal_g[i].freq =
				ath5k_eeprom_bin2freq(ee, val & 0xff,
						AR5K_EEPROM_MODE_11G);
			ee->ee_cal_piers_g++;

			if (((val >> 8) & 0xff) == 0)
				break;

			ee->ee_pwr_cal_g[++i].freq =
				ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff,
							AR5K_EEPROM_MODE_11G);
			ee->ee_cal_piers_g++;

		}
		offset = start_offset + (AR5K_EEPROM_N_2GHZ_CHAN_2413 / 2);
		gen_chan_info = ee->ee_pwr_cal_g;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < cal_piers; i++) {

		gen_chan_info[i].rf2413_info =
				malloc(sizeof(struct ath5k_chan_pcal_info_rf2413));

		chan_pcal_info = gen_chan_info[i].rf2413_info;

		chan_pcal_info->pd_gains = pd_gains;

		if (chan_pcal_info->pd_gains > 0) {
			/*
			 * Read pwr_i, pddac_i and the first
			 * 2 pd points (pwr, pddac)
			 */
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr_i[0] = val & 0x1f;
			chan_pcal_info->pddac_i[0] = (val >> 5) & 0x7f;
			chan_pcal_info->pwr[0][0] =
						(val >> 12) & 0xf;

			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[0][0] = val & 0x3f;
			chan_pcal_info->pwr[0][1] = (val >> 6) & 0xf;
			chan_pcal_info->pddac[0][1] =
						(val >> 10) & 0x3f;

			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr[0][2] = val & 0xf;
			chan_pcal_info->pddac[0][2] =
						(val >> 4) & 0x3f;

			chan_pcal_info->pwr[0][3] = 0;
			chan_pcal_info->pddac[0][3] = 0;
		}

		if (chan_pcal_info->pd_gains > 1) {
			/*
			 * Pd gain 0 is not the last pd gain
			 * so it only has 2 pd points.
			 * Continue wih pd gain 1.
			 */
			chan_pcal_info->pwr_i[1] = (val >> 10) & 0x1f;

			chan_pcal_info->pddac_i[1] = (val >> 15) & 0x1;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac_i[1] |= (val & 0x3F) << 1;

			chan_pcal_info->pwr[1][0] = (val >> 6) & 0xf;
			chan_pcal_info->pddac[1][0] =
						(val >> 10) & 0x3f;

			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr[1][1] = val & 0xf;
			chan_pcal_info->pddac[1][1] =
						(val >> 4) & 0x3f;
			chan_pcal_info->pwr[1][2] =
						(val >> 10) & 0xf;

			chan_pcal_info->pddac[1][2] =
						(val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[1][2] |=
						(val & 0xF) << 2;

			chan_pcal_info->pwr[1][3] = 0;
			chan_pcal_info->pddac[1][3] = 0;
		} else if (chan_pcal_info->pd_gains == 1) {
			/*
			 * Pd gain 0 is the last one so
			 * read the extra point.
			 */
			chan_pcal_info->pwr[0][3] =
						(val >> 10) & 0xf;

			chan_pcal_info->pddac[0][3] =
						(val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[0][3] |=
						(val & 0xF) << 2;
		}

		/*
		 * Proceed with the other pd_gains
		 * as above.
		 */
		if (chan_pcal_info->pd_gains > 2) {
			chan_pcal_info->pwr_i[2] = (val >> 4) & 0x1f;
			chan_pcal_info->pddac_i[2] = (val >> 9) & 0x7f;

			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr[2][0] =
						(val >> 0) & 0xf;
			chan_pcal_info->pddac[2][0] =
						(val >> 4) & 0x3f;
			chan_pcal_info->pwr[2][1] =
						(val >> 10) & 0xf;

			chan_pcal_info->pddac[2][1] =
						(val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[2][1] |=
						(val & 0xF) << 2;

			chan_pcal_info->pwr[2][2] =
						(val >> 4) & 0xf;
			chan_pcal_info->pddac[2][2] =
						(val >> 8) & 0x3f;

			chan_pcal_info->pwr[2][3] = 0;
			chan_pcal_info->pddac[2][3] = 0;
		} else if (chan_pcal_info->pd_gains == 2) {
			chan_pcal_info->pwr[1][3] =
						(val >> 4) & 0xf;
			chan_pcal_info->pddac[1][3] =
						(val >> 8) & 0x3f;
		}

		if (chan_pcal_info->pd_gains > 3) {
			chan_pcal_info->pwr_i[3] = (val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr_i[3] |= ((val >> 0) & 0x7) << 2;

			chan_pcal_info->pddac_i[3] = (val >> 3) & 0x7f;
			chan_pcal_info->pwr[3][0] =
						(val >> 10) & 0xf;
			chan_pcal_info->pddac[3][0] =
						(val >> 14) & 0x3;

			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[3][0] |=
						(val & 0xF) << 2;
			chan_pcal_info->pwr[3][1] =
						(val >> 4) & 0xf;
			chan_pcal_info->pddac[3][1] =
						(val >> 8) & 0x3f;

			chan_pcal_info->pwr[3][2] =
						(val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr[3][2] |=
						((val >> 0) & 0x3) << 2;

			chan_pcal_info->pddac[3][2] =
						(val >> 2) & 0x3f;
			chan_pcal_info->pwr[3][3] =
						(val >> 8) & 0xf;

			chan_pcal_info->pddac[3][3] =
						(val >> 12) & 0xF;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pddac[3][3] |=
						((val >> 0) & 0x3) << 4;
		} else if (chan_pcal_info->pd_gains == 3) {
			chan_pcal_info->pwr[2][3] =
						(val >> 14) & 0x3;
			AR5K_EEPROM_READ(offset++, val);
			chan_pcal_info->pwr[2][3] |=
						((val >> 0) & 0x3) << 2;

			chan_pcal_info->pddac[2][3] =
						(val >> 2) & 0x3f;
		}

		for (c = 0; c < pd_gains; c++) {
			/* Recreate pwr table for this channel using pwr steps */
			chan_pcal_info->pwr[c][0] += chan_pcal_info->pwr_i[c] * 2;
			chan_pcal_info->pwr[c][1] += chan_pcal_info->pwr[c][0];
			chan_pcal_info->pwr[c][2] += chan_pcal_info->pwr[c][1];
			chan_pcal_info->pwr[c][3] += chan_pcal_info->pwr[c][2];
			if (chan_pcal_info->pwr[c][3] == chan_pcal_info->pwr[c][2])
				chan_pcal_info->pwr[c][3] = 0;

			/* Recreate pddac table for this channel using pddac steps */
			chan_pcal_info->pddac[c][0] += chan_pcal_info->pddac_i[c];
			chan_pcal_info->pddac[c][1] += chan_pcal_info->pddac[c][0];
			chan_pcal_info->pddac[c][2] += chan_pcal_info->pddac[c][1];
			chan_pcal_info->pddac[c][3] += chan_pcal_info->pddac[c][2];
			if (chan_pcal_info->pddac[c][3] == chan_pcal_info->pddac[c][2])
				chan_pcal_info->pddac[c][3] = 0;
		}
	}

	return 0;
}

static int ath5k_eeprom_read_pcal_info(struct ath5k_eeprom_info *ee, unsigned int mode)
{

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 1)
		return ath5k_eeprom_read_rf5112_pcal_info(ee, mode);
	else if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 2)
		return ath5k_eeprom_read_rf2413_pcal_info(ee, mode);
	/* Not sure if EEMAP existed in early eeproms */
	else if (ee->ee_version >= AR5K_EEPROM_VERSION_3_0 || AR5K_EEPROM_EEMAP(ee->ee_misc0) == 0)
		return ath5k_eeprom_read_rf5111_pcal_info(ee, mode);

	return 0;

}

/*
 * Read per rate target power (this is the maximum tx power
 * supported by the card). This info is used when setting
 * tx power, no matter the channel.
 *
 * This also works for v5 EEPROMs.
 */
static int ath5k_eeprom_read_target_rate_pwr_info(struct ath5k_eeprom_info *ee, unsigned int mode)
{
	u_int32_t offset;
	u_int16_t val;
	struct ath5k_rate_pcal_info *rate_pcal_info;
	u_int16_t *rate_target_pwr_num;
	int ret, i;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		offset = AR5K_EEPROM_TARGET_PWRSTART(ee->ee_misc1) +
				AR5K_EEPROM_TARGET_PWR_OFF_11A(ee->ee_version);
		rate_pcal_info = ee->ee_rate_tpwr_a;
		ee->ee_rate_target_pwr_num_a = AR5K_EEPROM_N_5GHZ_CHAN;
		rate_target_pwr_num = &ee->ee_rate_target_pwr_num_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		offset = AR5K_EEPROM_TARGET_PWRSTART(ee->ee_misc1) +
				AR5K_EEPROM_TARGET_PWR_OFF_11B(ee->ee_version);
		rate_pcal_info = ee->ee_rate_tpwr_b;
		ee->ee_rate_target_pwr_num_b = 2; /* 3rd is g mode's 1st */
		rate_target_pwr_num = &ee->ee_rate_target_pwr_num_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		offset = AR5K_EEPROM_TARGET_PWRSTART(ee->ee_misc1) +
				AR5K_EEPROM_TARGET_PWR_OFF_11G(ee->ee_version);
		rate_pcal_info = ee->ee_rate_tpwr_g;
		ee->ee_rate_target_pwr_num_g = AR5K_EEPROM_N_2GHZ_CHAN;
		rate_target_pwr_num = &ee->ee_rate_target_pwr_num_g;
		break;
	default:
		return -EINVAL;
	}

	/* Different freq mask for older eeproms (<= v3.2) */
	if(ee->ee_version <= AR5K_EEPROM_VERSION_3_2){
		for (i = 0; i < (*rate_target_pwr_num); i++) {
			AR5K_EEPROM_READ(offset++, val);
			rate_pcal_info[i].freq =
			    ath5k_eeprom_bin2freq(ee, (val >> 9) & 0x7f, mode);

			rate_pcal_info[i].target_power_6to24 = ((val >> 3) & 0x3f);
			rate_pcal_info[i].target_power_36 = (val << 3) & 0x3f;

			AR5K_EEPROM_READ(offset++, val);

			if (rate_pcal_info[i].freq == AR5K_EEPROM_CHANNEL_DIS ||
			    val == 0) {
				(*rate_target_pwr_num) = i;
				break;
			}

			rate_pcal_info[i].target_power_36 |= ((val >> 13) & 0x7);
			rate_pcal_info[i].target_power_48 = ((val >> 7) & 0x3f);
			rate_pcal_info[i].target_power_54 = ((val >> 1) & 0x3f);
		}
	} else {
		for (i = 0; i < (*rate_target_pwr_num); i++) {
			AR5K_EEPROM_READ(offset++, val);
			rate_pcal_info[i].freq =
			    ath5k_eeprom_bin2freq(ee, (val >> 8) & 0xff, mode);

			rate_pcal_info[i].target_power_6to24 = ((val >> 2) & 0x3f);
			rate_pcal_info[i].target_power_36 = (val << 4) & 0x3f;

			AR5K_EEPROM_READ(offset++, val);

			if (rate_pcal_info[i].freq == AR5K_EEPROM_CHANNEL_DIS ||
			    val == 0) {
				(*rate_target_pwr_num) = i;
				break;
			}

			rate_pcal_info[i].target_power_36 |= (val >> 12) & 0xf;
			rate_pcal_info[i].target_power_48 = ((val >> 6) & 0x3f);
			rate_pcal_info[i].target_power_54 = (val & 0x3f);
		}
	}

	return 0;
}

/*
 * Initialize EEPROM & capabilities data
 */
static int ath5k_eeprom_init(struct ath5k_eeprom_info *ee)
{
	unsigned int mode, i;
	int ret;
	u_int32_t offset;
	u_int16_t val;

	/* Initial TX thermal adjustment values */
	ee->ee_tx_clip = 4;
	ee->ee_pwd_84 = ee->ee_pwd_90 = 1;
	ee->ee_gain_select = 1;

	/*
	 * Read values from EEPROM and store them in the capability structure
	 */
	AR5K_EEPROM_READ(AR5K_EEPROM_MAGIC, ee->ee_magic);
	AR5K_EEPROM_READ(AR5K_EEPROM_PROTECT, ee->ee_protect);
	AR5K_EEPROM_READ(AR5K_EEPROM_REG_DOMAIN, ee->ee_regdomain);
	AR5K_EEPROM_READ(AR5K_EEPROM_VERSION, ee->ee_version);
	AR5K_EEPROM_READ(AR5K_EEPROM_HDR, ee->ee_header);

	/* Return if we have an old EEPROM */
	if (ee->ee_version < AR5K_EEPROM_VERSION_3_0)
		return 0;

#ifdef notyet
	/*
	 * Validate the checksum of the EEPROM date. There are some
	 * devices with invalid EEPROMs.
	 */
	for (cksum = 0, offset = 0; offset < AR5K_EEPROM_INFO_MAX; offset++) {
		AR5K_EEPROM_READ(AR5K_EEPROM_INFO(offset), val);
		cksum ^= val;
	}
	if (cksum != AR5K_EEPROM_INFO_CKSUM) {
		AR5K_PRINTF("Invalid EEPROM checksum 0x%04x\n", cksum);
		return -EIO;
	}
#endif

	AR5K_EEPROM_READ(AR5K_EEPROM_ANT_GAIN(ee->ee_version), ee->ee_ant_gain);

	if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0) {
		AR5K_EEPROM_READ(AR5K_EEPROM_MISC0, ee->ee_misc0);
		AR5K_EEPROM_READ(AR5K_EEPROM_MISC1, ee->ee_misc1);

		AR5K_EEPROM_READ(AR5K_EEPROM_MISC2, ee->ee_misc2);
		AR5K_EEPROM_READ(AR5K_EEPROM_MISC3, ee->ee_misc3);

		if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0){
			AR5K_EEPROM_READ(AR5K_EEPROM_MISC4, ee->ee_misc4);
			AR5K_EEPROM_READ(AR5K_EEPROM_MISC5, ee->ee_misc5);
			AR5K_EEPROM_READ(AR5K_EEPROM_MISC6, ee->ee_misc6);
		}
	}

	if (ee->ee_version < AR5K_EEPROM_VERSION_3_3) {
		AR5K_EEPROM_READ(AR5K_EEPROM_OBDB0_2GHZ, val);
		ee->ee_ob[AR5K_EEPROM_MODE_11B][0] = val & 0x7;
		ee->ee_db[AR5K_EEPROM_MODE_11B][0] = (val >> 3) & 0x7;

		AR5K_EEPROM_READ(AR5K_EEPROM_OBDB1_2GHZ, val);
		ee->ee_ob[AR5K_EEPROM_MODE_11G][0] = val & 0x7;
		ee->ee_db[AR5K_EEPROM_MODE_11G][0] = (val >> 3) & 0x7;
	}

	/*
	 * Get values for 802.11a (5GHz)
	 */
	mode = AR5K_EEPROM_MODE_11A;

	ee->ee_turbo_max_power[mode] =
	    AR5K_EEPROM_HDR_T_5GHZ_DBM(ee->ee_header);

	offset = AR5K_EEPROM_MODES_11A(ee->ee_version);

	ret = ath5k_eeprom_read_ants(ee, &offset, mode);
	if (ret)
		return ret;

	ret = ath5k_eeprom_read_modes(ee, &offset, mode);
	if (ret)
		return ret;

	/*
	 * Get values for 802.11b (2.4GHz)
	 */
	mode = AR5K_EEPROM_MODE_11B;
	offset = AR5K_EEPROM_MODES_11B(ee->ee_version);

	ret = ath5k_eeprom_read_ants(ee, &offset, mode);
	if (ret)
		return ret;

	ret = ath5k_eeprom_read_modes(ee, &offset, mode);
	if (ret)
		return ret;

	/*
	 * Get values for 802.11g (2.4GHz)
	 */
	mode = AR5K_EEPROM_MODE_11G;
	offset = AR5K_EEPROM_MODES_11G(ee->ee_version);

	ret = ath5k_eeprom_read_ants(ee, &offset, mode);
	if (ret)
		return ret;

	ret = ath5k_eeprom_read_modes(ee, &offset, mode);
	if (ret)
		return ret;

	/*
	 * Get conformance test limit values
	 */
	offset = AR5K_EEPROM_CTL(ee->ee_version);
	ee->ee_ctls = 0;

	for (i = 0; i < AR5K_EEPROM_N_CTLS(ee->ee_version); i++) {
		AR5K_EEPROM_READ(offset++, val);

		if (((val >> 8) & 0xff) == 0)
			break;

		ee->ee_ctl[i] = (val >> 8) & 0xff;
		ee->ee_ctls++;

		if ((val & 0xff) == 0)
			break;

		ee->ee_ctl[i + 1] = val & 0xff;
		ee->ee_ctls++;
	}

	/*
	 * Read power calibration info
	 */
	mode = AR5K_EEPROM_MODE_11A;
	ret = ath5k_eeprom_read_pcal_info(ee, mode);
	if (ret)
		return ret;

	mode = AR5K_EEPROM_MODE_11B;
	ret = ath5k_eeprom_read_pcal_info(ee, mode);
	if (ret)
		return ret;

	mode = AR5K_EEPROM_MODE_11G;
	ret = ath5k_eeprom_read_pcal_info(ee, mode);
	if (ret)
		return ret;

	/*
	 * Read per rate target power info
	 */
	mode = AR5K_EEPROM_MODE_11A;
	ret = ath5k_eeprom_read_target_rate_pwr_info(ee, mode);
	if (ret)
		return ret;

	mode = AR5K_EEPROM_MODE_11B;
	ret = ath5k_eeprom_read_target_rate_pwr_info(ee,  mode);
	if (ret)
		return ret;

	mode = AR5K_EEPROM_MODE_11G;
	ret = ath5k_eeprom_read_target_rate_pwr_info(ee, mode);
	if (ret)
		return ret;

	return 0;
}

static const char *ath5k_hw_get_mac_name(u_int8_t val)
{
	const char *name = "?????";
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ath5k_mac_names); i++) {

		if ((val & 0xf0) == ath5k_mac_names[i].sr_val)
			name = ath5k_mac_names[i].sr_name;

		if ((val & 0xff) == ath5k_mac_names[i].sr_val) {
			name = ath5k_mac_names[i].sr_name;
			break;
		}
	}

	return name;
}

static const char *ath5k_hw_get_phy_name(u_int8_t val)
{
	const char *name = "?????";
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ath5k_phy_names); i++) {

		if ((val & 0xf0) == ath5k_phy_names[i].sr_val)
			name = ath5k_phy_names[i].sr_name;

		if ((val & 0xff) == ath5k_phy_names[i].sr_val) {
			name = ath5k_phy_names[i].sr_name;
			break;
		}

	}

	return name;
}

/* returns -1 on unknown name */
static int eeprom_name2addr(const char *name)
{
	unsigned int i;

	if (!name || !name[0])
		return -1;
	for (i = 0; i < ARRAY_SIZE(eeprom_addr); i++)
		if (!strcmp(name, eeprom_addr[i].name))
			return eeprom_addr[i].addr;
	return -1;
}

/* returns "<unknown>" on unknown address */
static const char *eeprom_addr2name(int addr)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(eeprom_addr); i++)
		if (eeprom_addr[i].addr == addr)
			return eeprom_addr[i].name;
	return "<unknown>";
}

static int do_write_pairs(int anr, int argc, char **argv)
{
#define MAX_NR_WRITES 16
	struct {
		int addr;
		unsigned int val;
	} wr_ops[MAX_NR_WRITES];
	int wr_ops_len = 0;
	int i;
	char *end;
	int errors = 0;		/* count errors during write/verify */

	if (anr >= argc) {
		err("missing values to write.");
		usage(argv[0]);
		return 1;
	}

	if ((argc - anr) % 2) {
		err("write spec. needs an even number of arguments.");
		usage(argv[0]);
		return 2;
	}

	if ((argc - anr) / 2 > MAX_NR_WRITES) {
		err("too many values to write (max. %d)", MAX_NR_WRITES);
		return 3;
	}

	/* get the (addr,val) pairs we have to write */
	i = 0;
	while (anr < (argc - 1)) {
		wr_ops[i].addr = strtoul(argv[anr], &end, 16);
		if (end == argv[anr]) {
			/* maybe a symbolic name for the address? */
			if ((wr_ops[i].addr =
			     eeprom_name2addr(argv[anr])) == -1) {
				err("pair %d: bad address %s", i, argv[anr]);
				return 4;
			}
		}

		if (wr_ops[i].addr >= AR5K_EEPROM_INFO_BASE) {
			err("offset 0x%04x in CRC protected area is "
			    "not supported", wr_ops[i].addr);
			return 5;
		}

		anr++;
		wr_ops[i].val = strtoul(argv[anr], &end, 16);
		if (end == argv[anr]) {
			err("pair %d: bad val %s", i, argv[anr]);
			return 5;
		}

		if (wr_ops[i].val > 0xffff) {
			err("pair %d: value %u too large", i, wr_ops[i].val);
			return 6;
		}
		anr++;
		i++;
	}

	if (!(wr_ops_len = i)) {
		err("no (addr,val) pairs given");
		return 7;
	}

	if (verbose || !force_write) {
		for (i = 0; i < wr_ops_len; i++)
			printf("%20s (0x%04x) := 0x%04x\n",
			       eeprom_addr2name(wr_ops[i].addr), wr_ops[i].addr,
			       wr_ops[i].val);
	}

	if (!force_write) {
		int c;
		printf
		    ("WARNING: The write function may easy brick your device or\n"
		     "violate state regulation on frequency usage.\n"
		     "Proceed on your own risk!\n"
		     "Shall I write the above value(s)? (y/n)\n");
		c = getchar();
		if (c != 'y' && c != 'Y') {
			printf("user abort\n");
			return 0;
		}
	}

	for (i = 0; i < wr_ops_len; i++) {
		u_int16_t oldval, u;

		if (ath5k_hw_eeprom_read
		    (wr_ops[i].addr, &oldval)) {
			err("failed to read old value from offset 0x%04x ",
			    wr_ops[i].addr);
			errors++;
		}

		if (oldval == wr_ops[i].val) {
			dbg("pair %d: skipped, value already there", i);
			continue;
		}

		dbg("writing *0x%04x := 0x%04x", wr_ops[i].addr, wr_ops[i].val);
		if (ath5k_hw_eeprom_write(wr_ops[i].addr, wr_ops[i].val)) {
			err("failed to write 0x%04x to offset 0x%04x",
			    wr_ops[i].val, wr_ops[i].addr);
			errors++;
		} else {
			if (ath5k_hw_eeprom_read(wr_ops[i].addr, &u)) {
				err("failed to read offset 0x%04x for "
				    "verification", wr_ops[i].addr);
				errors++;
			} else {
				if (u != wr_ops[i].val) {
					err("offset 0x%04x: wrote 0x%04x but "
					    "read 0x%04x", wr_ops[i].addr,
					    wr_ops[i].val, u);
					errors++;
				}
			}
		}
	}

	return errors ? 11 : 0;
}

static void usage(const char *n)
{
	unsigned int i;

	fprintf(stderr, "%s [-w [-g N:M]] [-v] [-f] [-d] [-r] [-R addr] [-W addr val] <base_address> [dumpfile] "
		"[<name1> <val1> [<name2> <val2> ...]]\n\n", n);
	fprintf(stderr,
		"-w      write values into EEPROM\n"
		"-g N:M  set GPIO N to level M (only used with -w)\n"
		"-v      verbose output\n"
		"-f      force; suppress question before writing\n"
		"-d      dump EEPROM (file 'ath-eeprom-dump.bin' and screen)\n"
		"-r	 restore EEPROM from dumpfile provided\n"
		"-M <mac_addr>   write provided mac address on EEPROM\n"
		"-R <addr>       read register at <addr> (hex)\n"
		"-W <addr> <val> write <val> (hex) into register at <addr> (hex)\n"
		"<base_address>  device base address (see lspci output)\n\n");

	fprintf(stderr,
		"- read info:\n"
		"  %s <base_address>\n\n"
		"- set regdomain to N:\n"
		"  %s -w <base_address> regdomain N\n\n"
		"- set a PCI id field to value N:\n"
		"  %s -w <base_address> <field> N\n"
		"  where <field> is one of:\n    ", n, n, n);
	for (i = 0; i < ARRAY_SIZE(eeprom_addr); i++)
		fprintf(stderr, " %s", eeprom_addr[i].name);
	fprintf(stderr, "\n\n");
	fprintf(stderr,
		"You may need to set a GPIO to a certain value in order to enable\n"
		"writing to the EEPROM with newer chipsets, e.g. set GPIO 4 to low:\n"
		"  %s -g 4:0 -w <base_address> regdomain N\n", n);
	fprintf(stderr,
		"\nDISCLAIMER: The authors are not responsible for any damages caused by\n"
		"this program. Writing improper values may damage the card or cause\n"
		"unlawful radio transmissions!\n\n");
}

static void dump_capabilities(struct ath5k_eeprom_info *ee)
{
	u_int8_t has_a, has_b, has_g, has_rfkill, turbog_dis, turboa_dis;
	u_int8_t xr2_dis, xr5_dis, has_crystal;

	has_a = AR5K_EEPROM_HDR_11A(ee->ee_header);
	has_b = AR5K_EEPROM_HDR_11B(ee->ee_header);
	has_g = AR5K_EEPROM_HDR_11G(ee->ee_header);
	has_rfkill = AR5K_EEPROM_HDR_RFKILL(ee->ee_header);
	has_crystal = AR5K_EEPROM_HAS32KHZCRYSTAL(ee->ee_misc1);
	turbog_dis = AR5K_EEPROM_HDR_T_2GHZ_DIS(ee->ee_header);
	turboa_dis = AR5K_EEPROM_HDR_T_5GHZ_DIS(ee->ee_header);
	xr2_dis = AR5K_EEPROM_HDR_XR2_DIS(ee->ee_misc0);
	xr5_dis = AR5K_EEPROM_HDR_XR5_DIS(ee->ee_misc0);

	printf("|================= Capabilities ================|\n");

	printf("| 802.11a Support: ");
	if (has_a)
		printf(" yes |");
	else
		printf(" no  |");

	printf(" Turbo-A disabled:");
	if (turboa_dis)
		printf(" yes |\n");
	else
		printf(" no  |\n");

	printf("| 802.11b Support: ");
	if (has_b)
		printf(" yes |");
	else
		printf(" no  |");

	printf(" Turbo-G disabled:");
	if (turbog_dis)
		printf(" yes |\n");
	else
		printf(" no  |\n");

	printf("| 802.11g Support: ");
	if (has_g)
		printf(" yes |");
	else
		printf(" no  |");

	printf(" 2GHz XR disabled:");
	if (xr2_dis)
		printf(" yes |\n");
	else
		printf(" no  |\n");

	printf("| RFKill  Support: ");
	if (has_rfkill)
		printf(" yes |");
	else
		printf(" no  |");

	printf(" 5GHz XR disabled:");
	if (xr5_dis)
		printf(" yes |\n");
	else
		printf(" no  |\n");

	if (has_crystal != 2) {
		printf("| 32kHz   Crystal: ");
		if (has_crystal)
			printf(" yes |");
		else
			printf(" no  |");

		printf("                       |\n");
	}

	printf("\\===============================================/\n");
}

static void dump_calinfo_for_mode(int mode, struct ath5k_eeprom_info *ee)
{
	int i;

	printf("|=========================================================|\n");
	printf("| I power:              0x%02x |", ee->ee_i_cal[mode]);
	printf(" Q power:              0x%02x |\n", ee->ee_q_cal[mode]);
	printf("| Use fixed bias:       0x%02x |", ee->ee_fixed_bias[mode]);
	printf(" Max turbo power:      0x%02x |\n", ee->ee_turbo_max_power[mode]);
	printf("| Max XR power:         0x%02x |", ee->ee_xr_power[mode]);
	printf(" Switch Settling Time: 0x%02x |\n", ee->ee_switch_settling[mode]);
	printf("| Tx/Rx attenuation:    0x%02x |", ee->ee_atn_tx_rx[mode]);
	printf(" TX end to XLNA On:    0x%02x |\n", ee->ee_tx_end2xlna_enable[mode]);
	printf("| TX end to XPA Off:    0x%02x |", ee->ee_tx_end2xpa_disable[mode]);
	printf(" TX end to XPA On:     0x%02x |\n", ee->ee_tx_frm2xpa_enable[mode]);
	printf("| 62db Threshold:       0x%02x |", ee->ee_thr_62[mode]);
	printf(" XLNA gain:            0x%02x |\n", ee->ee_xlna_gain[mode]);
	printf("| XPD:                  0x%02x |", ee->ee_xpd[mode]);
	printf(" XPD gain:             0x%02x |\n", ee->ee_x_gain[mode]);
	printf("| I gain:               0x%02x |", ee->ee_i_gain[mode]);
	printf(" Tx/Rx margin:         0x%02x |\n", ee->ee_margin_tx_rx[mode]);
	printf("| False detect backoff: 0x%02x |", ee->ee_false_detect[mode]);
	printf(" Noise Floor Threshold: %3d |\n", ee->ee_noise_floor_thr[mode]);
	printf("| ADC desired size:      %3d |", ee->ee_adc_desired_size[mode]);
	printf(" PGA desired size:      %3d |\n", ee->ee_pga_desired_size[mode]);
	printf("|=========================================================|\n");
	for (i = 0; i < AR5K_EEPROM_N_PCDAC; i++) {
		printf("| Antenna control  %2i:  0x%02x |", i, ee->ee_ant_control[mode][i]);
		i++;
		printf(" Antenna control  %2i:  0x%02x |\n", i, ee->ee_ant_control[mode][i]);
	}
	printf("|=========================================================|\n");
	for (i = 0; i < AR5K_EEPROM_N_OBDB; i++) {
		printf("| Octave Band %i:          %2i |", i, ee->ee_ob[mode][i]);
		printf(" db %i:                   %2i |\n", i, ee->ee_db[mode][i]);
	}
	printf("\\=========================================================/\n");

	if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 && (mode == AR5K_EEPROM_MODE_11A
	|| mode == AR5K_EEPROM_MODE_11G)) {
	printf("/==================== Turbo mode infos ===================\\\n");
	printf("| Switch Settling time: 0x%02x |", ee->ee_switch_settling_turbo[mode]);
	printf(" Tx/Rx margin:         0x%02x |\n", ee->ee_margin_tx_rx_turbo[mode]);
	printf("| Tx/Rx attenuation:    0x%02x |", ee->ee_atn_tx_rx_turbo[mode]);
	printf(" ADC desired size:      %3d |\n", ee->ee_adc_desired_size_turbo[mode]);
	printf("| PGA desired size:      %3d ", ee->ee_pga_desired_size_turbo[mode]);
	printf("|                            |\n");
	printf("\\=========================================================/\n");
	}
}

static void dump_rf5111_power_calinfo_for_mode(int mode, struct ath5k_eeprom_info *ee)
{
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf5111 *chan_pcal_info;
	u_int16_t cal_piers;
	int i, c;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		gen_chan_info = ee->ee_pwr_cal_a;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		gen_chan_info = ee->ee_pwr_cal_b;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		gen_chan_info = ee->ee_pwr_cal_g;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return;
	}

	printf("/=============================== Per channel power calibration ================================\\\n");
	printf("| Freq | pwr_0 | pwr_1 | pwr_2 | pwr_3 | pwr_4 | pwr_5 | pwr_6 | pwr_7 | pwr_8 | pwr_9 | pwr10 |\n");
	printf("|      | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac |\n");

	for (i = 0; i < cal_piers; i++) {
		char buf[16];
		chan_pcal_info = gen_chan_info[i].rf5111_info;

		printf("|======|=======|=======|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");
		printf("| %4i |", gen_chan_info[i].freq);
		if (ee->ee_version <= AR5K_EEPROM_VERSION_3_2) {
			for (c = 0; c < AR5K_EEPROM_N_PWR_POINTS_5111; c++) {
				printf(" %2i.%02i |", chan_pcal_info->pwr[c] / 2,
				       chan_pcal_info->pwr[c] % 2 * 50);
			}

			printf("\n|      |");
		} else {
			for (c = 0; c < AR5K_EEPROM_N_PWR_POINTS_5111; c++) {
				printf(" %2i.%02i |", chan_pcal_info->pwr[c] / 4,
				       chan_pcal_info->pwr[c] % 4 * 25);
			}

			printf("\n|      |");
		}

		for (c = 0; c < AR5K_EEPROM_N_PWR_POINTS_5111; c++) {
			snprintf(buf, sizeof(buf), "[%i]",
				 chan_pcal_info->pcdac[c]);
			printf("%6s |", buf);
		}

		printf("\n");

	}
	printf("\\==============================================================================================/\n");
}

static void dump_rf5112_power_calinfo_for_mode(int mode, struct ath5k_eeprom_info *ee)
{
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf5112 *chan_pcal_info;
	u_int16_t cal_piers;
	int i, c;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		gen_chan_info = ee->ee_pwr_cal_a;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		gen_chan_info = ee->ee_pwr_cal_b;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		gen_chan_info = ee->ee_pwr_cal_g;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return;
	}

	printf("/=================== Per channel power calibration ====================\\\n");
	printf("| Freq | pwr_0 | pwr_1 | pwr_2 | pwr_3 |pwrx3_0|pwrx3_1|pwrx3_2|max_pwr|\n");
	printf("|      | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac | pcdac |       |\n");

	for (i = 0; i < cal_piers; i++) {
		char buf[16];

		chan_pcal_info = gen_chan_info[i].rf5112_info;

		printf("|======|=======|=======|=======|=======|=======|=======|=======|=======|\n");
		printf("| %4i |", gen_chan_info[i].freq);
		for (c = 0; c < AR5K_EEPROM_N_XPD0_POINTS; c++) {
			printf(" %2i.%02i |", chan_pcal_info->pwr_x0[c] / 4,
			       chan_pcal_info->pwr_x0[c] % 4 * 25);
		}
		for (c = 0; c < AR5K_EEPROM_N_XPD3_POINTS; c++) {
			printf(" %2i.%02i |", chan_pcal_info->pwr_x3[c] / 4,
			       chan_pcal_info->pwr_x3[c] % 4 * 25);
		}
		printf(" %2i.%02i |\n", gen_chan_info[i].max_pwr / 4,
		       gen_chan_info[i].max_pwr % 4 * 25);

		printf("|      |");
		for (c = 0; c < AR5K_EEPROM_N_XPD0_POINTS; c++) {
			snprintf(buf, sizeof(buf), "[%i]",
				 chan_pcal_info->pcdac_x0[c]);
			printf("%6s |", buf);
		}
		for (c = 0; c < AR5K_EEPROM_N_XPD3_POINTS; c++) {
			snprintf(buf, sizeof(buf), "[%i]",
				 chan_pcal_info->pcdac_x3[c]);
			printf("%6s |", buf);
		}
		printf("       |\n");

	}
	printf("\\======================================================================/\n");
}

static void dump_rf2413_power_calinfo_for_mode(int mode, struct ath5k_eeprom_info *ee)
{
	struct	ath5k_chan_pcal_info *gen_chan_info;
	struct ath5k_chan_pcal_info_rf2413 *chan_pcal_info;
	u_int16_t cal_piers;
	int i, c;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		gen_chan_info = ee->ee_pwr_cal_a;
		cal_piers = ee->ee_cal_piers_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		gen_chan_info = ee->ee_pwr_cal_b;
		cal_piers = ee->ee_cal_piers_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		gen_chan_info = ee->ee_pwr_cal_g;
		cal_piers = ee->ee_cal_piers_g;
		break;
	default:
		return;
	}

	printf("/====================== Per channel power calibration ===================\\\n");
	printf("| Freq |  pwr_i  |    pwr_0    |    pwr_1    |    pwr_2    |    pwr_3    |\n");
	printf("|      | pddac_i |   pddac_0   |   pddac_1   |   pddac_2   |   pddac_3   |\n");

	for (i = 0; i < cal_piers; i++) {

		chan_pcal_info = gen_chan_info[i].rf2413_info;

		printf("|======|=========|=============|=============|=============|=============|\n");
		printf("| %4i |         |             |             |             |             |\n", gen_chan_info[i].freq);
		printf("|------|---------|-------------|-------------|-------------|-------------|\n");
		for (c = 0; c < chan_pcal_info->pd_gains; c++){
			printf("|      |    %2i   |    %2i.%02i    |    %2i.%02i    |    %2i.%02i    |    %2i.%02i    |\n",
							chan_pcal_info->pwr_i[c],
							chan_pcal_info->pwr[c][0] / 2,
							chan_pcal_info->pwr[c][0] % 2 * 50,
							chan_pcal_info->pwr[c][1] / 2,
							chan_pcal_info->pwr[c][1] % 2 * 50,
							chan_pcal_info->pwr[c][2] / 2,
							chan_pcal_info->pwr[c][2] % 2 * 50,
							chan_pcal_info->pwr[c][3] / 2,
							chan_pcal_info->pwr[c][3] % 2 * 50);

			printf("|      |   %3i   |      %3i    |      %3i    |      %3i    |      %3i    |\n",
							chan_pcal_info->pddac_i[c],
							chan_pcal_info->pddac[c][0],
							chan_pcal_info->pddac[c][1],
							chan_pcal_info->pddac[c][2],
							chan_pcal_info->pddac[c][3]);
			if ( c < chan_pcal_info->pd_gains - 1)
			printf("|------|---------|-------------|-------------|-------------|-------------|\n");
		}
	}
	printf("\\========================================================================/\n");
}

static void dump_rate_calinfo_for_mode(int mode, struct ath5k_eeprom_info *ee)
{
	int i;
	struct ath5k_rate_pcal_info *rate_pcal_info;
	u_int16_t rate_target_pwr_num;

	switch (mode) {
	case AR5K_EEPROM_MODE_11A:
		rate_pcal_info = ee->ee_rate_tpwr_a;
		rate_target_pwr_num = ee->ee_rate_target_pwr_num_a;
		break;
	case AR5K_EEPROM_MODE_11B:
		rate_pcal_info = ee->ee_rate_tpwr_b;
		rate_target_pwr_num = ee->ee_rate_target_pwr_num_b;
		break;
	case AR5K_EEPROM_MODE_11G:
		rate_pcal_info = ee->ee_rate_tpwr_g;
		rate_target_pwr_num = ee->ee_rate_target_pwr_num_g;
		break;
	default:
		return;
	}

	printf("/============== Per rate power calibration ===========\\\n");
	if (mode == AR5K_EEPROM_MODE_11B)
		printf("| Freq |   1Mbit/s  | 2Mbit/s  | 5.5Mbit/s | 11Mbit/s |\n");
	else
		printf("| Freq | 6-24Mbit/s | 36Mbit/s |  48Mbit/s | 54Mbit/s |\n");

	for (i = 0; i < rate_target_pwr_num; i++) {

		printf("|======|============|==========|===========|==========|\n");
		printf("| %4i |", rate_pcal_info[i].freq);
		printf("    %2i.%02i   |",rate_pcal_info[i].target_power_6to24 /2,
					rate_pcal_info[i].target_power_6to24 % 2);
		printf("  %2i.%02i   |",rate_pcal_info[i].target_power_36 /2,
					rate_pcal_info[i].target_power_36 % 2);
		printf("   %2i.%02i   |",rate_pcal_info[i].target_power_48 /2,
					rate_pcal_info[i].target_power_48 % 2);
		printf("  %2i.%02i   |\n",rate_pcal_info[i].target_power_54 /2,
					rate_pcal_info[i].target_power_54 % 2);
	}
	printf("\\=====================================================/\n");
}

static u_int32_t extend_tu(u_int32_t base_tu, u_int32_t val, u_int32_t mask)
{
	u_int32_t result;

	result = (base_tu & ~mask) | (val & mask);
	if ((base_tu & mask) > (val & mask))
		result += mask + 1;
	return result;
}

static void dump_timers_register(void)
{
#define AR5K_TIMER0_5210		0x802c	/* next TBTT */
#define AR5K_TIMER0_5211		0x8028
#define AR5K_TIMER0			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TIMER0_5210 : AR5K_TIMER0_5211)

#define AR5K_TIMER1_5210		0x8030	/* next DMA beacon */
#define AR5K_TIMER1_5211		0x802c
#define AR5K_TIMER1			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TIMER1_5210 : AR5K_TIMER1_5211)

#define AR5K_TIMER2_5210		0x8034	/* next SWBA interrupt */
#define AR5K_TIMER2_5211		0x8030
#define AR5K_TIMER2			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TIMER2_5210 : AR5K_TIMER2_5211)

#define AR5K_TIMER3_5210		0x8038	/* next ATIM window */
#define AR5K_TIMER3_5211		0x8034
#define AR5K_TIMER3			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TIMER3_5210 : AR5K_TIMER3_5211)

#define AR5K_TSF_L32_5210		0x806c	/* TSF (lower 32 bits) */
#define AR5K_TSF_L32_5211		0x804c
#define AR5K_TSF_L32			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TSF_L32_5210 : AR5K_TSF_L32_5211)

#define AR5K_TSF_U32_5210		0x8070
#define AR5K_TSF_U32_5211		0x8050
#define AR5K_TSF_U32			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_TSF_U32_5210 : AR5K_TSF_U32_5211)

#define AR5K_BEACON_5210		0x8024
#define AR5K_BEACON_5211		0x8020
#define AR5K_BEACON			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_BEACON_5210 : AR5K_BEACON_5211)

#define AR5K_LAST_TSTP			0x8080

	const int timer_mask = 0xffff;

	u_int32_t timer0, timer1, timer2, timer3, now_tu;
	u_int32_t timer0_tu, timer1_tu, timer2_tu, timer3_tu;
	u_int64_t now_tsf;

	timer0 = AR5K_REG_READ(AR5K_TIMER0);		/* 0x0000ffff */
	timer1 = AR5K_REG_READ(AR5K_TIMER1_5211);	/* 0x0007ffff */
	timer2 = AR5K_REG_READ(AR5K_TIMER2_5211);	/* 0x?1ffffff */
	timer3 = AR5K_REG_READ(AR5K_TIMER3_5211);	/* 0x0000ffff */

	now_tsf = ((u_int64_t)AR5K_REG_READ(AR5K_TSF_U32_5211) << 32)
		| (u_int64_t)AR5K_REG_READ(AR5K_TSF_L32_5211);

	now_tu = now_tsf >> 10;

	timer0_tu = extend_tu(now_tu, timer0, 0xffff);
	printf("TIMER0: 0x%08x, TBTT: %5u, TU: 0x%08x\n", timer0,
	       timer0 & timer_mask, timer0_tu);
	timer1_tu = extend_tu(now_tu, timer1 >> 3, 0x7ffff >> 3);
	printf("TIMER1: 0x%08x, DMAb: %5u, TU: 0x%08x (%+d)\n", timer1,
	       (timer1 >> 3) & timer_mask, timer1_tu, timer1_tu - timer0_tu);
	timer2_tu = extend_tu(now_tu, timer2 >> 3, 0x1ffffff >> 3);
	printf("TIMER2: 0x%08x, SWBA: %5u, TU: 0x%08x (%+d)\n", timer2,
	       (timer2 >> 3) & timer_mask, timer2_tu, timer2_tu - timer0_tu);
	timer3_tu = extend_tu(now_tu, timer3, 0xffff);
	printf("TIMER3: 0x%08x, ATIM: %5u, TU: 0x%08x (%+d)\n", timer3,
	       timer3 & timer_mask, timer3_tu, timer3_tu - timer0_tu);
	printf("TSF: 0x%016llx, TSFTU: %5u, TU: 0x%08x\n",
	       (unsigned long long)now_tsf, now_tu & timer_mask, now_tu);

	printf("BEACON: 0x%08x\n", AR5K_REG_READ(AR5K_BEACON));
	printf("LAST_TSTP: 0x%08x\n", AR5K_REG_READ(AR5K_LAST_TSTP));
}

#define AR5K_KEYTABLE_0_5210		0x9000
#define AR5K_KEYTABLE_0_5211		0x8800
#define AR5K_KEYTABLE_0			(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_KEYTABLE_0_5210 : \
					AR5K_KEYTABLE_0_5211)

#define AR5K_KEYTABLE(_n)		(AR5K_KEYTABLE_0_5211 + ((_n) << 5))
#define AR5K_KEYTABLE_OFF(_n, x)	(AR5K_KEYTABLE(_n) + ((x) << 2))
#define AR5K_KEYTABLE_VALID		0x00008000

#define AR5K_KEYTABLE_SIZE_5210		64
#define AR5K_KEYTABLE_SIZE_5211		128
#define AR5K_KEYTABLE_SIZE		(mac_revision == AR5K_SREV_AR5210 ? \
					AR5K_KEYTABLE_SIZE_5210 : \
					AR5K_KEYTABLE_SIZE_5211)

static void keycache_dump(void)
{
	int i;
	u_int32_t val0, val1, val2, val3, val4, keytype, ant, mac0, mac1;

	/* dump all 128 entries */
	printf("Dumping keycache entries...\n");
	for (i = 0; i < AR5K_KEYTABLE_SIZE; i++) {
		mac1 = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 7));
		if (mac1 & AR5K_KEYTABLE_VALID) {
			val0    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 0));
			val1    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 1));
			val2    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 2));
			val3    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 3));
			val4    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 4));
			keytype = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 5));
			ant = keytype & 8;
			keytype &= ~8;
			mac0 = AR5K_REG_READ(AR5K_KEYTABLE_OFF(i, 6));

			printf("[%3u] keytype %d [%s%s%s%s%s%s%s%s] mac %02x:%02x:%02x:%02x:%02x:%02x key:%08x-%08x-%08x-%08x-%08x\n",
			       i,
			       keytype,
			       keytype == 0 ? "WEP40 " : "",
			       keytype == 1 ? "WEP104" : "",
			       keytype == 3 ? "WEP128" : "",
			       keytype == 4 ? "TKIP  " : "",
			       keytype == 5 ? "AES   " : "",
			       keytype == 6 ? "CCM   " : "",
			       keytype == 7 ? "NULL  " : "",
			       ant     == 8 ? "+ANT"   : "",
			       ((mac0 <<  1) & 0xff),
			       ((mac0 >>  7) & 0xff),
			       ((mac0 >> 15) & 0xff),
			       ((mac0 >> 23) & 0xff),
			       ((mac1 <<  1) & 0xff) | (mac0 >> 31),
			       ((mac1 >>  7) & 0xff),
			       val0, val1, val2, val3, val4);
		}
	}
}

/* copy key index (0) to key index (idx) */

static void keycache_copy(int idx)
{
	u_int32_t val0, val1, val2, val3, val4, keytype, mac0, mac1;

	printf("Copying keycache entry 0 to %d\n", idx);
	if (idx < 0 || idx >= AR5K_KEYTABLE_SIZE) {
		printf("invalid keycache index\n");
		return;
	}

	val0    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 0));
	val1    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 1));
	val2    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 2));
	val3    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 3));
	val4    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 4));
	keytype = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 5));
	mac0    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 6));
	mac1    = AR5K_REG_READ(AR5K_KEYTABLE_OFF(0, 7));

	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 0), val0);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 1), val1);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 2), val2);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 3), val3);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 4), val4);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 5), keytype);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 6), mac0);
	AR5K_REG_WRITE(AR5K_KEYTABLE_OFF(idx, 7), mac1);
}

static void sta_id0_id1_dump(void)
{
#define AR5K_STA_ID0			0x8000
#define AR5K_STA_ID1			0x8004
#define AR5K_STA_ID1_AP                 0x00010000
#define AR5K_STA_ID1_ADHOC              0x00020000
#define AR5K_STA_ID1_NO_KEYSRCH		0x00080000

	u_int32_t sta_id0, sta_id1;

	sta_id0 = AR5K_REG_READ(AR5K_STA_ID0);
	sta_id1 = AR5K_REG_READ(AR5K_STA_ID1);
	printf("STA_ID0: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       (sta_id0 >>  0) & 0xff,
	       (sta_id0 >>  8) & 0xff,
	       (sta_id0 >> 16) & 0xff,
	       (sta_id0 >> 24) & 0xff,
	       (sta_id1 >>  0) & 0xff,
	       (sta_id1 >>  8) & 0xff);
	printf("STA_ID1: 0x%08x, AP: %d, IBSS: %d, KeyCache Disable: %d\n",
	       sta_id1,
	       sta_id1 & AR5K_STA_ID1_AP ? 1 : 0,
	       sta_id1 & AR5K_STA_ID1_ADHOC ? 1 : 0,
	       sta_id1 & AR5K_STA_ID1_NO_KEYSRCH ? 1 : 0);
}

static void show_eeprom_info(struct ath5k_eeprom_info *ee)
{
	u_int8_t eemap;

	eemap = AR5K_EEPROM_EEMAP(ee->ee_misc0);

	printf("/============== EEPROM Information =============\\\n");
	printf("| EEPROM Version:   %1x.%1x |",
	       (ee->ee_version & 0xF000) >> 12, ee->ee_version & 0xFFF);

	printf(" EEPROM Size: %3d kbit |\n", eeprom_size * 8 / 1024);

	printf("| EEMAP:              %i |", eemap);

	printf(" Reg. Domain:     0x%02X |\n", ee->ee_regdomain);

	dump_capabilities(ee);
	printf("\n");
}

int main(int argc, char *argv[])
{
	unsigned long long dev_addr;
	u_int16_t srev, phy_rev_5ghz, phy_rev_2ghz, ee_magic;
	u_int8_t error, dev_type;
	u_int16_t eesize;
	struct ath5k_eeprom_info *ee;
	int fd;
	unsigned int i;
	int anr = 1;
	int do_write = 0;	/* default: read only */
	int do_dump = 0;
	int do_restore = 0;
	int change_mac = 0;
	int reg_read = 0;
	int reg_write = 0;
	u_int8_t *mac_addr = NULL;
	char* colon = NULL;
	unsigned int reg_write_val = 0;
	unsigned int timer_count = 1;
	int do_keycache_dump = 0;
	int keycache_copy_idx = 0;

	struct {
		int valid;
		int value;
	} gpio_set[AR5K_NUM_GPIO];
	int nr_gpio_set = 0;

	for (i = 0; i < ARRAY_SIZE(gpio_set); i++)
		gpio_set[i].valid = 0;

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	while (anr < argc && argv[anr][0] == '-') {
		switch (argv[anr][1]) {
		case 't':
			if (++anr < argc) {
				timer_count = atoi(argv[anr]);
				printf("timer_count:%d\n", timer_count);
			} else {
				usage(argv[0]);
				return 0;
			}
			break;
		case 'w':
			do_write = 1;
			break;
		case 'g':
			anr++;
			if (strlen(argv[anr]) != 3 || argv[anr][1] != ':' ||
			    argv[anr][0] < '0' || argv[anr][0] > '5' ||
			    (argv[anr][2] != '0' && argv[anr][2] != '1')) {
				err("invalid GPIO spec. %s", argv[anr]);
				return 2;
			}
			gpio_set[argv[anr][0] - '0'].valid = 1;
			gpio_set[argv[anr][0] - '0'].value = argv[anr][2] - '0';
			nr_gpio_set++;
			break;

		case 'f':
			force_write = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'r':
			do_restore = 1;
			break;

		case 'd':
			do_dump = 1;
			break;

		case 'M':
			change_mac = 1;
			anr++;
			colon = strchr(argv[anr], ':');
			if(!colon) {
				printf("Invalid MAC address\n");
				return -1;
			}
			mac_addr = malloc(sizeof(u_int8_t) * 6);
			strcpy(colon, "\0");
			mac_addr[0] = (u_int8_t) strtoul(argv[anr], NULL, 16);
			for (i = 1; i < 5; i++) {
				mac_addr[i] = (u_int8_t) strtoul(colon + 1, NULL, 16);
				colon = strchr(colon + 1, ':');
				if(!colon) {
					printf("Invalid MAC address\n");
					return -1;
				}
				strcpy(colon, "\0");
			}
			mac_addr[5] = (u_int8_t) strtoul(colon + 1, NULL, 16);
			break;

		case 'R':
			anr++;
			reg_read = strtoul(argv[anr], NULL, 16);
			break;

		case 'W':
			anr++;
			reg_write = strtoul(argv[anr++], NULL, 16);
			reg_write_val = strtoul(argv[anr], NULL, 16);
			break;

		case 'k':
			do_keycache_dump = 1;
			break;

		case 'K':
			keycache_copy_idx = atoi(argv[++anr]);
			break;

		case 'h':
			usage(argv[0]);
			return 0;
			break;

		default:
			err("unknown option %s", argv[anr]);
			return 2;
		}

		anr++;
	}

	if (anr >= argc) {
		err("missing device address");
		usage(argv[0]);
		return 3;
	}

	dev_addr = strtoull(argv[anr], NULL, 16);

	fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		printf("Opening /dev/mem failed!\n");
		return -2;
	}

	mem = mmap(NULL, AR5K_PCI_MEM_SIZE, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_FILE, fd, dev_addr);

	if (mem == MAP_FAILED) {
		printf("mmap of device at 0x%08llX for 0x%X bytes failed - "
		       "%s\n", dev_addr, AR5K_PCI_MEM_SIZE, strerror(errno));
		return -3;
	}

	/* wake from power-down and remove reset (in case the driver isn't running) */
	{
		u_int32_t
		    sleep_ctl = AR5K_REG_READ(AR5K_SLEEP_CTL),
		    reset_ctl = AR5K_REG_READ(AR5K_RESET_CTL);

		dbg("sleep_ctl reg %08x   reset_ctl reg %08x",
		    sleep_ctl, reset_ctl);
		if (sleep_ctl & AR5K_SLEEP_CTL_SLE_SLP) {
			dbg("waking up the chip");
			AR5K_REG_WRITE(AR5K_SLEEP_CTL,
				       (sleep_ctl & ~AR5K_SLEEP_CTL_SLE_SLP));
		}

		if (reset_ctl) {
			dbg("removing resets");
			AR5K_REG_WRITE(AR5K_RESET_CTL, 0);
		}
	}

	AR5K_REG_DISABLE_BITS(AR5K_PCICFG, AR5K_PCICFG_SPWR_DN);
	usleep(500);

	if (reg_read) {
		printf("READ %04x = %08x\n", reg_read, AR5K_REG_READ(reg_read));
		return 0;
	}

	if (reg_write) {
		printf("WRITE %04x = %08x\n", reg_write, reg_write_val);
		AR5K_REG_WRITE(reg_write, reg_write_val);
		return 0;
	}

	srev = AR5K_REG_READ(AR5K_SREV);
	if (srev >= 0x0100) {
		printf("MAC revision 0x%04x is not supported!\n", srev);
		return -1;
	}
	mac_revision = srev & AR5K_SREV_REV;
	if (mac_revision < AR5K_SREV_AR5311)
		eeprom_access = AR5K_EEPROM_ACCESS_5210;
	else
		eeprom_access = AR5K_EEPROM_ACCESS_5211;

	printf(" -==Device Information==-\n");

	printf("MAC Revision: %-5s (0x%02x)\n",
	       ath5k_hw_get_mac_name(mac_revision), mac_revision);

	/* Verify EEPROM magic value first */
	error = ath5k_hw_eeprom_read(AR5K_EEPROM_MAGIC, &ee_magic);

	if (error) {
		printf("Unable to read EEPROM Magic value!\n");
		return -1;
	}

	if (ee_magic != AR5K_EEPROM_MAGIC_VALUE) {
		printf("Warning: Invalid EEPROM Magic number!\n");
	}

	ee = calloc(sizeof(struct ath5k_eeprom_info), 1);
	if (!ee) {
		printf("Cannot allocate memory for EEPROM information\n");
		return -1;
	}

	if (ath5k_eeprom_init(ee)) {
		printf("EEPROM init failed\n");
		return -1;
	}

	/* 1 = ?? 2 = ?? 3 = card 4 = wmac */
	dev_type = AR5K_EEPROM_HDR_DEVICE(ee->ee_header);
	printf("Device type:  %1i\n", dev_type);

	if (AR5K_EEPROM_HDR_11A(ee->ee_header))
		phy_rev_5ghz = ath5k_hw_radio_revision(1);
	else
		phy_rev_5ghz = 0;

	if (AR5K_EEPROM_HDR_11B(ee->ee_header))
		phy_rev_2ghz = ath5k_hw_radio_revision(0);
	else
		phy_rev_2ghz = 0;

	if (phy_rev_5ghz) {
		printf("5GHz PHY Revision: %-5s (0x%02x)\n",
		       ath5k_hw_get_phy_name(phy_rev_5ghz), phy_rev_5ghz);
	}
	if (phy_rev_2ghz) {
		printf("2GHz PHY Revision: %-5s (0x%02x)\n",
		       ath5k_hw_get_phy_name(phy_rev_2ghz), phy_rev_2ghz);
	}

	printf("\n");

	eesize = AR5K_REG_MS(AR5K_REG_READ(AR5K_PCICFG), AR5K_PCICFG_EESIZE);

	if (eesize == 0)
		eeprom_size = 4096 / 8;
	else if (eesize == 1)
		eeprom_size = 8192 / 8;
	else if (eesize == 2)
		eeprom_size = 16384 / 8;
	else
		eeprom_size = 0;

	show_eeprom_info(ee);

	printf("/=========================================================\\\n");
	printf("|          Calibration data common for all modes          |\n");
	printf("|=========================================================|\n");
	printf("|          CCK/OFDM gain delta:            %2i             |\n", ee->ee_cck_ofdm_gain_delta);
	printf("|          CCK/OFDM power delta:           %2i             |\n", ee->ee_cck_ofdm_power_delta);
	printf("|          Scaled CCK delta:               %2i             |\n", ee->ee_scaled_cck_delta);
	printf("|          2GHz Antenna gain:              %2i             |\n", AR5K_EEPROM_ANT_GAIN_2GHZ(ee->ee_ant_gain));
	printf("|          5GHz Antenna gain:              %2i             |\n", AR5K_EEPROM_ANT_GAIN_5GHZ(ee->ee_ant_gain));
	printf("|          Turbo 2W maximum dBm:           %2i             |\n", AR5K_EEPROM_HDR_T_5GHZ_DBM(ee->ee_header));
	printf("|          Target power start:          0x%03x             |\n", AR5K_EEPROM_TARGET_PWRSTART(ee->ee_misc1));
	printf("|          EAR Start:                   0x%03x             |\n", AR5K_EEPROM_EARSTART(ee->ee_misc0));
	printf("\\=========================================================/\n");

	printf("\n");
	if (AR5K_EEPROM_HDR_11A(ee->ee_header)) {
		printf("/=========================================================\\\n");
		printf("|          Calibration data for 802.11a operation         |\n");
		dump_calinfo_for_mode(AR5K_EEPROM_MODE_11A, ee);
		dump_rate_calinfo_for_mode(AR5K_EEPROM_MODE_11A, ee);
		if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 1)
				dump_rf5112_power_calinfo_for_mode(AR5K_EEPROM_MODE_11A, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 2)
				dump_rf2413_power_calinfo_for_mode(AR5K_EEPROM_MODE_11A, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_3_0 || AR5K_EEPROM_EEMAP(ee->ee_misc0) == 0)
				dump_rf5111_power_calinfo_for_mode(AR5K_EEPROM_MODE_11A, ee);
		printf("\n");
	}

	if (AR5K_EEPROM_HDR_11B(ee->ee_header)) {
		printf("/=========================================================\\\n");
		printf("|          Calibration data for 802.11b operation         |\n");
		dump_calinfo_for_mode(AR5K_EEPROM_MODE_11B, ee);
		dump_rate_calinfo_for_mode(AR5K_EEPROM_MODE_11B, ee);
		if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 1)
				dump_rf5112_power_calinfo_for_mode(AR5K_EEPROM_MODE_11B, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 2)
				dump_rf2413_power_calinfo_for_mode(AR5K_EEPROM_MODE_11B, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_3_0 || AR5K_EEPROM_EEMAP(ee->ee_misc0) == 0)
				dump_rf5111_power_calinfo_for_mode(AR5K_EEPROM_MODE_11B, ee);
		printf("\n");
	}

	if (AR5K_EEPROM_HDR_11G(ee->ee_header)) {
		printf("/=========================================================\\\n");
		printf("|          Calibration data for 802.11g operation         |\n");
		dump_calinfo_for_mode(AR5K_EEPROM_MODE_11G, ee);
		dump_rate_calinfo_for_mode(AR5K_EEPROM_MODE_11G, ee);
		if (ee->ee_version >= AR5K_EEPROM_VERSION_4_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 1)
				dump_rf5112_power_calinfo_for_mode(AR5K_EEPROM_MODE_11G, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_5_0 && AR5K_EEPROM_EEMAP(ee->ee_misc0) == 2)
				dump_rf2413_power_calinfo_for_mode(AR5K_EEPROM_MODE_11G, ee);
		else if (ee->ee_version >= AR5K_EEPROM_VERSION_3_0 || AR5K_EEPROM_EEMAP(ee->ee_misc0) == 0)
				dump_rf5111_power_calinfo_for_mode(AR5K_EEPROM_MODE_11G, ee);
		printf("\n");
	}

	/* print current GPIO settings */
	printf("GPIO registers: CR 0x%08x, DO 0x%08x, DI 0x%08x\n",
	       AR5K_REG_READ(AR5K_GPIOCR), AR5K_REG_READ(AR5K_GPIODO),
	       AR5K_REG_READ(AR5K_GPIODI));

	sta_id0_id1_dump();

	for (i = 0; i < timer_count; i++)
		dump_timers_register();

	if (do_keycache_dump)
		keycache_dump();

	if (keycache_copy_idx > 0)
		keycache_copy(keycache_copy_idx);

	if (do_dump) {
		u_int16_t data;
		FILE *dumpfile = fopen("ath-eeprom-dump.bin", "w");

		printf("\nEEPROM dump (%d bytes)\n", eeprom_size);
		printf("==============================================");
		for (i = 0; i < eeprom_size / 2; i++) {
			error =
			    ath5k_hw_eeprom_read(i, &data);
			if (error) {
				printf("\nUnable to read at %04x\n", i);
				continue;
			}
			if (!(i % 8))
				printf("\n%04x: ", i);
			printf(" %04x", data);
			fwrite(&data, 2, 1, dumpfile);
		}
		printf("\n==============================================\n");
		fclose(dumpfile);
	}

	if (do_restore) {
		u_int16_t data;
		if (argc < 2) {
			fprintf(stderr, "No dumpfile provided\n");
			return -1;
		}
		FILE *dumpfile = fopen(argv[anr + 1], "rb");
		printf("\nEEPROM restore (%d bytes)\n", eeprom_size);
		printf("==============================================");
		for (i = 0; i < eeprom_size / 2; i++) {
			fread(&data, 2, 1, dumpfile);
			error =
			    ath5k_hw_eeprom_write(i, data);
			if (error) {
				printf("\nUnable to write at %04x\n", i);
				continue;
			}
			if (!(i % 8))
				printf("\n%04x: ", i);
			printf(" %04x", data);
		}
		printf("\n==============================================\n");
		fclose(dumpfile);
	}

	if (change_mac){
		printf("MAC address to write: %02x:%02x:%02x:%02x:%02x:%02x\n",
				mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5]);

		if (!force_write) {
			int c;
			printf
			    ("WARNING: The write function may easy brick your device or\n"
			     "violate state regulation on frequency usage.\n"
			     "Proceed on your own risk!\n"
			     "Shall I write the above value(s)? (y/n)\n");
			c = getchar();
			if (c != 'y' && c != 'Y') {
				printf("user abort\n");
				return 0;
			}
		}
		/* Write MAC octets */
		ath5k_hw_eeprom_write(0xa5, (mac_addr[0] | (mac_addr[1] << 8)));
		ath5k_hw_eeprom_write(0xa6, (mac_addr[2] | (mac_addr[3] << 8)));
		ath5k_hw_eeprom_write(0xa7, (mac_addr[4] | (mac_addr[5] << 8)));

		/* And again reversed */
		ath5k_hw_eeprom_write(0x1d, (mac_addr[5] | (mac_addr[4] << 8)));
		ath5k_hw_eeprom_write(0x1e, (mac_addr[3] | (mac_addr[2] << 8)));
		ath5k_hw_eeprom_write(0x1f, (mac_addr[1] | (mac_addr[0] << 8)));
	}

	if (do_write) {
		u_int32_t rcr = AR5K_REG_READ(AR5K_GPIOCR),
		    rdo = AR5K_REG_READ(AR5K_GPIODO);
		u_int32_t old_cr = rcr, old_do = rdo;
		int rc;

		if (mac_revision >= AR5K_SREV_AR5213 && !nr_gpio_set) {
			dbg("new MAC %x (>= AR5213) set GPIO4 to low",
			    mac_revision);
			gpio_set[4].valid = 1;
			gpio_set[4].value = 0;
		}

		/* set GPIOs */
		dbg("old GPIO CR %08x DO %08x DI %08x",
		    rcr, rdo, AR5K_REG_READ(AR5K_GPIODI));

		for (i = 0; i < ARRAY_SIZE(gpio_set); i++) {
			if (gpio_set[i].valid) {
				rcr |= AR5K_GPIOCR_OUT(i);	/* we use mode 3 */
				rcr &= ~AR5K_GPIOCR_INT_SEL(i);
				rdo &= ~(1 << i);
				rdo |= (gpio_set[i].value << i);
			}
		}

		if (rcr != old_cr) {
			dbg("GPIO CR %x -> %x", old_cr, rcr);
			AR5K_REG_WRITE(AR5K_GPIOCR, rcr);
		}
		usleep(5);

		if (rdo != old_do) {
			dbg("GPIO CR %x -> %x", old_do, rdo);
			AR5K_REG_WRITE(AR5K_GPIODO, rdo);
		}

		/* dump current values again if we have written anything */
		if (rcr != old_cr || rdo != old_do)
			dbg("new GPIO CR %08x DO %08x DI %08x",
			    AR5K_REG_READ(AR5K_GPIOCR),
			    AR5K_REG_READ(AR5K_GPIODO),
			    AR5K_REG_READ(AR5K_GPIODI));

		/* let argv[anr] be the first write parameter */
		anr++;

		rc = do_write_pairs(anr, argc, argv);

		/* restore old GPIO settings */
		if (rcr != old_cr) {
			dbg("restoring GPIO CR %x -> %x", rcr, old_cr);
			AR5K_REG_WRITE(AR5K_GPIOCR, old_cr);
		}
		usleep(5);

		if (rdo != old_do) {
			dbg("restoring GPIO CR %x -> %x", rdo, old_do);
			AR5K_REG_WRITE(AR5K_GPIODO, old_do);
		}

		return rc;
	}

	return 0;
}
