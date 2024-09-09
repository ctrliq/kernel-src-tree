# CIQ Maintained Kernels
This is single kernel structure we will use to manage the CIQ Linux Kernel.
We will keep remote references to `kernel.org - mainline`; all relevant 
`CentOS-##-stream` repositories; and branch Rocky versions off the closest
CentOS base version.

This README is a How-To interact with the CIQ Kernel Repository because its
different than many other source code repositories. We are using this model as 
we want a common ancestor history between all branches regardless of mainline 
version as ALL kernels branch from a Tag release on `kernel.org`.  The common 
ancestry will help with utilizing `git cherry-pick` to move commits between the
various kernel version branches.

## How is this different?
For all non-CentOS-stream CentOS series, and all RESF based Rocky builds we run 
a rebuild history command against the src.rpm for that specific release. ex: 
`kernel-4.18.0-80.el8.src.rpm` is CentOS 8 and its the release 80 for CentOS
during for the 4.18 kernel. We do a build prep of the source and then attempt to 
replay all rpm changelog entries as commits via `git cherry-pick`.  If a commit
cannot be `cherry-picked` we will perform an empty commit with a stashed copy of
the resulting merge conflict in a 
`ciq/ciq_backports/<kernel_version>/<sha>.failed` file in case an engineer needs 
to references this in the future.  Once the our process reaches the top (most 
recent change) of the rpm change log the script will move everything out of the 
kernel repo then copy the `rpmbuild -bp` source output into the kernel repo. 
This allows for `git` to track all the changes that could not be attributed to
empty commits and changes we could not match with upstream commits.  Finally some 
additional data like `.git` and `ciq` and `src.rpm:SOURCES/*.confg` are copied 
into the repo path to make the repo as ready for a `make olddefconfig` and build 
as possible.

## Include Remotes
We cannot include/commit the remotes that would be a part of `.git/config` when
added.  We can however include a dot file in the base repo to set these up with
the following command.
```
git config --local include.path ../.gitremotes
```

## Structure
* main - README Only
* centos8 - Branch off v4.18 where all CentOS8 release are committed
* rocky8_y - Branch and tag off the nearest centos8 release where the minor release was forked.
* ciq8_y - This is when CIQ-LTS (Long Term Support) picks up from Rocky and RESF.
* centos9 - Branch Checkout off v5.14 + merge from CentOS-9-streams
* rocky9_y - Branch and tag off the nearest centos-9-stream where minor release was forked
* ciq9_y- Same as above

**Special Branches**
* rockyX_y-RT / ciqX_y-RT - Real Time Kernel is a separate kernel tree until CentOS 10 so we will treat this as a separate branch as the rocky8 and minor release process will need to be used here.
* sig-cloud-X-<kernel-version> - These are branches that will be used for the SIG-Cloud Kernel until it can be integrated into the RESF.  This is a "shifting base" branch that will only be used per kernel-version, if that kernel version updates all the additional content needs to be migrated to the newer branch.
* FIPS - FIPS kernels 
  * FIPS-X   -- Base for Certification 
  * FIPS-FT-X - Base for certification testing
  * FIPS-compatible-X - Extension of Certification branch (NO LONGER CERTIFIED)

Example Branch Diagram
```
* HEAD - Kernel.org
|
|  * HEAD - CentOS9-streams
|  |
|  |    * [HEAD->CIQ LTS 9.2] CVE
|  |    * CVE
|  |    * ciq9_2 - CIQ Rocky 9.2 Branch Create
|  |   /
|  |  ? Repeat per RESF build
|  |  |
|  |  * RESF tar.xz Splat
|  |  * Cherry-Picks - Multiple Commits
|  |  * rocky9_2 - Branch Create
|  | /
|  * centos9-streams/tags/kernel-5.14.0-284
|  * Merge CentOS9-Streams
|  * centos9 - Branch Create
| /
* kernel/tags/v5.14
|
|  * HEAD - CIQ Import of most recent CentOS base build until 8.10
|  |
|  |    * [HEAD->CIQ LTS 8.8] CVE
|  |    * CVE
|  |    * ciq8_8 - CIQ Rocky 8.8 Branch Create
|  |   /
|  |  ? Repeat per RESF build
|  |  |
|  |  * RESF tar.xz Splat
|  |  * Cherry-Picks - Multiple Commits
|  |  * rocky8_8 - Branch Create
|  | /
|  ? Repeat CentOS8.7 -> CentOS8.6:
|  * CentOS Tar.xz Splat
|  * Cherry-Picks - Multiple Commnits
|  |    * [HEAD->CIQ LTS 8.6] CVE
|  |    * CVE
|  |    * ciq8_6 - CIQ Rocky 8.6 Branch Create
|  |   /
|  |  ? Repeat per RESF build
|  |  |
|  |  * RESF tar.xz Splat
|  |  * Cherry-Picks - Multiple Commits
|  |  * rocky8_6 - Branch Create
|  | /
|  ? Repeat CentOS8.0 -> CentOS8.6:
|  * CentOS Tar.xz Splat
|  * Cherry-Picks - Multiple Commnits
|  |
|  |  
|  * centos8 - Branch Create
| /
* kernel/tags/v4.18
|
|
* 1st Commit Kernel
```

Branches
```
FIPS-8/4.18.0-425.13.1    : FIPS-8 production branch
FIPS-FT-8/4.18.0-425.13.1 : FIPS-8 Functional Test branch

FIPS-9/5.14.0-284.30.1    : FIPS-9 production branch

sig-cloud-8/latest     : Sig-Cloud-8 current branch.
sig-cloud-8/<base>     : Sig-Cloud-8 branch based on EL <base>.

sig-cloud-9/latest     : Sig-Cloud-9 current branch.
sig-cloud-9/<base>     : Sig-Cloud-9 branch based on EL <base>.

```
