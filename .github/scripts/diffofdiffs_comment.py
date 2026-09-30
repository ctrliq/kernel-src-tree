# SPDX-License-Identifier: GPL-2.0-only
from datetime import datetime, timezone
import re

MARKER = "<!-- diffofdiffs-comment -->"

# Collapse longer tables so the result and report link stay in view
INLINE_ROWS = 8

# Stay well under GitHub's 65,536-character limit for comments
MAX_LENGTH = 60000

# Mentions and issue references still notify people and link issues after
# backslash escaping, but they're inert inside code spans.
_REFERENCE = re.compile(r"(?<![\w@])@[A-Za-z0-9][A-Za-z0-9-]*|#[0-9]+")
_SPECIAL = re.compile(r"[\\`*_\[\]<>|~$&]")
_FULL_SHA = re.compile(r"[0-9a-f]{40}")


def escape(text):
    parts = []
    start = 0
    for match in _REFERENCE.finditer(text):
        parts.append(_SPECIAL.sub(r"\\\g<0>", text[start:match.start()]))
        parts.append(f"`{match[0]}`")
        start = match.end()
    parts.append(_SPECIAL.sub(r"\\\g<0>", text[start:]))
    return "".join(parts)


def _count(number, singular, plural):
    return f"{number} {singular if number == 1 else plural}"


def _heading(total, differs, failures):
    if differs:
        if total == 1:
            heading = "The backport differs from upstream"
        elif differs == total:
            heading = f"All {total} backports differ from upstream"
        else:
            verb = "differs" if differs == 1 else "differ"
            heading = f"{differs} of {total} backports {verb} from upstream"
        if failures:
            heading += f", {failures} couldn't be compared"
        return heading
    if total == 1:
        return "The backport couldn't be compared with upstream"
    if failures == total:
        return f"None of the {total} backports could be compared with upstream"
    return (f"{failures} of {total} backports couldn't be compared with "
            "upstream")


def _fit(lines, budget, more):
    kept = []
    # Reserve room to summarize anything left out
    budget -= len(more(len(lines))) + 1
    for index, line in enumerate(lines):
        if len(line) + 1 > budget:
            kept.append(more(len(lines) - index))
            break
        kept.append(line)
        budget -= len(line) + 1
    return kept


def render_comment(results, *, repo_url, pr_number, run_url, report_url=None,
                   expires_at=None, public=True):
    """Return the comment and whether to post it when no earlier one exists"""
    differs = [result for result in results if result["status"] == "differs"]
    failures = [result for result in results if result["status"] == "failed"]
    total = len(results)
    matches = total - len(differs) - len(failures)

    # Without anything to review, only replace an earlier comment
    if not total:
        return (f"{MARKER}\n### No backports to compare\n\n"
                "The latest run found no commits that reference an upstream "
                "commit.\n", False)
    if not differs and not failures:
        heading = ("The backport now matches upstream" if total == 1 else
                   f"All {total} backports now match upstream")
        return (f"{MARKER}\n### {heading}\n\n"
                "The latest run found no differences or comparison "
                "failures.\n", False)

    def link(sha, url):
        return f"[`{sha[:12]}`]({url})"

    def backport(result):
        sha = result["backport"]
        return link(sha, f"{repo_url}/pull/{pr_number}/commits/{sha}")

    def upstream(result):
        sha = result.get("upstream")
        # A reference that didn't resolve may not name a real commit
        if not sha or not _FULL_SHA.fullmatch(sha):
            return f"`{result['reference'][:12]}`"
        return link(sha, f"{repo_url}/commit/{sha}")

    matching = ""
    if matches == 1:
        matching = "The other backport matches upstream."
    elif matches:
        matching = f"The other {matches} backports match upstream."

    head = [MARKER, f"### {_heading(total, len(differs), len(failures))}", ""]
    if differs and report_url:
        noun = "difference" if len(differs) == 1 else "differences"
        if public:
            access = "Publicly accessible; sign in with any GitHub account."
        else:
            access = ("Sign in with a GitHub account that can access this "
                      "repository.")
        if expires_at:
            expiry = datetime.fromisoformat(expires_at.replace("Z", "+00:00"))
            access += expiry.astimezone(timezone.utc).strftime(
                " Expires %Y-%m-%d at %H:%M UTC.")
        head += [
            f"Review the {noun} in the "
            f"**[diffofdiffs report.]({report_url})** "
            "Backports often differ because of conflict fixes or context "
            "changes, so a difference alone isn't a bug or a failed check.",
            "",
            f"*{access}*",
            "",
        ]
    elif differs:
        head += [
            "The diffofdiffs report couldn't be uploaded. Details are in the "
            f"[workflow run log]({run_url}).",
            "",
        ]
    elif len(failures) == 1:
        head += ["This comparison didn't finish, so the backport below "
                 f"hasn't been checked. {matching}".rstrip(), ""]
    else:
        head += ["These comparisons didn't finish, so the backports below "
                 f"haven't been checked. {matching}".rstrip(), ""]

    # Quote the failures when they follow differences, to set them apart
    prefix = "> " if differs else ""
    failure_lines = [
        f"{prefix}- {backport(result)} {escape(result['subject'])} "
        f"(upstream {upstream(result)}): {escape(result['error'])}"
        for result in failures
    ]
    rows = [
        f"| {number} | {escape(result['subject'])} | {backport(result)} | "
        f"{upstream(result)} |"
        for number, result in enumerate(differs, 1)
    ]

    def assemble(rows, failure_lines):
        lines = list(head)
        if differs:
            table = ["| Report | Subject | Backport | Upstream |",
                     "| ---: | --- | --- | --- |", *rows]
            if len(differs) > INLINE_ROWS:
                summary = _count(len(differs), "backport", "backports")
                table = ["<details>",
                         f"<summary>{summary} with differences</summary>", "",
                         *table, "", "</details>"]
            lines += [*table, ""]
            if matching:
                lines += [f"*{matching}*", ""]
        if failures and differs:
            failed = _count(len(failures), "backport", "backports")
            checked = "it hasn't" if len(failures) == 1 else "they haven't"
            lines += [
                "> [!WARNING]",
                f"> **{failed} couldn't be compared with upstream**, so "
                f"{checked} been checked.",
                ">",
                *failure_lines,
                ">",
                f"> Error details are in the [workflow run log]({run_url}).",
                "",
            ]
        elif failures:
            lines += [
                *failure_lines,
                "",
                f"Error details are in the [workflow run log]({run_url}).",
                "",
            ]
        return "\n".join(lines)

    budget = MAX_LENGTH - len(assemble([], []))
    failure_lines = _fit(failure_lines, budget // 2 if rows else budget,
                         lambda count: f"{prefix}- {count} more in the "
                                       "workflow run log")
    budget -= sum(len(line) + 1 for line in failure_lines)
    rows = _fit(rows, budget,
                lambda count: f"| ... | {count} more in the report | | |")
    return assemble(rows, failure_lines), True
