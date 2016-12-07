/* 
 * JaldiMAC PCI related stuff
 */
 
#include <linux/pci.h>
#include <linux/ath9k_platform.h>
#include <linux/netdevice.h>
#include "jaldi.h"

/* device id table taken from ath9k/pci.c */
//static struct pci_device_id jaldi_pci_id_table[] __devinitdata = {
static DEFINE_PCI_DEVICE_TABLE(jaldi_pci_id_table) = {
	{ PCI_VDEVICE(ATHEROS, 0x0023) }, /* PCI   */
	{ PCI_VDEVICE(ATHEROS, 0x0024) }, /* PCI-E */
	{ PCI_VDEVICE(ATHEROS, 0x0027) }, /* PCI   */
	{ PCI_VDEVICE(ATHEROS, 0x0029) }, /* PCI   */
	{ PCI_VDEVICE(ATHEROS, 0x002A) }, /* PCI-E */
	{ PCI_VDEVICE(ATHEROS, 0x002B) }, /* PCI-E */
	{ PCI_VDEVICE(ATHEROS, 0x002C) }, /* PCI-E 802.11n bonded out */
	{ PCI_VDEVICE(ATHEROS, 0x002D) }, /* PCI   */
	{ PCI_VDEVICE(ATHEROS, 0x002E) }, /* PCI-E */
	{ 0 }
};

/* bus ops */
static void jaldi_pci_read_cachesize(struct jaldi_softc *sc, int *csz) {
	u8 u8tmp;
	DBG_START_MSG;
	pci_read_config_byte(to_pci_dev(sc->dev), PCI_CACHE_LINE_SIZE, &u8tmp);
	*csz = (int)u8tmp;

	/* Apparently cache line size register sometimes is not set, so we check here */
	if (*csz == 0) { *csz = DEFAULT_CACHELINE >> 2; }
}

static bool jaldi_pci_eeprom_read(struct jaldi_softc *sc, u32 off, u16 *data)
{
	u32 val;
	struct ath9k_platform_data *pdata = sc->dev->platform_data;

	if (pdata) {
		if (off >= ARRAY_SIZE(pdata->eeprom_data)) {
			jaldi_print(JALDI_FATAL, "%s: eeprom read failed, offset %08x is out of range.\n", 
					__func__, off);
		}

		*data = pdata->eeprom_data[off];
	} else {
		jaldi_print(JALDI_WARN, "Uh-oh, not using pdata for eeprom_read. EEPROM probably ain't going to work.\n");
		struct jaldi_hw *hw = sc->hw;

		hw->reg_ops->read(hw, AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S));

		if (!jaldi_hw_wait(hw,
			   AR_EEPROM_STATUS_DATA,
			   AR_EEPROM_STATUS_DATA_BUSY | 
			   AR_EEPROM_STATUS_DATA_PROT_ACCESS, 0,
			   JALDI_WAIT_TIMEOUT))
			   { return false; }

		/* The value we have specified via the passed offset is placed
		 * in this register. We save save the last two bytes of the value
		 * we read, and no shift occurs (shift is 0). */
		val = hw->reg_ops->read(hw, AR_EEPROM_STATUS_DATA);	 
		*data = MS(val, AR_EEPROM_STATUS_DATA_VAL);
	//	jaldi_print(JALDI_DEBUG, "pci_eeprom_read reg_off: %lx val: %8x data: %8x\n", 
	//		(unsigned long)(AR5416_EEPROM_OFFSET + (off << AR5416_EEPROM_S)), val, *data);
	}
	return true;
}

static const struct jaldi_bus_ops jaldi_pci_bus_ops = {
	.type	= JALDI_PCI,
	.read_cachesize = jaldi_pci_read_cachesize,
	.eeprom_read = jaldi_pci_eeprom_read,
};

static int jaldi_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	void __iomem *mem;
	struct jaldi_softc *sc;
	struct net_device *jaldi_dev;	
	u8 csz;
	u16 subsysid;
	u32 val;
	int ret = 0;

	DBG_START_MSG;
	if (pci_enable_device(pdev)) {
		return -EIO;
	}
	
	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret) {
		printk(KERN_ERR "jaldi: 32-bit DMA not available\n");
		goto err_dma;
	}
	
	ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret) {
		printk(KERN_ERR "jaldi: 32-bit DMA consistent "
			"DMA enable failed\n");
		goto err_dma;
	}

	/*
	 * Cache line size is used to size and align various
	 * structures used to communicate with the hardware.
	 */
	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &csz);
	if (csz == 0) {
		/*
		 * Linux 2.4.18 (at least) writes the cache line size
		 * register as a 16-bit wide register which is wrong.
		 * We must have this setup properly for rx buffer
		 * DMA to work so force a reasonable value here if it
		 * comes up zero.
		 */
		csz = L1_CACHE_BYTES / sizeof(u32);
		pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, csz);
	}
	/*
	 * The default setting of latency timer yields poor results,
	 * set it to the value used by other systems. It may be worth
	 * tweaking this setting more.
	 */
	pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0xa8);

	pci_set_master(pdev);
	
	/*
	 * Disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state.
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);
		
	ret = pci_request_region(pdev, 0, "jaldi");
	if (ret) {
		dev_err(&pdev->dev, "PCI memory region reserve error\n");
		ret = -ENODEV;
		goto err_region;
	}
	
	mem = pci_iomap(pdev, 0, 0);
	if (!mem) {
		jaldi_print(JALDI_FATAL, "PCI memory map error\n") ;
		ret = -EIO;
		goto err_iomap;
	}
	
	/* allocate and register net_dev, also allocs softc */
	jaldi_dev = jaldi_init_netdev();

	if (jaldi_dev < 0) {
		jaldi_print(JALDI_FATAL, "could not init netdev\n");
		goto err_alloc_hw;
	}

	sc = netdev_priv(jaldi_dev);

	if (!sc) {
		dev_err(&pdev->dev, "No memory for jaldi_softc\n");
		ret = -ENOMEM;
		goto err_no_softc;
	}

	
	jaldi_print(JALDI_DEBUG, "jnd: %p jnd_priv: %p\n",
			jaldi_dev, netdev_priv(jaldi_dev)); 
	jaldi_print(JALDI_DEBUG, "sc->dev: %p sc: %p\n",
			sc->net_dev, sc);
	pci_set_drvdata(pdev, sc);
	sc->dev = &pdev->dev;
	sc->mem = mem;


	sc->sc_flags |= SC_OP_INVALID; // irq is not ready, don't try to use the device yet.

	ret = request_irq(pdev->irq, jaldi_isr, IRQF_SHARED, "jaldi", sc);

	sc->irq = pdev->irq; // Keep track of IRQ in softc

	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_irq;
	}

	jaldi_print(JALDI_INFO, "sc->mem: %lx irq:%d\n", (unsigned long)mem, pdev->irq);
	jaldi_print(JALDI_INFO, "irq:%d\n", ((struct jaldi_softc *)netdev_priv(sc->net_dev))->irq);

	pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &subsysid);
	ret = jaldi_init_device(id->device, sc, subsysid, &jaldi_pci_bus_ops); 
	if (ret) {
		jaldi_print(JALDI_FATAL, "Failed to initialize device.\n");
		goto err_init;
	}

	/* alright let's do this */
	ret = jaldi_start_netdev(sc);
	if (ret) {
		jaldi_print(JALDI_FATAL, "Failed to register netdev\n");
		goto err_start;
	}

	jaldi_print(JALDI_INFO, "pci probe done\n");

	return 0;

err_start:
	jaldi_deinit_device(sc);	
err_init:
	free_irq(sc->irq, sc);
err_irq:
	/* nothing, init_netdev cleans itself up now */
err_no_softc:
	free_netdev(jaldi_dev);
err_alloc_hw:
	pci_iounmap(pdev, mem);
err_iomap:
	pci_release_region(pdev, 0);
err_region:
	/* Nothing */
err_dma:
	pci_disable_device(pdev);
	return ret;
}

static void jaldi_pci_remove(struct pci_dev *pdev)
{
	struct jaldi_softc *sc; 
	void __iomem *mem;

	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	sc = pci_get_drvdata(pdev);
	mem = sc->mem;

	unregister_netdev(sc->net_dev);

	jaldi_deinit_device(sc);
	free_irq(sc->irq, sc);
	
	free_netdev(sc->net_dev);

	pci_iounmap(pdev, mem);
	pci_disable_device(pdev);
	pci_release_region(pdev, 0);
}

MODULE_DEVICE_TABLE(pci, jaldi_pci_id_table);

static struct pci_driver jaldi_pci_driver = {
	.name       = "jaldi",
	.id_table   = jaldi_pci_id_table,
	.probe      = jaldi_pci_probe,
	.remove     = jaldi_pci_remove,
};

int jaldi_pci_init(void)
{
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	return pci_register_driver(&jaldi_pci_driver);
}

void jaldi_pci_exit(void)
{
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	pci_unregister_driver(&jaldi_pci_driver);
}
