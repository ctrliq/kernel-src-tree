ifeq ($(filter help dist-% distg-%,$(MAKECMDGOALS)),)
	include Makefile
endif

MAKEFLAGS += --no-print-directory
_OUTPUT := "."
# this section is needed in order to make O= to work
ifeq ("$(origin O)", "command line")
  _OUTPUT := "$(abspath $(O))"
  _EXTRA_ARGS := O=$(_OUTPUT)
endif
help::
	@echo "## Distribution Targets"
	@$(MAKE) -C redhat dist-help
	@echo ""
	@echo "## Kernel Targets"
	@$(MAKE) -f Makefile $(@)

dist-%::
	@$(MAKE) -C redhat $(@) $(_EXTRA_ARGS)

distg-%::
	@$(MAKE) -C redhat $(@) $(_EXTRA_ARGS)

