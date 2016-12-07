# Copyright (c) 2016 Aurelien Jarno <aurelien@aurel32.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This makefile allows to quickly build minimalistic cross-compilers
# for various targets. They only support the C language and do not
# support a C library.
#
# Most of the code is borrowed from the openbios Debian source package
#

# Support multiple makes at once based on number of processors
ifneq (,$(filter parallel=%,$(DEB_BUILD_OPTIONS)))
njobs = -j $(patsubst parallel=%,%,$(filter parallel=%,$(DEB_BUILD_OPTIONS)))
endif

gcc_major_version     = 6

# GCC does not build with some hardening options and anyway we do not
# ship the resulting binaries in the package
toolchain_build_flags = CFLAGS="-g -O2" CXXFLAGS="-g -O2" CPPFLAGS="" LDFLAGS=""

target                = $(filter-out %_,$(subst _,_ ,$@))
toolchain_dir         = $(CURDIR)/cross-toolchain
stamp                 = $(toolchain_dir)/stamp-
binutils_src_dir      = /usr/src/binutils
binutils_unpack_dir   = $(toolchain_dir)/binutils-source
binutils_build_dir    = $(toolchain_dir)/binutils-$(target)
gcc_src_dir           = /usr/src/gcc-$(gcc_major_version)
gcc_unpack_dir        = $(toolchain_dir)/gcc-source
gcc_build_dir         = $(toolchain_dir)/gcc-$(target)

# Use only xtensa-specific patches on top of upstream version
binutils_patch        = local/patches/binutils.patch
# Only needed for binutils 2.27 and earlier
binutils_2.27_fix_patch = local/patches/binutils-2.27_fixup.patch
gcc_patch             = local/patches/gcc.patch

$(stamp)binutils_unpack:
	mkdir -p $(binutils_unpack_dir)
	cd $(binutils_unpack_dir) && \
		tar --strip-components=1 -xf $(binutils_src_dir)/binutils-*.tar.* && \
		patch -p1 < $(CURDIR)/$(binutils_patch) && \
		{ patch -p1 < $(CURDIR)/$(binutils_2.27_fix_patch) || true; }
	touch $@

$(stamp)binutils_%: $(stamp)binutils_unpack
	mkdir -p $(binutils_build_dir)
	cd $(binutils_build_dir) && \
		$(binutils_unpack_dir)/configure \
			--build=$(DEB_BUILD_GNU_TYPE) \
			--host=$(DEB_HOST_GNU_TYPE) \
			--target=$(target) \
			--prefix=$(toolchain_dir) \
			--disable-nls \
			--disable-plugins \
			$(toolchain_build_flags)
	$(MAKE) $(njobs) -C $(binutils_build_dir) all
	$(MAKE) $(njobs) -C $(binutils_build_dir) install
	touch $@

$(stamp)gcc_unpack:
	mkdir -p $(gcc_unpack_dir)
	cd $(gcc_unpack_dir) && \
		tar --strip-components=1 -xf $(gcc_src_dir)/gcc-*.tar.* && \
		patch -p1 < $(CURDIR)/$(gcc_patch) && \
		patch -p2 < $(gcc_src_dir)/patches/gcc-gfdl-build.diff
	touch $@

$(stamp)gcc_%: $(stamp)binutils_% $(stamp)gcc_unpack
	mkdir -p $(gcc_build_dir)
	cd $(gcc_build_dir) && \
		$(gcc_unpack_dir)/configure \
			--build=$(DEB_BUILD_GNU_TYPE) \
			--host=$(DEB_HOST_GNU_TYPE) \
			--target=$(target) \
			--prefix=$(toolchain_dir) \
			--enable-languages="c" \
			--disable-multilib \
			--disable-libffi \
			--disable-libgomp \
			--disable-libmudflap \
			--disable-libquadmath \
			--disable-libssp \
			--disable-nls \
			--disable-shared \
			--disable-threads \
			--disable-tls \
			--disable-plugins \
			--with-gnu-as \
			--with-gnu-ld \
			--with-headers=no \
			--without-newlib \
			$(toolchain_build_flags)
	$(MAKE) $(njobs) -C $(gcc_build_dir) all
	$(MAKE) $(njobs) -C $(gcc_build_dir) install
	touch $@
