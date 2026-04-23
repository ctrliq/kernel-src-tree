# Migration package: transitions users from the old non-namespaced
# CIQ kernel 6.12.x packages to the new kernel-clk6.12 namespace.
#
# Base package uses rich deps to conditionally pull in subpackages only
# for old packages the user actually has installed. Each subpackage carries
# the Obsoletes + Requires for its specific package.
#
# Usage: dnf install kernel-clk6.12-migrate

Name:    kernel-clk6.12-migrate
Version: 1.0
Release: 11%{?dist}
Summary: Migrate non-namespaced CIQ kernel 6.12.x to kernel-clk6.12
License: GPL-2.0-only
BuildArch: noarch
BuildRequires: systemd-rpm-macros

# Old released versions of the non-namespaced CIQ 6.12 kernel packages
%define old_ver1 6.12.15-3.1.0.1.rocky9
%define old_ver2 6.12.30-1.1.0.0.el9_clk
%define old_ver3 6.12.43-1.1.0.0.el9_clk
%define old_ver4 6.12.63-1.1.el9_clk
%define old_ver5 6.12.74-1.1.0.0.el9_clk

# =============================================================================
# Base package: always installs the new namespaced kernel. Uses rich deps
# to conditionally pull in migration subpackages only if the user has the
# corresponding old package installed.
#
# No Obsoletes here for the old kernel base meta-package. It's installonly
# and can't be removed during the transaction if it's the running kernel.
# The systemd cleanup service removes it (and other installonly packages)
# on the next boot. Obsoleting it also causes dnf update failures when the
# old repo is still enabled.
# =============================================================================

Requires: kernel-clk6.12

# Conditional: pull in migration subpackages only if old package is installed
Requires: (%{name}-headers if kernel-headers)
Requires: (%{name}-doc if kernel-doc)
Requires: (%{name}-cross-headers if kernel-cross-headers)
Requires: (%{name}-tools if kernel-tools)
Requires: (%{name}-tools-libs if kernel-tools-libs)
Requires: (%{name}-tools-libs-devel if kernel-tools-libs-devel)
Requires: (kernel-clk6.12-migrate-perf if perf)
Requires: (kernel-clk6.12-migrate-python3-perf if python3-perf)
Requires: (kernel-clk6.12-migrate-libperf if libperf)
Requires: (kernel-clk6.12-migrate-libperf-devel if libperf-devel)
Requires: (kernel-clk6.12-migrate-rtla if rtla)
Requires: (kernel-clk6.12-migrate-rv if rv)
Requires: (%{name}-devel if kernel-devel)
Requires: (%{name}-devel-matched if kernel-devel-matched)
Requires: (%{name}-selftests-internal if kernel-selftests-internal)
Requires: (%{name}-ipaclones-internal if kernel-ipaclones-internal)
Requires: (%{name}-abi-stablelists if kernel-abi-stablelists)
Requires: (%{name}-uki-virt-addons if kernel-uki-virt-addons)

%description
Migrates from the old non-namespaced CIQ kernel 6.12.x packages to the new
kernel-clk6.12 namespace.

Install this package to transition to namespaced kernel packages:

    dnf install kernel-clk6.12-migrate

Only packages you currently have installed will be migrated. Old installonly
packages (kernel-core, kernel-modules) are automatically removed by a
cleanup service on the next reboot after migrating.

# =============================================================================
# Subpackages: each Obsoletes the old package and Requires the replacement.
# Only pulled in by the base package's rich deps if the old package is
# actually installed.
# =============================================================================

# --- kernel-headers ---
%package headers
Summary: Migrate kernel-headers 6.12.x to kernel-clk6.12-headers
Obsoletes: kernel-headers <= %{old_ver5}
Requires:  kernel-clk6.12-headers
# Tell DNF this shim provides kernel-headers so stock kernel-headers won't
# be pulled in to satisfy glibc-devel during the migration transaction.
Provides:  kernel-headers = %{old_ver5}

%description headers
Migrates kernel-headers 6.12.x to kernel-clk6.12-headers. The namespaced
headers package provides generic kernel-headers, satisfying dependencies
like glibc-devel.

# --- kernel-doc ---
%package doc
Summary: Migrate kernel-doc 6.12.x to kernel-clk6.12-doc
Obsoletes: kernel-doc <= %{old_ver5}
Requires:  kernel-clk6.12-doc

%description doc
Migrates kernel-doc 6.12.x to kernel-clk6.12-doc.

# --- kernel-cross-headers ---
%package cross-headers
Summary: Migrate kernel-cross-headers 6.12.x to kernel-clk6.12-cross-headers
Obsoletes: kernel-cross-headers <= %{old_ver5}
Requires:  kernel-clk6.12-cross-headers

%description cross-headers
Migrates kernel-cross-headers 6.12.x to kernel-clk6.12-cross-headers.

# --- kernel-tools ---
%package tools
Summary: Migrate kernel-tools 6.12.x to kernel-clk6.12-tools
Obsoletes: kernel-tools <= %{old_ver5}
Requires:  kernel-clk6.12-tools

%description tools
Migrates kernel-tools 6.12.x to kernel-clk6.12-tools.

# --- kernel-tools-libs ---
%package tools-libs
Summary: Migrate kernel-tools-libs 6.12.x to kernel-clk6.12-tools-libs
Obsoletes: kernel-tools-libs <= %{old_ver5}
Requires:  kernel-clk6.12-tools-libs

%description tools-libs
Migrates kernel-tools-libs 6.12.x to kernel-clk6.12-tools-libs.

# --- kernel-tools-libs-devel ---
%package tools-libs-devel
Summary: Migrate kernel-tools-libs-devel 6.12.x to kernel-clk6.12-tools-libs-devel
Obsoletes: kernel-tools-libs-devel <= %{old_ver5}
Requires:  kernel-clk6.12-tools-libs-devel

%description tools-libs-devel
Migrates kernel-tools-libs-devel 6.12.x to kernel-clk6.12-tools-libs-devel.

# --- kernel-devel ---
%package devel
Summary: Migrate kernel-devel 6.12.x to kernel-clk6.12-devel
Obsoletes: kernel-devel <= %{old_ver5}
Requires:  kernel-clk6.12-devel

%description devel
Migrates kernel-devel 6.12.x to kernel-clk6.12-devel.

# --- kernel-devel-matched ---
%package devel-matched
Summary: Migrate kernel-devel-matched 6.12.x to kernel-clk6.12-devel-matched
Obsoletes: kernel-devel-matched <= %{old_ver5}
Requires:  kernel-clk6.12-devel-matched

%description devel-matched
Migrates kernel-devel-matched 6.12.x to kernel-clk6.12-devel-matched.

# --- perf ---
%package -n kernel-clk6.12-migrate-perf
Summary: Migrate perf 6.12.x to perf-clk6.12
Obsoletes: perf <= %{old_ver5}
Requires:  perf-clk6.12

%description -n kernel-clk6.12-migrate-perf
Migrates perf 6.12.x to perf-clk6.12.

# --- python3-perf ---
%package -n kernel-clk6.12-migrate-python3-perf
Summary: Migrate python3-perf 6.12.x to python3-perf-clk6.12
Obsoletes: python3-perf <= %{old_ver5}
Requires:  python3-perf-clk6.12

%description -n kernel-clk6.12-migrate-python3-perf
Migrates python3-perf 6.12.x to python3-perf-clk6.12.

# --- libperf ---
%package -n kernel-clk6.12-migrate-libperf
Summary: Migrate libperf 6.12.x to libperf-clk6.12
Obsoletes: libperf <= %{old_ver5}
Requires:  libperf-clk6.12

%description -n kernel-clk6.12-migrate-libperf
Migrates libperf 6.12.x to libperf-clk6.12.

# --- libperf-devel ---
%package -n kernel-clk6.12-migrate-libperf-devel
Summary: Migrate libperf-devel 6.12.x to libperf-clk6.12-devel
Obsoletes: libperf-devel <= %{old_ver5}
Requires:  libperf-clk6.12-devel

%description -n kernel-clk6.12-migrate-libperf-devel
Migrates libperf-devel 6.12.x to libperf-clk6.12-devel.

# --- rtla ---
%package -n kernel-clk6.12-migrate-rtla
Summary: Migrate rtla 6.12.x to rtla-clk6.12
Obsoletes: rtla <= %{old_ver5}
Requires:  rtla-clk6.12

%description -n kernel-clk6.12-migrate-rtla
Migrates rtla 6.12.x to rtla-clk6.12.

# --- rv ---
%package -n kernel-clk6.12-migrate-rv
Summary: Migrate rv 6.12.x to rv-clk6.12
Obsoletes: rv <= %{old_ver5}
Requires:  rv-clk6.12

%description -n kernel-clk6.12-migrate-rv
Migrates rv 6.12.x to rv-clk6.12.

# --- kernel-selftests-internal ---
%package selftests-internal
Summary: Migrate kernel-selftests-internal 6.12.x to kernel-clk6.12-selftests-internal
Obsoletes: kernel-selftests-internal <= %{old_ver5}
Requires:  kernel-clk6.12-selftests-internal

%description selftests-internal
Migrates kernel-selftests-internal 6.12.x to kernel-clk6.12-selftests-internal.

# --- kernel-ipaclones-internal ---
%package ipaclones-internal
Summary: Migrate kernel-ipaclones-internal 6.12.x to kernel-clk6.12-ipaclones-internal
Obsoletes: kernel-ipaclones-internal <= %{old_ver5}
Requires:  kernel-clk6.12-ipaclones-internal

%description ipaclones-internal
Migrates kernel-ipaclones-internal 6.12.x to kernel-clk6.12-ipaclones-internal.

# --- kernel-abi-stablelists ---
%package abi-stablelists
Summary: Migrate kernel-abi-stablelists 6.12.x to kernel-clk6.12-abi-stablelists
Obsoletes: kernel-abi-stablelists <= %{old_ver5}
Requires:  kernel-clk6.12-abi-stablelists

%description abi-stablelists
Migrates kernel-abi-stablelists 6.12.x to kernel-clk6.12-abi-stablelists.

# --- kernel-uki-virt-addons ---
%package uki-virt-addons
Summary: Migrate kernel-uki-virt-addons 6.12.x to kernel-clk6.12-uki-virt-addons
Obsoletes: kernel-uki-virt-addons <= %{old_ver5}
Requires:  kernel-clk6.12-uki-virt-addons

%description uki-virt-addons
Migrates kernel-uki-virt-addons 6.12.x to kernel-clk6.12-uki-virt-addons.

# ===== Build =====

%prep
%build

%install
mkdir -p %{buildroot}%{_libexecdir}/kernel-clk6.12-migrate
mkdir -p %{buildroot}%{_unitdir}

# Cleanup script: removes old non-namespaced 6.12 installonly packages
cat > %{buildroot}%{_libexecdir}/kernel-clk6.12-migrate/cleanup-old-kernels.sh << EOFSCRIPT
#!/bin/bash
# Remove old non-namespaced CIQ 6.12 installonly kernel packages.
# Only removes specific released versions, and only if not the running kernel.
# Installed by kernel-clk6.12-migrate, runs once on boot via systemd.

RUNNING=\$(uname -r)
REMOVED=0

for ver in \
    %{old_ver1} \
    %{old_ver2} \
    %{old_ver3} \
    %{old_ver4} \
    %{old_ver5} \
; do
    # Check if this version's core package is installed
    rpm -q "kernel-core-\${ver}" &>/dev/null || continue
    # Check if this is the running kernel
    pkg_uname=\$(rpm -q --qf '%%{VERSION}-%%{RELEASE}.%%{ARCH}\n' "kernel-core-\${ver}" 2>/dev/null)
    [ "\$pkg_uname" = "\$RUNNING" ] && continue
    # Not the running kernel, safe to remove all installonly siblings
    echo "Removing old non-namespaced kernel \${ver}..."
    # Collect only packages that are actually installed
    TO_REMOVE=""
    for pkg in \
        "kernel-\${ver}" \
        "kernel-core-\${ver}" \
        "kernel-modules-\${ver}" \
        "kernel-modules-core-\${ver}" \
        "kernel-modules-extra-\${ver}" \
        "kernel-modules-internal-\${ver}" \
        "kernel-modules-partner-\${ver}" \
        "kernel-devel-\${ver}" \
        "kernel-devel-matched-\${ver}" \
        "kernel-uki-virt-\${ver}" \
        "kernel-uki-virt-addons-\${ver}" \
    ; do
        rpm -q "\$pkg" &>/dev/null && TO_REMOVE="\$TO_REMOVE \$pkg"
    done
    if [ -n "\$TO_REMOVE" ]; then
        echo "Removing:\$TO_REMOVE"
        rpm -e \$TO_REMOVE 2>&1 || { rc=\$?; echo "rpm -e failed with exit code \$rc"; }
    fi
    REMOVED=1
done

# Disable the service and remove migration shim packages if cleanup is complete
if [ "\$REMOVED" = "1" ] || ! rpm -qa | grep '^kernel-core-6\.12\.' 2>/dev/null | grep -qv clk6.12; then
    systemctl disable kernel-clk6.12-migrate-cleanup.service 2>/dev/null || true
    # Remove migration shim packages via rpm -e (lighter than dnf at boot).
    # Namespaced packages survive because %posttrans marked them as user-installed.
    SHIMS=\$(rpm -qa | grep '^kernel-clk6\.12-migrate' || true)
    if [ -n "\$SHIMS" ]; then
        echo "Removing migration shims..."
        rpm -e \$SHIMS 2>/dev/null || true
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
# the migration shim packages. Runs after the entire transaction completes.
# After this, users can safely run: dnf remove 'kernel-clk6.12-migrate*'
%posttrans
# Mark namespaced packages as user-installed so they survive shim removal
for pkg in \
    kernel-clk6.12 \
    kernel-clk6.12-core \
    kernel-clk6.12-modules-core \
    kernel-clk6.12-headers \
    kernel-clk6.12-doc \
    kernel-clk6.12-cross-headers \
    kernel-clk6.12-tools \
    kernel-clk6.12-tools-libs \
    kernel-clk6.12-tools-libs-devel \
    kernel-clk6.12-devel \
    kernel-clk6.12-devel-matched \
    perf-clk6.12 \
    python3-perf-clk6.12 \
    libperf-clk6.12 \
    libperf-clk6.12-devel \
    rtla-clk6.12 \
    rv-clk6.12 \
    kernel-clk6.12-selftests-internal \
    kernel-clk6.12-ipaclones-internal \
    kernel-clk6.12-abi-stablelists \
    kernel-clk6.12-uki-virt-addons \
; do
    rpm -q "$pkg" &>/dev/null && dnf mark install "$pkg" -y &>/dev/null || true
done

# Enable the cleanup service to remove old installonly packages on next boot.
# Can't use rpm -e from %posttrans (RPM db lock held), so defer to systemd.
systemctl enable kernel-clk6.12-migrate-cleanup.service &>/dev/null || true

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
