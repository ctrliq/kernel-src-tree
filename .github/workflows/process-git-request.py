import sys
import subprocess
import os

requestors = {"gvrose8192": ""}

def file_prepend(file, str):
    with open(file, 'r') as fd:
        contents = fd.read()
        new_contents = str + contents
    
    # Overwrite file but now with prepended string on it
    with open(file, 'w') as fd:
        fd.write(new_contents)

def process_git_request(fname, target_branch, source_branch, prj_dir):
    retcode = 200  # presume success
    # print(f"Opening file {fname}")
    file = open(fname, "w")
    working_dir = prj_dir
    # print(f"Working Dir : {working_dir}")
    os.chdir(working_dir)
    # print(f"pwd : {os.getcwd()}")
    git_cmd = f"git log --oneline --no-abbrev-commit origin/{target_branch}..origin/{source_branch}"
    print(git_cmd)
    try:
        out, err = subprocess.Popen(git_cmd, shell=True, stdout=subprocess.PIPE, 
                                  stderr=subprocess.PIPE, text=True).communicate()
        if err:
            print(f"Command error output is {err}")
            file.write(f"Command error output is {err}")
            file.close()
            retcode = 201
            return retcode
            
        output_lines = out.split()
        # we just want the commit sha IDs
        for x in output_lines:
            print(f"This is output_lines {x}")
            
        file.close()
        return retcode
    except Exception as e:
        print(f"Error executing git command: {str(e)}")
        file.close()
        return 201

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

retcode = process_git_request(fname, target_branch, source_branch, prj_dir)

if retcode != 200:
    with open(fname, 'r') as fd:
        contents = fd.read()
        print(contents)
    sys.exit(1)
else:
    print("Done")

sys.exit(0)

