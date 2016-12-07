/*
 * Jaldi debugging
 */

#include "jaldi.h"

void jaldi_print(int level, const char *fmt, ...)
{
	va_list args;

	if (JALDI_DEBUG_ON && level <= JALDI_DEBUG_LEVEL) {
		va_start(args, fmt);
		printk(KERN_EMERG "jaldi [%d]: ", level);
		vprintk(fmt, args);
		va_end(args);
	}
}

void jaldi_print_skb(struct sk_buff *skb) 
{
	int i;
	char str[3500]; 
	char *pStr = str;
	
	for (i=0; i < skb->len && pStr < str+3900; i++) {
		sprintf(pStr,"%02x", skb->data[i]);
		pStr+=2;

		if( (i+1)%8 == 0){
			sprintf(pStr," ");
			pStr++;
		}
		
		if( (i+1)%32 == 0){
			sprintf(pStr,"\n");
			pStr++;
		}
	}

	jaldi_print(JALDI_DEBUG, "%s\n", str);
}

static int jaldi_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t read_file_profile(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct jaldi_softc *sc = file->private_data;
	char *buf; 
	unsigned int len = 0, size = 8192;
	ssize_t retval = 0;
	int i,j;
	
	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL) 
		return -ENOMEM;
	for(i=0; i < 2048 && i < sc->debug.actual_tx_idx-1; i++) {
	//	printk(KERN_EMERG "loop da loop i:%d idx:%d\n", i, sc->debug.actual_tx_idx);
		len += snprintf(buf + len, size - len, "%s %d: %lld\n",
				"Actual TX Start", i, sc->debug.actual_tx_times[i]); 
	}

	for(i=0; i < 2048 && i < sc->debug.intended_tx_idx-1; i++) {
		len += snprintf(buf + len, size - len, "%s %d: %lld\n",
				"Intended TX Start", i, sc->debug.intended_tx_times[i]);
	}
	if (len > size)
		len = size;

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return retval;
}

static const struct file_operations fops_profiling = {
	.read = read_file_profile,
	.open = jaldi_debugfs_open,
	.owner = THIS_MODULE
};

int jaldi_init_debug(struct jaldi_hw *hw)
{
	struct jaldi_softc *sc = hw->sc;

	if (!jaldi_debugfs_root)
		return -ENOENT;

	memset(&sc->debug, 0, sizeof(struct jaldi_debug));
	OHAI;
	sc->debug.debugfs = debugfs_create_dir("jaldi",
						      jaldi_debugfs_root);
	OHAI;
	if (!sc->debug.debugfs)
		return -ENOMEM;

	OHAI;
	if (!debugfs_create_file("profile", S_IRUSR,
			sc->debug.debugfs, sc, &fops_profiling))
		goto err;
	OHAI;

	sc->debug.regidx = 0;
	return 0;
err:
	jaldi_exit_debug(hw);
	return -ENOMEM;
}

void jaldi_exit_debug(struct jaldi_hw *hw)
{
	struct jaldi_softc *sc = (struct jaldi_softc *) hw->sc;

	debugfs_remove_recursive(sc->debug.debugfs);
}
EXPORT_SYMBOL(jaldi_exit_debug);

int jaldi_debug_create_root(void)
{
	jaldi_debugfs_root = debugfs_create_dir("jaldi", NULL);
	if (!jaldi_debugfs_root)
		return -ENOENT;

	return 0;
}

void jaldi_debug_remove_root(void)
{
	debugfs_remove(jaldi_debugfs_root);
	jaldi_print(JALDI_INFO, "removed debugfs\n");
	jaldi_debugfs_root = NULL;
}
