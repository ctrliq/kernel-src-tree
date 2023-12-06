#!/usr/bin/python3

# x86_disable_unused_drivers.py allow_list_file
#
# Automatically disable the driver which is not required by x86.
#
# Usage:
# 1. Create a JSON file in redhat/scripts/filter-tools/x86_allow folder.
# 2. Switch to the root of the kernel source tree and run the script.
#
# Example:
# $redhat/scripts/filter-tools/x86_disable_unused_drivers.py redhat/scripts/filter-tools/x86_allow/x86_accel_allow

import argparse
import jinja2
import json
import sys
import os
import re
import shutil

from git import Repo
from git import config
from git import Git

'''
The allow list files are placed in redhat/scripts/x86_allow folder.
The config file in x86 folder is used to disable the driver and
the example in JSON format is shown below:

{
  "name": "iio_accel",
  "driver_path": "drivers/iio/accel/",
  "redhat_config_path": "redhat/configs/fedora/generic",
  "redhat_x86_config_path": "redhat/configs/fedora/generic/x86",
  "allow_list": ["CONFIG_BMC150_ACCEL_I2C",
                 "CONFIG_DA280",
                 "CONFIG_HID_SENSOR_ACCEL_3D",
                 "CONFIG_KXCJK1013",
                 "CONFIG_MMA7660",
                 "CONFIG_MMA8452",
                 "CONFIG_MXC4005",
                 "CONFIG_MXC6255",
                 "CONFIG_IIO_ST_ACCEL_I2C_3AXIS",
                 "CONFIG_IIO_ST_ACCEL_3AXIS",
                 "CONFIG_BMC150_ACCEL"
                 ],
  "commit_title": "Disable drivers for Fedora x86",
  "commit_msg": "Disable the driver because the driver is not used on any x86 boards and listed below:\n\n{{ config_list }}"
}

{{ config_list }} is list of disabled config name and it will replaced by Jinja.

'''

class GitManager:
    def __init__(self, teardown=False):
        self.teardown = teardown
        self.environment = jinja2.Environment()

        try:
            path = os.getcwd()
            self.repo = Repo(path)
            self.git_obj = Git(path)
        except Exception as e:
            print(e)
            sys.exit(1)

        # switch to working branch
        try:
            branch = self.repo.active_branch
            self.base_branch = branch.name
            self.repo.git.checkout(b="wip/driver/x86_auto_disable")
        except Exception as e:
            print(e)
            sys.exit(1)

    def commit_patch(self, redhat_config_x86_path:str, files:list,
                     commit_title:str, commit_template:str):
        last_commit = None

        # get the last commit of the repo
        for commit in self.repo.iter_commits():
            last_commit = commit
            break

        for file in files:
            self.repo.index.add([os.path.join(redhat_config_x86_path, file)])

        template = self.environment.from_string(commit_template)
        file_names = ''.join([file + '\n' for file in files])
        replaced_msg = template.render(config_list=file_names)

        commit_msg = ("{}\n\n"
                      "{}".format(commit_title,
                                  replaced_msg))
        self.repo.index.commit(commit_msg)

        git_path = shutil.which("git")
        self.git_obj.execute([git_path, "commit", "--amend", "--signoff", "--no-edit"])
        self.git_obj.execute([git_path, "format-patch", last_commit.hexsha])

    def teardown_branch(self):
        self.repo.git.checkout(self.base_branch)
        self.repo.git.branch("-D", "wip/driver/unused_iio_accel")

def disable_driver(file:str, redhat_config_path:str, redhat_x86_config_path:str):
    valid = re.compile(file+"=m|y")
    with open(os.path.join(redhat_config_path, file), "r+") as f:
        lines = f.readlines()
        # match the first line of the config.
        # if the config is set, we create a CONFIG_ file in x86 folder
        # to disable the driver.
        if valid.match(lines[0]):
            with open(os.path.join(redhat_x86_config_path, file), "w") as x86:
                x86.write("# "+file+" is not set\n")

def get_kconfig(driver_path:str, redhat_config_path:str):
    config_list = []

    valid = re.compile("^config [A-Z0-9]+(_[A-Z0-9]*)*")
    with open(os.path.join(driver_path, "Kconfig")) as f:
        config = f.readlines()
        for line in config:
            if valid.match(line):
                config_list.append("CONFIG_"+line[7:-1])

    print("Kconfig are found in {}:".format(driver_path))
    for file in config_list:
        if os.path.isfile(os.path.join(redhat_config_path, file)):
            print(file)

    return config_list

def is_required(file, allow_list):
    if file in allow_list:
        return True
    else:
        return False

def config_clean(gitobj, driver_path:str, redhat_config_path:str,
                 x86_redhat_config_path:str, allow_list:list,
                 commit_title:str, commit_template:str):
    kconfig = get_kconfig(driver_path, redhat_config_path)
    file_list = []

    print("Start to scan the config files.")
    for file in os.listdir(redhat_config_path):
       if os.path.isfile(os.path.join(redhat_config_path, file)) and file in kconfig:
           if is_required(file, allow_list) == False:
               print("{} is not required.".format(file))
               disable_driver(file, redhat_config_path, x86_redhat_config_path)
               file_list.append(file)

    gitobj.commit_patch(x86_redhat_config_path, file_list, commit_title, commit_template)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file_name", help="The allow list file name")
    parser.add_argument("--teardown",
                        help="Delete the working branch (wip/driver/unused_iio_accel)",
                        action="store_true")
    args = parser.parse_args()

    gitobj = GitManager(args.teardown)

    file = args.file_name
    name = None
    driver_path = None
    redhat_config_path = None
    redhat_x86_config_path = None
    allow_list = None
    with open(file, "r") as json_file:
        try:
            json_data = json.load(json_file)
            name = json_data.get("name")
            driver_path = json_data.get("driver_path")
            redhat_config_path = json_data.get("redhat_config_path")
            redhat_x86_config_path = json_data.get("redhat_x86_config_path")
            allow_list = json_data.get("allow_list")
            commit_title = json_data.get("commit_title")
            commit_msg = json_data.get("commit_msg")
            print("Driver catalog name: {}".format(name))
            print("Driver path: {}".format(driver_path))
            print("Red Hat config path: {}".format(redhat_config_path))
            print("Red Hat x86 config path: {}".format(redhat_x86_config_path))
        except Exception as e:
            print("Error: ignore incorrect allow file.")

    config_clean(gitobj, driver_path, redhat_config_path,
                 redhat_x86_config_path, allow_list,
                 commit_title, commit_msg)

    #teardown the working branch
    if args.teardown:
        gitobj.teardown_branch()

if __name__ == "__main__":
    main()
