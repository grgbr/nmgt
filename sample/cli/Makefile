CFLAGS  := -Wall -Wextra \
           -fsanitize=address \
           $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --cflags sysrepo libyang) \
           $(EXTRA_CFLAGS)
LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --libs sysrepo libyang) \
           $(EXTRA_LDFLAGS)

.PHONY: all
all: build

.PHONY: install
install: $(DESTDIR)$(PREFIX)/bin/nmgmt-schema \
         $(DESTDIR)$(PREFIX)/bin/nmgmt-data \
         $(DESTDIR)$(PREFIX)/bin/nmgmt-cli
$(DESTDIR)$(PREFIX)/bin/nmgmt-schema: $(BUILDDIR)/nmgmt-schema
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/nmgmt-data: $(BUILDDIR)/nmgmt-data
	$(INSTALL) --mode=755 -D $(<) $(@)
$(DESTDIR)$(PREFIX)/bin/nmgmt-cli: $(BUILDDIR)/nmgmt-cli
	$(INSTALL) --mode=755 -D $(<) $(@)

.PHONY: build
build: $(BUILDDIR)/nmgmt-schema \
	$(BUILDDIR)/nmgmt-data \
	$(BUILDDIR)/nmgmt-cli
$(BUILDDIR)/nmgmt-schema: schema.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
$(BUILDDIR)/nmgmt-data: data.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)
$(BUILDDIR)/nmgmt-cli: common.c yang.c cli.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(^) $(LDFLAGS)

$(BUILDDIR)/:
	$(MKDIR) -p $(@)

.PHONY: dist
dist:

.PHONY: distclean
distclean:
