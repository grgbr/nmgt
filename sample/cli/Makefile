CFLAGS  := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --cflags sysrepo libyang) \
           $(EXTRA_CFLAGS)
LDFLAGS := $(shell env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                       $(PKG_CONFIG) --libs sysrepo libyang) \
           $(EXTRA_LDFLAGS)

.PHONY: all
all: build

.PHONY: install
install: $(DESTDIR)$(PREFIX)/bin/nmgmt-schema
$(DESTDIR)$(PREFIX)/bin/nmgmt-schema: $(BUILDDIR)/nmgmt-schema
	$(INSTALL) --mode=755 -D $(<) $(@)

.PHONY: build
build: $(BUILDDIR)/nmgmt-schema
$(BUILDDIR)/nmgmt-schema: schema.c | $(BUILDDIR)/
	$(CC) -o $(@) $(CFLAGS) $(<) $(LDFLAGS)

$(BUILDDIR)/:
	$(MKDIR) -p $(@)
