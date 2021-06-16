# Red Hat KWF: Quick Start Guide (QSG)

[prarit Feb 5 2021: I can definitely use some help with this document.  I’m too close to the main KWF documentation to offer any sort of sane Quick Start Guide.  If you have any suggestions, or things that should be changed, please feel free to suggest changes!  Thanks :)]

These are simple instructions and command-line examples to start contributing to the Red Hat kernel.  This document is not meant to be an introduction to the Red Hat Kernel Workflow.  Questions on this document likely can be answered in the [main KWF documentation](https://red.ht/kernel_workflow_doc).


# Links to Important Docs

[What is a GitLab Fork?](https://docs.google.com/document/d/1z8pPGb4OP4Y8pKOG9UmlDlf1IWs-G0NfH7K_fWRLCBg/)

[CommitRules](https://docs.google.com/document/d/15Y8Io4N2gtvt1nn5WzfFtssiKz5vbS7YoBy81XMg9S0) ([https://red.ht/kwf_commit_rules](https://red.ht/kwf_commit_rules))

[Red Hat and GitLab Configuration](https://docs.google.com/document/d/17AIthPv1jCPWc9MdxKFMo55JG4sZERKpeo_uKtiEc8s)

[Gitlab ‘lab’ utility and the Red Hat Kernel](https://docs.google.com/document/d/145y8pf6tq1-H3GI3ZBmHypqUqZhVb-AsHpXdMhyDYxA/)

[GitLab ‘bichon’ utility and the Red Hat Kernel](https://docs.google.com/document/d/10CGz1kiUiatPiUGDcNvAMEAZNS6_QkJfqOBCDA6b1b4)

[Red Hat Specific ‘revumatic’ tool](https://gitlab.cee.redhat.com/kernel-review/revumatic/)


# Useful Hints



1. Setup Red Hat Bugzilla Account
    *   [https://bugzilla.redhat.com/createaccount.cgi](https://bugzilla.redhat.com/createaccount.cgi)
2. Setup [GitLab account SSO](https://docs.google.com/document/d/17AIthPv1jCPWc9MdxKFMo55JG4sZERKpeo_uKtiEc8s/edit#bookmark=id.foj3lmb03es), etc. ([https://www.gitlab.com](https://www.gitlab.com))
    *   Red SSO GitLab Login: [https://red.ht/GitLabSSO](https://red.ht/GitLabSSO) 
    *   Configure account access
        *   SSH: [https://docs.gitlab.com/ee/ssh/README.html#adding-an-ssh-key-to-your-gitlab-account](https://docs.gitlab.com/ee/ssh/README.html#adding-an-ssh-key-to-your-gitlab-account)
        *   API Personal Access Token (PAT) for tools: [https://docs.gitlab.com/ee/user/profile/personal_access_tokens.html](https://docs.gitlab.com/ee/user/profile/personal_access_tokens.html)
        *   Setup [lab](https://docs.google.com/document/d/145y8pf6tq1-H3GI3ZBmHypqUqZhVb-AsHpXdMhyDYxA/edit#heading=h.6qv6e1qo2vfr) and [bichon](https://docs.google.com/document/d/10CGz1kiUiatPiUGDcNvAMEAZNS6_QkJfqOBCDA6b1b4)
            *   RPMs, or installed & compiled from source
            *   Run tools and configure (requires API PAT)
3. Red Hat namespace for Project(s)

        [https://red.ht/GitLab](https://red.ht/GitLab) or


        [https://gitlab.com/redhat/rhel](https://gitlab.com/redhat/rhel)

4. Clone Project

        git clone &lt;project SSH>

5. Fork Project

        git clone &lt;project SSH>; cd project_name


        lab fork # kernel repos take a long time to fork.  Grab a coffee.

    *   ----Find the name of the fork

            git remote -v  | grep &lt;username>

6. Create an MR on main
    *   **BEFORE CREATING A MERGE REQUEST READ THE [CommitRules](https://docs.google.com/document/d/15Y8Io4N2gtvt1nn5WzfFtssiKz5vbS7YoBy81XMg9S0)**

        git checkout -b &lt;branch_name>


        # do work


        git push -u &lt;fork_name> &lt;branch_name>


        lab mr create  [&lt;origin>] # [verify the MR information](https://docs.google.com/document/d/1_qqRHloFRVAR8tqeqBJz0nrz5R-2DpSOq20gWZ6nikA/)

        *   Outputs a Merge Request URL that contains the MR ID
        *   _It is strongly recommended lab users use the lab mr create --draft option to pass the webhook checks, and when successful, remove the Draft status on the MR withcd _

				lab mr edit &lt;mrID> --ready 



7. [Create an MR against a specific branch](https://docs.google.com/document/d/1gNUkvexDbs_-VKK69RIW7Oz1BsnslNXJFoBcvbw_8no/edit#) (ie, z-stream)
8. Get a list of MRs

    bichon # available on main page


    or


    git fetch --all


        lab mr list --all

9. Checkout code from an MR

        git fetch --all


        lab mr list --all # to find the mrID


        lab mr checkout &lt;mrID>

10. Get patches from an MR

        git fetch --all


        lab mr checkout &lt;mrIgitnD>cd 


        git-format-patch -&lt;number_of_patches>


        	or


        git-format-patch origin/main	

11. View code without checkout

        bichon #select MR from main page


        or


        lab mr show --patch

12. Show comments on an MR

        bichon


        or


        lab mr show &lt;mrID> --comments

13. Comment on a MR
    *   Non-blocking

            bichon #select description, and add comment


            or


            lab mr comment &lt;mrID>

    *   Blocking (ie, NACK)

            bichon #select commit, add comment, select ‘Enable replies to comment’


            or


            lab mr discussion &lt;mrID>


            lab mr reply &lt;mrID>:&lt;comment_id>

14. Approve an MR

        bichon #select MR and ‘a’


        or


        lab mr approve &lt;mrID>

15. Unapprove an MR (ie, Rescind-Acked-by)

        bichon #select MR and ‘A’


        or


        lab mr unapprove &lt;mrID>

16. Close an MR

		lab mr close &lt;mrID>



17. [Updating or Fixing an MR](https://docs.google.com/document/d/1rlPfueLbE9cNqRu6yh8x4LkJKG2Dn0lH9uDF3iG1J4o/edit#heading=h.yp1gtaa8c6f0)


# Tips and Tricks



1. (Ab)use Draft/WIP state. Any Merge Request in Draft state will not generate emails to RHKL.
    1. You can open a Merge Request in Draft state, and leave it in that state until you’re happy with everything -- CommitRefs checks, Signoff checks all pass, MR description looks sane, etc. Move the MR to Ready when you’re happy.
    2. You can move a Merge Request from Ready back to Draft before you make updates to it. You can push additional commits, force-push modified commits, and update the MR description (add v2: notes) while it’s back in Draft state without generating any emails to RHKL, fix things up as needed, and then again, move to Ready to send emails. This use of Draft state also solves a problem where you want to update both commits and MR description, which when done while the MR is in Ready state, would send both a v2 and a v3.
    3. Create your MRs early, in Draft state, and feel free to continuously update them with additional code from upstream until you’re happy with the progress. For example, update your driver to 5.12-rc1 code, but leave the MR in Draft state while you bring in additional code up through 5.12.0 (and maybe even stable releases).
