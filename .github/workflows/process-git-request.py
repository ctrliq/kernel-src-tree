#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import subprocess
import os
import re

def file_prepend(file, str):
    with open(file, 'r') as fd:
        contents = fd.read()
        new_contents = str + contents

    # Overwrite file but now with prepended string on it
    with open(file, 'w') as fd:
        fd.write(new_contents)

def process_git_request(fname, target_branch, source_branch, prj_dir):
    retcode = 0  # presume success
#    print(f"Opening file {fname}")
    file = open(fname, "w")
    working_dir = prj_dir
#    print(f"Working Dir : {working_dir}")
    os.chdir(working_dir)
#    print(f"pwd : {os.getcwd()}")
    git_cmd = f"git log --oneline --no-abbrev-commit " + target_branch + ".." + source_branch
    print(f"git command is {git_cmd}")
    loglines_to_check = 13
    try:
        out, err = subprocess.Popen(git_cmd, shell=True, stdout=subprocess.PIPE, 
                                  stderr=subprocess.PIPE, text=True).communicate()
        if err:
            print(f"Command error output is {err}")
            file.write(f"Command error output is {err}")
            file.close()
            return 1

        output_lines = out.splitlines()
        commit_sha = ""
        # we just want the commit sha IDs
        for x in output_lines:
            print(f"This is output_lines {x}")
            if not bool(re.search(r'[^\x30-\x39a-fA-F]', x)):  # equivalent to Ruby's !x[/\H/]
                continue
            else:
                y = x.split()
                print(f"This is y {y}")
                commit_sha = str(y[0])
                print(f"Found a commit in line ", commit_sha)

            git_cmd = "git show " + commit_sha
            gitlog_out, gitlog_err = subprocess.Popen(git_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True).communicate()

            loglines = gitlog_out.splitlines()
            lines_counted = 0
            local_diffdiff_sha = commit_sha
            upstream_diffdiff_sha = ""
            upstream_diff = False

            for logline in loglines:
                print(f"Processing logline {commit_sha}")
                lines_counted += 1
                if lines_counted == 1:
                    file.write("Merge Request sha: " + local_diffdiff_sha)
                    file.write("\n")
                if lines_counted == 2:  # email address
                    if "ciq.com" not in logline.lower():
                        # Bad Author
                        s = f"error:\nBad {logline}\n"
                        print(s)
                        file.write(s)
                        file.close()
                        return retcode
                if lines_counted > 1:
                    if "jira" in logline.lower():
                        file.write("\t" + logline + "\n")

                    if "upstream-diff" in logline.lower():
                        upstream_diff = True

                    if "commit" in logline.lower():
                        commit_sha = re.search(r'[0-9a-f]{40}', logline)
                        upstream_diffdiff_sha = str(commit_sha.group(0)) if commit_sha else ""
                        print(f"Upstream : " + upstream_diffdiff_sha)
                        if upstream_diffdiff_sha:
                            file.write("\tUpstream sha: " + upstream_diffdiff_sha)
                            file.write("\n")

                    if lines_counted > loglines_to_check:  # Everything we need should be in the first loglines_to_check lines
 #                       print(f"Breaking after {loglines_to_check} lines of commit checking")
                        break

            if local_diffdiff_sha and upstream_diffdiff_sha:
                diff_cmd = os.path.join(os.getcwd(), ".github/workflows/diffdiff.py") + " --colour --commit " + local_diffdiff_sha
 #               print("diffdiff: " + diff_cmd)
                process = subprocess.run(diff_cmd, shell=True, capture_output=True, text=True)
                diff_out = process.stdout
                diff_err = process.stderr
                diff_status = process.returncode

                if diff_status != 0 and not upstream_diff:
                    print(f"diffdiff out: " + diff_out)
                    print(f"diffdiff err: " + diff_err)
                    retcode = 1
                    file.write("error:\nCommit: " + local_diffdiff_sha + " differs with no upstream tag in commit message\n")
    except Exception as error:
        print(f"Exception in git log command error {error}")

    finally:
        file.close()

    return retcode

first_arg, *argv_in = sys.argv[1:]  # Skip script name in sys.argv

if len(argv_in) < 5:
    print("Not enough arguments: fname, target_branch, source_branch, prj_dir, pull_request, requestor")
    sys.exit()

fname = str(first_arg)
fname = "tmp-" + fname
# print("filename is " + fname)
target_branch = str(argv_in[0])
# print("target branch is " + target_branch)
source_branch = str(argv_in[1])
# print("source branch is " + source_branch)
prj_dir = str(argv_in[2])
# print("project dir is " + prj_dir)
pullreq = str(argv_in[3])
# print("pull request is " + pullreq)
requestor = str(argv_in[4])

retcode = process_git_request(fname, target_branch, pullreq, prj_dir)

if retcode != 0:
    with open(fname, 'r') as fd:
        contents = fd.read()
        print(contents)
    sys.exit(1)
else:
    print("Done")

sys.exit(0)

