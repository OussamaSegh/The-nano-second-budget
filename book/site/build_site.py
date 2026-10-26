#!/usr/bin/env python3
"""Build the book's website: a landing page with downloads and contents, and
the whole book as browsable HTML, one page per chapter.

  python3 build_site.py <manuscript-dir> <build-out-dir> <site-dir>

<build-out-dir> is the output of build-book/build.sh (the EPUB, the reading
PDF, the covers). The code archive is made here from the repository.
"""
import html, json, shutil, subprocess, sys, zipfile
from pathlib import Path
import yaml
from PIL import Image

HERE = Path(__file__).resolve().parent
BUILD = HERE.parent / "build-book"
REPO = HERE.parent.parent
sys.path.insert(0, str(BUILD))
import preprocess  # noqa: E402

MS, OUT, SITE = (Path(a).resolve() for a in sys.argv[1:4])
C = yaml.safe_load((BUILD / "edition.yaml").read_text())
E = html.escape

shutil.rmtree(SITE, ignore_errors=True)
(SITE / "assets").mkdir(parents=True)
(SITE / "downloads").mkdir()

# ---------------------------------------------------------------- the reader
tmp = OUT / "site-ms"
ms = preprocess.prepare(MS, tmp, C)
subprocess.run(["pandoc", *sorted(str(p) for p in ms.glob("*.md")), "-t", "chunkedhtml", "-o", str(SITE / "read"),
                "--split-level=2", "--toc", "--toc-depth=2", "--lua-filter", str(BUILD / "chapnum.lua"),
                "--template", str(HERE / "template.html"), "--resource-path", str(BUILD),
                "--highlight-style=monochrome", "--metadata", f"title={C['title']}", "--metadata", "lang=en"], check=True)
fig_src = BUILD / "figures"
if not fig_src.exists():
    subprocess.run([sys.executable, str(BUILD / "figures.py"), str(fig_src)], check=True)
shutil.copytree(fig_src, SITE / "read" / "figures", dirs_exist_ok=True)
sitemap = json.loads((SITE / "read" / "sitemap.json").read_text())

# ---------------------------------------------------------------- assets and downloads
shutil.copy(HERE / "site.css", SITE / "assets" / "site.css")
cover = OUT / "cover" / "cover-ebook.jpg"
im = Image.open(cover).convert("RGB")
im.resize((600, round(600 * im.height / im.width))).save(SITE / "assets" / "cover.jpg", quality=88)
im.resize((96, round(96 * im.height / im.width))).save(SITE / "assets" / "cover-thumb.jpg", quality=85)
for name in ("cover-wrap-preview.png",):
    if (OUT / "cover" / name).exists():
        shutil.copy(OUT / "cover" / name, SITE / "assets" / name)

code_zip = SITE / "downloads" / "llds-book-code.zip"
with zipfile.ZipFile(code_zip, "w", zipfile.ZIP_DEFLATED) as z:
    for top in ("CMakeLists.txt", "LICENSE", "README.md", "include", "examples", "exercises", "solutions", "tests", "sample-output"):
        p = REPO / top
        for f in ([p] if p.is_file() else sorted(p.rglob("*"))):
            if f.is_file():
                z.write(f, "llds-book-code/" + f.relative_to(REPO).as_posix())

downloads = [
    ("the-nanosecond-budget.pdf", OUT / "the-nanosecond-budget.pdf", "PDF", "Full colour, with bookmarks and links. Best on a computer or tablet."),
    ("the-nanosecond-budget.epub", OUT / "the-nanosecond-budget.epub", "EPUB", "Reflowable ebook for Apple Books, Kobo, Google Play and e-readers."),
    ("llds-book-code.zip", None, "Code", "The llds library, every example, the 25 exercises and their solutions (MIT)."),
]
def size(p):
    n = p.stat().st_size
    return f"{n / 1e6:.1f} MB" if n > 1e6 else f"{n / 1e3:.0f} kB"
cards = []
for fname, src, label, desc in downloads:
    dst = SITE / "downloads" / fname
    if src is not None:
        shutil.copy(src, dst)
    cards.append(f'<div class="card"><h3>{label}</h3><p>{E(desc)}</p>'
                 f'<a class="btn" href="downloads/{fname}" download>Download</a><span class="size">{size(dst)}</span></div>')
cards.append('<div class="card"><h3>Read online</h3><p>Every chapter as a web page, with a contents sidebar and next and previous links.</p>'
             '<a class="btn" href="read/index.html">Start reading</a></div>')

# ---------------------------------------------------------------- contents
def chapter_rows(sec):
    rows = []
    for sub in sec["subsections"]:
        s = sub["section"]
        title = s["title"]
        num, _, rest = title.partition(" ")
        if title.startswith("Chapter ") or title.startswith("Appendix "):
            kind, _, tail = title.partition(" ")
            n, _, rest = tail.partition(" ")
            num = n.rstrip(".:")
        elif title[:1] == "F" and title[1:3].isdigit():
            num, rest = title[:3], title[3:].lstrip(" .:")
        else:
            num, rest = "", title
        rows.append(f'<li><a href="read/{s["path"]}"><span class="n">{E(num)}</span>{E(rest)}</a></li>')
    return rows
parts = []
front = []
for top in sitemap["subsections"]:
    s = top["section"]
    t = s["title"]
    if t.lower().startswith("part ") and top["subsections"]:
        label, _, name = t.partition(". ")
        parts.append(f'<div class="part"><h3>{E(label)}: {E(name)}</h3><ol>{"".join(chapter_rows(top))}</ol></div>')
    elif top["subsections"] and t.lower().startswith("appendi"):
        parts.append(f'<div class="part"><h3>{E(t)}</h3><ol>{"".join(chapter_rows(top))}</ol></div>')
    else:
        front.append(f'<li><a href="read/{s["path"]}">{E(t)}</a></li>')
if front:
    parts.insert(0, f'<div class="part"><h3>Front and back matter</h3><ol>{"".join(front)}</ol></div>')

# a few charts, linked to the page that discusses them
featured = [("chart-spsc", "The SPSC ring, five steps to 55× the throughput"), ("chart-chase", "The memory hierarchy, measured"),
            ("chart-locks", "Six locks: throughput against the tail"), ("pet-ring", "A PET detector ring and a coincidence")]
fig_page = {}
for page in sorted((SITE / "read").glob("*.html")):
    t = page.read_text()
    for name, _ in featured:
        if name not in fig_page and f"figures/{name}.svg" in t:
            fig_page[name] = page.name
gallery = "".join(f'<a href="read/{fig_page.get(n, "index.html")}"><img src="read/figures/{n}.svg" alt="{E(cap)}" loading="lazy"/><span>{E(cap)}</span></a>'
                  for n, cap in featured)

desc = (BUILD / "description.txt").read_text().strip().split("\n\n")
blurb = []
for para in desc:
    lines = para.splitlines()
    if all(l.startswith("- ") for l in lines):
        blurb.append("<ul>" + "".join(f"<li>{E(l[2:])}</li>" for l in lines) + "</ul>")
    else:
        blurb.append(f"<p>{E(para)}</p>".replace("*The Nanosecond Budget*", "<em>The Nanosecond Budget</em>"))

page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{E(C['title'])}: {E(C['subtitle'])}</title>
<meta name="description" content="{E(C['subtitle'])}. {E(C['tagline'])}."/>
<meta property="og:title" content="{E(C['title'])}"/><meta property="og:description" content="{E(C['subtitle'])}"/>
<meta property="og:image" content="assets/cover.jpg"/>
<link rel="stylesheet" href="assets/site.css"/><link rel="icon" href="assets/cover-thumb.jpg"/>
</head>
<body>
<header class="bar"><a class="brand" href="index.html">{E(C['title'])}</a>
<nav class="links"><a href="#download">Download</a><a href="#contents">Contents</a><a href="read/index.html">Read online</a><a href="{E(C['code_url'])}">Code on GitHub</a></nav></header>
<section class="hero"><div class="inner">
<img class="cover" src="assets/cover.jpg" alt="Cover of {E(C['title'])}: a PET detector ring that is also a ring buffer"/>
<div>
<h1>{E(C['title'])}</h1>
<p class="sub">{E(C['subtitle'])}</p>
<p class="tag">{E(C['tagline'])}</p>
<p class="by">{E(C['author'])} · {E(C['edition'])}, version {E(str(C['version']))}</p>
<div class="btns"><a class="btn primary" href="downloads/the-nanosecond-budget.pdf" download>Download the PDF</a>
<a class="btn ghost" href="downloads/the-nanosecond-budget.epub" download>EPUB</a>
<a class="btn ghost" href="read/index.html">Read online</a>
<a class="btn ghost" href="{E(C['code_url'])}">Code</a></div>
</div></div></section>
<section class="wrap blurb">{''.join(blurb)}</section>
<section class="wrap" id="download"><h2>Download</h2><div class="cards">{''.join(cards)}</div></section>
<section class="wrap"><h2>Inside</h2><div class="gallery">{gallery}</div></section>
<section class="wrap" id="contents"><h2>Contents</h2><div class="parts">{''.join(parts)}</div></section>
<footer><div class="wrap">© {E(str(C['year']))} {E(C['author'])}. Text all rights reserved; companion code under the {E(C['code_licence'])}.
Source, errata and issues: <a href="{E(C['code_url'])}">{E(C['code_url'])}</a>.</div></footer>
</body></html>
"""
(SITE / "index.html").write_text(page)
(SITE / ".nojekyll").write_text("")
print(f"site in {SITE}: {len(list((SITE / 'read').glob('*.html')))} pages")
