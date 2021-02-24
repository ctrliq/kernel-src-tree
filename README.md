The current documentation is not in a format available for viewing in a
terminal.

See https://red.ht/kernel_workflow_doc for information on the Red Hat Kernel
Workflow.

The layout is

- docs/ contains documentation links, etc.
- scripts/ contains scripts for generating documentation, RHMAINTAINERS, etc.
- info/ contains the latest information (owners.yaml, etc.)

**Making Changes**

Changes must be made through a Merge Request to this project.  Changes must be
accompanied by a description of the modifications.  In most cases, a "Update
x86 maintainers" will do, however, the maintainers may ask for a more detailed
write-up.

Changes are NOT accepted for the RHMAINTAINERS or CODEOWNERS files, and changes
are only accepted for the owners.yaml file.  Merge requests must have changes
have owners.yaml changes isolated in one commit, and new versions of
RHMAINTAINERS & CODEOWNERS in a separate commit.  These secondary files can be
generated using the commands (executed from the top level of documentation)

```
        cd scripts
        make
        cd ../info
        ../scripts/yaml2CODEOWNERS owners.yaml > CODEOWNERS
        ../scripts/yaml2RHMAINTAINERS owners.yaml > RHMAINTAINERS
```
