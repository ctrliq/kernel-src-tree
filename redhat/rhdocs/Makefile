MAKEFLAGS := --no-print-directory

.PHONY: all clean docs-prepare docs-clean docs
all:
	scripts/checks.sh
	python3 scripts/owners-tool.py convert info/owners.yaml templates/RHMAINTAINERS.template > info/RHMAINTAINERS
	python3 scripts/owners-tool.py convert info/owners.yaml templates/CODEOWNERS.template > info/CODEOWNERS

clean: docs-clean

themes/hugo-geekdoc/theme.toml:
	mkdir -p themes/hugo-geekdoc
	cd themes/hugo-geekdoc; \
	wget -q -O - https://github.com/thegeeklab/hugo-geekdoc/releases/download/v0.38.1/hugo-geekdoc.tar.gz | tar -xz

content/docs/owners_yaml_format.adoc: templates/owners-schema.yaml
	python3 scripts/owners-tool.py doc $< > $@

docs-prepare: themes/hugo-geekdoc/theme.toml content/docs/owners_yaml_format.adoc

docs-clean:
	rm -rf resources themes
	rm -f content/docs/owners_yaml_format.adoc

docs: docs-prepare
	hugo server
