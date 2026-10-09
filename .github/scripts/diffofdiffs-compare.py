#!/usr/bin/env python3
"""Compare each backport in a pull request against its upstream commit.

Each PR commit whose message names an upstream commit is compared with
that commit by diffofdiffs in tree mode. The outcomes are written to
comparisons.json for the comment workflow, and the HTML reports of the
backports that differ are embedded in one page with a report selector.
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


def fetch(repo, url, commits, depth):
    """Fetch commits with just enough history for diffofdiffs to diff each one"""
    run(["git", "-C", repo, "fetch", "--no-tags", f"--depth={depth}", url,
         *(f"{sha}:refs/comparisons/{sha}" for sha in sorted(set(commits)))],
        FETCH_SECONDS)


def fail(result, error):
    result["status"] = "failed"
    result["error"] = str(error)[:4000]


def compare(repository, pr_number, pr_commits, head_url, head_sha, repo, binary, reports):
    """Compare every backport and return the result list plus the embedded reports"""
    commits = gh_api(f"repos/{repository}/pulls/{pr_number}/commits",
                     "--paginate", "--jq", ".[].sha").split()
    if len(commits) != pr_commits:
        raise CompareError(f"The PR has {pr_commits} commits, but GitHub lists "
                           f"{len(commits)} of them (the limit is {MAX_COMMITS})")
    run(["git", "init", "-q", "--bare", repo], 60)
    fetch(repo, head_url, [head_sha], len(commits) + 1)

    results = []
    for sha in commits:
        result = {"backport": sha, "subject": "", "status": "no_reference"}
        results.append(result)
        try:
            message = run(["git", "-C", repo, "log", "-1", "--format=%B", sha], 60)
            result["subject"] = message.split("\n", 1)[0][:300]
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
            reports.append({"backport": result["backport"], "subject": result["subject"],
                            "html": report.read_text(encoding="utf-8")})
    return results


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
    try:
        summary["results"] = compare(repository, int(os.environ["PR_NUMBER"]),
                                     int(os.environ["PR_COMMITS"]), head_url, head_sha,
                                     str(output / "repo.git"), os.environ["DIFFOFDIFFS"],
                                     reports)
        if not summary["results"]:
            raise CompareError("No comparison results were produced for this pull request")
    except CompareError as error:
        summary["error"] = str(error)[:4000]
    if reports:
        write_page(reports, output / f"diffofdiffs-report-{attempt}.html")
    (output / "comparisons.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")

    failed = "error" in summary
    for result in summary["results"]:
        print(f"{result['status']:>12}  {result['backport'][:12]}  {result['subject']}")
        if "error" in result:
            failed = True
            print("              " + result["error"].replace("\n", "\n              "))
    if "error" in summary:
        print(f"Comparison incomplete: {summary['error']}", file=sys.stderr)
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
