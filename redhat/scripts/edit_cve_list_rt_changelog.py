#!/usr/bin/env python3

from __future__ import print_function
import sys
import time
import bugzilla
import xmlrpc.client


# list of RT bzs to be added to the Resolves: line  of the changelog
resolves_bzs = {}

def usage():
    print("""Usage:\n\n\t%s <filename>

    This script sweeps a changelog file looking for CVEs. Whenever a CVE
    line is found the RHEL tracker BZs found in that line are replaced by
    the equivalent RHEL-RT BZ trackers.
""" % sys.argv[0])
    sys.exit(1)


def api_key_help():
    print("""Error: API_KEY for bugzilla.redhat.com not found.

    You will be prompted for an API KEY in order to authenticate yourself
    and perform the necessary Bugzilla operations.
    In case you don't have an API_KEY or want to create a new one to use
    exclusively with the hack-ish scripts, please got to:

        https://bugzilla.redhat.com/userprefs.cgi?tab=apikey

    But please consider creating a ~/.bugzillarc file with the following
    content to automate your authentication:

        [bugzilla.redhat.com]
        api_key = <your API key>

""")


def passes_filter(value, criteria):
    if criteria:
        for flt in criteria:
            if (flt.upper() in value.upper()):
                return True
        return False
    return True


def summary_lists_cve(summary, cvelist):
    for cve in summary.split():
        if cve[0:4] != "CVE-":
            return False
        if cve in cvelist:
            return True


def cve_list_from_summary(summary):
    cvelist = []
    for cve in summary.split():
        if cve[0:4] != "CVE-":
            break
        if cve not in cvelist:
            cvelist.append(cve)
    return cvelist


def is_tracker_creation_header(line):
    header = "Created <product> tracking bugs for this issue:"
    if (line[:8] == header[:8]) and (line[-30:] == header[-30:]):
        return True
    return False


def get_product_from_header(line):
    return line.split()[1]


def is_tracker_definition(line):
    line = line.split()
    if len(line) != 4:
        return False
    if (line[0] == "Affects:") and (line[2] == "[bug") and \
          (line[1][:5] == "rhel-") and (line[3][-1] == "]"):
       return True
    return False


def buglist_from_comments(comments):
    buglist = {}
    for entry in comments:
        start = False
        product = None
        text = entry['text'].split("\n")

        for line in text:
            if not start:
                if not is_tracker_creation_header(text[0]):
                    continue
                start = True

            if is_tracker_creation_header(line):
                product = get_product_from_header(line)
                if product not in buglist.keys():
                    buglist[product] = {}
                continue

            if is_tracker_definition(line):
                aux = line.split()
                release = aux[1]
                bz = aux[3][:-1]
                if product:
                    buglist[product][bz] = release
                    buglist[product][release] = bz

    return buglist


def format_changelog(summary, bz, cvenames):
    line = summary.split()
    if line[-1][0] == "[" and line[-1][-1] == "]":
        end = -1
    else:
        end = None
    start = 0
    for aux in line:
        if aux[:4] != "CVE-":
            break
        start += 1

    return "- %s [%s] {%s}" % (" ".join(line[start:end]), bz, cvenames)


def setup_bugzilla(url):
    try:
        bzapi = bugzilla.Bugzilla(url)
    except (xmlrpc.client.Fault, bugzilla.BugzillaError) as REASON:
        # Ask for the API key
        api_key_help()
        api_key = input("Enter Bugzilla API Key: ")
        # Try to log in again
        try:
            bzapi = bugzilla.Bugzilla(url, api_key=api_key)
        except (xmlrpc.client.Fault, bugzilla.BugzillaError) as REASON2:
            print("\nError in the login process. Please check your API key.\n")
            sys.exit(1)

    return bzapi


def setup_bugzilla_cve_query(bzapi, URL, cve_list):
    # list of fields we want from every BZ
    bz_query_fields = [ 'alias', 'assigned_to', 'blocks', 'bugzilla', 'cc',
        'classification', 'component', 'creation_time', 'creator', 'deadline',
        'depends_on', 'docs_contact', 'fixed_in', 'flags', 'groups', 'id',
        'keywords', 'op_sys', 'platform', 'priority', 'product', 'qa_contact',
        'resolution', 'see_also', 'severity', 'status', 'summary', 'version',
        'versions', 'weburl', 'whiteboard' ]

    # Query for the tracker BZs of the requested CVEs
    qry_cgi = '/buglist.cgi?'
    qry_product = 'product=Security Response'
    qry_CVElist = '&short_desc=' + " ".join(cve_list)
    qry_type = '&short_desc_type=anywords'

    query = bzapi.url_to_query(URL + qry_cgi + qry_product + qry_CVElist + qry_type)

    query["include_fields"] = bz_query_fields

    return query


def setup_bugzilla_rt_query(bzapi, URL, bz_list):
    # list of fields we want from every BZ
    bz_query_fields = [ 'alias', 'assigned_to', 'blocks', 'bugzilla', 'cc',
        'classification', 'component', 'creation_time', 'creator', 'deadline',
        'depends_on', 'docs_contact', 'fixed_in', 'flags', 'groups', 'id',
        'keywords', 'op_sys', 'platform', 'priority', 'product', 'qa_contact',
        'resolution', 'see_also', 'severity', 'status', 'summary', 'version',
        'versions', 'weburl', 'whiteboard' ]

    # Query for the tracker BZs of the requested CVEs
    qry_cgi = '/buglist.cgi?'
    qry_BZlist = 'quicksearch=' + " ".join(bz_list)

    query = bzapi.url_to_query(URL + qry_cgi + qry_BZlist)

    query["include_fields"] = bz_query_fields

    return query


def cve_changelog_line(line):
    if line.find("{CVE-") > 0:
        return True
    return False


def get_cve_from_line(line):
    cve_st = line.find("{CVE-") + 1
    cve_end = line.rfind("}")
    bz_st = line.rfind("[") + 1
    bz_end = line.rfind("]")
    cve = line[cve_st:cve_end]
    bzs = line[bz_st:bz_end]
    return cve, bzs


def jira_changelog_line(line):
    if line.rfind('[RHEL-') > 0:
        return True
    return False


def get_jira_issues_from_line(line):
    jira_st = line.rfind("[") + 1
    jira_end = line.rfind("]")
    return line[jira_st:jira_end]


def cves_from_changelog(data):
    cves = {}
    cves['list'] = []
    for line in data:
        if cve_changelog_line(line):
            cve, bzs = get_cve_from_line(line)
            cves[cve] = bzs
            for aux in cve.split():
                if aux not in cves['list']:
                    cves['list'].append(aux)

    if not cves['list']:
        return None

    return cves


def get_rt_cves(cve, bzs, cvelist):
    rtbzs = []
    for bz in bzs.split():
        if not cve in cvelist.keys():
            print("Error: %s has no entry for kernel in ProdSec Bugzilla tickets." % cve)
            return None
        if not bz in cvelist[cve]['kernel'].keys():
            print("Error: BZ %s is not listed as a kernel tracker for %s." % (bz, cve))
            return None
        rel = cvelist[cve]['kernel'][bz]
        if not 'kernel-rt' in cvelist[cve].keys():
            print("Error: %s has no entry for kernel-rt in ProdSec Bugzilla tickets." % cve)
            return None
        if not rel in cvelist[cve]['kernel-rt'].keys():
            print("Error: no kernel-rt tracker listed for %s (kernel:%s) on %s." % (rel, bz, cve))
            return None
        rt = cvelist[cve]['kernel-rt'][rel]
        rtbzs.append(rt)
        if not bz in resolves_bzs.keys():
            resolves_bzs[bz] = rt
        print("    %-15s %-12s %8s  -->  %8s" % (cve, rel, bz, rt))

    return " ".join(rtbzs)


def prepare_resolves_line(line):
    tmp = []
    bzs = []
    for aux in line.split("rhbz#"):
        tmp.append(aux.split()[0].split(",")[0])
    # replace the CVE tracker we found earlier
    for aux in tmp[1:]:
        if aux in resolves_bzs.keys():
            bzs.append(resolves_bzs[aux])
        else:
            bzs.append(aux)
    return bzs


def print_rt_cve_line(line, cvelist, fp):
    cve, bzs = get_cve_from_line(line)
    rtbzs = get_rt_cves(cve, bzs, cvelist)
    if rtbzs:
        bzidx = line.rfind("[")
        print("%s[%s] {%s}" % (line[:bzidx], rtbzs, cve), file=fp)
    else:
        print(line[:-1], file=fp)


def print_resolves_line(bugs, jira_issues, fp):
    line = "Resolves:"
    for bz in bugs:
        if bz.component == "kernel-rt":
            line = "%s rhbz#%s," % (line, bz.id)
    for issue in jira_issues:
        line = "%s %s," % (line, issue)
    print(line[:-1], file=fp)


## -------- MAIN

products = ['kernel', 'kernel-rt']
# list of Jira issues  mentioned in the original changelog
jira_issues = []

if len(sys.argv) < 2:
    # fp = open(sys.stdin)
    usage()
    sys.exit(1)

# open and read the changelog file
fp = open(sys.argv[1], 'r')
data = fp.readlines()
fp.close()

# create the new (rt) changelog file
fp = open(sys.argv[1] + ".rt", 'w')

# get the list of CVEs ( cve_list)
cve_list = cves_from_changelog(data)

# public instance of bugzilla.redhat.com.
URL = "bugzilla.redhat.com"
bzapi = setup_bugzilla(URL)

# even if there are CVEs in the changelog, perform the magic
if cve_list:
    query = setup_bugzilla_cve_query(bzapi, URL, cve_list['list'])
    bugs = bzapi.query(query)

    # Organize the CVE bugs
    cvelist = {}
    for bz in bugs:
        if summary_lists_cve(bz.summary, cve_list['list']):
            cves = cve_list_from_summary(bz.summary)
            cveidx = " ".join(cves)
            cvelist[cveidx] = {}
            buglist = buglist_from_comments(bz.getcomments())
            cvelist[cveidx] = buglist

    if len(bugs):
        print("Replacing kernel CVE trackers by their RT equivalents...")
        print("    %-15s %-12s  %-8s       %-8s" % \
              ("CVE --", "RELEASE", "KERNEL", "RT"))
else:
    print("No CVEs entries found in this changelog...")

for line in data:
    # is this a Jira issue entry?
    bugs = []
    if jira_changelog_line(line):
        for aux in get_jira_issues_from_line(line).split():
            if aux not in jira_issues:
                jira_issues.append(aux)
        # print the original changelog line
        print(line[:-1], file=fp)
        continue
    if not cve_changelog_line(line):
        # is this the Resolves: line?
        if line[:9] == "Resolves:":
            print("Removing non-rt entries from the 'Resolves:' line...")
            bzs = prepare_resolves_line(line)
            if bzs:
                query = setup_bugzilla_rt_query(bzapi, URL, bzs)
                bugs = bzapi.query(query)
            else:
                print("No rt Bugzilla tickets in the Resolves line.")
            print_resolves_line(bugs, jira_issues, fp)
            continue
        # print the original changelog line
        print(line[:-1], file=fp)
        continue
    # print the line with the respective RT CVE trackers, if found
    print_rt_cve_line(line, cvelist, fp)

fp.close()
sys.exit(0)


