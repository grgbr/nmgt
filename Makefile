#
# TODO
# * https://github.com/okda-networks/onm-cli
# * https://github.com/sysrepo/sysrepo-gnxi
#
OUTBASE         := $(CURDIR)/out
DOWNDIR         := $(OUTBASE)/download
STAMPDIR        := $(OUTBASE)/stamp
SRCDIR          := $(OUTBASE)/src
BUILDDIR        := $(OUTBASE)/build
STAGEDIR        := $(OUTBASE)/staging

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

NPROC           := $(shell echo $$(($$(nproc) * 3 / 4)))
CURUSER         := $(shell $(ID) --user --name)
CURGROUP        := $(shell $(ID) --group --name)

# $1: pathname to output file
# $2: upstream URI
define fetch_cmd
	$(CURL) --location --output '$(strip $(1))' '$(strip $(2))'
endef

# $1: pathname to output top-level directory
# $2: pathname to archive file
define untar_cmd
$(MKDIR) '$(strip $(1))'
$(TAR) --directory='$(strip $(1))' \
       --extract \
       --strip-components=1 \
       --file='$(strip $(2))'
endef

# $1: pathname to source directory
# $2: arbitrary cmake options
define cmake_cmd
env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
    cmake -S '$(strip $(1))' \
          -B '$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' \
          -DCMAKE_BUILD_TYPE='Debug' \
          -DCMAKE_EXE_LINKER_FLAGS='$(EXTRA_LDFLAGS)' \
          -DCMAKE_MODULE_LINKER_FLAGS='$(EXTRA_LDFLAGS)' \
          -DCMAKE_SHARED_LINKER_FLAGS='$(EXTRA_LDFLAGS)' \
          -DCMAKE_INSTALL_PREFIX='$(STAGEDIR)' \
          $(2) && \
env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
    cmake --build '$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' \
          --parallel $(NPROC) && \
env PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
    cmake --install '$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))'
endef

.PHONY: all
all: sample pyang

.PHONY: sample
sample: $(STAMPDIR)/sysrepo
	$(MAKE) --directory='$(@)' \
		PREFIX='$(STAGEDIR)' \
		BUILDDIR='$(BUILDDIR)/$(@)' \
		PKG_CONFIG='$(PKG_CONFIG)' \
		PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
		EXTRA_CFLAGS='$(EXTRA_CFLAGS)' \
		EXTRA_LDFLAGS='$(EXTRA_LDFLAGS)' \
		MKDIR='$(MKDIR)' \
		INSTALL='$(INSTALL)' \
		install

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
	$(MKDIR) --mode=700 '$(STAGEDIR)/var/lib/sysrepo'
	$(MKDIR) --mode=700 '$(STAGEDIR)/var/lib/sysrepo/factory'
	$(TOUCH) $(@)
$(SRCDIR)/sysrepo/: $(DOWNDIR)/$(SYSREPO_TARBALL_BASE) | $(SRCDIR)/
	$(call untar_cmd,$(@),$(<))
$(DOWNDIR)/$(SYSREPO_TARBALL_BASE): | $(DOWNDIR)/
	$(call fetch_cmd,$(@),$(SYSREPO_URI))

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

.PHONY: dev
dev:
	$(CTAGS) -R $(SRCDIR)
	$(CSCOPE) -bqR -s$(SRCDIR)

.PHONY: clean
clean:
	$(RM) -r $(filter-out $(realpath $(DOWNDIR)), \
	                      $(realpath $(wildcard $(OUTBASE)/*)))
	$(RM) tags cscope.*

.PHONY: clobber
clobber: clean
	$(RM) -r $(OUTBASE)

#
# Directory rules
#
$(DOWNDIR)/ $(STAMPDIR)/ $(SRCDIR)/ $(BUILDDIR)/:
	$(MKDIR) -p $(@)
