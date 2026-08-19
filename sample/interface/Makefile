include ../../utils.mk

IETF_INTERFACES_URI := https://raw.githubusercontent.com/YangModels/yang/refs/heads/main/standard/ietf/RFC

models := ietf-interfaces@2018-02-20.yang

.PHONY: all
all: build

.PHONY: install
install:
	for m in $(models); do \
		$(DESTDIR)$(PREFIX)/sbin/sysrepoctl -u $${m%%@*} 2>/dev/null || true; \
		$(DESTDIR)$(PREFIX)/sbin/sysrepoctl -i $$m; \
	done

.PHONY: build
build:

.PHONY: clean
clean:
	for m in $(models); do \
		$(DESTDIR)$(PREFIX)/sbin/sysrepoctl -u $${m%%@*} 2>/dev/null || true; \
	done

.PHONY: dist
dist: $(models)
$(models):
	$(call fetch_cmd,$(@),$(IETF_INTERFACES_URI)/$(@))

.PHONY: distclean
distclean:
	$(RM) $(model_files)
