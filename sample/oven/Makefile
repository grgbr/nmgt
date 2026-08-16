# Inspired by Sysrepo tutorials found here:
# * https://plugindev.sysrepo.org/tutorial/03-first-plugin#oven-plugin
# * https://netopeer.liberouter.org/doc/sysrepo/master/html/d3/d9c/srpd_example.html

CFLAGS  := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --cflags sysrepo libyang) \
           $(EXTRA_CFLAGS)
LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --libs sysrepo libyang) \
           $(EXTRA_LDFLAGS)

.PHONY: all
all: build

.PHONY: install
install: $(DESTDIR)$(PREFIX)/sbin/ovend \
         $(DESTDIR)$(PREFIX)/bin/oven-ctl \
         $(DESTDIR)$(PREFIX)/bin/oven-wait
$(DESTDIR)$(PREFIX)/sbin/ovend: $(BUILDDIR)/ovend oven.yang oven-cly.yang
	# Watch out ! Sysrepo /dev/shm entries must be cleaned up before
	# oven.yang install (see <topdir>/notes.txt)
	$(PREFIX)/sbin/sysrepoctl -u oven-cly.yang 2>/dev/null || true
	$(PREFIX)/sbin/sysrepoctl -u oven.yang 2>/dev/null || true
	$(PREFIX)/sbin/sysrepoctl -i oven.yang
	$(PREFIX)/sbin/sysrepoctl -i oven-cly.yang
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/oven-wait: $(BUILDDIR)/oven-wait oven.yang
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/oven-ctl: ctl.sh
	sed 's;@@__PREFIX__@@;$(PREFIX);' $(<) > $(@)
	chmod 755 $(@)

.PHONY: build
build: $(BUILDDIR)/ovend $(BUILDDIR)/oven-wait
$(BUILDDIR)/ovend: ovend.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
$(BUILDDIR)/oven-wait: wait.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)

$(BUILDDIR)/:
	$(MKDIR) -p $(@)

.PHONY: dist
dist:

.PHONY: distclean
distclean:
