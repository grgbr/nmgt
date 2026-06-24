#
# TODO
# * https://github.com/okda-networks/onm-cli
# * https://github.com/sysrepo/sysrepo-gnxi
#
# apt install libc-ares-dev libjansson-dev libev-dev
#
OUTBASE         := $(CURDIR)/out
DOWNDIR         := $(OUTBASE)/download
STAMPDIR        := $(OUTBASE)/stamp
SRCDIR          := $(OUTBASE)/src
BUILDDIR        := $(OUTBASE)/build
STAGEDIR        := $(OUTBASE)/staging
PATCHDIR        := $(CURDIR)/patch

EXTRA_CFLAGS    := -g -O0
EXTRA_LDFLAGS   := -Wl,-rpath,$(STAGEDIR)/lib
PKG_CONFIG_PATH := $(STAGEDIR)/lib/pkgconfig:$(PKG_CONFIG_PATH)

PYTHON3         := python3
CURL            := curl
TAR             := tar
MKDIR           := mkdir
CD              := cd
TOUCH           := touch
PKG_CONFIG      := pkg-config
INSTALL         := install
ID              := id
CTAGS           := ctags
CSCOPE          := cscope
PYTHON3         := python3
RSYNC           := rsync
PATCH           := patch

NPROC           := $(shell echo $$(($$(nproc) * 3 / 4)))
CURUSER         := $(shell $(ID) --user --name)
CURGROUP        := $(shell $(ID) --group --name)

include utils.mk

.PHONY: all
all: sample pyang onmcli nghttp2

.PHONY: sample
sample: $(STAMPDIR)/sysrepo $(STAMPDIR)/nghttp2
	$(call make_cmd,$(@),PREFIX='$(STAGEDIR)' install)

#
# Python Yang (requires a Python venv)
#
.PHONY: pyang
pyang: $(STAMPDIR)/pyang
$(STAMPDIR)/pyang: | $(STAMPDIR)/venv
	$(STAGEDIR)/bin/pip3 install pyang
	$(TOUCH) $(@)

#
# Python virtual environment.
#
.PHONY: venv
venv: $(STAMPDIR)/venv
$(STAMPDIR)/venv:
	$(PYTHON3) -m venv $(STAGEDIR)
	$(TOUCH) $(@)

#
# onm-cli
# Watch out ! Requires the libbsd-dev package to build !
#
ONMCLI_URI          := https://github.com/okda-networks/onm-cli/archive/refs/tags/v1.0.0.tar.gz
ONMCLI_TARBALL_EXT  := $(shell echo '$(notdir $(ONMCLI_URI))' | sed 's/v[0-9.]\+//')
ONMCLI_VERS         := $(patsubst v%.$(ONMCLI_TARBALL_EXT),%,$(notdir $(ONMCLI_URI)))
ONMCLI_TARBALL_BASE := onmcli-$(ONMCLI_VERS).$(ONMCLI_TARBALL_EXT)
onmcli: $(STAMPDIR)/onmcli
$(STAMPDIR)/onmcli: $(STAMPDIR)/sysrepo | $(SRCDIR)/onmcli/ $(STAMPDIR)/ $(BUILDDIR)/
	$(RSYNC) --archive --delete $(SRCDIR)/onmcli/ $(BUILDDIR)/onmcli
	cd $(BUILDDIR)/onmcli && $(PATCH) -p1 < $(PATCHDIR)/onmcli-$(ONMCLI_VERS).patch
	$(MAKE) --directory='$(BUILDDIR)/onmcli' \
	        INSTALL_DIR='$(STAGEDIR)/bin' \
	        LOG_DIR='$(STAGEDIR)/var/log/onmcli' \
	        CFLAGS='$(EXTRA_CFLAGS) -I$(STAGEDIR)/include -Wall --vtv-debug -DLOGFILE_NAME=\"$(STAGEDIR)/var/log/onmcli/onmcli.log\"' \
	        LIB_PATH='-L $(STAGEDIR)/lib $(EXTRA_LDFLAGS)' \
	        install
	$(TOUCH) $(@)
$(SRCDIR)/onmcli/: $(DOWNDIR)/$(ONMCLI_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(ONMCLI_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(ONMCLI_URI))

#
# Sysrepo
# See <sysrepo>/CMakeLists.txt for available build config options...
#
SYSREPO_URI          := https://github.com/sysrepo/sysrepo/archive/refs/tags/v4.5.4.tar.gz
SYSREPO_TARBALL_EXT  := $(shell echo '$(notdir $(SYSREPO_URI))' | sed 's/v[0-9.]\+//')
SYSREPO_VERS         := $(patsubst v%.$(SYSREPO_TARBALL_EXT),%,$(notdir $(SYSREPO_URI)))
SYSREPO_TARBALL_BASE := sysrepo-$(SYSREPO_VERS).$(SYSREPO_TARBALL_EXT)
.PHONY: sysrepo
sysrepo: $(STAMPDIR)/sysrepo
$(STAMPDIR)/sysrepo: $(STAMPDIR)/libyang | $(SRCDIR)/sysrepo/ $(STAMPDIR)/ $(BUILDDIR)/
	$(call cmake_cmd,$(firstword $(|)), \
	  -DENABLE_DS_MONGO=false \
	  -DENABLE_DS_REDIS=false \
	  -DENABLE_DS_REDIS=false \
	  -DLIBSYSTEMD_FOUND=false \
	  \
	  -DCMAKE_INSTALL_BINDIR='$(STAGEDIR)/sbin' \
	  -DREPO_PATH='$(STAGEDIR)/etc/sysrepo' \
	  -DFACTORY_DEFAULT_DATA_PATH='$(STAGEDIR)/var/lib/sysrepo/factory' \
	  -DSTARTUP_DATA_PATH='$(STAGEDIR)/var/lib/sysrepo/startup' \
	  -DNOTIFICATION_PATH='$(STAGEDIR)/var/lib/sysrepo/notif' \
	  -DYANG_MODULE_PATH='$(STAGEDIR)/var/lib/sysrepo/yang' \
	  \
	  -DSYSREPO_UMASK='00077' \
	  -DSYSREPO_GROUP='$(CURGROUP)' \
	  -DNACM_RECOVERY_USER='$(CURUSER)')
	$(MKDIR) --parents --mode=755 '$(STAGEDIR)/var/lib'
	$(MKDIR) --parents --mode=700 '$(STAGEDIR)/var/lib/sysrepo'
	$(MKDIR) --parents --mode=700 '$(STAGEDIR)/var/lib/sysrepo/factory'
	$(TOUCH) $(@)
$(SRCDIR)/sysrepo/: $(DOWNDIR)/$(SYSREPO_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(SYSREPO_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(SYSREPO_URI))

#
# Libyang
#
LIBYANG_URI          := https://github.com/CESNET/libyang/archive/refs/tags/v5.4.9.tar.gz
LIBYANG_TARBALL_EXT  := $(shell echo '$(notdir $(LIBYANG_URI))' | sed 's/v[0-9.]\+//')
LIBYANG_VERS         := $(patsubst v%.$(LIBYANG_TARBALL_EXT),%,$(notdir $(LIBYANG_URI)))
LIBYANG_TARBALL_BASE := libyang-$(LIBYANG_VERS).$(LIBYANG_TARBALL_EXT)
.PHONY: libyang
libyang: $(STAMPDIR)/libyang
$(STAMPDIR)/libyang: | $(SRCDIR)/libyang/ $(STAMPDIR)/ $(BUILDDIR)/
	$(call cmake_cmd,$(firstword $(|)))
	$(TOUCH) $(@)
$(SRCDIR)/libyang/: $(DOWNDIR)/$(LIBYANG_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(LIBYANG_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(LIBYANG_URI))

#
# Nghttp2
#
NGHTTP2_URI          := https://github.com/nghttp2/nghttp2/releases/download/v1.69.0/nghttp2-1.69.0.tar.gz
NGHTTP2_TARBALL_EXT  := $(shell echo '$(notdir $(NGHTTP2_URI))' | sed 's/nghttp2-[0-9.]\+\.//')
NGHTTP2_VERS         := $(patsubst nghttp2-%.$(NGHTTP2_TARBALL_EXT),%,$(notdir $(NGHTTP2_URI)))
NGHTTP2_TARBALL_BASE := nghttp2-$(NGHTTP2_VERS).$(NGHTTP2_TARBALL_EXT)
.PHONY: nghttp2
nghttp2: $(STAMPDIR)/nghttp2
$(STAMPDIR)/nghttp2: $(STAMPDIR)/libevent | $(SRCDIR)/nghttp2/ $(STAMPDIR)/ $(BUILDDIR)/
	$(call configure_cmd,$(firstword $(|)))
	$(TOUCH) $(@)
$(SRCDIR)/nghttp2/: $(DOWNDIR)/$(NGHTTP2_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(NGHTTP2_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(NGHTTP2_URI))

#
# Libevent
#
LIBEVENT_URI          := https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz
LIBEVENT_TARBALL_EXT  := $(shell echo '$(notdir $(LIBEVENT_URI))' | sed 's/libevent-[0-9.]\+-stable\.//')
LIBEVENT_VERS         := $(patsubst libevent-%-stable.$(LIBEVENT_TARBALL_EXT),%,$(notdir $(LIBEVENT_URI)))
LIBEVENT_TARBALL_BASE := libevent-$(LIBEVENT_VERS).$(LIBEVENT_TARBALL_EXT)
.PHONY: libevent
libevent: $(STAMPDIR)/libevent
$(STAMPDIR)/libevent: | $(SRCDIR)/libevent/ $(STAMPDIR)/ $(BUILDDIR)/
	$(call configure_cmd,$(firstword $(|)))
	$(TOUCH) $(@)
$(SRCDIR)/libevent/: $(DOWNDIR)/$(LIBEVENT_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(LIBEVENT_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(LIBEVENT_URI))

.PHONY: dev
dev:
	$(CTAGS) -f $(OUTBASE)/tags -R $(SRCDIR)
	cd $(SRCDIR) && $(CSCOPE) -f$(OUTBASE)/cscope.out -bqR

.PHONY: clean
clean:
	$(RM) -r $(filter-out $(realpath $(DOWNDIR)), \
	                      $(realpath $(wildcard $(OUTBASE)/*)))
	$(RM) tags cscope.*

.PHONY: clobber
clobber: clean
	$(RM) -r $(OUTBASE)
	
.PHONY: dist dist-sample
dist: dist-sample
dist-sample:
	$(call make_cmd,$(patsubst dist-%,%,$(@)),dist)

.PHONY: distclean distclean-sample
distclean: clobber distclean-sample
distclean-sample:
	$(call make_cmd,$(patsubst distclean-%,%,$(@)),distclean)

#
# Directory rules
#
$(DOWNDIR)/ $(STAMPDIR)/ $(SRCDIR)/ $(BUILDDIR)/:
	$(MKDIR) --parents $(@)
