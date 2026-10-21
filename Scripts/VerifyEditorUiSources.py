"""Check the migrated UI boundary and the actual frontend Classic/Sakura palettes."""

import json
import re
from pathlib import Path


def main():
    workspace = Path(__file__).resolve().parent.parent
    editor = workspace / "Root" / "Editor"
    frontend = workspace / "engine-web-prototype" / "frontend" / "src" / "themes.js"
    source = frontend.read_text(encoding="utf-8")
    token_count = 0
    for theme, following in (("classic", "sakura"), ("sakura", "slate")):
        section = source.split("{id:'" + theme + "'", 1)[1].split("{id:'" + following + "'", 1)[0]
        expected = dict(re.findall(r"'(--[^']+)'\s*:\s*'(#[^']+)'", section))
        actual = json.loads((editor / f"resources/ui/styles/{theme}.json").read_text(encoding="utf-8"))
        if not expected or actual != expected:
            raise SystemExit(f"Native {theme} palette differs from frontend themes.js")
        token_count += len(expected)

    # Older launcher/panels are outside this increment's migrated boundary.
    roots = [editor / "src/UI", editor / "inc/UI", editor / "src/Shell"]
    inline_style = re.compile(r'setStyleSheet\s*\(\s*(?:QStringLiteral\s*\(\s*)?"')
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.suffix in {".cpp", ".h"} and inline_style.search(path.read_text(encoding="utf-8-sig")):
                raise SystemExit(f"Inline styling in migrated UI: {path.relative_to(workspace)}")
    print(f"PASS: {token_count} frontend palette tokens (Classic + Sakura); no inline styles in migrated UI/Shell")


if __name__ == "__main__":
    main()
