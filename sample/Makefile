include ../utils.mk

subdirs := interface oven cli ntp iana ethernet #vlan 802 restconf netconf

install-oven: install-cli

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
.PHONY: $(addprefix dist-,$(subdirs))
$(addprefix dist-,$(subdirs)):
	$(call make_cmd,$(subst dist-,,$(@)),dist)
	
.PHONY: clean
clean: $(addprefix clean-,$(subdirs))
.PHONY: $(addprefix clean-,$(subdirs))
$(addprefix clean-,$(subdirs)):
	$(call make_cmd,$(subst clean-,,$(@)),clean)

.PHONY: distclean
distclean: $(addprefix distclean-,$(subdirs))
.PHONY: $(addprefix distclean-,$(subdirs))
$(addprefix distclean-,$(subdirs)):
	$(call make_cmd,$(subst distclean-,,$(@)),distclean)
