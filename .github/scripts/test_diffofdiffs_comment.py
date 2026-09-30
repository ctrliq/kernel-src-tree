# SPDX-License-Identifier: GPL-2.0-only
import unittest

from diffofdiffs_comment import MAX_LENGTH, MARKER, escape, render_comment

REPO = "https://github.com/octo/kernel"
RUN = f"{REPO}/actions/runs/7"
REPORT = f"{REPO}/actions/runs/7/artifacts/9"


def result(number, status, subject=None, **extra):
    return {
        "backport": f"{number:x}" * 40,
        "reference": f"{number + 10:x}" * 12,
        "upstream": f"{number + 10:x}" * 40,
        "subject": subject or f"subsystem: Change {number}",
        "status": status,
        **extra,
    }


def render(results, **options):
    options = {"repo_url": REPO, "pr_number": 12, "run_url": RUN,
               "report_url": REPORT, "expires_at": "2026-12-29T01:47:20Z",
               **options}
    return render_comment(results, **options)


class CommentTests(unittest.TestCase):
    def test_differences(self):
        results = [
            result(1, "matches"),
            result(2, "differs", "nbd: fix incomplete validation of ioctl arg"),
            result(3, "matches"),
            result(4, "differs", "scsi: ses: Fix desc_ptr accesses"),
            result(5, "matches"),
        ]
        body, post = render(results)
        self.assertTrue(post)
        self.assertEqual(body, f"""{MARKER}
### 2 of 5 backports differ from upstream

Review the differences in the **[diffofdiffs report.]({REPORT})** Backports \
often differ because of conflict fixes or context changes, so a difference \
alone isn't a bug or a failed check.

*Publicly accessible; sign in with any GitHub account. Expires 2026-12-29 at \
01:47 UTC.*

| Report | Subject | Backport | Upstream |
| ---: | --- | --- | --- |
| 1 | nbd: fix incomplete validation of ioctl arg | \
[`{"2" * 12}`]({REPO}/pull/12/commits/{"2" * 40}) | \
[`{"c" * 12}`]({REPO}/commit/{"c" * 40}) |
| 2 | scsi: ses: Fix desc\\_ptr accesses | \
[`{"4" * 12}`]({REPO}/pull/12/commits/{"4" * 40}) | \
[`{"e" * 12}`]({REPO}/commit/{"e" * 40}) |

*The other 3 backports match upstream.*
""")

    def test_singular_and_complete_wording(self):
        body, _ = render([result(1, "differs")])
        self.assertIn("### The backport differs from upstream\n", body)
        self.assertIn("Review the difference in the", body)
        self.assertNotIn("The other", body)

        body, _ = render([result(1, "differs"), *[result(n, "matches")
                                                  for n in (2, 3)]])
        self.assertIn("### 1 of 3 backports differs from upstream\n", body)
        self.assertIn("*The other 2 backports match upstream.*", body)

        body, _ = render([result(1, "differs"), result(2, "matches")])
        self.assertIn("*The other backport matches upstream.*", body)

        body, _ = render([result(n, "differs") for n in (1, 2, 3)])
        self.assertIn("### All 3 backports differ from upstream\n", body)

    def test_differences_with_failures(self):
        results = [
            result(1, "differs"),
            result(2, "failed", error="diffofdiffs exited with status 2"),
            result(3, "matches"),
        ]
        body, post = render(results)
        self.assertTrue(post)
        self.assertIn("### 1 of 3 backports differs from upstream, "
                      "1 couldn't be compared\n", body)
        self.assertIn(f"**[diffofdiffs report.]({REPORT})**", body)
        self.assertIn(f"""*The other backport matches upstream.*

> [!WARNING]
> **1 backport couldn't be compared with upstream**, so it hasn't been \
checked.
>
> - [`{"2" * 12}`]({REPO}/pull/12/commits/{"2" * 40}) subsystem: Change 2 \
(upstream [`{"c" * 12}`]({REPO}/commit/{"c" * 40})): diffofdiffs exited with \
status 2
>
> Error details are in the [workflow run log]({RUN}).
""", body)

    def test_failures_without_report(self):
        results = [
            result(1, "failed", upstream=None,
                   error="couldn't fetch the upstream commit"),
            result(2, "matches"),
            result(3, "failed", error="diffofdiffs exited with status 2"),
        ]
        body, post = render(results, report_url=None)
        self.assertTrue(post)
        self.assertEqual(body, f"""{MARKER}
### 2 of 3 backports couldn't be compared with upstream

These comparisons didn't finish, so the backports below haven't been \
checked. The other backport matches upstream.

- [`{"1" * 12}`]({REPO}/pull/12/commits/{"1" * 40}) subsystem: Change 1 \
(upstream `{"b" * 12}`): couldn't fetch the upstream commit
- [`{"3" * 12}`]({REPO}/pull/12/commits/{"3" * 40}) subsystem: Change 3 \
(upstream [`{"d" * 12}`]({REPO}/commit/{"d" * 40})): diffofdiffs exited with \
status 2

Error details are in the [workflow run log]({RUN}).
""")
        self.assertNotIn("differ", body)

        body, _ = render([result(n, "failed", error="failed")
                          for n in (1, 2)], report_url=None)
        self.assertIn("### None of the 2 backports could be compared with "
                      "upstream\n", body)
        self.assertNotIn("The other", body)

        body, _ = render([result(1, "failed", error="failed")],
                         report_url=None)
        self.assertIn("### The backport couldn't be compared with upstream\n",
                      body)
        self.assertIn("This comparison didn't finish, so the backport below "
                      "hasn't been checked.\n", body)

    def test_matches_only_replace_earlier_comments(self):
        body, post = render([result(n, "matches") for n in (1, 2)],
                            report_url=None)
        self.assertFalse(post)
        self.assertEqual(body, f"""{MARKER}
### All 2 backports now match upstream

The latest run found no differences or comparison failures.
""")

        body, post = render([], report_url=None)
        self.assertFalse(post)
        self.assertTrue(body.startswith(f"{MARKER}\n### No backports to "
                                        "compare\n"))

    def test_access_details(self):
        body, _ = render([result(1, "differs")], public=False,
                         expires_at="2026-12-28T20:47:20-05:00")
        self.assertIn("*Sign in with a GitHub account that can access this "
                      "repository. Expires 2026-12-29 at 01:47 UTC.*", body)

        body, _ = render([result(1, "differs")], expires_at=None)
        self.assertIn("*Publicly accessible; sign in with any GitHub "
                      "account.*", body)

        body, _ = render([result(1, "differs")], report_url=None)
        self.assertIn("The diffofdiffs report couldn't be uploaded. Details "
                      f"are in the [workflow run log]({RUN}).", body)
        self.assertNotIn("sign in", body)

    def test_escapes_subjects(self):
        subject = r"a|b `c` *d* _e_ [f](g) <img src=x> $x$ ~s~ &amp; \ "
        self.assertEqual(
            escape(subject),
            r"a\|b \`c\` \*d\* \_e\_ \[f\](g) \<img src=x\> \$x\$ \~s\~ "
            r"\&amp; \\ ")
        self.assertEqual(escape("Fix #12 for @octo-cat, not a@b.c"),
                         "Fix `#12` for `@octo-cat`, not a@b.c")

        body, _ = render([result(1, "differs", "x | y")])
        self.assertIn("| 1 | x \\| y | ", body)

    def test_collapses_long_tables(self):
        body, _ = render([result(n, "differs") for n in range(1, 9)])
        self.assertNotIn("<details>", body)

        body, _ = render([result(n, "differs") for n in range(1, 10)])
        self.assertIn("<details>\n<summary>9 backports with differences"
                      "</summary>\n\n| Report |", body)
        self.assertIn("| 9 | subsystem: Change 9 |", body)
        self.assertIn("|\n\n</details>\n", body)
        # The report link stays outside the collapsed section
        self.assertLess(body.index("[diffofdiffs report.]"),
                        body.index("<details>"))

    def test_stays_within_comment_limit(self):
        results = [result(n % 15 + 1, "differs", "x" * 200)
                   for n in range(1000)]
        results += [result(1, "failed", "y" * 200, error="failed")
                    for _ in range(1000)]
        body, _ = render(results)
        self.assertLessEqual(len(body), MAX_LENGTH)
        self.assertRegex(body, r"\| \.\.\. \| \d+ more in the report \| \| \|")
        self.assertRegex(body, r"> - \d+ more in the workflow run log")
        self.assertIn("### 1000 of 2000 backports differ from upstream, "
                      "1000 couldn't be compared\n", body)
        self.assertIn("> Error details are in the", body)

    def test_preserves_order_and_numbering(self):
        results = [result(n, "differs" if n % 2 else "matches")
                   for n in range(1, 8)]
        body, _ = render(results)
        numbered = [line.split(" | ")[:2] for line in body.splitlines()
                    if line.startswith("| ") and "`" in line]
        self.assertEqual(numbered, [["| 1", "subsystem: Change 1"],
                                    ["| 2", "subsystem: Change 3"],
                                    ["| 3", "subsystem: Change 5"],
                                    ["| 4", "subsystem: Change 7"]])


if __name__ == "__main__":
    unittest.main()
