include ../../utils.mk

IETF_NTP_YANG_URI := https://www.yangcatalog.org/all_modules/ietf-ntp@2022-07-05.yang

.PHONY: all
all: build

.PHONY: install
install:

.PHONY: build
build:

.PHONY: dist
dist: ietf-ntp@2022-07-05.yang
ietf-ntp@2022-07-05.yang:
	$(call fetch_cmd,$(@),$(IETF_NTP_YANG_URI))

.PHONY: distclean
distclean:
	$(RM) ietf-ntp@2022-07-05.yang
