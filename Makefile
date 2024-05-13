MAKEFLAGS := --no-print-directory

.PHONY: all
all:
	@awk -f scripts/check-multiple-entries.awk info/owners.yaml
	@$(MAKE) -C scripts fullbuild
	scripts/yaml2RHMAINTAINERS info/owners.yaml > info/RHMAINTAINERS
	scripts/yaml2CODEOWNERS info/owners.yaml > info/CODEOWNERS
	scripts/verifySubsystems info/owners.yaml
	scripts/checks.sh

clean:
	@$(MAKE) -C scripts clean
	rm -rf resources themes

themes/hugo-geekdoc/theme.toml:
	mkdir -p themes/hugo-geekdoc
	cd themes/hugo-geekdoc; \
	wget -q -O - https://github.com/thegeeklab/hugo-geekdoc/releases/download/v0.38.1/hugo-geekdoc.tar.gz | tar -xz

docs-prepare: themes/hugo-geekdoc/theme.toml

docs: docs-prepare
	hugo server
