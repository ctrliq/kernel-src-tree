# SPDX-License-Identifier: GPL-2.0-only
import json
from pathlib import Path


def write_report(reports, output):
    if not reports:
        raise ValueError("At least one comparison is required")

    template = Path(__file__).with_suffix(".html").read_text(encoding="utf-8")
    # Prevent report contents from closing the JSON script element
    payload = json.dumps(reports, ensure_ascii=True).replace("<", "\\u003c")
    Path(output).write_text(template.replace("@@REPORTS@@", payload),
                            encoding="utf-8")
