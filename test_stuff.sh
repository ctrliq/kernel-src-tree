#!/bin/bash
ls -l /dev/kvm
dnf -y install libvirt-daemon-common
virt-host-validate || /bin/true
podman run --name "rocky-9" -v /src:/src -v /usr/libexec/kernel_build.sh:/usr/local/bin/kernel_build.sh -it "pulp.prod.ciq.dev/ciq/cicd/lts-images/rocky-9-kernel-builder" /bin/true
image_from_container.sh 10G "rocky-9"
