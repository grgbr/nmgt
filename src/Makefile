include ../utils.mk

#
# Libyang
#
.PHONY: libyang
libyang: | $(BUILDDIR)/
	$(call cmake_cmd,$(@))

.PHONY: clean-libyang
clean-libyang:
	if [ -f "$(BUILDDIR)/libyang/install_manifest.txt" ]; then \
		xargs $(RM) < $(BUILDDIR)/libyang/install_manifest.txt; \
	fi
	$(RM) -r $(BUILDDIR)/libyang

$(BUILDDIR)/:
	$(MKDIR) --parents $(@)
