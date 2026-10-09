#!/usr/bin/env python3
"""Post a PR comment with the diffofdiffs results of a validation run.

The results arrive as artifacts of a workflow that ran in the pull
request's own context, so their content is data: every field is checked
before it is quoted, and the comment links only to commits and artifacts
that GitHub confirms belong to the run.
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
STATUSES = {"matches", "differs", "failed", "no_reference"}


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
                  "subject": " ".join(checked_text(item.get("subject", ""), 300).split())}
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


def comment_body(summary, results, links, report):
    # Shorten diagnostics, then lists, until the comment fits GitHub's limit
    for error_chars, limit in ((2000, 250), (300, 250), (0, 250), (0, 25)):
        body, problem = render(summary, results, links, report, error_chars, limit)
        if len(body) <= MAX_COMMENT_CHARS:
            break
    return body, problem


def main():
    repository = os.environ["GITHUB_REPOSITORY"]
    run_id = int(os.environ["RUN_ID"])
    run = gh_api(f"repos/{repository}/actions/runs/{run_id}")
    pr = find_pull_request(repository, run)
    links = {"repo": f"https://github.com/{repository}",
             "pr": f"https://github.com/{repository}/pull/{pr['number']}",
             "run": run["html_url"]}

    summary = {"head_sha": run["head_sha"], "base_sha": pr["base"]["sha"]}
    results, report = [], None
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
    else:
        html = find_artifact(artifacts, f"diffofdiffs-report-{attempt}.html")
        if html:
            report = {"url": f"{links['repo']}/actions/runs/{run_id}/artifacts/{html['id']}",
                      "expires": html["expires_at"]}

    body, problem = comment_body(summary, results, links, report)
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete_on_close=False) as file:
        file.write(body)
        file.close()
        gh("pr", "comment", str(pr["number"]), "-R", repository, "--body-file", file.name)
    return int(problem)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CommentError as error:
        sys.exit(str(error))
