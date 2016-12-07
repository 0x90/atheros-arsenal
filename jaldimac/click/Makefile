# ========================================
# Configuration
# ========================================

TESTS=test-encap test-demux test-gate test-sched
CONFIGURATIONS=master station-1 station-2 station-3 station-4 $(TESTS)
ELEMENTS_CONFIGURATION=--enable-userlevel
CHECK?=no

# ========================================
# Internal and derived variables
# ========================================

CONFIGDIR=configs
ELEMENTDIR=elements
BUILDDIR=build
DISTDIR=dist
CONFIGURATION_FILES=$(addprefix $(BUILDDIR)/,$(addsuffix .click,$(CONFIGURATIONS)))
CONFIGURATION_DOT=$(addprefix $(BUILDDIR)/,$(addsuffix .dot,$(CONFIGURATIONS)))
CONFIGURATION_GRAPH=$(addprefix $(DISTDIR)/,$(addsuffix .pdf,$(CONFIGURATIONS)))
PACKAGES=$(addprefix $(DISTDIR)/,$(addsuffix .clickpkg,$(CONFIGURATIONS)))
JALDI_PACKAGE_BINARY=$(ELEMENTDIR)/jaldi.uo
JALDI_ELEMENT_MAP=$(ELEMENTDIR)/elementmap-jaldi.xml

# ========================================
# Metarules
# ========================================

.PHONY: all dist packages pretty configurations elements clean
.SILENT:

# ========================================
# Targets
# ========================================

all: dist

dist: packages pretty

packages: $(PACKAGES)

pretty: $(CONFIGURATION_DOT) $(CONFIGURATION_GRAPH)

configurations: $(CONFIGURATION_FILES)

elements: $(JALDI_PACKAGE_BINARY) $(JALDI_ELEMENT_MAP)

clean:
	cd $(BUILDDIR); rm -f *
	cd $(DISTDIR); rm -f *
	cd $(ELEMENTDIR) && (make clean 2> /dev/null || true)
	cd $(ELEMENTDIR) && rm -f Makefile configure config.status config.log
	cd $(ELEMENTDIR) && rm -rf autom4te.cache

# ========================================
# Internal targets
# ========================================

%.click: ../$(CONFIGDIR)/%.slick
	gcc -E -x c $< -o $@

%.dot: %.clickpkg
	click-pretty --dot $< > $@

%.pdf: %.dot
	dot -Tpdf -o$@ $<

%.clickpkg: ../$(BUILDDIR)/%.click configurations elements
	cp $< $(BUILDDIR)/config
	ar rcs $@ $(BUILDDIR)/config $(JALDI_PACKAGE_BINARY) $(JALDI_ELEMENT_MAP)
	#click -q $@ || (rm -f $@ && rm -f $< && false)
ifeq ($(CHECK),no)
else
	click -q $@ || (echo "Click configuration failed sanity check; if the problem is real, remember to run 'make clean' before running 'make' again.\n" && true)
endif

$(ELEMENTDIR)/Makefile:
	cd $(ELEMENTDIR) && autoconf && ./configure $(ELEMENTS_CONFIGURATION)

$(JALDI_PACKAGE_BINARY): $(ELEMENTDIR)/Makefile
	cd $(ELEMENTDIR) && make

$(JALDI_ELEMENT_MAP):
	click-mkelemmap $(ELEMENTDIR)/*.hh > $@
