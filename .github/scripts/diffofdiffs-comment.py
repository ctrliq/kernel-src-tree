#!/usr/bin/env python3
"""Post PR comments with the diffofdiffs results of a validation run.

The results arrive as artifacts of a workflow that ran in the pull
request's own context, so their content is data: every field is checked
before it is quoted, and the comment links only to commits and artifacts
that GitHub confirms belong to the run.

A rolling-release rebase PR additionally carries a "rebase" summary
comparing its commits with the previous branch they were cherry-picked
from. Those results go into a second comment with one table for the
previous branch and one for the commits compared with upstream instead.
"""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import quote

MAX_ARTIFACT_BYTES = 4 * 1024 * 1024
MAX_COMMENT_CHARS = 60000
SHA = re.compile(r"[0-9a-f]{40}")
BRANCH_NAME = re.compile(r"[a-zA-Z0-9/_.{}-]+")
STATUSES = {"matches", "differs", "failed", "no_reference"}
PREV_STATUSES = {"matches", "differs", "failed"}
SKIP_KINDS = {"revert-carried", "dropped"}
# Icons for the fallback table's upstream comparisons
FALLBACK_MARKS = {"matches": " \u2705", "differs": " \u26a0\ufe0f", "failed": " \u274c"}


class CommentError(Exception):
    pass


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, check=True,
                          timeout=300).stdout


def gh_api(path, *args):
    return json.loads(gh("api", path, *args))


def find_pull_request(repository, run):
    """Find the run's PR through GitHub, even if it has already closed"""
    candidates = run["pull_requests"]

    # Fork runs can omit the PR association. Include closed PRs in the
    # fallback, since the PR may close before this workflow starts.
    if not candidates:
        head_repo = run["head_repository"]
        head = head_repo["owner"]["login"] + ":" + run["head_branch"]
        pages = gh_api(f"repos/{repository}/pulls?state=all&per_page=100&head={quote(head, safe='')}",
                      "--paginate", "--slurp")
        candidates = [pr for page in pages for pr in page
                      if pr["head"]["sha"] == run["head_sha"] and pr["head"]["repo"]
                      and pr["head"]["repo"]["id"] == head_repo["id"]]
    if len(candidates) != 1:
        raise CommentError("GitHub did not identify a single pull request for this run")
    return candidates[0]


def latest_comparison_job(repository, run_id):
    """Find the latest comparison execution, including attempts without artifacts"""
    pages = gh_api(f"repos/{repository}/actions/runs/{run_id}/jobs?filter=all&per_page=100",
                   "--paginate", "--slurp")
    jobs = [job for page in pages for job in page["jobs"]
            if job["name"].rsplit(" / ", 1)[-1] == "compare-backports"]
    if not jobs:
        raise CommentError("The run has no backport comparison job")
    attempt = max(job["run_attempt"] for job in jobs)
    latest = [job for job in jobs if job["run_attempt"] == attempt]
    if len(latest) != 1:
        raise CommentError("The run has more than one backport comparison job")
    return latest[0]


def find_artifact(artifacts, name):
    return next((artifact for artifact in artifacts
                 if not artifact["expired"] and artifact["name"] == name), None)


def download_results(repository, run_id, artifact):
    if artifact["size_in_bytes"] > MAX_ARTIFACT_BYTES:
        raise CommentError(f"The results artifact is {artifact['size_in_bytes']} bytes, "
                           f"over the {MAX_ARTIFACT_BYTES} byte limit")
    with tempfile.TemporaryDirectory() as directory:
        gh("run", "download", str(run_id), "-R", repository, "-n", artifact["name"],
           "-D", directory)
        try:
            return json.loads(Path(directory, "comparisons.json").read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise CommentError(f"The results artifact couldn't be read: {error}") from None


def checked_sha(value):
    if not isinstance(value, str) or not SHA.fullmatch(value):
        raise CommentError("The results contain a malformed commit ID")
    return value


def checked_text(value, limit):
    if not isinstance(value, str):
        raise CommentError("The results contain malformed text")
    return "".join(char if char.isprintable() or char == "\n" else " "
                   for char in value[:limit])


def checked_subject(value):
    return " ".join(checked_text(value, 300).split())


def checked_results(summary, head_sha, attempt):
    """Return the result list after checking every field the comment will quote"""
    if not isinstance(summary, dict) or summary.get("head_sha") != head_sha:
        raise CommentError("The results don't belong to this run's head commit")
    if type(summary.get("run_attempt")) is not int or summary["run_attempt"] != attempt:
        raise CommentError("The results don't belong to this comparison attempt")
    checked_sha(summary.get("base_sha"))
    if "error" in summary:
        if not checked_text(summary["error"], 4000).strip():
            raise CommentError("The results contain an empty error message")
    items = summary.get("results")
    if not isinstance(items, list):
        raise CommentError("The results are missing a comparison list")
    if not items and "error" not in summary:
        raise CommentError("The comparison produced no results")
    results = []
    for item in items:
        if (not isinstance(item, dict) or not isinstance(item.get("status"), str)
                or item["status"] not in STATUSES):
            raise CommentError("The results contain a malformed comparison")
        result = {"status": item["status"], "backport": checked_sha(item.get("backport")),
                  "subject": checked_subject(item.get("subject", ""))}
        if "upstream" in item:
            result["upstream"] = checked_sha(item["upstream"])
        if "reference" in item:
            result["reference"] = checked_text(item["reference"], 40)
        if "error" in item:
            result["error"] = checked_text(item["error"], 4000)
        if result["status"] == "failed" and not result.get("error", "").strip():
            raise CommentError("A failed comparison is missing its error message")
        results.append(result)
    return results


def checked_rebase(value):
    """Return the previous-branch summary after checking every quoted field"""
    if not isinstance(value, dict):
        raise CommentError("The rebase summary is malformed")
    branch = checked_text(value.get("previous_branch"), 255)
    if not branch or not BRANCH_NAME.fullmatch(branch):
        raise CommentError("The rebase summary has a malformed previous branch")
    rebase = {"previous_branch": branch}
    if "previous_sha" in value:
        rebase["previous_sha"] = checked_sha(value["previous_sha"])
    if value.get("partial") is not None:
        if not isinstance(value["partial"], bool):
            raise CommentError("The rebase summary has a malformed partial flag")
        rebase["partial"] = value["partial"]
    if "error" in value:
        rebase["error"] = checked_text(value["error"], 4000)
    for key in ("matches", "fallback", "skipped"):
        if key not in value:
            if "error" not in rebase:
                raise CommentError(f"The rebase summary is missing its {key} list")
            continue
        items = value[key]
        if not isinstance(items, list):
            raise CommentError(f"The rebase summary has a malformed {key} list")
        checked = []
        for item in items:
            if not isinstance(item, dict):
                raise CommentError(f"The rebase summary has a malformed {key} entry")
            if key == "matches":
                if not isinstance(item.get("status"), str) or item["status"] not in PREV_STATUSES:
                    raise CommentError("A previous-branch comparison has a malformed status")
                entry = {"status": item["status"],
                         "backport": checked_sha(item.get("backport")),
                         "subject": checked_subject(item.get("subject", "")),
                         "counterpart": checked_sha(item.get("counterpart"))}
                if item.get("marker") is not None:
                    if not isinstance(item["marker"], bool):
                        raise CommentError("A previous-branch comparison has a malformed marker flag")
                    entry["marker"] = item["marker"]
                if "error" in item:
                    entry["error"] = checked_text(item["error"], 4000)
                if entry["status"] == "failed" and not entry.get("error", "").strip():
                    raise CommentError("A failed previous-branch comparison is missing its error message")
            elif key == "fallback":
                entry = {"backport": checked_sha(item.get("backport")),
                         "subject": checked_subject(item.get("subject", ""))}
            else:  # skipped
                # kind must be a string before the set lookup, like status
                if (not isinstance(item.get("kind"), str)
                        or item["kind"] not in SKIP_KINDS):
                    raise CommentError("A skipped previous-branch commit has a malformed kind")
                entry = {"sha": checked_sha(item.get("sha")),
                         "subject": checked_subject(item.get("subject", "")),
                         "kind": item["kind"]}
            checked.append(entry)
        rebase[key] = checked
    return rebase


def code_span(text):
    # Code spans keep subjects and diagnostics literal. The fence must be
    # longer than any backtick run inside, and pipes still need escaping
    # because the table parser runs before inline code is recognized.
    fence = "`" * (1 + max((len(run) for run in re.findall(r"`+", text)), default=0))
    text = text.replace("|", r"\|") or " "
    if text[0] in "` " or text[-1] in "` ":
        text = f" {text} "
    return fence + text + fence


def code_block(text):
    fence = "`" * max(3, 1 + max((len(run) for run in re.findall(r"`+", text)), default=0))
    return f"{fence}text\n{text.strip()}\n{fence}"


def plural(count, noun):
    return f"{count} {noun}" + ("" if count == 1 else "s")


def heading(compared, differs, failures):
    if not differs:
        if compared == 1:
            return "1 backport couldn't be compared with upstream"
        if failures == compared:
            return f"None of the {compared} backports could be compared with upstream"
        return f"{failures} of {compared} backports couldn't be compared with upstream"
    if compared == 1:
        text = "1 backport differs from upstream"
    elif differs == compared:
        text = f"All {compared} backports differ from upstream"
    else:
        text = f"{differs} of {compared} backports {'differs' if differs == 1 else 'differ'} from upstream"
    if failures:
        text += f", {failures} couldn't be compared"
    return text


def render(summary, results, links, report, error_chars, limit):
    """Return the comment body and whether it reports a problem"""
    differs = [result for result in results if result["status"] == "differs"]
    failures = [result for result in results if result["status"] == "failed"]
    compared = [result for result in results if result["status"] != "no_reference"]
    skipped = len(results) - len(compared)
    error = summary.get("error")
    report_missing = bool(differs) and report is None
    problem = bool(error or failures or report_missing)
    run_log = f"[workflow run log]({links['run']})"

    def backport(result):
        sha = result["backport"]
        return f"[`{sha[:12]}`]({links['pr']}/commits/{sha})"

    def upstream(result):
        if "upstream" in result:
            sha = result["upstream"]
            return f"[`{sha[:12]}`]({links['repo']}/commit/{sha})"
        return code_span(result["reference"][:12]) if result.get("reference") else "Unavailable"

    if error:
        title = "Comparison incomplete"
    elif not compared:
        title = "No backports to compare"
    elif not differs and not failures:
        title = "No differences found"
    else:
        title = heading(len(compared), len(differs), len(failures))
    # A red X, a magnifying glass, or a green check mark
    icon = "\u274c" if problem else "\U0001f50d" if differs else "\u2705"
    lines = [f"## {icon} Diffofdiffs: {title}", ""]

    if error:
        lines += [code_block(error[:error_chars] or "See the workflow run log."), "",
                  f"See the {run_log}.", ""]
    if differs and report:
        expiry = datetime.fromisoformat(report["expires"].replace("Z", "+00:00"))
        expiry = expiry.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
        lines += [f"**[View the diffofdiffs report.]({report['url']})**", "",
                  f"*Open to anyone signed in to GitHub. Report expires {expiry}.*", ""]
    elif differs:
        lines += [f"The HTML report is unavailable. See the {run_log}.", ""]
    if differs:
        lines += ["| # | Subject | Backport | Upstream |", "| ---: | --- | --- | --- |"]
        for number, result in enumerate(differs[:limit], 1):
            lines.append(f"| {number} | {code_span(result['subject'])} | {backport(result)} | "
                         f"{upstream(result)} |")
        if len(differs) > limit:
            lines.append(f"| ... | {len(differs) - limit} more in the report | | |")
        lines.append("")
    if failures:
        lines += ["> [!WARNING]",
                  f"> **{plural(len(failures), 'backport')} couldn't be compared.** "
                  f"Details are in the {run_log}.", ""]
        for number, result in enumerate(failures[:limit], 1):
            subject = code_span(result["subject"]) if result["subject"] else "Subject unavailable"
            lines += [f"**{number}. {subject}**", "",
                      f"**Backport:** {backport(result)} \u00b7 **Upstream:** {upstream(result)}", ""]
            if error_chars:
                lines += ["<details>", "<summary>Show error</summary>", "",
                          code_block(result["error"][:error_chars]), "", "</details>", ""]
        if len(failures) > limit:
            lines += [f"{len(failures) - limit} more in the {run_log}.", ""]
    if compared and not differs and not failures:
        lines += [f"Compared {plural(len(compared), 'backport')} with upstream.", ""]
    if skipped and compared:
        lines += [f"{skipped} additional {'commit has' if skipped == 1 else 'commits have'} "
                 f"no upstream reference and {'was' if skipped == 1 else 'were'} not compared.", ""]
    elif skipped:
        lines += ["No commits in this PR reference an upstream commit.", ""]
    head, base = summary["head_sha"], summary["base_sha"]
    lines.append(f"Checked PR revision [`{head[:12]}`]({links['pr']}/commits/{head}) "
                 f"against base [`{base[:12]}`]({links['repo']}/commit/{base}).")
    return "\n".join(lines) + "\n", problem


def render_rebase(summary, rebase, results, links, report, error_chars, limit):
    """Return the previous-branch comment body and whether it reports a problem"""
    matches = rebase.get("matches", [])
    fallback = rebase.get("fallback", [])
    skipped = rebase.get("skipped", [])
    error = rebase.get("error")
    differs = [entry for entry in matches if entry["status"] == "differs"]
    failures = [entry for entry in matches if entry["status"] == "failed"]
    identical = sum(1 for entry in matches if entry["status"] == "matches")
    run_log = f"[workflow run log]({links['run']})"

    results_by_backport = {result["backport"]: result for result in results}

    def upstream_status(entry):
        # A fallback commit was compared with upstream instead; that
        # comparison's outcome is the one the first comment already reports
        return results_by_backport.get(entry["backport"], {}).get("status")

    # A fallback commit with no upstream differences has not necessarily
    # matched upstream: its comparison may have failed, or the commit may
    # have no upstream reference at all and never been compared
    fallback_differs = [entry for entry in fallback
                       if upstream_status(entry) == "differs"]
    fallback_failures = [entry for entry in fallback
                         if upstream_status(entry) == "failed"]
    fallback_unreferenced = [entry for entry in fallback
                             if upstream_status(entry) not in ("matches", "differs",
                                                               "failed")]
    # A table showing differences needs its inspectable report: the
    # previous-branch artifact carries both the differing pairs and the
    # fallback commits that differ from upstream
    report_missing = bool(differs or fallback_differs) and report is None
    problem = bool(error or failures or fallback_failures or report_missing)

    def backport(entry):
        sha = entry["backport"]
        return f"[`{sha[:12]}`]({links['pr']}/commits/{sha})"

    def counterpart(entry):
        sha = entry["counterpart"]
        return f"[`{sha[:12]}`]({links['repo']}/commit/{sha})"

    if error:
        title = "Previous-branch comparison incomplete"
    elif not matches:
        title = "No backports matched the previous branch"
    elif failures and not differs:
        title = f"{len(failures)} of {len(matches)} backports couldn't be compared with the previous branch"
    elif failures:
        title = (f"{len(differs)} of {len(matches)} backports differ from the previous branch, "
                 f"{len(failures)} couldn't be compared")
    elif differs:
        title = f"{len(differs)} of {len(matches)} backports differ from the previous branch"
    elif len(matches) == 1:
        title = "1 backport matches the previous branch"
    else:
        title = f"All {len(matches)} backports match the previous branch"
    if not error and fallback_failures:
        title += (f", {plural(len(fallback_failures), 'unmatched commit')} couldn't "
                  "be compared with upstream")
    icon = "\u274c" if problem else "\U0001f50d" if differs else "\u2705"
    lines = [f"## {icon} Diffofdiffs: {title}", "",
             "Matched by subject against the previous branch "
             f"{code_span(rebase['previous_branch'])} (the cherry-pick source named in the "
             "rebase process): "
             f"{plural(len(matches), 'backport')} matched, "
             f"{plural(len(fallback), 'commit')} had no counterpart and "
             f"{'was' if len(fallback) == 1 else 'were'} compared with "
             "upstream instead.", ""]

    def report_link():
        if not report:
            return [f"The HTML report is unavailable. See the {run_log}.", ""]
        expiry = datetime.fromisoformat(report["expires"].replace("Z", "+00:00"))
        expiry = expiry.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
        return [f"**[View the diffofdiffs report.]({report['url']})**", "",
                f"*Open to anyone signed in to GitHub. Report expires {expiry}.*", ""]

    lines += ["### Previous Branch", ""]
    if error:
        lines += [code_block(error[:error_chars] or "See the workflow run log."), "",
                  f"See the {run_log}.", ""]
    elif differs:
        lines += report_link()
    else:
        lines += ["No differences were found; no report was generated.", ""]
    if rebase.get("partial"):
        lines += ["*Compared against the most recent commits of the previous branch: its "
                  "import tag is missing, so the custom-changes range could not be bounded "
                  "exactly.*", ""]
    if differs:
        lines += ["| # | Subject | Backport | Previous Branch |", "| ---: | --- | --- | --- |"]
        for number, entry in enumerate(differs[:limit], 1):
            mark = " \u2020" if entry.get("marker") else ""
            lines.append(f"| {number} | {code_span(entry['subject'])}{mark} | "
                         f"{backport(entry)} | {counterpart(entry)} |")
        if len(differs) > limit:
            lines.append(f"| ... | {len(differs) - limit} more in the report | | |")
        lines.append("")
        if any(entry.get("marker") for entry in differs):
            lines += ["\u2020 The previous-branch counterpart is a `.failed` rebuild marker: "
                      "the fix reached the previous branch inside the RHEL source drop and was "
                      "re-ported from upstream. See the standard diffofdiffs comment for the "
                      "upstream comparison.", ""]
    if failures:
        lines += ["> [!WARNING]",
                  f"> **{plural(len(failures), 'backport')} couldn't be compared.** "
                  f"Details are in the {run_log}.", ""]
        for number, entry in enumerate(failures[:limit], 1):
            subject = code_span(entry["subject"]) if entry["subject"] else "Subject unavailable"
            lines += [f"**{number}. {subject}**", "",
                      f"**Backport:** {backport(entry)} \u00b7 "
                      f"**Previous branch:** {counterpart(entry)}", ""]
            if error_chars:
                lines += ["<details>", "<summary>Show error</summary>", "",
                          code_block(entry["error"][:error_chars]), "", "</details>", ""]
        if len(failures) > limit:
            lines += [f"{len(failures) - limit} more in the {run_log}.", ""]
    if identical and differs:
        lines.append(f"{plural(identical, 'backport')} "
                     f"{'is' if identical == 1 else 'are'} identical to their previous-branch "
                     "counterparts.")
        lines.append("")
    if skipped:
        lines.append(f"{plural(len(skipped), 'previous-branch commit')} "
                      f"{'was' if len(skipped) == 1 else 'were'} not carried into this rebase:")
        for entry in skipped[:limit]:
            sha = entry["sha"]
            note = ("only its revert is present" if entry["kind"] == "revert-carried"
                    else "dropped (\u201cskipping unneeded code\u201d)")
            lines.append(f"- [`{sha[:12]}`]({links['repo']}/commit/{sha}) "
                         f"{code_span(entry['subject'])} \u2014 {note}")
        if len(skipped) > limit:
            lines.append(f"- {len(skipped) - limit} more in the {run_log}.")
        lines.append("")

    lines += ["### Against Upstream", ""]
    if error:
        # Matching never ran, so claiming every backport matched would
        # overstate the outcome; the upstream comparisons are in the
        # first comment
        lines += ["The previous-branch comparison could not run, so no commits "
                  "were matched against it. The upstream comparisons are in "
                  "the standard diffofdiffs comment.", ""]
    elif not fallback:
        lines += ["Every backport matched the previous branch.", ""]
    else:
        # No differences among the fallback commits only means their
        # upstream comparisons matched when every one of them ran: failed
        # and unreferenced comparisons found no differences either
        if fallback_differs:
            lines += report_link()
        elif fallback_failures:
            lines += [f"{plural(len(fallback_failures), 'comparison')} couldn't be "
                      "completed; the errors are in the standard diffofdiffs comment "
                      f"and the {run_log}.", ""]
        elif fallback_unreferenced:
            compared = len(fallback) - len(fallback_unreferenced)
            if compared:
                lines += [f"No differences were found among the {plural(compared, 'comparison')}; "
                          f"{plural(len(fallback_unreferenced), 'commit')} "
                          f"{'has' if len(fallback_unreferenced) == 1 else 'have'} "
                          "no upstream reference. No report was generated.", ""]
            else:
                lines += [f"None of the {plural(len(fallback_unreferenced), 'commit')} has "
                          "an upstream reference, so none were compared with upstream. "
                          "No report was generated.", ""]
        else:
            lines += ["No differences were found; no report was generated.", ""]
        lines += ["| # | Subject | Backport | Upstream |", "| ---: | --- | --- | --- |"]
        for number, entry in enumerate(fallback[:limit], 1):
            result = results_by_backport.get(entry["backport"], {})
            mark = FALLBACK_MARKS.get(result.get("status"), "")
            if "upstream" in result:
                sha = result["upstream"]
                upstream = f"[`{sha[:12]}`]({links['repo']}/commit/{sha})"
            else:
                upstream = (code_span(result["reference"][:12])
                            if result.get("reference") else "Unavailable")
            lines.append(f"| {number} | {code_span(entry['subject'])}{mark} | "
                         f"{backport(entry)} | {upstream} |")
        if len(fallback) > limit:
            # The report holds only the fallback commits that differ from
            # upstream; the run log lists every fallback comparison
            lines.append(f"| ... | {len(fallback) - limit} more in the {run_log} | | |")
        lines.append("")

    previous = f"previous branch {code_span(rebase['previous_branch'])}"
    if "previous_sha" in rebase:
        sha = rebase["previous_sha"]
        previous += f" ([`{sha[:12]}`]({links['repo']}/commit/{sha}))"
    head, base = summary["head_sha"], summary["base_sha"]
    lines.append(f"Checked PR revision [`{head[:12]}`]({links['pr']}/commits/{head}) "
                 f"against base [`{base[:12]}`]({links['repo']}/commit/{base}) and {previous}.")
    return "\n".join(lines) + "\n", problem


def comment_body(summary, results, links, report):
    """Return the upstream comment body, shrunk to fit GitHub's limit"""
    for error_chars, limit in ((2000, 250), (300, 250), (0, 250), (0, 25)):
        body, problem = render(summary, results, links, report, error_chars, limit)
        if len(body) <= MAX_COMMENT_CHARS:
            return body, problem
    # Not even the smallest rendering fits: a pointer is still better
    # than a comment GitHub refuses to accept
    run_log = f"[workflow run log]({links['run']})"
    return (f"## \u274c Diffofdiffs: results too large for a comment\n\n"
            f"The comparison results exceed GitHub's comment limit even "
            f"truncated. See the {run_log}.\n"), True


def rebase_comment_body(summary, rebase, results, links, report):
    """Return the previous-branch comment body, shrunk to fit GitHub's limit"""
    for error_chars, limit in ((2000, 250), (300, 250), (0, 250), (0, 25)):
        body, problem = render_rebase(summary, rebase, results, links, report,
                                      error_chars, limit)
        if len(body) <= MAX_COMMENT_CHARS:
            return body, problem
    run_log = f"[workflow run log]({links['run']})"
    return (f"## \u274c Diffofdiffs: previous-branch results too large for "
            f"a comment\n\n"
            f"The previous-branch results exceed GitHub's comment limit "
            f"even truncated. See the {run_log}.\n"), True


def post_comment(pr_number, repository, body):
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete_on_close=False) as file:
        file.write(body)
        file.close()
        try:
            gh("pr", "comment", str(pr_number), "-R", repository,
               "--body-file", file.name)
        except subprocess.CalledProcessError as error:
            # A comment GitHub refuses must not die as a traceback: the
            # entry point only reports CommentError cleanly
            detail = (error.stderr or error.stdout or "").strip()
            raise CommentError(f"GitHub rejected the comment: {detail or error}") from None
        except subprocess.TimeoutExpired:
            raise CommentError("Posting the comment exceeded the GitHub "
                               "timeout") from None


def main():
    repository = os.environ["GITHUB_REPOSITORY"]
    run_id = int(os.environ["RUN_ID"])
    run = gh_api(f"repos/{repository}/actions/runs/{run_id}")
    pr = find_pull_request(repository, run)
    links = {"repo": f"https://github.com/{repository}",
             "pr": f"https://github.com/{repository}/pull/{pr['number']}",
             "run": run["html_url"]}

    summary = {"head_sha": run["head_sha"], "base_sha": pr["base"]["sha"]}
    results, report, raw = [], None, {}
    artifacts = []
    try:
        # A retry of another job can reuse these results, but a comparison
        # retry must never fall back to artifacts from an earlier attempt.
        job = latest_comparison_job(repository, run_id)
        attempt = job["run_attempt"]
        links["run"] += f"/attempts/{attempt}"
        if job["status"] != "completed":
            raise CommentError("The backport comparison job has not completed")
        pages = gh_api(f"repos/{repository}/actions/runs/{run_id}/artifacts?per_page=100",
                       "--paginate", "--slurp")
        artifacts = [artifact for page in pages for artifact in page["artifacts"]]
        results_artifact = find_artifact(artifacts, f"diffofdiffs-results-{attempt}")
        if results_artifact is None:
            raise CommentError(f"The results from comparison attempt {attempt} are missing or expired.")
        raw = download_results(repository, run_id, results_artifact)
        results = checked_results(raw, run["head_sha"], attempt)
        summary["base_sha"] = raw["base_sha"]
        if "error" in raw:
            summary["error"] = checked_text(raw["error"], 4000)
        if (job["conclusion"] != "success" and "error" not in summary
                and not any(result["status"] == "failed" for result in results)):
            summary["error"] = f"The comparison job ended with {job['conclusion']}."
    except CommentError as error:
        summary["error"] = str(error)
        # A rejected artifact must not supply rebase results either: its
        # head SHA and run attempt were never confirmed to be this run's
        raw = {}
    else:
        html = find_artifact(artifacts, f"diffofdiffs-report-{attempt}.html")
        if html:
            report = {"url": f"{links['repo']}/actions/runs/{run_id}/artifacts/{html['id']}",
                      "expires": html["expires_at"]}

    problem = False
    body, problem = comment_body(summary, results, links, report)
    post_comment(pr["number"], repository, body)

    # The previous-branch view of a rolling-release rebase, in its own
    # comment so the standard upstream comment stays untouched
    if "rebase" in raw:
        try:
            rebase = checked_rebase(raw["rebase"])
        except CommentError as error:
            rebase = {"previous_branch": "unknown", "error": str(error)}
        prev_report = None
        html = find_artifact(artifacts, f"diffofdiffs-report-prevbranch-{attempt}.html")
        if html:
            prev_report = {"url": f"{links['repo']}/actions/runs/{run_id}/artifacts/{html['id']}",
                           "expires": html["expires_at"]}
        rebase_body, rebase_problem = rebase_comment_body(summary, rebase, results,
                                                          links, prev_report)
        post_comment(pr["number"], repository, rebase_body)
        problem = problem or rebase_problem
    return int(problem)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CommentError as error:
        sys.exit(str(error))
