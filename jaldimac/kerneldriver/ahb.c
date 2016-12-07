#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#include <linux/netdevice.h>
#include "jaldi.h"

static void jaldi_ahb_read_cachesize(struct jaldi_softc *sc, int *csz) {
	*csz = L1_CACHE_BYTES >> 2;
}


static bool jaldi_ahb_eeprom_read(struct jaldi_softc *sc, u32 off, u16 *data)
{
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	struct platform_device *pdev = to_platform_device(sc->dev);
	struct ath9k_platform_data *pdata; // making use of linux/ath9k_platform

	pdata = (struct ath9k_platform_data *) pdev->dev.platform_data;
	if (off >= (ARRAY_SIZE(pdata->eeprom_data))) {
		jaldi_print(JALDI_FATAL,	
			  "%s: flash read failed, offset %08x "
			  "is out of range\n",
			  __func__, off);
		return false;
	}

	*data = pdata->eeprom_data[off];
	return true;
}

static struct jaldi_bus_ops jaldi_ahb_bus_ops = {
	.type = JALDI_AHB,
	.read_cachesize = jaldi_ahb_read_cachesize,
	.eeprom_read = jaldi_ahb_eeprom_read,
};

static int jaldi_ahb_probe(struct platform_device *pdev) {
	void __iomem *mem;
	struct jaldi_softc *sc;
	struct net_device *jaldi_dev;
	struct resource *res;
	int irq;
	int ret = 0;
	struct jaldi_hw *hw;
	//char hw_name[64];

	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data specified\n");
		ret = -EINVAL;
		goto err_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no memory resource found\n");
		ret = -ENXIO;
		goto err_out;
	}

	mem = ioremap_nocache(res->start, res->end - res->start + 1);
	if (mem == NULL) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no IRQ resource found\n");
		ret = -ENXIO;
		goto err_iounmap;
	}

	irq = res->start;

	/* allocate and register net_dev, also allocs softc */
	jaldi_dev = jaldi_init_netdev();

	if (jaldi_dev < 0) {
		jaldi_print(JALDI_FATAL, "could not init netdev\n");
		goto err_iounmap;
	}

	sc = netdev_priv(jaldi_dev);

	if (!sc) {
		dev_err(&pdev->dev, "no memory for jaldi_softc\n");
		ret = -ENOMEM;
		goto err_no_softc;
	}

	platform_set_drvdata(pdev,sc);
	sc->dev = &pdev->dev;
	sc->mem = mem;
	sc->irq = irq;

	// irq not ready, this is cleared during init (ath9k_start?)
	sc->sc_flags |= SC_OP_INVALID;

	ret = request_irq(irq, jaldi_isr, IRQF_SHARED, "jaldi", sc);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_free_hw;
	}

	ret = jaldi_init_device(AR5416_AR9100_DEVID, sc, 0x0, &jaldi_ahb_bus_ops); 
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize device\n");
		goto err_irq;
	}

	hw = sc->hw; // pointer to jaldi_hw struct
	//jaldi_hw_name(hw, hw_name, sizeof(hw_name)); // TODO
	//printk(KERN_INFO "jaldi: %s mem=0x%1x, irq=%d\n", hw_name, (unsigned long)mem, irq);


	/* alright let's do this */
	ret = jaldi_start_netdev(sc);
	if (ret) {
		jaldi_print(JALDI_FATAL, "Failed to register netdev\n");
		goto err_start;
	}

	return 0;

err_start:
	jaldi_deinit_device(sc);
err_irq:
	free_irq(irq, sc);
err_free_hw:
	platform_set_drvdata(pdev, NULL);
err_no_softc:
	free_netdev(jaldi_dev);
err_iounmap:
	iounmap(mem);
err_out:
	return ret;
};

static int jaldi_ahb_remove(struct platform_device *pdev) {
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	struct jaldi_softc *sc = platform_get_drvdata(pdev);

	if (sc) {
		void __iomem *mem = sc->mem;

		unregister_netdev(sc->net_dev);

		jaldi_deinit_device(sc); 
		free_irq(sc->irq, sc);

		free_netdev(sc->net_dev);

		iounmap(mem);
		platform_set_drvdata(pdev,NULL);
	}

	return 0;
}

// standard linux platform_driver stuff
static struct platform_driver jaldi_ahb_driver = {
	.probe 		= jaldi_ahb_probe,
	.remove		= jaldi_ahb_remove,
	.driver		= {
		.name	= "jaldi",
		.owner	= THIS_MODULE,
	},
};

int jaldi_ahb_init(void) {
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	return platform_driver_register(&jaldi_ahb_driver);
}

void jaldi_ahb_exit(void) {
	jaldi_print(JALDI_DEBUG,"Entering '%s'\n", __FUNCTION__);	
	platform_driver_unregister(&jaldi_ahb_driver);
}
