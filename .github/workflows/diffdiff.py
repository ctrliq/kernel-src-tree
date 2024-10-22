#!/usr/bin/env python3
# coding: utf-8
#

import argparse
import copy
import difflib
import io
import git
import os
import re
import subprocess
import sys
import tempfile

verbose = False


def get_upstream_commit(upstream, c):
    for l in c.message.splitlines():
        try:
            sha = re.match('\s*commit\s+(?P<sha>\S+)', l).groups()[0].upper()
            return upstream.commit(sha)
        except:
            True

def get_diff(d):
    dif = ''
    df = False
    for l in d.splitlines():
        if l[:10] == 'diff --git':
            df = True
        if not df:
            continue
        dif = dif + l + '\n'
    return dif


def trim_unchanged_files(lines):
    dl = []
    ld = 0       # Last line with a 'diff --git' we saw
    hd = False   # Have we seen a changed line since ld?
    i = 0
    for i, l in enumerate(lines):
        if l[:4] == '+++ ' or l[:4] == '--- ' :
            continue
        if l[0] == '+' or l[0] == '-':
            hd = True
        if l[:11] == ' diff --git':
            if ld:   # We are at a new diff now, last one started at 'ld'
                if not hd:
                    dl.insert(0, (ld, i+1),)
            ld = i
            hd = False # Reset hasdiff to False as we start a new section
    # and check the tail
    if not hd:
        dl.insert(0, (ld, i+1),)
    # delete the unchanged file sections
    for d in dl:
        del lines[d[0]:d[1]]
    return lines

            
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', action='store_true', help='Verbose')
    parser.add_argument('--colour', action='store_true', help='Colorize the diff. Green for additions, red for deletions')
    parser.add_argument('--commit', help='Commit in current tree to diffdiff. Default is the most recent commit.')
    parser.add_argument('--upstream', help='A directory that contains the current upstream of linus kernel tree where we can find the commits we reference. Default is the current repo')
    args = parser.parse_args()


    if args.v:
        verbose = True

    srcgit = git.Repo.init('.')
    upstream = git.Repo.init(args.upstream)
    c = srcgit.head.commit if not args.commit else srcgit.commit(args.commit)
    uc = get_upstream_commit(upstream, c)

    dc = get_diff(srcgit.git.show(c))
    duc = get_diff(upstream.git.show(uc))

    with open('c.diff', 'w') as f:
        f.write(dc)
    with open('u.diff', 'w') as f:
        f.write(duc)
        
    res = subprocess.run(['diff', '-u', 'u.diff', 'c.diff'],
                         check=False, stdout=subprocess.PIPE)
    lines = res.stdout.splitlines()
    dd = []
    for l in lines:
        l = str(l)[2:-1]
        if l[:6] == '-index':
            continue
        if l[:6] == '+index':
            continue
        if l[:3] == '-@@':
            continue
        if l[:3] == '+@@':
            dd.append(' ' + l[1:])
            continue
        dd.append(l)

    # trim diffs for files that did not change
    lines = trim_unchanged_files(dd)

    # colorize the diff
    diffs = 0
    if args.colour:
        dd = []
        for l in lines:
            if l[0:4] != '+++ ' and l[0:4] != '--- ':
                if l[0] == '+':
                    l = '\033[42m' + l + '\033[0m'
                    diffs = diffs + 1
                if l[0] == '-':
                    l = '\033[41m' + l + '\033[0m'
                    diffs = diffs + 1
            dd.append(l)
        lines = dd


    if diffs:
        for l in lines:
            print(l)

    sys.exit(diffs)
