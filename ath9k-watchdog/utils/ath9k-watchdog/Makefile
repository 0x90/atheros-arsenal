#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ath9k-watchdog
PKG_VERSION:=1
PKG_RELEASE:=1


include $(INCLUDE_DIR)/package.mk


define Package/ath9k-watchdog
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Watchdog for ath9k-outages
endef

define Package/ath9k-watchdog/description
 If pattern is seen in the logs, wifi reset is executed
endef


define Package/ath9k-watchdog/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) files/ath9k-watchdog $(1)/usr/sbin/
	$(INSTALL_BIN) files/ath9k-watchdog-pm $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) files/urlencode $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) files/ath9k-watchdog.init $(1)/etc/init.d/ath9k-watchdog
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef
$(eval $(call BuildPackage,ath9k-watchdog))
