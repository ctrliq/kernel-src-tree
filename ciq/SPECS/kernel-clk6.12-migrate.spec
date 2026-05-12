# Migration package: transitions users from the old non-namespaced
# CIQ kernel 6.12.x packages to the new kernel-clk6.12 namespace.
#
# Each subpackage shim carries Obsoletes (so dnf upgrade discovers it
# automatically) and Requires (to pull in the namespaced replacement).
# Shims intentionally have NO Provides for the old name -- the namespaced
# packages already Provide + Conflict the old names, and adding Provides
# here would hit those Conflicts.
#
# Works via: dnf upgrade (shims discovered via Obsoletes)

Name:    kernel-clk6.12-migrate
Version: 1.0
Release: 12%{?dist}
Summary: Migrate non-namespaced CIQ kernel 6.12.x to kernel-clk6.12
License: GPL-2.0-only
BuildArch: noarch
BuildRequires: systemd-rpm-macros

# Version ceiling for Obsoletes: any non-namespaced CIQ 6.12 kernel older
# than this gets obsoleted. This also catches stock Rocky/EL9 kernels
# (5.14.x < 6.12.86), which is acceptable since this package only lives
# in the CLK repo. Ceiling is set between the last non-namespaced release
# (6.12.85) and the first namespaced release (6.12.87) to avoid obsoleting
# what the namespaced kernel itself provides.
%define obs_ceil 6.12.86


# =============================================================================
# Base package: Obsoletes the old kernel meta-package so dnf upgrade
# discovers this package and pulls in the namespaced kernel. The old
# installonly packages (kernel-core, kernel-modules) can't be removed
# during the transaction if one is the running kernel -- the systemd
# cleanup service handles that on the next boot.
# =============================================================================

Obsoletes: kernel < %{obs_ceil}
# Require kernel-clk6.12-modules explicitly. This forces the actual
# meta-package to be installed (it's the only thing that pulls in modules).
# Without this, DNF satisfies "Requires: kernel-clk6.12" with just
# kernel-clk6.12-core, leaving kernel-clk6.12-modules and the meta-package
# uninstalled. That means missing non-core modules and no auto-install of
# future kernel versions.
Requires: kernel-clk6.12-modules
# Set the namespaced kernel as the default boot kernel. Without this,
# /etc/sysconfig/kernel still has DEFAULTKERNEL=kernel-core, and future
# namespaced kernel installs won't become the default boot entry.
Requires: kernel-clk6.12-default

%description
Migrates from the old non-namespaced CIQ kernel 6.12.x packages to the new
kernel-clk6.12 namespace.

Migration happens automatically on the next "dnf upgrade". Only packages
you currently have installed will be migrated. Old installonly
packages (kernel-core, kernel-modules) are automatically removed by a
cleanup service on the next reboot after migrating.

# =============================================================================
# Subpackages: each Obsoletes the old package (for dnf upgrade discovery)
# and Requires the namespaced replacement.
#
# NO Provides for old names -- the namespaced packages already carry
# Provides + Conflicts for the old names. Adding Provides here would
# trigger those Conflicts and break the transaction.
# =============================================================================

# --- kernel-headers ---
%package headers
Summary: Migrate kernel-headers to kernel-clk6.12-headers
Obsoletes: kernel-headers < %{obs_ceil}
Requires:  kernel-clk6.12-headers

%description headers
Migrates kernel-headers to kernel-clk6.12-headers.

# --- kernel-doc ---
%package doc
Summary: Migrate kernel-doc to kernel-clk6.12-doc
Obsoletes: kernel-doc < %{obs_ceil}
Requires:  kernel-clk6.12-doc

%description doc
Migrates kernel-doc to kernel-clk6.12-doc.

# --- kernel-cross-headers ---
%package cross-headers
Summary: Migrate kernel-cross-headers to kernel-clk6.12-cross-headers
Obsoletes: kernel-cross-headers < %{obs_ceil}
Requires:  kernel-clk6.12-cross-headers

%description cross-headers
Migrates kernel-cross-headers to kernel-clk6.12-cross-headers.

# --- kernel-tools ---
%package tools
Summary: Migrate kernel-tools to kernel-clk6.12-tools
Obsoletes: kernel-tools < %{obs_ceil}
Requires:  kernel-clk6.12-tools

%description tools
Migrates kernel-tools to kernel-clk6.12-tools.

# --- kernel-tools-libs ---
%package tools-libs
Summary: Migrate kernel-tools-libs to kernel-clk6.12-tools-libs
Obsoletes: kernel-tools-libs < %{obs_ceil}
Requires:  kernel-clk6.12-tools-libs

%description tools-libs
Migrates kernel-tools-libs to kernel-clk6.12-tools-libs.

# --- kernel-tools-libs-devel ---
%package tools-libs-devel
Summary: Migrate kernel-tools-libs-devel to kernel-clk6.12-tools-libs-devel
Obsoletes: kernel-tools-libs-devel < %{obs_ceil}
Requires:  kernel-clk6.12-tools-libs-devel

%description tools-libs-devel
Migrates kernel-tools-libs-devel to kernel-clk6.12-tools-libs-devel.

# --- kernel-devel ---
%package devel
Summary: Migrate kernel-devel to kernel-clk6.12-devel
Obsoletes: kernel-devel < %{obs_ceil}
Requires:  kernel-clk6.12-devel

%description devel
Migrates kernel-devel to kernel-clk6.12-devel.

# --- kernel-devel-matched ---
%package devel-matched
Summary: Migrate kernel-devel-matched to kernel-clk6.12-devel-matched
Obsoletes: kernel-devel-matched < %{obs_ceil}
Requires:  kernel-clk6.12-devel-matched

%description devel-matched
Migrates kernel-devel-matched to kernel-clk6.12-devel-matched.

# --- perf ---
%package -n kernel-clk6.12-migrate-perf
Summary: Migrate perf to perf-clk6.12
Obsoletes: perf < %{obs_ceil}
Requires:  perf-clk6.12

%description -n kernel-clk6.12-migrate-perf
Migrates perf to perf-clk6.12.

# --- python3-perf ---
%package -n kernel-clk6.12-migrate-python3-perf
Summary: Migrate python3-perf to python3-perf-clk6.12
Obsoletes: python3-perf < %{obs_ceil}
Requires:  python3-perf-clk6.12

%description -n kernel-clk6.12-migrate-python3-perf
Migrates python3-perf to python3-perf-clk6.12.

# --- libperf ---
%package -n kernel-clk6.12-migrate-libperf
Summary: Migrate libperf to libperf-clk6.12
Obsoletes: libperf < %{obs_ceil}
Requires:  libperf-clk6.12

%description -n kernel-clk6.12-migrate-libperf
Migrates libperf to libperf-clk6.12.

# --- libperf-devel ---
%package -n kernel-clk6.12-migrate-libperf-devel
Summary: Migrate libperf-devel to libperf-clk6.12-devel
Obsoletes: libperf-devel < %{obs_ceil}
Requires:  libperf-clk6.12-devel

%description -n kernel-clk6.12-migrate-libperf-devel
Migrates libperf-devel to libperf-clk6.12-devel.

# --- rtla ---
%package -n kernel-clk6.12-migrate-rtla
Summary: Migrate rtla to rtla-clk6.12
Obsoletes: rtla < %{obs_ceil}
Requires:  rtla-clk6.12

%description -n kernel-clk6.12-migrate-rtla
Migrates rtla to rtla-clk6.12.

# --- rv ---
%package -n kernel-clk6.12-migrate-rv
Summary: Migrate rv to rv-clk6.12
Obsoletes: rv < %{obs_ceil}
Requires:  rv-clk6.12

%description -n kernel-clk6.12-migrate-rv
Migrates rv to rv-clk6.12.

# --- kernel-selftests-internal ---
%package selftests-internal
Summary: Migrate kernel-selftests-internal to kernel-clk6.12-selftests-internal
Obsoletes: kernel-selftests-internal < %{obs_ceil}
Requires:  kernel-clk6.12-selftests-internal

%description selftests-internal
Migrates kernel-selftests-internal to kernel-clk6.12-selftests-internal.

# --- kernel-ipaclones-internal ---
%package ipaclones-internal
Summary: Migrate kernel-ipaclones-internal to kernel-clk6.12-ipaclones-internal
Obsoletes: kernel-ipaclones-internal < %{obs_ceil}
Requires:  kernel-clk6.12-ipaclones-internal

%description ipaclones-internal
Migrates kernel-ipaclones-internal to kernel-clk6.12-ipaclones-internal.

# --- kernel-abi-stablelists ---
%package abi-stablelists
Summary: Migrate kernel-abi-stablelists to kernel-clk6.12-abi-stablelists
Obsoletes: kernel-abi-stablelists < %{obs_ceil}
Requires:  kernel-clk6.12-abi-stablelists

%description abi-stablelists
Migrates kernel-abi-stablelists to kernel-clk6.12-abi-stablelists.

# --- kernel-uki-virt-addons ---
%package uki-virt-addons
Summary: Migrate kernel-uki-virt-addons to kernel-clk6.12-uki-virt-addons
Obsoletes: kernel-uki-virt-addons < %{obs_ceil}
Requires:  kernel-clk6.12-uki-virt-addons

%description uki-virt-addons
Migrates kernel-uki-virt-addons to kernel-clk6.12-uki-virt-addons.

# ===== Build =====

%prep
%build

%install
mkdir -p %{buildroot}%{_libexecdir}/kernel-clk6.12-migrate
mkdir -p %{buildroot}%{_unitdir}

# Cleanup script: removes old non-namespaced 6.12 installonly packages
cat > %{buildroot}%{_libexecdir}/kernel-clk6.12-migrate/cleanup-old-kernels.sh << 'EOFSCRIPT'
#!/bin/bash
# Remove old non-namespaced CIQ 6.12 installonly kernel packages.
# Dynamically finds any kernel-core-6.12.* that isn't a clk6.12 package,
# skips the running kernel, and removes all installonly siblings.
# Installed by kernel-clk6.12-migrate, runs once on boot via systemd.

RUNNING=$(uname -r)

# Find all non-namespaced 6.12.x kernel-core packages
for corepkg in $(rpm -qa kernel-core | grep '^kernel-core-6\.12\.' | grep -v clk6.12 | sort); do
    ver=$(rpm -q --qf '%%{VERSION}-%%{RELEASE}' "$corepkg" 2>/dev/null)
    [ -z "$ver" ] && continue
    # Check if this is the running kernel
    pkg_uname=$(rpm -q --qf '%%{VERSION}-%%{RELEASE}.%%{ARCH}\n' "$corepkg" 2>/dev/null)
    [ "$pkg_uname" = "$RUNNING" ] && continue
    # Not the running kernel, safe to remove all installonly siblings
    echo "Removing old non-namespaced kernel ${ver}..."
    TO_REMOVE=""
    for name in \
        kernel \
        kernel-core \
        kernel-modules \
        kernel-modules-core \
        kernel-modules-extra \
        kernel-modules-internal \
        kernel-modules-partner \
        kernel-devel \
        kernel-devel-matched \
        kernel-uki-virt \
        kernel-uki-virt-addons \
    ; do
        rpm -q "${name}-${ver}" &>/dev/null && TO_REMOVE="$TO_REMOVE ${name}-${ver}"
    done
    if [ -n "$TO_REMOVE" ]; then
        echo "Removing:$TO_REMOVE"
        rpm -e $TO_REMOVE 2>&1 || { rc=$?; echo "rpm -e failed with exit code $rc"; }
    fi
done

# Disable the service and remove migration shim packages only when ALL
# non-namespaced 6.12 kernel-core packages are gone. Don't short-circuit
# on "we removed something" -- the running kernel may have been skipped.
if ! rpm -qa kernel-core 2>/dev/null | grep '^kernel-core-6\.12\.' | grep -qv clk6.12; then
    systemctl disable kernel-clk6.12-migrate-cleanup.service 2>/dev/null || true
    # Remove migration shim packages via rpm -e (lighter than dnf at boot).
    # Namespaced packages survive because %posttrans marked them as user-installed.
    SHIMS=$(rpm -qa | grep '^kernel-clk6\.12-migrate' || true)
    if [ -n "$SHIMS" ]; then
        echo "Removing migration shims..."
        rpm -e $SHIMS 2>/dev/null || true
    fi
fi
EOFSCRIPT
chmod 755 %{buildroot}%{_libexecdir}/kernel-clk6.12-migrate/cleanup-old-kernels.sh

# Systemd oneshot service
cat > %{buildroot}%{_unitdir}/kernel-clk6.12-migrate-cleanup.service << 'EOFUNIT'
[Unit]
Description=Remove old non-namespaced CIQ 6.12 kernel packages
After=local-fs.target
ConditionPathExists=/usr/libexec/kernel-clk6.12-migrate/cleanup-old-kernels.sh

[Service]
Type=oneshot
ExecStart=/usr/libexec/kernel-clk6.12-migrate/cleanup-old-kernels.sh
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
EOFUNIT

# ===== Post-transaction =====

# Mark namespaced packages as user-installed so they survive removal of
# the migration shim packages. Each shim marks its own replacement.

%posttrans
# Base package: mark core kernel packages and enable cleanup service
for pkg in \
    kernel-clk6.12 \
    kernel-clk6.12-core \
    kernel-clk6.12-modules \
    kernel-clk6.12-modules-core \
    kernel-clk6.12-default \
; do
    rpm -q "$pkg" &>/dev/null && dnf mark install "$pkg" -y &>/dev/null || true
done
systemctl enable kernel-clk6.12-migrate-cleanup.service &>/dev/null || true

%posttrans headers
rpm -q kernel-clk6.12-headers &>/dev/null && dnf mark install kernel-clk6.12-headers -y &>/dev/null || true

%posttrans doc
rpm -q kernel-clk6.12-doc &>/dev/null && dnf mark install kernel-clk6.12-doc -y &>/dev/null || true

%posttrans cross-headers
rpm -q kernel-clk6.12-cross-headers &>/dev/null && dnf mark install kernel-clk6.12-cross-headers -y &>/dev/null || true

%posttrans tools
rpm -q kernel-clk6.12-tools &>/dev/null && dnf mark install kernel-clk6.12-tools -y &>/dev/null || true

%posttrans tools-libs
rpm -q kernel-clk6.12-tools-libs &>/dev/null && dnf mark install kernel-clk6.12-tools-libs -y &>/dev/null || true

%posttrans tools-libs-devel
rpm -q kernel-clk6.12-tools-libs-devel &>/dev/null && dnf mark install kernel-clk6.12-tools-libs-devel -y &>/dev/null || true

%posttrans devel
rpm -q kernel-clk6.12-devel &>/dev/null && dnf mark install kernel-clk6.12-devel -y &>/dev/null || true

%posttrans devel-matched
rpm -q kernel-clk6.12-devel-matched &>/dev/null && dnf mark install kernel-clk6.12-devel-matched -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-perf
rpm -q perf-clk6.12 &>/dev/null && dnf mark install perf-clk6.12 -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-python3-perf
rpm -q python3-perf-clk6.12 &>/dev/null && dnf mark install python3-perf-clk6.12 -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-libperf
rpm -q libperf-clk6.12 &>/dev/null && dnf mark install libperf-clk6.12 -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-libperf-devel
rpm -q libperf-clk6.12-devel &>/dev/null && dnf mark install libperf-clk6.12-devel -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-rtla
rpm -q rtla-clk6.12 &>/dev/null && dnf mark install rtla-clk6.12 -y &>/dev/null || true

%posttrans -n kernel-clk6.12-migrate-rv
rpm -q rv-clk6.12 &>/dev/null && dnf mark install rv-clk6.12 -y &>/dev/null || true

%posttrans selftests-internal
rpm -q kernel-clk6.12-selftests-internal &>/dev/null && dnf mark install kernel-clk6.12-selftests-internal -y &>/dev/null || true

%posttrans ipaclones-internal
rpm -q kernel-clk6.12-ipaclones-internal &>/dev/null && dnf mark install kernel-clk6.12-ipaclones-internal -y &>/dev/null || true

%posttrans abi-stablelists
rpm -q kernel-clk6.12-abi-stablelists &>/dev/null && dnf mark install kernel-clk6.12-abi-stablelists -y &>/dev/null || true

%posttrans uki-virt-addons
rpm -q kernel-clk6.12-uki-virt-addons &>/dev/null && dnf mark install kernel-clk6.12-uki-virt-addons -y &>/dev/null || true

# ===== Files =====

%files
%{_libexecdir}/kernel-clk6.12-migrate/cleanup-old-kernels.sh
%{_unitdir}/kernel-clk6.12-migrate-cleanup.service

%files headers
%files doc
%files cross-headers
%files tools
%files tools-libs
%files tools-libs-devel
%files devel
%files devel-matched
%files -n kernel-clk6.12-migrate-perf
%files -n kernel-clk6.12-migrate-python3-perf
%files -n kernel-clk6.12-migrate-libperf
%files -n kernel-clk6.12-migrate-libperf-devel
%files -n kernel-clk6.12-migrate-rtla
%files -n kernel-clk6.12-migrate-rv
%files selftests-internal
%files ipaclones-internal
%files abi-stablelists
%files uki-virt-addons

%changelog
* Mon May 12 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-12
- Switch to Obsoletes < %%{obs_ceil} so dnf upgrade discovers shims
  automatically without requiring explicit "dnf install". Shims have
  NO Provides for old names to avoid triggering Conflicts on the
  namespaced packages.
- Move dnf-mark-install from single base %%posttrans to per-subpackage
  %%posttrans so namespaced packages survive shim removal.
- Use version ceiling (6.13) for Obsoletes instead of per-release
  version pinning.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-11
- Add migration subpackages for kernel-devel and kernel-devel-matched
- Fix version list duplication: cleanup script now uses RPM macros
  instead of hardcoded version strings

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-10
- Cleanup service also removes migration shim packages after old kernel
  cleanup. Namespaced packages survive because %%posttrans already marked
  them as user-installed.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-9
- Add systemd oneshot service to remove old non-namespaced 6.12 installonly
  packages on next boot. rpm -e can't run from %%posttrans (RPM db lock),
  so deferred to systemd. Service disables itself after cleanup.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-8
- Best-effort removal of old non-namespaced 6.12 installonly packages in
  %%posttrans. Only removes specific released versions, and only if they
  are not the currently running kernel. Uses rpm -e directly.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-7
- Add %%posttrans to mark namespaced packages as user-installed so
  migration shim packages can be safely removed afterward

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-6
- Add migration subpackages for kernel-selftests-internal,
  kernel-ipaclones-internal, kernel-abi-stablelists, kernel-uki-virt-addons

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-5
- Remove Obsoletes for kernel base meta-package. It's installonly and
  coexists harmlessly. Obsoleting it breaks dnf update when the old
  CIQ repo is still enabled.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-4
- Split Obsoletes and rich deps across different packages: base package
  has rich deps pointing to subpackages, subpackages carry the Obsoletes.
  This avoids the solver poisoning issue where Obsoletes on the same
  package as the rich dep condition caused the condition to evaluate false.

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-3
- Subpackage approach with unconditional Requires from base (worked but
  installed packages user didn't have)

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-2
- Rich deps + Conflicts approach (failed - solver can't resolve Conflicts
  against all repo-available versions)

* Tue Apr 22 2026 Brett Mastbergen <bmastbergen@ciq.com> - 1.0-1
- Initial migration package
