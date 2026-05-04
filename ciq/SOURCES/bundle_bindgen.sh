#!/bin/sh
set -e

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
BINDGEN_CLI_SHA256="fded10ca0956afd0cbe5cf89cc71ae1a679e65b8216c651fca17ba7de8ac54dc"
CRATESIO_API_ENDPOINT=https://crates.io/api/v1/crates/bindgen-cli/${BINDGEN_CLI_VERSION}/download

curl -sfL $CRATESIO_API_ENDPOINT -o $SOURCES/$BINDGEN_CLI_CRATE

echo "$BINDGEN_CLI_SHA256  $SOURCES/$BINDGEN_CLI_CRATE" | sha256sum -c - || {
    echo "Error: SHA256 checksum mismatch for $BINDGEN_CLI_CRATE"
    echo "Expected: $BINDGEN_CLI_SHA256"
    echo "Got:      $(sha256sum $SOURCES/$BINDGEN_CLI_CRATE | awk '{print $1}')"
    rm -f $SOURCES/$BINDGEN_CLI_CRATE
    exit 1
}

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
