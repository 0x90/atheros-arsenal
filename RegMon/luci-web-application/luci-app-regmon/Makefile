#
# Copyright (C) 2016 LEDE project (Denis Roeper)
#
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-regmon
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_MAINTAINER:=Denis Roeper <denis.roeper@posteo.de>
PKG_LICENSE:=GPL-3.0+
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-regmon
	SECTION:=LuCI
	CATEGORY:=LuCI
	SUBMENU:=3. Applications
	TITLE:=LuCI Support for Regmon
	DEPENDS:=+lua +luafilesystem +luci-base +rrdtool1 +collectd +collectd-mod-tail-csv +luci-app-statistics
	MAINTAINER:=Denis Roeper <denis.roeper@posteo.de>
	PKGARCH:=all
endef
# depends an luci-app-statistics for /etc/collectd.conf generation and further integration

define Package/luci-app-regmon/description
Monitor minstrel power rate control
endef

define Build/Compile
endef

define Package/luci-app-regmon/install
		$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/regmon
		$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/regmon
		$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/regmon
		$(INSTALL_DIR) $(1)/etc/init.d
		$(INSTALL_DIR) $(1)/etc/config
		$(INSTALL_DIR) $(1)/etc/uci-defaults
		$(INSTALL_DIR) $(1)/usr/bin
		$(INSTALL_BIN) ./root/etc/init.d/luci-app-regmon $(1)/etc/init.d/luci-app-regmon
		$(INSTALL_DATA) ./root/etc/config/regmon $(1)/etc/config/regmon
		$(INSTALL_BIN) ./root/usr/bin/regmon-genconfig $(1)/usr/bin/regmon-genconfig
		$(INSTALL_DATA) ./luasrc/model/cbi/regmon/regmon_cbi.lua $(1)/usr/lib/lua/luci/model/cbi/regmon/regmon_cbi.lua
		$(INSTALL_DATA) ./luasrc/view/regmon/graph.htm $(1)/usr/lib/lua/luci/view/regmon/graph.htm
		$(INSTALL_DATA) ./luasrc/controller/regmon/regmon.lua $(1)/usr/lib/lua/luci/controller/regmon/regmon.lua
		$(INSTALL_DATA) ./root/etc/uci-defaults/41_regmon $(1)/etc/uci-defaults/41_regmon
endef

$(eval $(call BuildPackage,luci-app-regmon))
