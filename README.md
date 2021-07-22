All changes to this file and directories included at this level must be made
to the [Red Hat kernel documentation project](https://gitlab.com/redhat/rhel/src/kernel/documentation)
Please see the 'Making Changes' section below for detailed information on
providing changes for this project.

**Background**

The Red Hat kernel has long contained a RHMAINTAINERS file that is synonymous
with the upstream kernel's MAINTAINERS file.  The RHMAINTAINERS file contains
entries listing areas of responsiblity and the maintainers for each area, as
well as some additional data.

GitLab offers a similar file, called CODEOWNERS, that also provides the same
data.  In addition to that, some of the kernel webhooks also require data that
is mapped to areas of responsiblity in the kernel.

All this data is now unified in a owners.yaml file that is in the info/
directory.  It is now the canonical location for all information that must be
mapped to kernel areas of repsonsiblity.  As the name of the file indicates,
the file is in YAML format as many languages (python, go, etc.) include
YAML parsers.  The YAML file is parsed by scripts to create RHMAINTAINERS
and CODEOWNERS files that are always synchronized with the owners.yaml file.

**Code Approvals, maintainers, and reviewers**

Similar to the RHMAINTAINERS file, the owners.yaml file has two categories of
code approvers: maintainers, and reviewers.  Maintainers are engineers whose
approval are required, and reviewers are engineers that will be notified but
whose code approval is not required. Currently the notification of review for
both maintainers and reviewers occurs when the [kernel-webhooks](https://gitlab.com/cki-project/kernel-webhooks/) assign
"Reviewers" to a Merge Request.

**Making Changes**

Changes must be made through a Merge Request to the [Red Hat kernel documentation project](https://gitlab.com/redhat/rhel/src/kernel/documentation) project.  Changes must be
accompanied by a description of the modifications.  In most cases, a simple
explanation will do (for example, "Update x86 maintainers"), however the
maintainers may ask for a more detailed write-up.

Standalone changes are NOT accepted for the RHMAINTAINERS or CODEOWNERS files,
and changes are only accepted for the owners.yaml file.  Merge requests that
modify owners.yaml changes must include associated changes to RHMAINTAINERS &
CODEOWNERS.  These secondary files can be generated using these commands
executed from the top level of documentation:

```
	make # requires minimum golang version 1.14
```

Users making changes must include a "Signed-off-by:" tag on all commits that
acknowledges the DCO, https://developercertificate.org.

**Project Layout**

The layout is

- docs/ contains the general kernel workflow documentation, links, etc.

See https://red.ht/kernel_workflow_doc for information on the Red Hat Kernel
Workflow.

- scripts/ contains scripts for generating documentation, RHMAINTAINERS, etc.
- info/ contains the latest information (owners.yaml, etc.)


