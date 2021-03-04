MAKEFLAGS := --no-print-directory

.PHONY: all
all:
	@$(MAKE) -C scripts fullbuild
	scripts/yaml2RHMAINTAINERS info/owners.yaml > info/RHMAINTAINERS
	scripts/yaml2CODEOWNERS info/owners.yaml > info/CODEOWNERS

clean:
	@$(MAKE) -C scripts clean
