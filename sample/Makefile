include ../utils.mk

subdirs := oven restconf cli netconf

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

.PHONY: dist
dist: $(addprefix dist-,$(subdirs))
$(addprefix dist-,$(subdirs)):
	$(call make_cmd,$(subst dist-,,$(@)),dist)

.PHONY: distclean
distclean: $(addprefix distclean-,$(subdirs))
$(addprefix distclean-,$(subdirs)):
	$(call make_cmd,$(subst distclean-,,$(@)),distclean)
