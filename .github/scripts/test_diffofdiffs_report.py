# SPDX-License-Identifier: GPL-2.0-only
from html.parser import HTMLParser
import json
from pathlib import Path
import tempfile
import unittest

from diffofdiffs_report import write_report


class PayloadParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.in_payload = False
        self.payload = ""
        self.images = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "script" and attrs.get("id") == "reports":
            self.in_payload = True
        if tag == "img":
            self.images.append(attrs)

    def handle_endtag(self, tag):
        if tag == "script":
            self.in_payload = False

    def handle_data(self, data):
        if self.in_payload:
            self.payload += data


class ReportTests(unittest.TestCase):
    def render(self, reports):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.html"
            write_report(reports, output)
            parser = PayloadParser()
            parser.feed(output.read_text(encoding="utf-8"))
            return parser

    def test_preserves_complete_reports_in_order(self):
        reports = [
            {"backport": "a" * 40, "subject": "First comparison",
             "html": "<!doctype html><html><body>first</body></html>"},
            {"backport": "b" * 40, "subject": "Unicode: \u03bb \U0001f600",
             "html": "<html><body><script>const a = 1 < 2;</script></body></html>"},
        ]
        self.assertEqual(json.loads(self.render(reports).payload), reports)

    def test_report_contents_cannot_escape_payload(self):
        hostile = '</script><img src=x onerror="alert(1)"><!-- @@REPORTS@@'
        reports = [{"backport": "c" * 40, "subject": hostile, "html": hostile}]
        parser = self.render(reports)
        self.assertEqual(parser.images, [])
        self.assertEqual(json.loads(parser.payload), reports)

    def test_does_not_create_empty_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.html"
            with self.assertRaises(ValueError):
                write_report([], output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
