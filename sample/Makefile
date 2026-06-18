# $1: pathname to source directory
# $2: arbitrary make arguments
define make_cmd
$(MAKE) --directory='$(strip $(1))' \
        DESTDIR='$(DESTDIR)' \
        PREFIX='$(PREFIX)' \
        BUILDDIR='$(BUILDDIR)/$(strip $(1))' \
        PKG_CONFIG='$(PKG_CONFIG)' \
        PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
        EXTRA_CFLAGS='$(EXTRA_CFLAGS)' \
        EXTRA_LDFLAGS='$(EXTRA_LDFLAGS)' \
        MKDIR='$(MKDIR)' \
        INSTALL='$(INSTALL)' \
        $(2)
endef

subdirs := oven

.PHONY: all
all: build

.PHONY: build
build: $(addprefix build-,$(subdirs))

.PHONY: $(addprefix build-,$(subdirs))
$(addprefix build-,$(subdirs)):
	$(call make_cmd,$(subst build-,,$(@)),all)

.PHONY: install
install: $(addprefix install-,$(subdirs))

.PHONY: $(addprefix install-,$(subdirs))
$(addprefix install-,$(subdirs)):
	$(call make_cmd,$(subst install-,,$(@)),install)
