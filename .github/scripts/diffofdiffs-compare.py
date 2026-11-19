#!/usr/bin/env python3
"""Compare each backport in a pull request against its upstream commit.

Each PR commit whose message names an upstream commit is compared with
that commit by diffofdiffs in tree mode. The outcomes are written to
comparisons.json for the comment workflow, and the HTML reports of the
backports that differ are embedded in one page with a report selector.

A rolling-release rebase PR ("[RLC-X] Rebase Custom Changes") names the
previous branch it cherry-picks from. Its commits are additionally
matched with the commit carrying the same subject on that branch, and
each pair is compared the same way; commits without a counterpart on
the previous branch are compared with upstream only. Those outcomes are
written to comparisons.json under "rebase", and the differing pairs are
embedded in a second report page.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

# GitHub's REST API lists at most this many commits for a pull request
MAX_COMMITS = 250
FETCH_SECONDS = 15 * 60
COMPARE_SECONDS = 2 * 60

REFERENCE = r"([0-9a-fA-F]{4,40})"
DECORATION = r"(?:HEAD -> |tag: )?[-A-Za-z0-9_./@+]+"
UPSTREAM_LINES = (
    re.compile(r"commit " + REFERENCE + r"(?: upstream\.| \(" + DECORATION +
               r"(?:, " + DECORATION + r")*\)| Author: [^\r\n]+)?[ \t]*"),
    re.compile(r"\[ Upstream commit " + REFERENCE + r" \][ \t]*"),
)
CHERRY_PICK_LINE = re.compile(
    r"[ \t]*\(cherry picked from commit " + REFERENCE + r"\)[ \t]*")

# A rebase PR names the previous branch its changes were cherry-picked
# from. The marker form is for the PR template; the prose form is what
# the rebase process already writes. Branch names keep the shape the
# workflow itself accepts.
BRANCH_NAME = r"[a-zA-Z0-9/_.{}-]{1,255}"
REBASE_MARKER = re.compile(
    r"<!--\s*diffofdiffs: previous-branch=(" + BRANCH_NAME + r")\s*-->")
REBASE_LEGACY = re.compile(
    r"Cherry-pick all code from previous branch `(" + BRANCH_NAME + r")`")
REBASE_TITLE = re.compile(r"^\[RLC-\d+\]\s+Rebase Custom Changes\b")
REBASE_TITLE_BRANCH = re.compile(r"^\[RLC-\d+\]\s+Rebase Custom Changes to (\S+)$")
# The custom changes of a rolling-release branch are the commits above
# its RHEL import tag, whose name is the branch's version-release with
# the rlc-N/ prefix replaced by resf_kernel-
IMPORT_TAG = re.compile(r"^rlc-\d+/(.+)$")
# When the import tag is unknown, match against a window of this many
# commits beyond the PR size instead of the exact custom-changes range
PREV_EXTRA_DEPTH = 100
# A counterpart that only added a .failed marker recorded a cherry-pick
# that failed because the fix had already arrived in the RHEL source drop
MARKER_PATH = re.compile(r"^ciq/ciq_backports/.*\.failed$")


class CompareError(Exception):
    pass


def run(argv, timeout):
    """Run a command and turn its failure into a CompareError with its stderr"""
    try:
        return subprocess.run(argv, capture_output=True, text=True, errors="replace",
                              check=True, timeout=timeout).stdout
    except subprocess.CalledProcessError as error:
        raise CompareError(f"{argv[0]} failed (exit {error.returncode}):\n"
                           f"{error.stderr.strip()}") from None
    except subprocess.TimeoutExpired:
        raise CompareError(f"{argv[0]} exceeded its {timeout} second limit") from None


def gh_api(path, *args):
    return run(["gh", "api", path, *args], 60)


def upstream_reference(message):
    """Return the upstream commit named by a backport's message, or "" """
    # A backport can name its upstream commit and also carry cherry-pick
    # trailers from intermediate branches. Those trailers only count when
    # the message has no explicit upstream reference.
    lines = [line.rstrip("\r") for line in message.split("\n")]
    references = {match[1].lower() for line in lines for pattern in UPSTREAM_LINES
                  if (match := pattern.fullmatch(line))}
    if not references:
        references = {match[1].lower() for line in lines
                      if (match := CHERRY_PICK_LINE.fullmatch(line))}

    # Abbreviated and full forms of the same commit are one reference
    reference = max(references, key=len, default="")
    if any(not reference.startswith(other) for other in references):
        raise CompareError("The commit message names more than one upstream commit")
    return reference


def previous_branch(title, body):
    """Return the previous branch named by a rebase PR, or "" """
    match = REBASE_MARKER.search(body or "")
    if not match and REBASE_TITLE.match(title or ""):
        match = REBASE_LEGACY.search(body or "")
    if not match:
        return ""
    # The name reaches git as one argument, but validate its shape anyway
    # so a malformed marker cannot spend a fetch on it
    if not re.fullmatch(BRANCH_NAME, match[1]):
        print(f"Previous-branch warning: {match[1]!r} is not a branch name; "
              "not comparing with a previous branch")
        return ""
    return match[1]


def fetch_previous_branch(repo, url, branch, commits):
    """Fetch a rebase PR's previous branch and return its tip, the commit
    of its RHEL import tag ("" when unknown) and whether the fetch had to
    fall back to a bounded window of recent commits"""
    tag = IMPORT_TAG.match(branch)
    boundary = ""
    if tag and _remote_ref_exists(url, f"refs/tags/resf_kernel-{tag[1]}"):
        try:
            # History above the import tag is exactly the branch's custom
            # changes. The exclusion takes the tag by name: the fetch
            # negotiation rejects a tag object ID here.
            run(["git", "-C", repo, "fetch", "--no-tags",
                 f"--shallow-exclude=resf_kernel-{tag[1]}", url,
                 f"refs/heads/{branch}:refs/prevbranch"], FETCH_SECONDS)
            # Fetch the tag itself so the import commit it names is
            # present: it bounds the subject listing below.
            run(["git", "-C", repo, "fetch", "--no-tags", "--depth=1", url,
                 f"refs/tags/resf_kernel-{tag[1]}:refs/tags/resf_kernel-{tag[1]}"],
                FETCH_SECONDS)
            # The import commit is also the first parent of the oldest
            # custom change; deepening one generation reconnects the
            # shallow graft so tree mode can read that parent.
            run(["git", "-C", repo, "fetch", "--no-tags", "--deepen=1", url,
                 f"refs/tags/resf_kernel-{tag[1]}:refs/tags/resf_kernel-{tag[1]}"],
                FETCH_SECONDS)
            boundary = run(["git", "-C", repo, "rev-parse",
                            f"refs/tags/resf_kernel-{tag[1]}^{{commit}}"], 60).strip()
            return _previous_tip(repo), boundary, False
        except CompareError:
            pass  # the import tag is unknown; fall back to the window
    # The window reaches one generation beyond the commits the subject
    # listing indexes, so the oldest windowed commit's first parent, which
    # tree mode reads, is present
    run(["git", "-C", repo, "fetch", "--no-tags",
         f"--depth={commits + PREV_EXTRA_DEPTH + 1}", url,
         f"refs/heads/{branch}:refs/prevbranch"], FETCH_SECONDS)
    return _previous_tip(repo), boundary, True


def _remote_ref_exists(url, ref):
    """Whether a ref exists on the remote"""
    try:
        return bool(run(["git", "ls-remote", url, ref], 60).split())
    except CompareError:
        return False


def _previous_tip(repo):
    return run(["git", "-C", repo, "rev-parse", "refs/prevbranch"], 60).strip()


def marker_commit(repo, sha):
    """Whether a previous-branch commit only added .failed rebuild markers"""
    # A counterpart counts as marker-only when every changed path is a
    # marker addition: a commit that also changed code, removed a stale
    # marker or renamed one is a real change
    listing = run(["git", "-C", repo, "diff-tree", "--no-commit-id",
                   "--name-status", "-r", sha], 60)
    changes = [line.split("\t") for line in listing.splitlines()]
    return bool(changes) and all(len(change) == 2 and change[0] == "A"
                                 and MARKER_PATH.match(change[1])
                                 for change in changes)


def fetch(repo, url, commits, depth):
    """Fetch commits with just enough history for diffofdiffs to diff each one"""
    run(["git", "-C", repo, "fetch", "--no-tags", f"--depth={depth}", url,
         *(f"{sha}:refs/comparisons/{sha}" for sha in sorted(set(commits)))],
        FETCH_SECONDS)


def fail(result, error):
    result["status"] = "failed"
    result["error"] = str(error)[:4000]


def compare(repository, pr_number, pr_commits, head_url, head_sha, repo, binary, reports,
            title="", body="", base_ref=""):
    """Compare every backport, and for a rebase PR each previous-branch
    counterpart; return the upstream results, the rebase summary (None
    for a PR that names no previous branch) and the previous-branch
    report entries"""
    commits = gh_api(f"repos/{repository}/pulls/{pr_number}/commits",
                     "--paginate", "--jq", ".[].sha").split()
    if len(commits) != pr_commits:
        raise CompareError(f"The PR has {pr_commits} commits, but GitHub lists "
                          f"{len(commits)} of them (the limit is {MAX_COMMITS})")
    run(["git", "init", "-q", "--bare", repo], 60)
    fetch(repo, head_url, [head_sha], len(commits) + 1)

    # A rebase PR names the previous branch its changes were cherry-picked
    # from. Fetching that branch gives its custom changes, which each PR
    # commit's subject is matched against below.
    rebase = None
    prev_index = {}
    previous = previous_branch(title, body)
    if previous:
        rebase = {"previous_branch": previous}
        # The title and the base should name the same new branch
        titled = REBASE_TITLE_BRANCH.match(title or "")
        if titled and base_ref and titled[1] != base_ref:
            print(f"Previous-branch warning: the PR title names {titled[1]} "
                  f"but the PR base is {base_ref}")
        try:
            rebase["previous_sha"], boundary, rebase["partial"] = \
                fetch_previous_branch(repo, f"https://github.com/{repository}.git",
                                      previous, pr_commits)
            if boundary:
                # Exactly the branch's custom changes, bounded by the import
                listing = run(["git", "-C", repo, "log", "--format=%H|%s",
                               f"{boundary}..refs/prevbranch"], 60)
            else:
                # The fallback window: the fetch reaches one generation
                # beyond it, so the listing keeps to the window and the
                # shallow boundary commit stays out of the index while
                # every windowed commit keeps a readable first parent
                listing = run(["git", "-C", repo, "log", "--format=%H|%s",
                               "-n", str(pr_commits + PREV_EXTRA_DEPTH),
                               "refs/prevbranch"], 60)
            for line in listing.splitlines():
                sha, _, subject = line.partition("|")
                # The listing is newest first; the newest commit with a
                # duplicated subject wins
                prev_index.setdefault(subject, sha)
        except CompareError as error:
            rebase["error"] = str(error)[:4000]

    results = []
    for sha in commits:
        result = {"backport": sha, "subject": "", "status": "no_reference"}
        results.append(result)
        try:
            # %s folds a wrapped subject into one line the way the
            # previous-branch index keys do; %B keeps the raw message for
            # the upstream-reference lookup, and the NUL separates them
            subject, _, message = run(
                ["git", "-C", repo, "log", "-1", "--format=%s%x00%B", sha],
                60).partition("\0")
            # The full subject matches the previous-branch index below;
            # the comment workflow truncates it for display
            result["subject"] = subject
            reference = upstream_reference(message)
            if not reference:
                continue
            result["reference"] = reference

            # Resolve abbreviated references and confirm the commit exists
            # before fetching, since one missing commit fails the whole fetch
            try:
                commit = json.loads(gh_api(f"repos/{repository}/commits/{reference}"))
            except CompareError as error:
                raise CompareError(f"Cannot find upstream commit {reference} in "
                                   f"{repository}: {error}") from None
            result["upstream"] = commit["sha"]
            result["status"] = "pending"
        except CompareError as error:
            fail(result, error)

    # Backports commonly share upstream commits and source trees, so one
    # fetch of every upstream commit avoids repeated negotiation and downloads
    pending = [result for result in results if result["status"] == "pending"]
    try:
        fetch(repo, f"https://github.com/{repository}.git",
              [result["upstream"] for result in pending], 2)
    except CompareError as error:
        for result in pending:
            fail(result, error)
        pending = []

    html_by_backport = {}
    for result in pending:
        report = Path(repo).with_name("report.html")
        report.unlink(missing_ok=True)
        try:
            run([binary, "--html", "--backport-labels", f"--git-tree={repo}",
                 "-o", str(report), result["backport"], result["upstream"]],
                COMPARE_SECONDS)
        except CompareError as error:
            fail(result, error)
            continue

        # diffofdiffs writes nothing when the backport matches its upstream
        size = report.stat().st_size if report.exists() else 0
        result["status"] = "differs" if size else "matches"
        if size:
            html = report.read_text(encoding="utf-8")
            reports.append({"backport": result["backport"], "subject": result["subject"],
                            "html": html})
            html_by_backport[result["backport"]] = html

    prev_reports = []
    if rebase is not None and "error" not in rebase:
        # The commits without a counterpart keep their upstream comparison
        # as their only reference view; the ones with a counterpart are
        # compared against the previous branch, where matching the shipped
        # history matters more than matching upstream
        counterparts = {result["backport"]: prev_index.get(result["subject"])
                        for result in results}
        entries = []
        for result in results:
            counterpart = counterparts[result["backport"]]
            if not counterpart:
                continue
            entry = {"backport": result["backport"], "subject": result["subject"],
                     "counterpart": counterpart}
            entries.append(entry)
            report = Path(repo).with_name("report.html")
            report.unlink(missing_ok=True)
            try:
                # A probe failure is this pair's failure, not a reason to
                # discard the completed results of the whole comparison
                entry["marker"] = marker_commit(repo, counterpart)
                run([binary, "--html", "--backport-labels", f"--git-tree={repo}",
                     "-o", str(report), result["backport"], counterpart],
                    COMPARE_SECONDS)
            except CompareError as error:
                entry["status"] = "failed"
                entry["error"] = str(error)[:4000]
                continue
            size = report.stat().st_size if report.exists() else 0
            entry["status"] = "differs" if size else "matches"
            if size:
                prev_reports.append({"backport": result["backport"],
                                      "subject": result["subject"],
                                      "html": report.read_text(encoding="utf-8")})
        rebase["matches"] = entries
        rebase["fallback"] = [{"backport": result["backport"], "subject": result["subject"]}
                              for result in results
                              if not counterparts[result["backport"]]]
        # Fallback backports that differ from upstream belong in the
        # previous-branch report too, so one artifact serves both tables
        for result in results:
            if (counterparts[result["backport"]] is None
                    and result["status"] == "differs"):
                prev_reports.append({"backport": result["backport"],
                                     "subject": result["subject"],
                                     "html": html_by_backport[result["backport"]]})

        # Previous-branch changes no PR commit carries were either replaced
        # by their revert ("skipping unneeded code") or dropped entirely
        pr_subjects = {result["subject"] for result in results}
        skipped = []
        for subject, sha in prev_index.items():
            if subject in pr_subjects:
                continue
            kind = ("revert-carried" if f'Revert "{subject}"' in pr_subjects
                    else "dropped")
            skipped.append({"sha": sha, "subject": subject, "kind": kind})
        rebase["skipped"] = skipped
    return results, rebase, prev_reports


def write_page(reports, path):
    """Embed the reports in the selector page beside this script"""
    template = Path(__file__).with_name("diffofdiffs-report.html")

    # Escaping "<" keeps a report from closing the script element carrying it
    payload = json.dumps(reports, ensure_ascii=True).replace("<", "\\u003c")
    path.write_text(template.read_text(encoding="utf-8").replace("@@REPORTS@@", payload),
                    encoding="utf-8")


def main():
    repository = os.environ["GITHUB_REPOSITORY"]
    head_sha, base_sha = os.environ["HEAD_SHA"], os.environ["BASE_SHA"]
    head_url = os.environ["HEAD_CLONE_URL"]
    if not all(re.fullmatch(r"[0-9a-f]{40}", sha) for sha in (head_sha, base_sha)):
        sys.exit("The pull request commit IDs are malformed")
    if not head_url.startswith("https://github.com/"):
        sys.exit("The pull request repository must be on github.com")
    os.environ["GIT_TERMINAL_PROMPT"] = "0"

    output = Path(os.environ["OUTPUT_DIR"])
    output.mkdir(parents=True, exist_ok=True)
    attempt = int(os.environ["GITHUB_RUN_ATTEMPT"])
    summary = {"head_sha": head_sha, "base_sha": base_sha, "run_attempt": attempt,
               "results": []}
    reports = []
    prev_reports = []
    try:
        summary["results"], rebase, prev_reports = compare(
            repository, int(os.environ["PR_NUMBER"]), int(os.environ["PR_COMMITS"]),
            head_url, head_sha, str(output / "repo.git"), os.environ["DIFFOFDIFFS"],
            reports, title=os.environ.get("PR_TITLE", ""),
            body=os.environ.get("PR_BODY", ""),
            base_ref=os.environ.get("PR_BASE_REF", ""))
        if not summary["results"]:
            raise CompareError("No comparison results were produced for this pull request")
        if rebase is not None:
            summary["rebase"] = rebase
    except CompareError as error:
        summary["error"] = str(error)[:4000]
    if reports:
        write_page(reports, output / f"diffofdiffs-report-{attempt}.html")
    if prev_reports:
        write_page(prev_reports,
                   output / f"diffofdiffs-report-prevbranch-{attempt}.html")
    (output / "comparisons.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")

    failed = "error" in summary
    for result in summary["results"]:
        print(f"{result['status']:>12}  {result['backport'][:12]}  {result['subject']}")
        if "error" in result:
            failed = True
            print("              " + result["error"].replace("\n", "\n              "))
    rebase = summary.get("rebase")
    if rebase is not None:
        if "error" in rebase:
            # A previous-branch comparison that could not run at all is
            # at least as much a failure as one that failed per commit
            failed = True
            print(f"previous-branch comparison error: {rebase['error']}")
        else:
            for entry in rebase["matches"]:
                print(f"{entry['status']:>12}  {entry['backport'][:12]}  "
                      f"(previous branch)  {entry['subject']}")
                if "error" in entry:
                    failed = True
                    print("              " + entry["error"].replace("\n", "\n              "))
            # The skipped list is only fully visible here: the comment
            # workflow truncates it and points the rest here
            for entry in rebase["skipped"]:
                print(f"{entry['kind']:>12}  {entry['sha'][:12]}  "
                      f"(previous branch)  {entry['subject']}")
    if "error" in summary:
        print(f"Comparison incomplete: {summary['error']}", file=sys.stderr)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
