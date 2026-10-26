# Building the book

```
book/build-book/build.sh book/manuscript out        # print interior, colour interior, cover, EPUB, reading PDF
python3 book/site/build_site.py book/manuscript out site   # the website, with downloads
```

| Output | What it is |
|---|---|
| `out/interior.pdf` | Paperback interior, 7.5 × 9.25 in, black and white, for KDP Print or IngramSpark |
| `out/cover/cover-wrap.pdf` | Paperback cover with spine and bleed; the spine width follows the page count |
| `out/the-nanosecond-budget.pdf` | Reading PDF in colour, with the front cover, bookmarks and links |
| `out/the-nanosecond-budget.epub` | EPUB 3 ebook (EPUBCheck clean, EPUB Accessibility 1.1 metadata) |
| `site/` | The website: landing page, downloads, every chapter as HTML |

Everything that names the edition (title, author, ISBNs, version, code URL, trim size, paper) is in [`build-book/edition.yaml`](build-book/edition.yaml). [`PUBLISHING.md`](PUBLISHING.md) has the store listing and the upload checklist.

**Requirements:** Pandoc 3.1.3, Python 3 with PyYAML, Beautiful Soup, pypdf and Pillow, Node.js with Playwright's Chromium, poppler-utils, and the Inter and JetBrains Mono system fonts (`fonts-inter`, `fonts-jetbrains-mono` on Ubuntu). The [Book workflow](../.github/workflows/book.yml) installs exactly these.

**Figures** are drawn by [`build-book/figures.py`](build-book/figures.py), in colour for the EPUB, the reading PDF and the website, and in gray for the print interior (`--gray`). The charts plot the measurements printed in the text.
