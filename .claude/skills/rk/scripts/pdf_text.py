#!/usr/bin/env python3
"""
pdf_text.py — extract text from a Rockchip SDK PDF, optionally narrowed to a
page range or filtered to a keyword's neighborhood.

Reason this exists: a typical Rockchip developer guide is 30–200 pages. Reading
the whole thing into the agent's context wastes tokens; reading nothing forces
the agent to guess. This script lets the agent ask for *just the pages that
matter*, either by page numbers it learned from the TOC or by grepping for a
keyword and pulling a window of context around each hit.

Backed by pypdf (handles Chinese text reliably; same library used by the
companion `pdf` skill, so no extra system deps needed).

Usage:
    pdf_text.py <pdf_path>                          # whole document
    pdf_text.py <pdf_path> --pages 5-12             # page range (1-based)
    pdf_text.py <pdf_path> --pages 5,8,11           # specific pages
    pdf_text.py <pdf_path> --grep "pinctrl-names"   # 30-line window around each hit
    pdf_text.py <pdf_path> --grep "iomux" --window 50
    pdf_text.py <pdf_path> --toc                    # first 8 pages (usually the TOC)
    pdf_text.py <pdf_path> --outline                # embedded outline/bookmarks if any

Output is plain text on stdout, with `=== page N ===` separators so the agent
can cite page numbers in distilled notes.
"""

import argparse
import re
import sys
from pathlib import Path

from pypdf import PdfReader


def parse_pages(spec: str, total: int) -> list[int]:
    pages: set[int] = set()
    for chunk in spec.split(","):
        chunk = chunk.strip()
        if "-" in chunk:
            a, b = chunk.split("-", 1)
            for p in range(int(a), int(b) + 1):
                pages.add(p)
        else:
            pages.add(int(chunk))
    return sorted(p for p in pages if 1 <= p <= total)


def page_text(reader: PdfReader, page_idx: int) -> str:
    try:
        return reader.pages[page_idx].extract_text() or ""
    except Exception as e:  # extraction can fail on individual pages; keep going
        return f"(extract_text failed on page {page_idx + 1}: {e})"


def extract_pages(reader: PdfReader, pages: list[int]) -> str:
    out = []
    for p in pages:
        out.append(f"\n=== page {p} ===\n")
        out.append(page_text(reader, p - 1))
    return "".join(out)


def grep_context(reader: PdfReader, pattern: str, window: int) -> str:
    """Find `pattern` page-by-page and emit a window of lines around each hit.
    Iterating page-by-page lets us tag every hit with its page number."""
    rx = re.compile(pattern, re.IGNORECASE)
    out: list[str] = []
    hits = 0
    for p_idx in range(len(reader.pages)):
        text = page_text(reader, p_idx)
        lines = text.splitlines()
        for i, line in enumerate(lines):
            if rx.search(line):
                hits += 1
                lo = max(0, i - window // 2)
                hi = min(len(lines), i + window // 2 + 1)
                out.append(f"\n=== page {p_idx + 1}, hit {hits} ({pattern!r}) ===\n")
                out.append("\n".join(lines[lo:hi]))
                out.append("\n")
    if not hits:
        return f"(no matches for {pattern!r})\n"
    return f"# {hits} match(es) for {pattern!r}\n" + "".join(out)


def extract_outline(reader: PdfReader) -> str:
    """Walk the embedded outline tree. Many Rockchip guides have one; if not
    we just print nothing and the caller should fall back to --toc."""
    lines: list[str] = []

    def walk(items, depth: int) -> None:
        for it in items:
            if isinstance(it, list):
                walk(it, depth + 1)
                continue
            try:
                page = reader.get_destination_page_number(it) + 1
            except Exception:
                page = "?"
            lines.append(f"{'  ' * depth}- p.{page}  {it.title}")

    try:
        walk(reader.outline, 0)
    except Exception as e:
        return f"(outline extraction failed: {e})\n"
    return ("\n".join(lines) + "\n") if lines else "(no embedded outline)\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pdf", type=Path)
    ap.add_argument("--pages", help="page range, e.g. '5-12' or '5,8,11'")
    ap.add_argument("--grep", help="keyword/regex; emits a window of lines around each hit")
    ap.add_argument("--window", type=int, default=30, help="lines of context around each grep hit (default 30)")
    ap.add_argument("--toc", action="store_true", help="extract first 8 pages (usually the TOC)")
    ap.add_argument("--outline", action="store_true", help="dump embedded outline/bookmarks")
    args = ap.parse_args()

    if not args.pdf.exists():
        print(f"error: {args.pdf} not found", file=sys.stderr)
        return 1

    reader = PdfReader(str(args.pdf))
    total = len(reader.pages)

    if args.outline:
        sys.stdout.write(extract_outline(reader))
        return 0

    if args.toc:
        sys.stdout.write(extract_pages(reader, list(range(1, min(8, total) + 1))))
        return 0

    if args.grep:
        sys.stdout.write(grep_context(reader, args.grep, args.window))
        return 0

    if args.pages:
        pages = parse_pages(args.pages, total)
        sys.stdout.write(extract_pages(reader, pages))
        return 0

    sys.stdout.write(extract_pages(reader, list(range(1, total + 1))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
