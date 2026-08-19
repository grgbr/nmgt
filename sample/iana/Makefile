include ../../utils.mk

IANA_URI := https://raw.githubusercontent.com/YangModels/yang/refs/heads/main/standard/iana

models := iana-if-type@2026-03-17.yang

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
	$(call fetch_cmd,$(@),$(IANA_URI)/$(@))

.PHONY: distclean
distclean:
	$(RM) $(model_files)

