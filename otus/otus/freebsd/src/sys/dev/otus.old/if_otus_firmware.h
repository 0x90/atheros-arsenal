#ifndef	__IF_OTUS_FIRMWARE_H__
#define	__IF_OTUS_FIRMWARE_H__

/* Shared firmware descriptor header from Linux carl9170 */
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef	uint16_t	__le16;
typedef	uint32_t	__le32;

#define	BIT(x)		(1 << (x))

#define	le32_to_cpu(c)	le32toh(c)
#define	le16_to_cpu(c)	le16toh(c)

#include "fwdesc.h"

/*
 * This represents the carl9170 firmware, information
 * and capability block.
 *
 * It's populated as part of the firmware load process.
 */
struct carl9170_firmware_info {
	const struct carl9170fw_desc_head *desc;
	const struct firmware *fw;
	unsigned int offset;
	unsigned int address;
	unsigned int cmd_bufs;
	unsigned int api_version;
	unsigned int vif_num;
	unsigned int err_counter;
	unsigned int bug_counter;
	uint32_t beacon_addr;
	unsigned int beacon_max_len;
	bool rx_stream;
	bool tx_stream;
	bool rx_filter;
	bool hw_counters;
	unsigned int mem_blocks;
	unsigned int mem_block_size;
	unsigned int rx_size;
	unsigned int tx_seq_table;
	bool ba_filter;
	bool disable_offload_fw;
};

extern	int otus_firmware_load(struct carl9170_firmware_info *);
extern	void otus_firmware_cleanup(struct carl9170_firmware_info *);

#endif	/* __IF_OTUS_FIRMWARE__ */
