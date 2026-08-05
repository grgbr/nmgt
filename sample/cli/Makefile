CFLAGS  := -Wall -Wextra \
           -fsanitize=address \
           -fsanitize=pointer-compare \
           -fsanitize=pointer-subtract \
           -fsanitize=leak \
           -fsanitize=undefined \
           $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --cflags sysrepo libyang smartcols readline tinfo) \
           $(EXTRA_CFLAGS)
LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --libs sysrepo libyang smartcols readline tinfo) \
           $(EXTRA_LDFLAGS)
plugin_ldflags := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                              $(PKG_CONFIG) --libs libyang) \
                  $(EXTRA_LDFLAGS)

.PHONY: all
all: build

.PHONY: install
install: $(DESTDIR)$(PREFIX)/bin/nmgmt-schema \
         $(DESTDIR)$(PREFIX)/bin/nmgmt-data \
         $(DESTDIR)$(PREFIX)/bin/nmgmt-cli \
         $(DESTDIR)$(PREFIX)/lib/libyang/extensions/cli_lyplg.so
$(DESTDIR)$(PREFIX)/bin/nmgmt-schema: $(BUILDDIR)/nmgmt-schema
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/nmgmt-data: $(BUILDDIR)/nmgmt-data
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/nmgmt-cli: $(BUILDDIR)/nmgmt-cli
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/lib/libyang/extensions/cli_lyplg.so: \
	$(BUILDDIR)/cli_lyplg.so cli-extensions.yang
	$(INSTALL) --mode=755 -D $(<) $(@)
	$(PREFIX)/sbin/sysrepoctl -i cli-extensions.yang

.PHONY: build
build: $(BUILDDIR)/nmgmt-schema \
	$(BUILDDIR)/nmgmt-data \
	$(BUILDDIR)/nmgmt-cli \
	$(BUILDDIR)/cli_lyplg.so
$(BUILDDIR)/nmgmt-schema: old/schema.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
$(BUILDDIR)/nmgmt-data: old/data.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
$(BUILDDIR)/nmgmt-cli: cli.c build.c config.c status.c lyd_table.c list.c cd.c pwd.c find.c xpath.c schema.c quit.c table.c shell.c match.c expr.c yang.c dir.c cmd.c path.c arg.c node.c common.c | $(BUILDDIR)/
	$(CC) -dM -o $(@) $(CFLAGS) $(^) $(LDFLAGS)
$(BUILDDIR)/cli_lyplg.so: cli_lyplg.c | $(BUILDDIR)/
	$(CC) -dM -o $(@) -shared -Bsymbolic -fpic $(CFLAGS) $(^) \
	      $(plugin_ldflags)

$(BUILDDIR)/:
	$(MKDIR) -p $(@)

.PHONY: dist
dist:

.PHONY: distclean
distclean:
