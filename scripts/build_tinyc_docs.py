#!/usr/bin/env python3
"""Pre-build step for the TinyC docs site.

Populates `tinyc_docs/` with:
- reference.md / reference.de.md  (copied from tasmota/tinyc/TinyC_Reference*.md)
- custom-builds.md                (copied from tasmota/tinyc/TinyC_Custom_Builds.md)
- examples/index.md + examples/<name>.md  (one page per .tc file)
- releases.md                     (which release file is for which device)

User-authored pages (index.md, getting-started.md, gallery/*.md) are never overwritten.
"""
from __future__ import annotations

import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TINYC = ROOT / "tasmota" / "tinyc"
DOCS = ROOT / "tinyc_docs"
EXAMPLES_SRC = TINYC / "examples"
EXAMPLES_DST = DOCS / "examples"


def copy_reference() -> None:
    """Copy the long-form reference markdown files into the docs tree."""
    mapping = {
        "TinyC_Reference.md": "reference.md",
        "TinyC_Reference_DE.md": "reference.de.md",
        "TinyC_Custom_Builds.md": "custom-builds.md",
    }
    for src_name, dst_name in mapping.items():
        src = TINYC / src_name
        if not src.exists():
            print(f"  skip: {src} not found", file=sys.stderr)
            continue
        text = src.read_text(encoding="utf-8")
        # Rewrite cross-document markdown links to their built page names so
        # `mkdocs build --strict` resolves them. The source files keep the
        # repo-relative names (e.g. TinyC_Reference.md) so links also work when
        # browsing the docs directly on GitHub. Only link targets `](name)` are
        # rewritten — code spans like `TinyC_Reference.md` are left intact.
        for a, b in mapping.items():
            text = text.replace(f"]({a})", f"]({b})")
        # Guides in tasmota/tinyc/docs/ (Matter plugin, PicoTTS plugin, ...) are not pages of
        # this site: `mkdocs --strict` fails on the repo-relative link, so point to GitHub.
        text = re.sub(r"\]\(docs/([A-Za-z0-9_.\-]+\.md)\)",
                      r"](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/docs/\1)",
                      text)
        dst = DOCS / dst_name
        dst.write_text(text, encoding="utf-8")
        print(f"  {src.relative_to(ROOT)} -> {dst.relative_to(ROOT)}")


def generate_example_pages() -> None:
    """One markdown page per example with source embedded + optional screenshot."""
    EXAMPLES_DST.mkdir(parents=True, exist_ok=True)
    tc_files = sorted(EXAMPLES_SRC.glob("*.tc"))
    if not tc_files:
        print("  no .tc examples found", file=sys.stderr)
        return

    # Skip diag / test scratch files from public listing
    skip = {"bug4_http_taskloop_diag", "test_strlit_arg", "crash_test"}
    visible = [p for p in tc_files if p.stem not in skip]

    index_lines = [
        "# Examples",
        "",
        f"{len(visible)} example programs covering sensors, displays, networking, and device drivers.",
        "",
        "Source lives in [`tasmota/tinyc/examples/`](https://github.com/gemu2015/Sonoff-Tasmota/tree/universal/tasmota/tinyc/examples). "
        "Pre-compiled bytecode is available from the in-device program repository (*TinyCChkpt* → Download).",
        "",
        "| Example | Summary |",
        "|---------|---------|",
    ]

    for tc in visible:
        name = tc.stem
        summary = _first_comment_line(tc)
        index_lines.append(f"| [`{name}.tc`]({name}.md) | {summary} |")

        source = tc.read_text(encoding="utf-8", errors="replace")
        screenshot_md = _screenshot_block(name)

        page = [
            f"# {name}.tc",
            "",
            summary,
            "",
            screenshot_md,
            f"[Source on GitHub](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/examples/{name}.tc)",
            "",
            "```c",
            source.rstrip(),
            "```",
            "",
        ]
        (EXAMPLES_DST / f"{name}.md").write_text("\n".join(page), encoding="utf-8")

    (EXAMPLES_DST / "index.md").write_text("\n".join(index_lines) + "\n", encoding="utf-8")
    print(f"  wrote {len(visible)} example pages + index")


def _first_comment_line(tc_path: Path) -> str:
    """Use the first meaningful `//` comment line as the example summary.

    Skips decorative banner lines (purely punctuation / box-drawing characters)
    and skips `// @directive:` metadata lines like `// @defines: -DBOARD_X`.
    """
    import re
    for raw in tc_path.read_text(encoding="utf-8", errors="replace").splitlines():
        s = raw.strip()
        if s.startswith("//"):
            text = s.lstrip("/").strip()
            if not text:
                continue
            # Skip decorative banners (no letters or digits)
            if not re.search(r"[A-Za-z0-9]", text):
                continue
            # Skip @directive metadata lines
            if text.startswith("@"):
                continue
            return text
        elif s:
            break
    return "_(no description — see source)_"


def _screenshot_block(name: str) -> str:
    """Emit an image tag if tinyc_docs/images/examples/<name>.{png,jpg,gif} exists."""
    for ext in ("png", "jpg", "jpeg", "gif", "webp"):
        img = DOCS / "images" / "examples" / f"{name}.{ext}"
        if img.exists():
            return f"![{name}](../images/examples/{name}.{ext}){{ loading=lazy }}\n"
    return ""


RELEASES_PAGE = r"""# Releases

Pre-built firmware, the browser IDE bundle, plugins and documentation for each build live on the [GitHub Releases page](https://github.com/gemu2015/Sonoff-Tasmota/releases). Every build has its own `v<version>` release; the `testing` tag is a rolling tag that always carries the newest files, for stable download URLs.

## Which file is for me

| File | What it is |
|------|------------|
| `tinyc32-4M-plain` | ESP32 4 MB classic WROOM — plain (no Matter, no camera) |
| `tinyc32s3` | ESP32-S3 — Matter built in, camera, LVGL |
| `tinyc32c3`, `tinyc32c6` | ESP32-C3 / C6 — Matter as a plugin |
| `tinyc32-p4-full` | ESP32-P4 16 MB + PSRAM — display + LVGL, MIPI camera, audio; Matter as a plugin |
| `tinyc8266-4M` | ESP8266 4 MB — lean (no Matter, LVGL or camera), one VM slot; only a `.bin` / `.bin.gz` |
| `MATTERF_32.bin` / `MATTERF_32r.bin` | Matter plugin — `_32` for the S3, `_32r` for C3 / C6 / P4 ([how to install](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/docs/MATTER_PLUGIN.md)) |
| `I2SAUDIO_32.bin`, `PICOTTS_32.bin` | Audio and text-to-speech plugins for the S3 / ESP32 ([PICOTTS](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/docs/PICOTTS_PLUGIN.md)) |
| `tinyc_ide.html.gz` | The browser IDE — a file for the device's file system |

`.bin` is for updating a running device (Firmware Upgrade page, or `OtaUrl` + `Upgrade 1`); `.factory.bin` is for a fresh install over a cable and replaces the file system. Plugins need a plugin partition (`chkpt a`), which also formats the file system — read the plugin guide first.

Upload the matching file for your chip, then update the IDE on the device (`TinyCIde`); see [Getting Started](getting-started.md). What changed in each version is in the [changelog](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/CHANGELOG.md).
"""


def write_releases_page() -> None:
    """The page is generated (and git-ignored), so the text lives here. It is rewritten on every
    build: a copy left over from an older build must not survive."""
    path = DOCS / "releases.md"
    path.write_text(RELEASES_PAGE, encoding="utf-8")
    print(f"  wrote {path.relative_to(ROOT)}")


def main() -> int:
    if not TINYC.exists():
        print(f"error: {TINYC} not found", file=sys.stderr)
        return 1
    DOCS.mkdir(exist_ok=True)
    print("Populating tinyc_docs/:")
    copy_reference()
    generate_example_pages()
    write_releases_page()
    return 0


if __name__ == "__main__":
    sys.exit(main())
