# Inspired by Sysrepo tutorials found here:
# * https://plugindev.sysrepo.org/tutorial/03-first-plugin#oven-plugin
# * https://netopeer.liberouter.org/doc/sysrepo/master/html/d3/d9c/srpd_example.html

CFLAGS  := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --cflags sysrepo libyang libnghttp2 libevent) \
           $(EXTRA_CFLAGS)
LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --libs sysrepo libyang libnghttp2 libevent) \
           $(EXTRA_LDFLAGS)

.PHONY: all
all: build

.PHONY: install
install: $(DESTDIR)$(PREFIX)/sbin/restconfd \
         $(DESTDIR)$(PREFIX)/sbin/start-http \
         $(DESTDIR)$(PREFIX)/www/index.html
$(DESTDIR)$(PREFIX)/sbin/restconfd: $(BUILDDIR)/restconfd
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/sbin/start-http: start-http.sh
	sed 's;@@__PREFIX__@@;$(PREFIX);' $(<) > $(@)
	chmod 755 $(@)
$(DESTDIR)$(PREFIX)/www/index.html: index.html
	$(INSTALL) --mode=644 -D $(<) $(@)

.PHONY: build
build: $(BUILDDIR)/restconfd
$(BUILDDIR)/restconfd: restconfd.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)

$(BUILDDIR)/:
	$(MKDIR) -p $(@)

.PHONY: dist
dist:

.PHONY: distclean
distclean:
