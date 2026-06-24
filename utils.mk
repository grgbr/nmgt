# $1: pathname to output file
# $2: upstream URI
define fetch_cmd
	$(CURL) --location --output '$(strip $(1))' '$(strip $(2))'
endef

# $1: pathname to output top-level directory
# $2: pathname to archive file
define untar_cmd
$(MKDIR) '$(strip $(1))' && \
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

# $1: pathname to source directory
# $2: arbitrary configure options
define configure_cmd
$(MKDIR) --parents '$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' && \
cd '$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' && \
'$(strip $(1))/configure' --prefix='$(STAGEDIR)' \
                          --srcdir='$(strip $(1))' \
                          INCLUDES='-I$(STAGEDIR)/include' \
                          LDFLAGS='$(EXTRA_LDFLAGS)' \
                          PKG_CONFIG_PATH='$(PKG_CONFIG_PATH)' \
                          $(2) && \
make --directory='$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' && \
make --directory='$(BUILDDIR)/$(notdir $(patsubst %/,%,$(strip $(1))))' install
endef

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
        CURL='$(CURL)' \
        $(2)
endef
