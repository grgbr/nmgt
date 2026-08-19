include ../../utils.mk

IEEE802_ETHERNET_URI := https://raw.githubusercontent.com/YangModels/yang/refs/heads/main/standard/ieee/published/802.3

models := ieee802-ethernet-phy-type.yang \
          ieee802-ethernet-phy.yang \
          ieee802-ethernet-interface.yang

.PHONY: all
all: build

.PHONY: install
install:

.PHONY: build
build:

.PHONY: clean
clean:

.PHONY: dist
dist: $(models)
$(models):
	$(call fetch_cmd,$(@),$(IEEE802_ETHERNET_URI)/$(@))

.PHONY: distclean
distclean:
	$(RM) $(model_files)
