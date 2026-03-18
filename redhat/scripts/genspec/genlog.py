#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2023 Red Hat, Inc.

"""Parses git log from stdin, expecting format of:
$ git log [<options>] -z --format="- %s (%an)%n%b" [<range>] [[--] <path>...] | ...
and prints changelog output to stdout."""

import re
import sys


# CommitTags and parse_commit() in kmt and genlog.py should match
class CommitTags:
    tag_patterns = {
        'Bugzilla': [
            r'(\d{4,8})\s*$',
            r'https?://bugzilla\.redhat\.com/(?:show_bug\.cgi\?id=)?(\d{4,8})',
        ],
        'JIRA': [r'https://(?:issues\.redhat\.com|redhat\.atlassian\.net)/(?:browse|projects/RHEL/issues)/(RHEL-\d{1,8})'],
        'CVE': [r'(CVE-\d{4}-\d{4,7})'],
        'MR': [r'(.*)'],
        'Y-Commit': [r'([0-9a-fA-F]+)'],
        'Patchwork-id': [r'(.*)'],
        'Patchwork-instance': [r'(.*)'],
    }
    tag_patterns['Y-Bugzilla'] = tag_patterns['Bugzilla']
    tag_patterns['Y-JIRA'] = tag_patterns['JIRA']
    tag_patterns['O-JIRA'] = tag_patterns['JIRA']

    compiled_patterns = {}
    for tag_name, tag_pattern_list in tag_patterns.items():
        tag_pattern_list.append(r'(N/A)')
        compiled_patterns[tag_name] = []
        for tag_pattern in tag_pattern_list:
            pattern = r'^' + tag_name + ': ' + tag_pattern + r'\s*$'
            tag_regex = re.compile(pattern, re.MULTILINE)
            compiled_patterns[tag_name].append(tag_regex)

    def __init__(self, input_str):
        self.parse_tags(input_str)

    def get_tag_values(self, tag_name):
        if tag_name not in CommitTags.tag_patterns:
            raise Exception('Unsupported tag name: ' + tag_name)
        return self.tag_dict[tag_name]

    def parse_tags(self, input_str):
        tag_values = {}
        for tag_name, tag_pattern_list in CommitTags.compiled_patterns.items():
            tag_values[tag_name] = set()
            for tag_regex in tag_pattern_list:
                for value in tag_regex.findall(input_str):
                    tag_values[tag_name].add(value)
        self.tag_dict = tag_values

    def get_changelog_str(self):
        chnglog = []
        tickets = sorted(self.tag_dict['Bugzilla']) + sorted(self.tag_dict['JIRA'])
        if tickets:
            chnglog.append('[' + ' '.join(tickets) + ']')
        if self.tag_dict['CVE']:
            chnglog.append('{' + ' '.join(self.tag_dict['CVE']) + '}')
        return ' '.join(chnglog)

    def get_resolved_tickets(self):
        ret = set()
        for ticket in sorted(self.tag_dict['Bugzilla']) + sorted(self.tag_dict['JIRA']):
            if ticket.isnumeric():
                ticket = 'rhbz#' + ticket
            ret.add(ticket)
        return ret


def parse_commit(commit):
    return CommitTags(commit)


if __name__ == "__main__":
    all_resolved = set()
    commits = sys.stdin.read().split('\0')
    for c in commits:
        if not c:
            continue
        # Escape '%' as it will be used inside the rpm spec changelog
        entry_pos = c.find('\n')
        entry = c[:entry_pos].replace("%", "%%")

        tags = parse_commit(c)
        chnglog = tags.get_changelog_str()
        if chnglog:
            entry = entry + ' ' + chnglog
        print(entry)
        all_resolved = all_resolved.union(tags.get_resolved_tickets())

    print('Resolves: ' + ', '.join(sorted(all_resolved)) + '\n')
