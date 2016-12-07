#
# Makefile for nfc devices
#

obj-$(CPTCFG_NFC_PN544)		+= pn544/
obj-$(CPTCFG_NFC_MICROREAD)	+= microread/
obj-$(CPTCFG_NFC_PN533)		+= pn533.o
obj-$(CPTCFG_NFC_WILINK)	+= nfcwilink.o
obj-$(CPTCFG_NFC_MEI_PHY)	+= mei_phy.o
obj-$(CPTCFG_NFC_SIM)		+= nfcsim.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
