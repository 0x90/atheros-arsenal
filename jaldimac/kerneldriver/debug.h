/*
 * Jaldi Debugging
 * 
 * Originally based on ath's debug.
 */

#ifndef JALDI_DEBUG_H
#define JALDI_DEBUG_H

#include <linux/debugfs.h>
#include "jaldi.h"

#define JALDI_DEBUG_ON 1
#define WHERESTR "[file %s, line %d]: "
#define WHEREARG __FILE__, __LINE__
#define DBG_START_MSG jaldi_print(JALDI_DEBUG,"---> Entering '%s' [file %s, line %d]\n", __FUNCTION__, WHEREARG)
#define DBG_END_MSG jaldi_print(JALDI_DEBUG,"<--- Exiting '%s' [file %s, line %d]\n", __FUNCTION__, WHEREARG)
#define	OHAI jaldi_print(JALDI_DEBUG,"OHAI! %s [file %s, line %d]\n", __FUNCTION__, WHEREARG)

enum JALDI_DEBUG_LEVEL {
	JALDI_FATAL = 0,
	JALDI_WARN = 1,
	JALDI_ALERT = 2,
	JALDI_INFO = 3,
	JALDI_DEBUG = 4,
};

struct jaldi_debug {
	struct dentry *debugfs;
	u32 regidx;
	struct timespec ts;
	int actual_tx_idx;
	int intended_tx_idx;
	s64 actual_tx_times[2048];
	s64 intended_tx_times[2048];
};


static struct dentry *jaldi_debugfs_root;

int jaldi_init_debug(struct jaldi_hw *hw);
void jaldi_exit_debug(struct jaldi_hw *hw);
int jaldi_debug_create_root(void);
void jaldi_debug_remove_root(void);

void jaldi_print_skb(struct sk_buff *skb);

void jaldi_print(int level, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));

#define JALDI_DEBUG_LEVEL JALDI_FATAL

#endif /* JALDI_DEBUG_H */
