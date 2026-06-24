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
install: $(DESTDIR)$(PREFIX)/sbin/netconf-server
$(DESTDIR)$(PREFIX)/sbin/netconf-server: netconf-server.sh ietf-netconf-server-deviations.yang
	$(PREFIX)/sbin/sysrepoctl -i ietf-netconf-server-deviations.yang -v3
	sed 's;@@__PREFIX__@@;$(PREFIX);' $(<) > $(@)
	chmod 755 $(@)

.PHONY: build
build:

