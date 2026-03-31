#!/bin/sh

# Bundle the bindgen-cli source code to be included in the kernel build.
# https://crates.io/crates/bindgen-cli
#
# The bindgen tool, required to build Rust code in the Linux kernel, is
# currently only packaged in Fedora/ELN. In order to build CLK kernels
# on Rocky Linux we need to build bindgen as part of the kernel build.

SOURCES=$1

BINDGEN_CLI=bindgen-cli
BINDGEN_CLI_VERSION="0.71.1"
BINDGEN_CLI_CRATE=bindgen-cli.crate
CRATESIO_API_ENDPOINT=https://crates.io/api/v1/crates/bindgen-cli/${BINDGEN_CLI_VERSION}/download

curl -sL $CRATESIO_API_ENDPOINT -o $SOURCES/$BINDGEN_CLI_CRATE
tar -xf $SOURCES/$BINDGEN_CLI_CRATE -C $SOURCES
mv $SOURCES/$BINDGEN_CLI-$BINDGEN_CLI_VERSION $SOURCES/$BINDGEN_CLI

# vendor bindgen-cli
cd $SOURCES/$BINDGEN_CLI
mkdir .cargo
cat > .cargo/config.toml <<EOF
[source.crates-io]
replace-with = "vendored-sources"

[source.vendored-sources]
directory = "vendor"
EOF

cargo vendor --locked --quiet

cd ..
tar czf $BINDGEN_CLI.tar.gz $BINDGEN_CLI

# clean up
rm -f $SOURCES/$BINDGEN_CLI_CRATE
rm -rf $SOURCES/$BINDGEN_CLI
